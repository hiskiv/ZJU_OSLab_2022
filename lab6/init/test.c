#include "printk.h"
#include "defs.h"
#include "proc.h"

extern struct task_struct* task[NR_TASKS];

void test() {
    printk("idle process is running!\n");
    while(1) {}
    // switch_to(task[1]);
    // size_t print_loop = 0;
    // while (1) {
    //     if (print_loop == 200000000) {
    //         printk("kernel is running!\n");
    //         print_loop = 0;
    //     } else print_loop++;
    // }
}
