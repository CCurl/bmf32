# Makefile for Bare Metal OS

# Compiler and tools
CC = clang
LD = ld
OBJCOPY = objcopy
GRUB_MKRESCUE = grub-mkrescue
QEMU = qemu-system-i386

# Flags
CFLAGS = -ffreestanding -fno-stack-protector -fno-pie -m32 -c
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Directories
BUILD_DIR = build
ISODIR = $(BUILD_DIR)/iso
BOOTDIR = $(ISODIR)/boot
GRUBDIR = $(BOOTDIR)/grub

# Files
KERNEL = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
ISO_IMAGE = $(BUILD_DIR)/os.iso
OS_SUPPORT_OBJ = $(BUILD_DIR)/os.o
DWC_VM_OBJ = $(BUILD_DIR)/dwc-vm.o

# Default target
all: $(KERNEL)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile kernel
$(BUILD_DIR)/kernel.o: kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) kernel.c -o $(BUILD_DIR)/kernel.o

# Create boot.h from boot.f
boot.h: boot.f block-01.fth
	fwc

# Compile OS-specific support
$(OS_SUPPORT_OBJ): os.c boot.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) os.c -o $(OS_SUPPORT_OBJ)

# Compile the DWC VM
$(DWC_VM_OBJ): dwc-vm.c dwc-vm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) dwc-vm.c -o $(DWC_VM_OBJ)

# Link kernel
$(KERNEL): $(BUILD_DIR)/kernel.o $(OS_SUPPORT_OBJ) $(DWC_VM_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(BUILD_DIR)/kernel.o $(OS_SUPPORT_OBJ) $(DWC_VM_OBJ)

# Create bootable ISO image
iso: $(KERNEL)
	mkdir -p $(GRUBDIR)
	cp $(KERNEL) $(BOOTDIR)/kernel.elf
	echo 'menuentry "Bare Metal OS" {' > $(GRUBDIR)/grub.cfg
	echo '  multiboot2 /boot/kernel.elf' >> $(GRUBDIR)/grub.cfg
	echo '}' >> $(GRUBDIR)/grub.cfg
	$(GRUB_MKRESCUE) -o $(ISO_IMAGE) $(ISODIR)

# Run on QEMU
run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -m 16M -serial stdio -d guest_errors

# Run from ISO
run-iso: iso
	$(QEMU) -cdrom $(ISO_IMAGE) -m 4M -serial stdio -d guest_errors

# Debug with GDB
debug: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -m 4M -serial stdio -S -gdb tcp::1234 -d guest_errors

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) boot.h

.PHONY: all iso run run-iso debug clean
