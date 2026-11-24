# Host-ABI matrix — evidence and results

This directory proves the rvemu emulator has **no hidden host-ABI dependencies**.
The *host* binary is cross-compiled for a matrix of CPU architectures and run
under qemu-user with a fixed guest program. Any guest-visible difference between
the same fixed guest on different hosts would prove the emulator depends on the
host's ABI (endianness, errno numbering, open-flag layout, 128-bit arithmetic,
...); in practice none do once the four defect classes below are removed.

- Guest is fixed: little-endian rv64i/lp64. `rtest.bin` (ISA) and `stest.bin`
  (syscall) are mandatory gates; `ttest.bin` is a mandatory negative control;
  `net.bin` (sockets) is best-effort.
- `net` is hermetic since the loopback redesign: it dials a local HTTP server
  (`test/net/net.py` serves `test/net/index.html` on `127.0.0.1:8931` in-process
  and runs the guest), so no external network or DNS and no separate server
  process is needed; a runner without `python3` reports `net=N/A`.
- Verdict: a guest PASSES iff the emulator process it runs exits 0 (guests only
  `exit_group(0)` after every self-check passed). `ttest.bin` is inverted: it
  executes a reserved CUSTOM-0 instruction, so it PASSES iff the emulator traps
  it and exits NON-ZERO (rc≠124); a zero exit or a timeout would mean the core
  silently kept running — a bug.
- Environment: `ruby`/`podman`, image `rvemu-matrix:local`, Ubuntu 24.04 cross
  gcc toolchains + `qemu-user`.
- Harness: `matrix.py` (single Python file; see `Dockerfile`). Final table is the
  last all-green run (after all fixes, riscv32 dropped): `out/matrix-final.log`,
  exit 0, "MATRIX: all mandatory guests passed".

## Final matrix (all 10 runnable arches pass the mandatory gates)

| arch | rtest | stest | ttest | net (best-effort) |
|------|-------|-------|-------|-------------------|
| x86_64 | PASS | PASS | PASS (trapped, rc=1) | PASS |
| aarch64 | PASS | PASS | PASS (trapped, rc=1) | PASS |
| arm | PASS | PASS | PASS (trapped, rc=1) | PASS |
| mipsel | PASS | PASS | PASS (trapped, rc=1) | PASS |
| powerpc64le | PASS | PASS | PASS (trapped, rc=1) | PASS |
| powerpc64 | PASS | PASS | PASS (trapped, rc=1) | PASS |
| powerpc | PASS | PASS | PASS (trapped, rc=1) | PASS |
| s390x | PASS | PASS | PASS (trapped, rc=1) | PASS |
| riscv64 | PASS | PASS | PASS (trapped, rc=1) | PASS |
| mips | PASS | PASS | PASS (trapped, rc=1) | PASS |

**riscv32 is excluded** — see "Scope decision" below.

All 10 runnable arches pass the mandatory gates (rtest, stest, and the ttest
negative control). `net` is an optional best-effort guest and is not counted
toward the pass/fail verdict; it is listed for completeness.

> **Note on `net`:** the original design fetched `google.com:80`, so pass/fail
> depended on external egress. It is now hermetic: `net.bin` dials a throwaway
> local HTTP server on `127.0.0.1:8931` (`test/net/net.py`) and checks an
> `HTTP 200` response with a known marker. Net remains best-effort (not a
> mandatory gate), but now passes on every runnable arch. The earlier MIPS/BE
> failures under nested qemu-user were caused by forwarding Linux-generic guest
> socket values unchanged through rvemu while qemu-user translated them as
> target-ABI syscalls; fixed by guest→host socket ABI translation in
> `src/syscall.c` (see Class 5 below).

## Defect classes found and fixed

### Class A — host endianness
*Symptom:* stent failed at `fstat` on big-endian arches (powerpc64, powerpc,
s390x).
*Cause:* struct fields were read/written assuming the host's native byte order.
*Fix:* byte-wise little-endian load/store of guest struct data in `src/syscall.c`.
*Result:* all three BE arches pass.

### Class 2 — int128 multiplication
*Symptom:* 128-bit multiply misbehaved on 32-bit hosts where `__int128_t` is
unavailable or non-conformant.
*Cause:* use of `__int128_t` for the hi/lo multiply.
*Fix:* portable `mul_hi64()` using 32-bit limbs in `src/librv64i.c` (no
`__int128_t`).
*Result:* MULH/MULHU/MULHSU correct on 32-bit and big-endian arches.

### Class 3 — open/pipe flag ABI
*Symptom:* mips/mipsel stent failed at `openat` (open-flag bits misread).
*Cause:* guest `O_*` flag bits (asm-generic) do not line up with the host's
`open()` flag numbering.
*Fix:* RVOF asm-generic bit enum + `guest_open_to_host_flags()` in
`src/syscall.c`, wired into `sys_openat` and `sys_pipe2`. Guest access mode
(`O_ACCMODE`, 0/1/2) passes through; everything else is translated via
host-`#ifdef`-guarded `O_*` constants.
*Result:* mips/mipsel openat/write/lseek/read pass.

### Class 4 — host errno leak
*Symptom:* mips/mipsel stent failed: the `unknown syscall` check expects the
guest value `-ENOSYS` (guest 38) but received the host's `-ENOSYS` (MIPS 89);
analogous mismatches for `ENOTSOCK` (guest 88 vs MIPS 95), `ENAMETOOLONG`
(guest 36 vs MIPS 78), `EADDRINUSE` (guest 98 vs MIPS 125), etc.
*Cause:* `fail(cpu, err)` wrote the **host's** errno number into guest `a0`.
*Fix:* `guest_errno()` — a host→guest translation at the single `fail(cpu,
err)` chokepoint in `src/syscall.c` (every error path funnels through it, and
`host_result()` routes host `errno` through `fail()`). Host errno symbols are
matched with `#ifdef` guards and mapped to the guest/asm-generic numbers
(anchored to the riscv64 reference / `test/linux.h`). Unmapped host values pass
through unchanged.
*Evidence:* `out/probe/errno-tables.txt` dumps each host ABI's errno numbers;
MIPS is the only divergent ABI, every other arch matches the guest table.
*Result:* mips/mipsel pass both gates.

### Class 5 — socket ABI translation
*Symptom:* net.bin passed on LE-non-MIPS arches but failed on MIPS (mips/mipsel)
and BE (powerpc64, powerpc, s390x) under nested qemu-user.
*Cause:* rvemu forwarded guest/Linux-generic socket domain/type/level/optname
values and raw `sockaddr` bytes directly to host syscalls. When rvemu is
cross-compiled for a target arch and run under qemu-user, qemu-user translates
those syscalls as target-ABI calls: MIPS renumbers `SOCK_STREAM`/`SOCK_DGRAM`
and `SOL_SOCKET`/optname values, and BE targets read `sa_family`/host-order
`sockaddr_in6` fields in target byte order. The guest's Linux-generic bytes
were therefore mis-translated.
*Fix:* guest→host translation in `src/syscall.c`: `guest_sock_domain_to_host`,
`guest_sock_type_to_host` (including `SOCK_NONBLOCK`/`SOCK_CLOEXEC` flag bits),
`guest_sockopt_level_to_host`/`guest_sockopt_optname_to_host`, and
`guest_sockaddr_to_host`/`host_sockaddr_to_guest` (fix `sa_family` and, for
AF_INET6, `sin6_flowinfo`/`sin6_scope_id` byte order; port/addr remain NBO).
Applied to `socket`, `connect`, `sendto`, `recvfrom`, `setsockopt`,
`getsockopt`.
*Result:* net.bin passes on all 10 runnable arches.

## Scope decisions

**riscv32 — excluded (toolchain limit, not an emulator defect).** The
`riscv64-linux-gnu` cross toolchain bundles only an **rv64** glibc; a static
rv32 build requires `gnu/stubs-ilp32d.h`, which is absent:

```
/usr/riscv64-linux-gnu/include/gnu/stubs.h:11:11: fatal error:
    gnu/stubs-ilp32d.h: No such file or directory
```

The emulator sources are architecture-independent and already compile cleanly
(and pass every test) on 32-bit arches (mips/mipsel). riscv32 is therefore
dropped from the matrix rather than forcing a different toolchain; it adds no
host-ABI coverage that the 32-bit mips pair already exercises.

**MIPS toolchain availability (per-distro).** `packages.txt` is a list of
*intent*; consumers (CI workflow + `Dockerfile`) install only what the current
distro actually provides and warn about skipped entries. Noble
(ubuntu:24.04 — the image base and the CI runner) ships every entry, including
`gcc-mips(-el)-linux-gnu`. Newer Ubuntu releases (26.04 "resolute") dropped
those packages; on such a distro `matrix.py` reports `mips`/`mipsel` as
`N/A [no <arch>-linux-gnu-gcc]` — an absent toolchain, not an emulator
failure, and it does not gate the matrix.

**Untested-but-known host-dependent surfaces (documented, not expanded):**
- `sys_fcntl` `F_GETFL` — returns a host `open` mode; not exercised by the
  mandatory gates.
- Socket `level` / `optname` translation covers `SOL_SOCKET` and common
  `SO_*` options; uncommon option names/values and option payload layouts may
  still be ABI-dependent and are not exhaustively exercised.

## Artifacts
> `out/` and `bin/` are gitignored — no build or run artifacts are committed.
> The logs below are from the historical runs this document cites; reproduce
> any of them in the `rvemu-matrix:local` container (see `Dockerfile`; run
> `python3 test/matrix/matrix.py` with the repo bind-mounted at `/workspace`)
> or via CI, where every run uploads `out/` as the `rvemu-logs` artifact
> (retention 30 days).

- Final all-green matrix log: `out/matrix-final.log`
- Run C (post-class-4, pre-riscv32-drop) and Run B (pre-class-4, original
  failures): `out/matrix-runC.log`, `out/matrix-runB.log`
- Per-arch errno ground truth: `out/probe/errno-tables.txt`
- Class-3 probe: `probe-openat2.c`
- riscv32 build failure: `out/build-riscv32.log`
