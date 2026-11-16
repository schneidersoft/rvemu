TESTSRC=
TESTSRC+=test/test.c test/sha256.c test/aes.c test/dbg.c

GUESTTEST=test/startup.rv64i.s test/test.c test/sha256.c test/aes.c test/dbg.c test/sim.c test/platform.c test/stdlib/stdio.c test/stdlib/string.c
SYSCALLTEST=test/startup.rv64i.s test/linux.c test/platform.c test/stdlib/stdio.c test/stdlib/string.c

NETTEST=test/net.c test/startup.rv64i.s test/dbg.c test/sim.c test/platform.c test/stdlib/stdio.c test/stdlib/string.c test/stdlib/stdlib.c test/stdlib/net.c
NETHEADERS=test/linux.h test/stdlib/stdio.h test/stdlib/string.h test/stdlib/stdlib.h test/stdlib/errno.h test/stdlib/netdb.h test/stdlib/unistd.h test/stdlib/time.h test/stdlib/sys/time.h test/stdlib/sys/socket.h test/stdlib/sys/types.h

.PHONY: all test clean

all: bin/riscv64i test net

test: bin/riscv64i bin/rtest.bin bin/htest.elf bin/stest.bin
	./bin/htest.elf
	@echo "-------"
	./bin/riscv64i bin/rtest.bin
	@echo "-------"
	./bin/riscv64i bin/stest.bin

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

bin/syscall.o: src/syscall.c src/syscall.h src/librv64i.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/dbg.o: test/dbg.c test/dbg.h
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/rtest.bin: $(GUESTTEST) test/riscv.ld test/linux.h test/stdlib/stdio.h
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/rtest.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/ -I./test/ -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(GUESTTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/rtest.elf bin/rtest.bin
	riscv64-unknown-elf-objdump -d bin/rtest.elf > bin/rtest.elf.disas

bin/stest.bin: $(SYSCALLTEST) test/riscv.ld test/linux.h test/stdlib/stdio.h
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/stest.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/ -I./test/ -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(SYSCALLTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/stest.elf bin/stest.bin
	riscv64-unknown-elf-objdump -d bin/stest.elf > bin/stest.elf.disas

bin/htest.elf: $(TESTSRC)
	@mkdir -p bin
	gcc -ggdb -o bin/htest.elf -Wall -Werror $(TESTSRC)

bin/net.bin: $(NETTEST) $(NETHEADERS) test/riscv.ld
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/net.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/ -I./test/ -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld $(NETTEST) -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/net.elf bin/net.bin
	riscv64-unknown-elf-objdump -d bin/net.elf > bin/net.elf.disas

net: bin/riscv64i bin/net.bin
	@echo "-------"
	./bin/riscv64i bin/net.bin

clean:
	rm -rf bin
