#ifndef _PROC_H_
#define _PROC_H_
#include "types.h"
#include "stdint.h"

#define NR_TASKS  (1 + 15) // 用于控制 最大线程数量 （idle 线程 + 31 内核线程）

#define TASK_RUNNING    0 // 为了简化实验, 所有的线程都只有一种状态

#define PRIORITY_MIN 1
#define PRIORITY_MAX 10

#define PTE_U_MASK 0x00000000000000016
#define PTE_X_MASK 0x0000000000000008
#define PTE_W_MASK 0x0000000000000004
#define PTE_R_MASK 0x0000000000000002
#define PTE_V_MASK 0x0000000000000001

#define VM_X_MASK         0x0000000000000008
#define VM_W_MASK         0x0000000000000004
#define VM_R_MASK         0x0000000000000002
#define VM_ANONYM         0x0000000000000001

struct vm_area_struct {
    uint64_t vm_start;
    uint64_t vm_end;
    uint64_t vm_flags; // oring masks VM_?_MASK and VM_ANONYM
    uint64_t file_offset_on_disk;
    uint64_t vm_content_offset_in_file;
    uint64_t vm_content_size_in_file;
    char if_alloc; // whether this vm_area has part of memory allocated
};

struct thread_struct {
    uint64_t ra;
    uint64_t sp;                     
    uint64_t s[12];

    uint64_t sepc, sstatus, sscratch; 
};

struct task_struct {
    uint64_t state;
    uint64_t counter;
    uint64_t priority;
    uint64_t pid;

    struct thread_struct thread;
    pagetable_t pgd;

    uint64_t vma_cnt;
    struct vm_area_struct vmas[0];
};

struct pt_regs {
    uint64_t reg[31];
    uint64_t sepc, sstatus, stval, sscratch, scause;
};

static uint64_t load_elf_program(struct task_struct*);

static uint64_t load_binary_program(struct task_struct*);

/* 线程初始化 创建 NR_TASKS 个线程 */
void task_init();

/* 创建一个新的 vma */
void do_mmap(struct task_struct *task, uint64_t addr, uint64_t length, uint64_t flags,
    uint64_t file_offset_on_disk, uint64_t vm_content_offset_in_file, uint64_t vm_content_size_in_file);

/* 查找包含某个 addr 的 vma */
struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr);

/* 在时钟中断处理中被调用 用于判断是否需要进行调度 */
void do_timer();

/* 调度程序 选择出下一个运行的线程 */
void schedule();

/* 线程切换入口函数*/
void switch_to(struct task_struct* next);

/* dummy funciton: 一个循环程序, 循环输出自己的 pid 以及一个自增的局部变量 */
void dummy();

#endif