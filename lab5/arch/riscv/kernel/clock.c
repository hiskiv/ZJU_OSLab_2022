#include "sbi.h"
#include "clock.h"

// Clock frequency in QEMU = 10MHz
// ==> 1 second = 10000000 cycles
unsigned long TIMECLOCK = 10000000;

unsigned long get_cycles() {
    // using rdtime pseudo-inst to read value in mtime register
    unsigned long ret;
    __asm__ volatile (
        "rdtime %[ret]\n": [ret] "=r" (ret) ::
    );
    return ret;
}

void clock_set_next_event() {
    // set the register mtimecmp,
    // next interrupt happens TIMECLOCK cycles later
    unsigned long next = get_cycles() + TIMECLOCK;
    sbi_ecall(0, 0, next, 0, 0, 0, 0, 0);
} 
