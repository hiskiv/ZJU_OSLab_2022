#include "syscall.h"
#include "printk.h"

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