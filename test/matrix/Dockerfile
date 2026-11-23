# Cross-compilation matrix image: ubuntu base + cross toolchains + qemu-user.
# Rebuilt rarely; `run.sh` skips the build when this image already exists.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        gcc \
        binutils-mipsel-linux-gnu \
        gcc-mipsel-linux-gnu \
        binutils-mips-linux-gnu \
        gcc-mips-linux-gnu \
        gcc-arm-linux-gnueabi \
        gcc-aarch64-linux-gnu \
        gcc-powerpc64le-linux-gnu \
        gcc-powerpc64-linux-gnu \
        gcc-powerpc-linux-gnu \
        gcc-s390x-linux-gnu \
        gcc-riscv64-linux-gnu \
        # libc header packages (Recommends of gcc; kept here explicitly because
        # of --no-install-recommends). Without these, no stdint.h/stdio.h.
        libc6-dev \
        libc6-dev-arm64-cross \
        libc6-dev-armel-cross \
        libc6-dev-mips-cross \
        libc6-dev-mipsel-cross \
        libc6-dev-powerpc-cross \
        libc6-dev-ppc64-cross \
        libc6-dev-ppc64el-cross \
        libc6-dev-s390x-cross \
        libc6-dev-riscv64-cross \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get update && apt-get install -y --no-install-recommends qemu-user \
    && rm -rf /var/lib/apt/lists/*

LABEL purpose="cross-building + qemu-user execution of the rvemu host binary"
