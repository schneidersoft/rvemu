# RVEMU

An emulator for the risc-v 64bit integer core.

Compile for this machine using `-mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld test/startup.rv64i.s`

The core has 1 Mib of RAM entrypoint is assumed to be 0x00000000

