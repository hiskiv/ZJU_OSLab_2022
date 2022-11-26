#include "clock.h"
#include "printk.h"
#include "types.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

void trap_handler(uint64 scause, uint64 sepc, struct pt_regs *regs) {
    char catch = 0;
    uint64 interrupt_sig = 0x8000000000000000;
    if (scause & interrupt_sig) { // it's interrupt
        scause = scause - interrupt_sig;
        unsigned long timer_int = 0x5;
        if (!(scause ^ timer_int)) { // it's Supervisor timer interrupt
            clock_set_next_event();
            do_timer();
            catch = 1;
        }
    }
    else { // it's exception
        unsigned long ecall_from_umode = 0x8;
        if (scause == ecall_from_umode) { // it's environmental call from U-mode
            switch (regs->reg[16]) { // x17: ecall number, x9: a0 (return value)
                case SYS_WRITE:
                    regs->reg[9] = sys_write(regs->reg[9], (const char*)regs->reg[10], regs->reg[11]);
                    break;
                case SYS_GETPID:
                    regs->reg[9] = sys_getpid();
                    break;
            }
            regs->sepc += 4; // return to the next inst after ecall
            catch = 1;
        }
    }
    
    if (!catch) {
        printk("unhandled trap: %llx\n", scause);
        // printk("%lx\n", regs->...);
        printk("%lx\n", sepc);
        while (1);
    }
}
