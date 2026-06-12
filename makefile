BITS ?= 64

# Hosted build
all: kernel.elf

clean:
	rm -f *.o kernel.elf disk.img

# Bare metal 32-bit kernel for QEMU
CROSS_CC ?= gcc
FASM ?= fasm
LD ?= ld

CFLAGS_BARE = -m32 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -Idrivers
LDFLAGS_BARE = -m elf_i386 -T linker.ld --oformat=elf32-i386

# Bootloader
boot.o: boot.asm
	$(FASM) boot.asm boot.o

# Drivers (consolidated into drivers.c)
drivers.o: drivers.c drivers.h
	$(CROSS_CC) $(CFLAGS_BARE) -c drivers.c -o drivers.o

idt-asm.o: idt.asm
	$(FASM) idt.asm idt-asm.o

# VM and Bare Metal System
kernel.o: system.c bmf-vm.h bmf-vm.c drivers.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c system.c -o kernel.o

bmf-vm.o: bmf-vm.c bmf-vm.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c bmf-vm.c -o bmf-vm.o

bmf-boot.o: bmf-boot.c bmf-vm.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c bmf-boot.c -o bmf-boot.o

# Kernel ELF image
kernel.elf: boot.o drivers.o idt-asm.o kernel.o bmf-vm.o bmf-boot.o
	$(LD) $(LDFLAGS_BARE) \
		boot.o \
		idt-asm.o \
		drivers.o \
		kernel.o bmf-vm.o bmf-boot.o \
		-o kernel.elf
	ls -l kernel.elf

# Create disk image (1MB)
disk.img:
	@echo "Creating disk image (1MB)..."
	qemu-img create -f raw disk.img 1M

# Run in QEMU
run: kernel.elf disk.img
	@echo "Starting BMF Bare Metal in QEMU (interactive serial console)..."
	@echo "Press Ctrl+C to exit QEMU"
	@echo ""
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none \
		-drive file=disk.img,format=raw,if=ide

run-debug: kernel.elf disk.img
	@echo "Starting BMF Bare Metal in QEMU with debug output..."
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none \
		-drive file=disk.img,format=raw,if=ide \
		-d int,guest_errors

run-test: kernel.elf
	@echo "Running QEMU test (output to /tmp/qemu_serial.log)..."
	bash test_qemu.sh

run-gdb: kernel.elf disk.img
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none \
		-drive file=disk.img,format=raw,if=ide \
		-d int,guest_errors,cpu_reset -S -gdb tcp::1234
