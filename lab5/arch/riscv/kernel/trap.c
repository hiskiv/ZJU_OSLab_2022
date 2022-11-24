#include "clock.h"
#include "printk.h"
#include "types.h"
#include "proc.h"

void trap_handler(uint64 scause, uint64 sepc) {
    uint64 interrupt_sig = 0x8000000000000000;
    if (scause & interrupt_sig){ // it's interrupt
        scause = scause - interrupt_sig;
        unsigned long timer_int = 0x5;
        if (!(scause ^ timer_int)){ // it's Supervisor timer interrupt
            clock_set_next_event();
            do_timer();
        }
    }
}
