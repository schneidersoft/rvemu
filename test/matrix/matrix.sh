#!/usr/bin/env bash
# Cross-compile the rvemu *host* binary for a matrix of architectures and run
# the prebuilt guest programs under qemu-user:
#   rtest/bin stest.bin = mandatory gates;
#   ttest.bin           = mandatory negative control — it executes an illegal
#                         instruction, which the emulator MUST trap and exit
#                         non-zero for;
#   net.bin             = best-effort (external network), never gates.
#
# The guest is fixed little-endian RISC-V (rv64i/lp64). Any disagreement between
# guest-visible behaviour on a big-endian host proves the emulator depends on
# host endianness; on little-endian hosts it must behave identically to x86_64.
#
# Verdict: a guest PASSES iff the emulator process it runs exits 0 (guests call
# exit_group(0) only when every self-check passed). ttest.bin is inverted: it
# PASSES iff the emulator exits NON-ZERO (rc!=124), i.e. it trapped the illegal
# instruction; a zero exit or a timeout means the core kept running — a bug.
set -u

ROOT="$( cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd )"   # <root>/test/matrix -> repo root
cd "$ROOT"

SRC="src/librv64i.c src/riscv64i.c src/syscall.c test/dbg.c"
COMMON="-O2 -static -Wall -Werror -Itest -Isrc"
GUESTS="rtest.bin stest.bin ttest.bin net.bin"
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
    if ! command -v "$gcc" >/dev/null 2>&1; then
        # The gcc driver for this arch is not installed on this distro (newer
        # Ubuntu releases dropped the mips/mipsel cross gccs; noble/24.04 still
        # ships them). A missing toolchain is not a test failure: report N/A.
        echo "  N/A: $gcc not available on this system"
        TABLE="$TABLE
arch=$arch | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [no $gcc]"
        continue
    fi
    if ! "$gcc" $COMMON $extra -o "$BIN" $SRC >"$OUTDIR/build-$arch.log" 2>&1; then
        echo "  build: FAIL (see out/build-$arch.log)"
        TABLE="$TABLE
arch=$arch | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [build failed]"
        FAILURES=$((FAILURES + 1))
        continue
    fi
    if ! command -v "${qemu:-true}" >/dev/null 2>&1 && [ -n "$qemu" ]; then
        echo "  qemu missing: $qemu"
        TABLE="$TABLE
arch=$arch | rtest=N/A | stest=N/A | ttest=N/A | net=N/A  [no $qemu]"
        FAILURES=$((FAILURES + 1))
        continue
    fi

    ARCH_RESULTS=""
    for guest in $GUESTS; do
        run_guest "$qemu" "$BIN" "$guest"
        short=${guest%.bin}
        name="${arch}.${short}"
        # net: best-effort, never gates. ttest: negative control (inverted
        # verdict — the emulator MUST exit non-zero here, and not by timeout).
        optional=0
        inverted=0
        [ "$short" = "net" ] && optional=1
        [ "$short" = "ttest" ] && inverted=1
        if [ $inverted -eq 1 ]; then
            if [ $rc -ne 0 ] && [ $rc -ne 124 ]; then
                ok=1; v="PASS (trapped, rc=$rc)"
            elif [ $rc -eq 124 ]; then
                ok=0; v="TIMEOUT"
            else
                ok=0; v="FAIL(rc=$rc — ran past illegal instruction)"
            fi
        else
            if [ $rc -eq 0 ]; then
                ok=1; v="PASS"
            elif [ $rc -eq 124 ]; then
                ok=0; v="TIMEOUT"
            else
                ok=0; v="FAIL(rc=$rc)"
            fi
        fi
        if [ $ok -eq 0 ] && [ $optional -eq 0 ]; then
            FAILURES=$((FAILURES + 1))
        fi
        echo "  $short: $v"
        if [ $ok -eq 0 ] && [ $optional -eq 0 ]; then
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
    echo "MATRIX: $FAILURES mandatory failure(s) (net is best-effort and not counted; ttest is the mandatory negative control)"
    exit 1
fi
