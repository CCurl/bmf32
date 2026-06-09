BITS ?= 64

# Hosted build
all: kernel.elf

clean:
	rm -f *.o *.o kernel.elf kernel.o fwc-vm.o

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
kernel.o: system.c fwc-vm.h fwc-vm.c drivers.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c system.c -o kernel.o

fwc-vm.o: fwc-vm.c fwc-vm.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c fwc-vm.c -o fwc-vm.o

fwc-boot.o: fwc-boot.c fwc-vm.h
	$(CROSS_CC) $(CFLAGS_BARE) -DBARE_METAL -c fwc-boot.c -o fwc-boot.o

# Kernel ELF image
kernel.elf: boot.o drivers.o idt-asm.o kernel.o fwc-vm.o fwc-boot.o
	$(LD) $(LDFLAGS_BARE) \
		boot.o \
		idt-asm.o \
		drivers.o \
		kernel.o fwc-vm.o fwc-boot.o \
		-o kernel.elf
	ls -l kernel.elf

# Run in QEMU
run: kernel.elf
	@echo "Starting FWC Bare Metal in QEMU (interactive serial console)..."
	@echo "Press Ctrl+C to exit QEMU"
	@echo ""
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none

run-debug: kernel.elf
	@echo "Starting FWC Bare Metal in QEMU with debug output..."
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none \
		-d int,guest_errors

run-test: kernel.elf
	@echo "Running QEMU test (output to /tmp/qemu_serial.log)..."
	bash test_qemu.sh

run-gdb: kernel.elf
	qemu-system-i386 -kernel kernel.elf -serial stdio -nographic -m 64 -monitor none \
		-d int,guest_errors,cpu_reset -S -gdb tcp::1234

clean:
