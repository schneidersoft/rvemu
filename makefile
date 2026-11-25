TESTSRC=
TESTSRC+=test/test.c test/sha256.c test/aes.c test/dbg.c

GUESTTEST=test/startup.rv64i.s test/test.c test/sha256.c test/aes.c test/dbg.c test/sim.c test/platform.c test/stdlib/src/stdio.c test/stdlib/src/string.c
SYSCALLTEST=test/startup.rv64i.s test/linux.c test/platform.c test/stdlib/src/stdio.c test/stdlib/src/string.c
# Negative control: a guest that executes an illegal (reserved CUSTOM-0)
# instruction; a correct emulator must trap it and exit non-zero.
TRAPTEST=test/startup.rv64i.s test/trap.c test/platform.c test/stdlib/src/stdio.c test/stdlib/src/string.c

NETTEST=test/net.c test/startup.rv64i.s test/dbg.c test/sim.c test/platform.c test/stdlib/src/stdio.c test/stdlib/src/string.c test/stdlib/src/stdlib.c test/stdlib/src/net.c
NETHEADERS=test/stdlib/internal/linux.h test/stdlib/include/stdio.h test/stdlib/include/string.h test/stdlib/include/stdlib.h test/stdlib/include/errno.h test/stdlib/include/netdb.h test/stdlib/include/unistd.h test/stdlib/include/time.h test/stdlib/include/sys/time.h test/stdlib/include/sys/socket.h test/stdlib/include/sys/types.h

.PHONY: all test net clean

# `all` is the mandatory gate only. `net` dials a throwaway local HTTP server
# (test/net/net.py, 127.0.0.1:8931) — hermetic, no external network or DNS
# — and is therefore best-effort, never gating CI.
all: bin/riscv64i test

test: bin/riscv64i bin/rtest.bin bin/htest.elf bin/stest.bin bin/ttest.bin
	./bin/htest.elf
	@echo "-------"
	./bin/riscv64i bin/rtest.bin
	@echo "-------"
	./bin/riscv64i bin/stest.bin
	@echo "-------"
	@# Negative control: ttest.bin executes an illegal instruction, so the
	@# emulator must trap it and exit non-zero. A zero exit would mean the
	@# core kept running — a silent-corruption bug, reported as a failure.
	@if ./bin/riscv64i bin/ttest.bin; then \
	    echo "FAIL: ttest - emulator ran past an illegal instruction"; \
	    exit 1; \
	   else \
	     echo "OK: ttest illegal instruction trapped with non-zero exit"; \
	   fi

bin/riscv64i: bin/riscv64i.o bin/librv64i.a bin/syscall.o bin/dbg.o
	gcc -Wall -Werror -I./test/ bin/riscv64i.o bin/librv64i.a bin/syscall.o bin/dbg.o -o $@

bin/librv64i.a: bin/librv64i.o
	ar rcs $@ $<

bin/librv64i.o: src/librv64i.c src/librv64i.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/riscv64i.o: src/riscv64i.c src/librv64i.h src/syscall.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/syscall.o: src/syscall.c src/syscall.h src/librv64i.h src/guest.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/dbg.o: test/dbg.c test/dbg.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/rtest.bin: $(GUESTTEST) test/riscv.ld test/stdlib/internal/linux.h src/guest.h test/stdlib/include/stdio.h
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/rtest.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/include -I./test/stdlib/internal -I./test -I./src -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(GUESTTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/rtest.elf bin/rtest.bin
	riscv64-unknown-elf-objdump -d bin/rtest.elf > bin/rtest.elf.disas

bin/stest.bin: $(SYSCALLTEST) test/riscv.ld test/stdlib/internal/linux.h src/guest.h test/stdlib/include/stdio.h
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/stest.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/include -I./test/stdlib/internal -I./test -I./src -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(SYSCALLTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/stest.elf bin/stest.bin
	riscv64-unknown-elf-objdump -d bin/stest.elf > bin/stest.elf.disas

bin/ttest.bin: $(TRAPTEST) test/riscv.ld test/stdlib/include/stdio.h
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/ttest.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/include -I./test/stdlib/internal -I./test -I./src -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(TRAPTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/ttest.elf bin/ttest.bin
	riscv64-unknown-elf-objdump -d bin/ttest.elf > bin/ttest.elf.disas

bin/htest.elf: $(TESTSRC)
	@mkdir -p bin
	gcc -ggdb -o bin/htest.elf -Wall -Werror $(TESTSRC)

bin/net.bin: $(NETTEST) $(NETHEADERS) src/guest.h test/riscv.ld
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/net.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/include -I./test/stdlib/internal -I./test -I./src -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(NETTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/net.elf bin/net.bin
	riscv64-unknown-elf-objdump -d bin/net.elf > bin/net.elf.disas

net: bin/riscv64i bin/net.bin
	@echo "-------"
	python3 test/net/net.py ./bin/riscv64i bin/net.bin

clean:
	rm -rf bin out
