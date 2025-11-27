    .section .init
    .globl _start
    .align  4

_start:
    # Set stack pointer
    la  sp, _stack_top

    # Zero .bss section
    la  t0, _bss_start
    la  t1, _bss_end
loop1:
    bge t0, t1, exit2
    sd  zero, 0(t0)
    addi t0, t0, 8
    j   loop1
exit2:

    # Call main()
    la  a0, 0          # argc = 0
    la  a1, 0          # argv = NULL
    call main

    # If main returns, trap with ebreak
    ebreak
