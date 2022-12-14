#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "vm.h"
#include "elf.h"

extern void __dummy();
extern uint64_t uapp_start;
extern uint64_t uapp_end;
extern unsigned long* swapper_pg_dir;

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

static uint64_t load_elf_program(struct task_struct* task) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)(&uapp_start);

    uint64_t phdr_start = (uint64_t)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr* phdr;
    int load_phdr_cnt = 0;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // initialize vma
            // compute # of pages for code
            uint64_t pg_num = (PGOFFSET(phdr->p_vaddr) + phdr->p_memsz - 1) / PGSIZE + 1;
            do_mmap(task, phdr->p_vaddr, pg_num * PGSIZE, (phdr->p_flags << 1), (uint64_t)&uapp_start, phdr->p_offset, phdr->p_filesz);
        }
    }

    // initialize vma for stack
    do_mmap(task, USER_END - PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, (uint64_t)&uapp_start, 0, 0);

    // following code has been written for you
    // pc for the user program
    task->thread.sepc = ehdr->e_entry; // the program starting address
    // sstatus bits set
    task->thread.sstatus = (1 << 18) | (1 << 5);
    // user stack for user program
    task->thread.sscratch = USER_END;
}

static uint64_t load_binary_program(struct task_struct* task) {
    // copy the user code to a new page
    uint64_t pg_num = ((uint64_t)(&uapp_end) - (uint64_t)(&uapp_start) - 1) / PGSIZE + 1; // compute # of pages for code
    uint64_t uapp_new = alloc_pages(pg_num); // allocate new space for copied code
    memcpy((void*)(uapp_new), (void*)(&uapp_start), pg_num * PGSIZE); // copy code
    uint64_t u_stack_begin = alloc_page(); // allocate U-mode stack

    // note the U bits for the following PTEs are set to 1
    // mapping of user text segment
    create_mapping((uint64*)PA2VA((uint64_t)task->pgd), 0, VA2PA(uapp_new), pg_num, 31);
    // mapping of user stack segment
    create_mapping((uint64*)PA2VA((uint64_t)task->pgd), USER_END - PGSIZE, VA2PA(u_stack_begin), 1, 23);

    // set CSRs
    task->thread.sepc = 0; // set sepc at user space
    task->thread.sstatus = (1 << 18) | (1 << 5); // set SPP = 0, SPIE = 1, SUM = 1
    task->thread.sscratch = USER_END; // U-mode stack end (initial sp)
}

void task_init() {
    memset(task, 0, NR_TASKS * sizeof(task));

    uint64_t addr_idle = kalloc();
    idle = (struct task_struct*)addr_idle;
    idle->state = TASK_RUNNING;
    idle->counter = idle->priority = 0;
    idle->pid = 0;
    idle->pgd = swapper_pg_dir; // page table for idle is the kernel root page
    idle->thread.sscratch = 0;

    current = task[0] = idle;

    // only initialize one task
    uint64_t task_addr = kalloc();
    task[1] = (struct task_struct*)task_addr;
    task[1]->state = TASK_RUNNING;
    task[1]->counter = 0;
    task[1]->priority = rand() % 10 + 1;
    task[1]->pid = 1;

    // config page table
    task[1]->pgd = (pagetable_t)alloc_page();
    // copy the root page
    memcpy((void*)(task[1]->pgd), (void*)((&swapper_pg_dir)), PGSIZE);
    task[1]->pgd = (pagetable_t)VA2PA((uint64_t)task[1]->pgd); // turn physical address
    load_elf_program(task[1]);
    // load_binary_program(task[i]);

    task[1]->thread.ra = (uint64_t)__dummy;
    task[1]->thread.sp = task_addr + PGSIZE; // initial kernel stack pointer

    printk("...proc_init done!\n");
}


void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t file_offset_on_disk, uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file) {
    struct vm_area_struct* vma = &(task->vmas[task->vma_cnt++]);
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = flags;
    vma->file_offset_on_disk = file_offset_on_disk;
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
    vma->if_alloc = 0;
}

// find the VMA that contains the addr (null if not found)
struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr) {
    for (int i = 0; i < task->vma_cnt; i++) {
        if (task->vmas[i].vm_start <= addr && addr < task->vmas[i].vm_end) {
            return &(task->vmas[i]);
        }
    }
    return NULL;
}


extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next) {
    if (next == current) return; // switching to current is needless
    else {
        struct task_struct* current_saved = current;
        current = next;
        __switch_to(current_saved, next);
    }
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    if (current == task[0]) schedule();
    else {
        current->counter -= 1;
        if (current->counter == 0) schedule();
    }
}

// #define DSJF
#ifdef DSJF 
void schedule(void){
    uint64_t min_count = INF;
    struct task_struct* next = NULL;
    char all_zeros = 1;
    for(int i = 1; i < NR_TASKS; i++){
        if (task[i] == NULL) continue;
        if (task[i]->state == TASK_RUNNING && task[i]->counter > 0) {
            if (task[i]->counter < min_count) {
                min_count = task[i]->counter;
                next = task[i];
            }
            all_zeros = 0;
        }
    }

    if (all_zeros) {
        printk("\n");
        for(int i = 1; i < NR_TASKS; i++){
            if (task[i] == NULL) continue;
            task[i]->counter = rand() % 10 + 1;
            printk("SET [PID = %d COUNTER = %d]\n", task[i]->pid, task[i]->counter);
        }
        schedule();
    }
    else {
        if (next) {
            printk("\nswitch to [PID = %d COUNTER = %d]\n", next->pid, next->counter);
            switch_to(next);
        }
    }
}
#endif

#ifdef DPRIORITY
void schedule(void){
    uint64_t c, i, next;
    struct task_struct** p;
	while(1) {
		c = 0;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while(--i) {
			if (!*--p) continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
        
		if (c) break;
        // else all counters are 0s
        printk("\n");
        for(p = &task[NR_TASKS-1]; p > &task[0]; --p) {
            if (*p) {
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", (*p)->pid, (*p)->priority, (*p)->counter);
            }
        }
	}
    
    printk("\nswitch to [PID = %d PRIORITY = %d COUNTER = %d]\n", task[next]->pid, task[next]->priority, task[next]->counter);
	switch_to(task[next]);
}
#endif