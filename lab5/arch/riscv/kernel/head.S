    .extern start_kernel
    .section .text.init
    .globl _start
_start:
    # la sp, boot_stack_top # set the stack pointer
    li t0, 0xffffffdf80000000
    la sp, boot_stack_top
    sub sp, sp, t0

    jal ra, setup_vm # set up vitual memory (just a transition)
    jal ra, relocate
    
    jal ra, mm_init # initialize physical memory

    jal ra, setup_vm_final # set up real virtual memory
    
    jal ra, task_init # initialize task threads

    # set stvec
    la t0, _traps
    csrw stvec, t0

    # set on STIE bit of sie
    csrr t0, sie
    ori t0, t0, 0x20
    csrw sie, t0

    # set on SIE bit and SPIE of sstatus
    csrr t0, sstatus
    # ori t0, t0, 0x22
    ori t0, t0, 0x20  # don't set SIE: schedule() can be done at the beginning
    csrw sstatus, t0

    # set first time interrupt
    jal ra, clock_set_next_event

    jal x0, start_kernel  # jump to start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)

    ###################### 
    #   YOUR CODE HERE   #
    ######################

    # calculate 0xffffffdf80000000 (PA2VA_OFFSET) in t0
    li t0, 0xffffffdf80000000

    add ra, ra, t0 # set ra = ra + PA2VA_OFFSET
    add sp, sp, t0 # set sp = sp + PA2VA_OFFSET

    # set satp with early_pgtbl
    la t2, early_pgtbl
    sub t2, t2, t0
    add t1, x0, t2
    srli t1, t1, 12
    # MODE field = 8
    li t0, 0x8000000000000000
    or t1, t1, t0
    csrw satp, t1

    ###################### 
    #   YOUR CODE HERE   #
    ######################

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 * 4

    .globl boot_stack_top
boot_stack_top:
