#!/usr/bin/env bash
# Cross-compile the rvemu *host* binary for a matrix of architectures and run the
# prebuilt guest programs (rtest/stest = gates, net = best-effort) under qemu-user.
#
# The guest is fixed little-endian RISC-V (rv64i/lp64). Any disagreement between
# guest-visible behaviour on a big-endian host proves the emulator depends on
# host endianness; on little-endian hosts it must behave identically to x86_64.
#
# Verdict: a guest PASSES iff the emulator process it runs exits 0.
# (Guests call exit_group(0) only when every self-check passed.)
set -u

ROOT="$( cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd )"   # <root>/test/matrix -> repo root
cd "$ROOT"

SRC="src/librv64i.c src/riscv64i.c src/syscall.c test/dbg.c"
COMMON="-O2 -static -Wall -Werror -Itest -Isrc"
GUESTS="rtest.bin stest.bin net.bin"
RUN_TIMEOUT=90
OUTDIR="out"
mkdir -p "$OUTDIR"

# arch | gcc | extra-flags | qemu ("" = native)
# NOTE: Noble cross toolchains expose triple-prefixed drivers (aarch64-linux-gnu-gcc,
# ...); the legacy "gcc-<arch>" wrapper names are NOT in /usr/bin, do not use them.
ARCHES="
x86_64|gcc||
aarch64|aarch64-linux-gnu-gcc||qemu-aarch64
arm|arm-linux-gnueabi-gcc||qemu-arm
mipsel|mipsel-linux-gnu-gcc||qemu-mipsel
powerpc64le|powerpc64le-linux-gnu-gcc||qemu-ppc64le
powerpc64|powerpc64-linux-gnu-gcc||qemu-ppc64
powerpc|powerpc-linux-gnu-gcc||qemu-ppc
s390x|s390x-linux-gnu-gcc||qemu-s390x
riscv64|riscv64-linux-gnu-gcc||qemu-riscv64
mips|mips-linux-gnu-gcc||qemu-mips
"

run_guest() { # $1 qemu (or "")  $2 binary  $3 guest  -> sets rc
    if [ -z "$1" ]; then
        ( cd "$ROOT" && timeout "$RUN_TIMEOUT" "$2" "bin/$3" ) >"$OUTDIR/$3.log" 2>&1
    else
        ( cd "$ROOT" && timeout "$RUN_TIMEOUT" "$1" "$2" "bin/$3" ) >"$OUTDIR/$3.log" 2>&1
    fi
    rc=$?
}

TABLE=""
FAILURES=0

while IFS='|' read -r arch gcc extra qemu; do
    [ -z "$arch" ] && continue
    echo
    echo "### arch=$arch"

    BIN="$OUTDIR/riscv64i-$arch"
    if ! "$gcc" $COMMON $extra -o "$BIN" $SRC >"$OUTDIR/build-$arch.log" 2>&1; then
        echo "  build: FAIL (see out/build-$arch.log)"
        TABLE="$TABLE
arch=$arch | rtest=N/A | stest=N/A | net=N/A  [build failed]"
        FAILURES=$((FAILURES + 1))
        continue
    fi
    if ! command -v "${qemu:-true}" >/dev/null 2>&1 && [ -n "$qemu" ]; then
        echo "  qemu missing: $qemu"
        TABLE="$TABLE
arch=$arch | rtest=N/A | stest=N/A | net=N/A  [no $qemu]"
        FAILURES=$((FAILURES + 1))
        continue
    fi

    ARCH_RESULTS=""
    for guest in $GUESTS; do
        run_guest "$qemu" "$BIN" "$guest"
        short=${guest%.bin}
        name="${arch}.${short}"
        if [ $rc -eq 0 ]; then
            v="PASS"
        elif [ $rc -eq 124 ]; then
            v="TIMEOUT"
        else
            v="FAIL(rc=$rc)"
        fi
        optional=0
        [ "$short" = "net" ] && optional=1
        if [ $rc -ne 0 ] && [ $optional -eq 0 ]; then
            FAILURES=$((FAILURES + 1))
        fi
        echo "  $short: $v"
        if [ $rc -ne 0 ] && [ $optional -eq 0 ]; then
            echo "  --- guest output (first 25 lines) ---"
            head -25 "$OUTDIR/$guest.log" | sed 's/^/  | /'
            echo "  --- (full log: out/$guest.log) ---"
        fi
        ARCH_RESULTS="$ARCH_RESULTS | $short=$v"
    done
    TABLE="$TABLE
arch=$arch$ARCH_RESULTS"
done <<< "$ARCHES"

echo
echo "==================== MATRIX ===================="
echo "$TABLE" | sed '/^$/d'
echo "================================================="
echo
if [ $FAILURES -eq 0 ]; then
    echo "MATRIX: all mandatory guests passed"
    exit 0
else
    echo "MATRIX: $FAILURES mandatory failure(s) (net is best-effort and not counted)"
    exit 1
fi
