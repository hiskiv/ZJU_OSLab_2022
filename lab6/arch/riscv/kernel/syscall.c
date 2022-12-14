#include "syscall.h"
#include "defs.h"
#include "printk.h"
#include "proc.h"
#include "string.h"
#include "vm.h"
#include "mm.h"

extern struct task_struct* current;

long sys_write(unsigned int fd, const char* buf, int count) {
    uint64_t res = 0;
    for (int i = 0; i < count; i++) {
        if (fd == 1) {
            printk("%c", buf[i]);
            res++;
        }
    }
    return res;
}

long sys_getpid() {
    return current->pid;
}

extern struct task_struct* task[];
extern uint64_t __ret_from_fork;
extern unsigned long* swapper_pg_dir;

long sys_clone(struct pt_regs *regs) {
    /*
     1. 参考 task_init 创建一个新的 task, 将的 parent task 的整个页复制到新创建的 
        task_struct 页上(这一步复制了哪些东西?）。将 thread.ra 设置为 
        __ret_from_fork, 并正确设置 thread.sp
        (仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推)

     2. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
        并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)

     3. 为 child task 申请 user stack, 并将 parent task 的 user stack 
        数据复制到其中。 

     4. 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射

     5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存

     6. 返回子 task 的 pid
    */
    
    // create and initialize a new task
    int pid = 1; // the new pid
    while(task[pid] && pid < NR_TASKS) pid++; // find the first empty task_struct
    if (pid == NR_TASKS) { // no more room, fork fail
        printk("Maximum threads number exceeds.\n");
        return -1;
    }

    uint64_t task_addr = alloc_page(); // new PCB address
    task[pid] = (struct task_struct*)task_addr;
    memcpy(task[pid], current, PGSIZE); // copy PCB from father
    task[pid]->pid = pid;
    task[pid]->thread.ra = (uint64_t)(&__ret_from_fork);
    
    struct pt_regs *child_regs = (struct pt_regs*)(task_addr + PGOFFSET((uint64_t)regs));
    task[pid]->thread.sp = (uint64_t)child_regs; // child's kernel sp setting
    child_regs->reg[9] = 0; // child process return 0
    child_regs->reg[1] = task[pid]->thread.sp; // pt_regs sp must be also be changed
    // because we saved the kernel sp into pt_regs before calling trap_handler
    child_regs->sepc = regs->sepc + 4; // the next inst after fork

    // config page table
    task[pid]->pgd = (pagetable_t)alloc_page();
    // copy the root page
    memcpy((void*)(task[pid]->pgd), (void*)((&swapper_pg_dir)), PGSIZE);
    task[pid]->pgd = (pagetable_t)VA2PA((uint64_t)task[pid]->pgd); // turn physical address
    for (int i = 0; i < current->vma_cnt; i++) {
        struct vm_area_struct *vma = &(current->vmas[i]);
        if (vma->if_alloc) { // if some of pages are allocated
            uint64_t copy_head = vma->vm_start;
            while (copy_head < vma->vm_end) {
                if (!has_mapping((pagetable_t)PA2VA((uint64_t)current->pgd), PGROUNDDOWN(copy_head))) {
                    copy_head += PGSIZE; // if no mapping
                    continue;
                }
                uint64_t page = alloc_page();
                create_mapping((uint64*)PA2VA((uint64_t)task[pid]->pgd), PGROUNDDOWN(copy_head), VA2PA(page), 1, (vma->vm_flags & (~(uint64_t)VM_ANONYM)) | PTE_U_MASK | PTE_V_MASK);
                memcpy((void*)page, (void*)PGROUNDDOWN(copy_head), PGSIZE); // using kernel virtual address to copy
                copy_head += PGSIZE;
            }
        }
    }

    return pid;
}
