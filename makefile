TESTSRC=
TESTSRC+=test/test.c
TESTSRC+=test/sha256.c
TESTSRC+=test/aes.c
TESTSRC+=test/uECC.c
TESTSRC+=test/dbg.c

all: bin/riscv64i bin/rv64i.bin bin/htest
	./bin/htest.elf
	@echo "-------"
	./bin/riscv64i bin/rv64i.bin

bin/riscv64i: bin/librv64i.a
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ test/dbg.c -c -o bin/dbg.o
	gcc -Wall -Werror -I./test/ src/riscv64i.c -c -o bin/riscv64i.o
	gcc -Wall -Werror -I./test/ bin/riscv64i.o bin/librv64i.a bin/dbg.o -o $@

bin/librv64i.a: bin/librv64i.o
	ar rcs $@ $<

bin/librv64i.o: src/librv64i.c
	@mkdir -p bin
	gcc -Wall -Werror -I./test/ $< -c -o $@

bin/rv64i.bin:
	@mkdir -p bin
	riscv64-unknown-elf-gcc -o bin/rv64i.elf -ggdb -Wall -Werror -nostdinc -I./test/stdlib/ -I./test/ -nostdlib -nodefaultlibs -ffreestanding -nostartfiles -static -mcmodel=medlow -march=rv64i -mabi=lp64 -T test/riscv.ld test/startup.rv64i.s test/sim.c $(TESTSRC) test/stdlib/stdio.c -lgcc
	riscv64-unknown-elf-objcopy -O binary bin/rv64i.elf bin/rv64i.bin
	riscv64-unknown-elf-objdump -d bin/rv64i.elf > bin/rv64i.elf.disas

bin/htest:
	@mkdir -p bin
	gcc -ggdb -o bin/htest.elf -Wall -Werror  $(TESTSRC)

clean:
	rm -rf bin
