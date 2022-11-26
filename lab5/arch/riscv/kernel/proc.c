#include "proc.h"
#include "mm.h"
#include "rand.h"
#include "printk.h"
#include "defs.h"
#include "string.h"
#include "vm.h"

extern void __dummy();
extern uint64_t uapp_start;
extern uint64_t uapp_end;
extern unsigned long* swapper_pg_dir;

struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

void task_init() {
    // 1. 调用 kalloc() 为 idle 分配一个物理页
    // 2. 设置 state 为 TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    // 4. 设置 idle 的 pid 为 0
    // 5. 将 current 和 task[0] 指向 idle

    /* YOUR CODE HERE */
    uint64_t addr_idle = kalloc();
    idle = (struct task_struct*)addr_idle;
    idle->state = TASK_RUNNING;
    idle->counter = idle->priority = 0;
    idle->pid = 0;
    idle->pgd = swapper_pg_dir; // page table for idle is the kernel root page
    idle->thread.sscratch = 0;

    current = task[0] = idle;

    // for each user process
    for(int i = 1; i < NR_TASKS; i++) {
        uint64_t task_addr = kalloc();
        task[i] = (struct task_struct*)task_addr;
        task[i]->state = TASK_RUNNING;
        task[i]->counter = 0;
        task[i]->priority = rand() % 10 + 1;
        task[i]->pid = i;

        // copy the user code to a new page
        uint64_t pg_num = ((uint64_t)(&uapp_end) - (uint64_t)(&uapp_start) - 1) / PGSIZE + 1; // compute # of pages for code
        uint64_t uapp_new = alloc_pages(pg_num); // allocate new space for copied code
        memcpy((void*)(uapp_new), (void*)(&uapp_start), pg_num * PGSIZE); // copy code
        uint64_t u_stack_begin = alloc_page(); // allocate U-mode stack

        // config page table
        task[i]->pgd = (pagetable_t)alloc_page();
        // copy the root page
        copy_mapping(task[i]->pgd, (pagetable_t)(&swapper_pg_dir));
        task[i]->pgd = (pagetable_t)VA2PA((uint64_t)task[i]->pgd);
        // note the U bits for the following PTEs are set to 1
        // mapping of user text segment
        create_mapping((uint64*)PA2VA((uint64_t)task[i]->pgd), uapp_new, VA2PA(uapp_new), pg_num, 13);
        // mapping of user stack segment
        create_mapping((uint64*)PA2VA((uint64_t)task[i]->pgd), u_stack_begin, VA2PA(u_stack_begin), 1, 11);

        // set CSRs
        task[i]->thread.sepc = uapp_new; // set sepc
        task[i]->thread.sstatus = (1 << 18) | (1 << 5); // set SPP = 0, SPIE = 1, SUM = 1
        task[i]->thread.sscratch = u_stack_begin + PGSIZE; // U-mode stack end (initial sp)

        task[i]->thread.ra = (uint64_t)__dummy;
        task[i]->thread.sp = task_addr + PGSIZE; // initial kernel stack pointer
    }

    printk("...proc_init done!\n");
}

void dummy() {
    uint64_t MOD = 1000000007;
    uint64_t auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if (last_counter == -1 || current->counter != last_counter) {
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            // printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
            printk("[PID = %d] is running. thread space begin at 0x%016lx\n", current->pid, current);
        }
    }
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