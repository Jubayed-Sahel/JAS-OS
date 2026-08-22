CC ?= gcc
LD ?= ld
OBJCOPY ?= objcopy

CFLAGS = -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
         -mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -O2 \
         -fno-asynchronous-unwind-tables -Ikernel/include
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

KERNEL_OBJS = \
	build/start.o build/klib.o build/hw.o build/input.o build/gfx.o \
	build/gui.o build/kernel.o build/task.o build/scheduler.o \
	build/memory.o build/paging.o build/banker.o build/filesystem.o \
	build/sync.o build/demos.o build/event_log.o build/storage.o build/concepts.o \
	build/commands.o

.PHONY: all iso clean dirs

all: iso

dirs:
	mkdir -p build

build/boot.bin: boot/boot.S dirs
	$(CC) -m32 -fno-asynchronous-unwind-tables -c boot/boot.S -o build/boot.o
	$(LD) -m elf_i386 -Ttext 0x7C00 --oformat binary -o build/boot.bin build/boot.o
	@python3 -c "import os; n=os.path.getsize('build/boot.bin'); print('boot.bin', n, 'bytes'); \
assert n<=4096, 'bootloader too large'; open('build/boot.bin','ab').write(b'\\x00'*(4096-n))"

build/%.o: kernel/src/%.c dirs
	$(CC) $(CFLAGS) -c $< -o $@

build/start.o: kernel/src/start.S dirs
	$(CC) -m32 -fno-asynchronous-unwind-tables -c kernel/src/start.S -o $@

build/kernel.elf: $(KERNEL_OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
	@python3 -c "import os; print('kernel.bin', os.path.getsize('build/kernel.bin'), 'bytes')"

iso: build/boot.bin build/kernel.bin
	python3 scripts/make_iso.py build/boot.bin build/kernel.bin build/jas-os.iso
	@echo "ISO ready: build/jas-os.iso"

clean:
	rm -rf build
