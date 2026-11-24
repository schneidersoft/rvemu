#!/usr/bin/env python3
# Cross-arch portability matrix, single file (replaces test/matrix/matrix.sh).
#
# Cross-compiles the rvemu *host* binary for a matrix of architectures and
# runs the prebuilt guest programs under qemu-user:
#   rtest/stest = mandatory gates;
#   ttest.bin   = mandatory negative control — it executes an illegal
#                 instruction, which the emulator MUST trap and exit
#                 non-zero for;
#   net.bin     = best-effort, hermetic (test/net/net.py serves a local
#                 HTTP server on 127.0.0.1 in-process and runs the guest),
#                 never gates.
#
# The guest is fixed little-endian RISC-V (rv64i/lp64). Any disagreement
# between guest-visible behaviour on a big-endian host proves the emulator
# depends on host endianness; on little-endian hosts it must behave
# identically to x86_64.
#
# Verdict: a guest PASSES iff the emulator process it runs exits 0 (guests
# only exit_group(0) after every self-check passed). ttest.bin is inverted:
# it PASSES iff the emulator exits NON-ZERO (rc!=124), i.e. it trapped the
# illegal instruction; a zero exit or a timeout means the core kept running
# — a bug.
#
# Note: noble/24.04 cross toolchains expose triple-prefixed drivers
# (aarch64-linux-gnu-gcc, ...); the legacy "gcc-<arch>" wrapper names are
# NOT in /usr/bin, do not use them.
import os
import shutil
import signal
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent   # <root>/test/matrix -> repo root

SRC = "src/librv64i.c src/riscv64i.c src/syscall.c test/dbg.c"
COMMON = "-O2 -static -Wall -Werror -Itest -Isrc"
GUESTS = ["rtest.bin", "stest.bin", "ttest.bin", "net.bin"]
RUN_TIMEOUT = 90
OUTDIR = "out"

# arch | gcc | extra-flags | qemu ("" = native)
ARCHES = [
    ("x86_64", "gcc", "", ""),
    ("aarch64", "aarch64-linux-gnu-gcc", "", "qemu-aarch64"),
    ("arm", "arm-linux-gnueabi-gcc", "", "qemu-arm"),
    ("mipsel", "mipsel-linux-gnu-gcc", "", "qemu-mipsel"),
    ("powerpc64le", "powerpc64le-linux-gnu-gcc", "", "qemu-ppc64le"),
    ("powerpc64", "powerpc64-linux-gnu-gcc", "", "qemu-ppc64"),
    ("powerpc", "powerpc-linux-gnu-gcc", "", "qemu-ppc"),
    ("s390x", "s390x-linux-gnu-gcc", "", "qemu-s390x"),
    ("riscv64", "riscv64-linux-gnu-gcc", "", "qemu-riscv64"),
    ("mips", "mips-linux-gnu-gcc", "", "qemu-mips"),
]


def run_cmd(args, logfile, timeout):
    """Run args in the repo root, all output to OUTDIR/logfile.

    The command gets its own session/process group, so a timeout kills the
    whole tree (not just the wrapper — e.g. net.py's child emulator).
    Returns the exit code, 124 (coreutils `timeout` convention) on timeout,
    or 128+N when the command is killed by signal N.
    """
    with open(ROOT / OUTDIR / logfile, "w") as log:
        proc = subprocess.Popen(
            args,
            cwd=ROOT,
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except OSError:
                proc.kill()
            proc.wait()
            return 124
    return 128 + (-rc) if rc < 0 else rc


def verdict(short, rc):
    """Return (ok, label) for a guest run; ok is False on a failed verdict.

    ttest is the inverted negative control (must exit non-zero, not 124).
    Every other guest (incl. the best-effort net guest) passes on rc 0.
    """
    if short == "ttest":
        if rc not in (0, 124):
            return True, f"PASS (trapped, rc={rc})"
        if rc == 124:
            return False, "TIMEOUT"
        return False, f"FAIL(rc={rc} — ran past illegal instruction)"
    if rc == 0:
        return True, "PASS"
    if rc == 124:
        return False, "TIMEOUT"
    return False, f"FAIL(rc={rc})"


def main():
    os.makedirs(ROOT / OUTDIR, exist_ok=True)

    table = []
    failures = 0

    for arch, gcc, extra, qemu in ARCHES:
        print()
        print(f"### arch={arch}")

        binpath = f"./riscv64i-{arch}"
        if shutil.which(gcc) is None:
            # The gcc driver for this arch is not installed on this distro
            # (newer Ubuntu releases dropped the mips/mipsel cross gccs;
            # noble/24.04 still ships them). A missing toolchain is not a
            # test failure: report N/A.
            print(f"  N/A: {gcc} not available on this system")
            table.append(f"arch={arch} | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [no {gcc}]")
            continue
        build = [gcc] + COMMON.split() + extra.split() + ["-o", binpath] + SRC.split()
        if run_cmd(build, f"build-{arch}.log", None) != 0:
            print(f"  build: FAIL (see {OUTDIR}/build-{arch}.log)")
            table.append(f"arch={arch} | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [build failed]")
            failures += 1
            continue
        if qemu and shutil.which(qemu) is None:
            print(f"  qemu missing: {qemu}")
            table.append(f"arch={arch} | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [no {qemu}]")
            failures += 1
            continue

        arch_results = []
        for guest in GUESTS:
            short = guest.removesuffix(".bin")
            cmd = ([qemu] if qemu else []) + [binpath, f"bin/{guest}"]
            if guest == "net.bin":
                # net dials a local HTTP server: net.py serves test/net/ on
                # 127.0.0.1 in-process, runs the guest against it, and exits
                # with the guest's exit code.
                cmd = [sys.executable, "test/net/net.py"] + cmd
            rc = run_cmd(cmd, f"{guest}.log", RUN_TIMEOUT)
            ok, value = verdict(short, rc)
            optional = guest == "net.bin"     # best-effort: never gates
            if not ok and not optional:
                failures += 1
            print(f"  {short}: {value}")
            if not ok and not optional:
                print("  --- guest output (first 25 lines) ---")
                log = (ROOT / OUTDIR / f"{guest}.log").read_text(errors="replace").splitlines()
                for line in log[:25]:
                    print(f"  | {line}")
                print(f"  --- (full log: {OUTDIR}/{guest}.log) ---")
            arch_results.append(f" | {short}={value}")
        table.append(f"arch={arch}" + "".join(arch_results))

    print()
    print("=" * 20 + " MATRIX " + "=" * 20)
    print("\n".join(table))
    print("=" * 49)
    print()
    if failures == 0:
        print("MATRIX: all mandatory guests passed")
        return 0
    print(f"MATRIX: {failures} mandatory failure(s) (net is best-effort and not counted; "
          f"ttest is the mandatory negative control)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
