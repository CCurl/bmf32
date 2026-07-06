.PHONY: all clean run iso qemu

# Assembler and linker settings
FASM ?= fasm
LD ?= ld

# Flags
LDFLAGS = -T linker.ld -m elf_i386 --oformat=elf32-i386

# Files
KERNEL_OBJ = kernel.o
KERNEL_ELF = kernel.elf
ISO_DIR = isodir
ISO_NAME = kernel.iso

all: $(KERNEL_ELF)

$(KERNEL_OBJ): kernel.asm
	$(FASM) kernel.asm $(KERNEL_OBJ)

$(KERNEL_ELF): $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) $(KERNEL_OBJ) -o $(KERNEL_ELF)

qemu: $(KERNEL_ELF)
	qemu-system-i386 -kernel $(KERNEL_ELF) -m 32M -serial stdio

run: qemu

clean:
	rm -f $(KERNEL_OBJ) $(KERNEL_ELF)
