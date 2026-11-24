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
- Verdict: a guest PASSES iff the emulator process it runs exits 0 (guests only
  `exit_group(0)` after every self-check passed). `ttest.bin` is inverted: it
  executes a reserved CUSTOM-0 instruction, so it PASSES iff the emulator traps
  it and exits NON-ZERO (rc≠124); a zero exit or a timeout would mean the core
  silently kept running — a bug.
- Environment: `ruby`/`podman`, image `rvemu-matrix:local`, Ubuntu 24.04 cross
  gcc toolchains + `qemu-user`.
- Harness: `matrix.sh` (see `Dockerfile`). Final table is the last all-green run
  (after all fixes, riscv32 dropped): `out/matrix-final.log`, exit 0,
  "MATRIX: all mandatory guests passed".

## Final matrix (all 9 runnable arches pass both gates)

| arch | rtest | stent | net (best-effort) |
|------|-------|-------|-------------------|
| x86_64 | PASS | PASS | PASS |
| aarch64 | PASS | PASS | PASS |
| arm | PASS | PASS | TIMEOUT |
| mipsel | PASS | PASS | FAIL |
| powerpc64le | PASS | PASS | TIMEOUT |
| powerpc64 | PASS | PASS | FAIL |
| powerpc | PASS | PASS | FAIL |
| s390x | PASS | PASS | FAIL |
| riscv64 | PASS | PASS | PASS |

**riscv32 is excluded** — see "Scope decision" below.

All 9 runnable arches pass both mandatory gates (rtest + stent). `net` is an
optional best-effort guest: it is sensitive to the host's socket errno values
(ENOTSOCK/EADDRINUSE, ...) and to qemu-user socket emulation, so it is not
counted toward the pass/fail verdict. It is listed for completeness.

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
those packages; on such a distro `matrix.sh` reports `mips`/`mipsel` as
`N/A [no <arch>-linux-gnu-gcc]` — an absent toolchain, not an emulator
failure, and it does not gate the matrix.

**Untested-but-known host-dependent surfaces (documented, not expanded):**
- `sys_fcntl` `F_GETFL` — returns a host `open` mode; not exercised by the
  mandatory gates.
- Socket `level` / `optname` values — host-numbered; only reachable via the
  best-effort `net` guest.

## Artifacts
> `out/` and `bin/` are gitignored — no build or run artifacts are committed.
> The logs below are from the historical runs this document cites; reproduce
> any of them with `test/matrix/run.sh` (podman image `rvemu-matrix:local`) or
> via CI, where every run uploads `out/` as the `rvemu-logs` artifact
> (retention 30 days).

- Final all-green matrix log: `out/matrix-final.log`
- Run C (post-class-4, pre-riscv32-drop) and Run B (pre-class-4, original
  failures): `out/matrix-runC.log`, `out/matrix-runB.log`
- Per-arch errno ground truth: `out/probe/errno-tables.txt`
- Class-3 probe: `probe-openat2.c`
- riscv32 build failure: `out/build-riscv32.log`
