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
`test/stdlib/src/platform.c` implements these with Linux syscalls for the guest tests.

The same directory also provides `string.h`, `stdlib.h` (`exit`, `atoi`,
`perror`, `strerror`), `unistd.h`, `time.h`/`sys/time.h`, and a small BSD socket
layer (`sys/socket.h`, `netdb.h`, `errno.h`). `getaddrinfo` is backed by a stub
DNS resolver that reads `/etc/resolv.conf` and falls back to `8.8.8.8` and
`1.1.1.1` (IPv4 literals short-circuit it, so local addresses need no DNS).
`test/net.c`, built and run by `make net`, is a hermetic net test: a single
script (`test/net/net.py`) serves `test/net/index.html` on `127.0.0.1:8931`
in-process and runs the emulator against it; the guest verifies an `HTTP 200`
with a known marker — no external network, no DNS.

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
terminates the emulator with the value in `a0` (the guest `main` result), so
failing test programs make `make` fail.

The syscall proxy currently covers:

- Files and descriptors: `read`, `write`, `readv`, `writev`, `pread64`,
  `pwrite64`, `openat`, `close`, `lseek`, `dup`, `dup3`, `fcntl`, `ioctl`
  (limited), `pipe2`, `getdents64`, `fstat`, `newfstatat`, `getcwd`, `chdir`,
  `mkdirat`, `unlinkat`, `symlinkat`, `linkat`, `renameat`, `faccessat`.
- Memory: `brk`, anonymous `mmap`, `munmap`, `mprotect`, `madvise`.
- Time: `clock_gettime`, `clock_getres`, `gettimeofday`, `nanosleep`.
- Network: `socket`, `connect`, `sendto`, `recvfrom`, `setsockopt`,
  `getsockopt`, `shutdown`. Address and option buffers are copied between guest
  and host memory, and the remaining stream socket calls are expressed through
  `sendto`/`recvfrom` with a null destination.
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

# Debugging with GDB

rvemu includes a minimal GDB Remote Serial Protocol (RSP) stub. Start the emulator with `-g <port>` to listen for a GDB connection on `127.0.0.1:<port>`; the guest starts halted so you can set breakpoints before it runs.

```sh
# terminal 1: start rvemu and wait for GDB
./bin/riscv64i -g 1234 bin/rtest.bin

# terminal 2: connect with GDB
gdb-multiarch bin/rtest.elf -ex "target remote :1234"
```

If you have a RISC-V toolchain GDB, `riscv64-unknown-elf-gdb` works the same way. From GDB you can inspect registers and memory, set software breakpoints (`break`), single-step (`stepi`), and continue (`continue`). Use `continue` to start the guest; `run` is not supported by the GDB remote target.

## Attach / Detach / Reattach

By default, `detach` resumes the guest and leaves it running. rvemu keeps listening on the same port, so you can reattach later; on reattach, the guest is stopped for inspection.

```sh
# detach and keep guest running
(gdb) detach

# reattach later (guest stops on attach)
gdb-multiarch bin/rtest.elf -ex "target remote :1234"
```

To detach and keep the guest halted, use the monitor command before detaching:

```sh
(gdb) monitor halt-on-detach
detach will keep target halted
(gdb) detach
```

rvemu waits for a reconnection while the guest is halted. After reattaching, inspect state and `continue` when ready. To restore default behavior:

```sh
(gdb) monitor run-on-detach
detach will resume target
```

When the guest exits, GDB receives the exit status.
