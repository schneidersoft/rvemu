# RVEMU

An emulator for the risc-v 64bit integer core.

# compiling code for this machine

There is a convenient startup assembly file and linker script.

The core has 1 Mib of RAM and the entrypoint is assumed to be 0x00000000

Compile for this machine using `-mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld test/startup.rv64i.s`

# using the library

You can link agains rv64i like so:

`gcc -Wall -Werror -I./test/ main.c bin/librv64i.a -o main`

see riscv64i.c for an example implementation

# syscall

This core does not support breakpoints. So you will need execute ebreak or ecall from C. console ouput and other syscalls can be achieved this way.
 
# rv64im

This core also supports rv64im instructions. So you should be able to compile with `-march=rv64im -mabi=lp64` as well
