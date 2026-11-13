# RVEMU

RVEMU is an emulator for the risc-v 64bit integer core processor. It is written in C and has virtually no dependancies.

# compiling code for this machine

There is a convenient startup assembly file and linker script.

The core has 1 Mib of RAM and the entrypoint is assumed to be 0x00000000

Compile for this machine using `-mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld test/startup.rv64i.s`

You may also use medany but will need to adjust the dram size.

# bundled standard library

`test/stdlib` contains the small set of standard headers and a minimal `stdio`
implementation used by the bare-metal tests. Its declarations and exported
stdio functions use the normal C names, so code built against it can also be
linked against the system C library.

`stdin`, `stdout`, and `stderr` are real buffered `FILE` streams backed by file
descriptors 0, 1, and 2. `stdout` is line-buffered and `stderr` is unbuffered.
`fprintf`, `fputc`, `fputs`, `fwrite`, `fflush`, `fgetc`, `fgets`, and `fread`
operate on them. Ports provide the low-level `_read()` and `_write()` hooks;
`test/platform.c` implements these with Linux syscalls for the guest tests.

# using the library

You can link against rv64i like so:

`gcc -Wall -Werror -I./test/ main.c bin/librv64i.a -o main`

see riscv64i.c for an example implementation

# syscalls

Guest code can use the standard Linux RISC-V syscall ABI:

- `a7` holds the syscall number.
- `a0` through `a5` hold up to six arguments.
- The result is returned in `a0`; failures are negative `errno` values (`-EFAULT`, `-ENOSYS`, and so on).

The emulator forwards supported calls to the host operating system. Guest
pointers are checked against the 1 MiB DRAM window before they are used, and
invalid buffers produce `-EFAULT`.

Every `ecall` is treated as a Linux syscall; there is no separate private
ECALL ABI. Unknown syscall numbers, including 0, return `-ENOSYS`. `ebreak`
exits successfully and is used by the test programs to terminate.

The syscall proxy currently covers:

- Files and descriptors: `read`, `write`, `readv`, `writev`, `pread64`,
  `pwrite64`, `openat`, `close`, `lseek`, `dup`, `dup3`, `fcntl`, `ioctl`
  (limited), `pipe2`, `getdents64`, `fstat`, `newfstatat`, `getcwd`, `chdir`,
  `mkdirat`, `unlinkat`, `symlinkat`, `linkat`, `renameat`, `faccessat`.
- Memory: `brk`, anonymous `mmap`, `munmap`, `mprotect`, `madvise`.
- Time: `clock_gettime`, `clock_getres`, `gettimeofday`, `nanosleep`.
- Process and system information: `getpid`, `getppid`, `getuid`, `geteuid`,
  `getgid`, `getegid`, `gettid`, `uname`, `getrandom`, `sched_yield`,
  `getrlimit`, `setrlimit`, `prlimit64`, `exit`, `exit_group`.

Unsupported calls return `-ENOSYS`. Threads, signals, futexes, `clone`,
`execve`, and loading unmodified Linux ELF binaries are not implemented.

Paths are passed directly to the host filesystem, relative to the emulator's
working directory. Guest code can therefore read and write host files.

`brk` and anonymous `mmap` are backed by otherwise unused DRAM. They are simple
bump allocations; there is no demand paging or reclaim.

Set `RVEMU_TRACE_SYSCALLS=1` to print each syscall and its result to stderr.
See `test/linux.c` and `test/linux.h` for a small freestanding example; it is
built as `bin/stest.bin` and run by `make test`.
 
# rv64im

This core also supports rv64im instructions. So you should be able to compile with `-march=rv64im -mabi=lp64` as well
