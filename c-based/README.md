# Bare Metal OS - QEMU

A minimal bare metal operating system shell written in pure C for QEMU. No assembly required.

## Features

- **Pure C Implementation**: The entire kernel is written in C without assembly
- **Multiboot1 Support**: Uses the Multiboot1 standard for bootloader compatibility
- **VGA Text Mode**: Display text output to the screen
- **Serial Port**: Debug output via serial port (COM1)
- **QEMU Compatible**: Boots directly with QEMU's `-kernel` flag

## Architecture

```
kernel.c        - Main kernel implementation with VGA and serial I/O
linker.ld       - Memory layout and linking script
Makefile        - Build system
run.sh          - Convenience script to build and run
```

## Building

### Prerequisites

You'll need the following tools installed:

```bash
# Ubuntu/Debian
sudo apt-get install build-essential qemu-system-x86 gcc-multilib grub-pc-bin

# Fedora/RHEL
sudo dnf install gcc gcc-multilib glibc-devel.i686 qemu-system-x86 grub2-tools
```

### Build the Kernel

```bash
make
```

This produces `build/kernel.elf` which is the bootable kernel image.

## Running

### Quick Start (Direct Boot)

```bash
make run
```

This builds the kernel and boots it in QEMU using direct kernel boot.

**Output**: You should see a white-on-black terminal with:
```
=== Bare Metal OS ===
Kernel loaded successfully!
```

### ISO Boot

For full Multiboot2 compliance with GRUB:

```bash
make iso
make run-iso
```

### Debug with GDB

```bash
make debug
```

Then in another terminal:
```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) continue
```

## Project Structure

### kernel.c

The kernel implements:

- **Multiboot2 Header**: Required magic numbers and header for bootloader recognition
- **Serial I/O**: Functions for debug output via COM1 serial port
  - `serial_init()` - Initialize serial port
  - `serial_putchar()` - Write single character
  - `serial_puts()` - Write string
  
- **VGA Text Mode**: Display functions
  - `vga_clear()` - Clear screen
  - `vga_putchar()` - Write single character
  - `vga_puts()` - Write string
  - `vga_set_cursor()` - Position cursor
  
- **Hardware I/O**: Port read/write functions
  - `outb()` - Write byte to port
  - `inb()` - Read byte from port
  
- **kernel_main()**: Entry point after bootloader

### linker.ld

Defines the memory layout:
- Code starts at 0x100000 (1MB, standard kernel location)
- Sections: .multiboot, .text, .rodata, .data, .bss

## Extending the Kernel

### Adding C Code

Simply add new `.c` files and update the Makefile to compile them.

### Important Restrictions

- **No standard library**: The kernel uses `-ffreestanding` flag, so no libc functions
- **Inline assembly only**: Use `asm()` for assembly, not separate .s files
- **No dynamic memory**: No malloc/free in initial implementation
- **No interrupts yet**: The CPU just runs to halt

### Common Additions

1. **Keyboard Input**:
   ```c
   uint8_t inb(uint16_t port);  // Read from keyboard port 0x60
   ```

2. **Memory Management**:
   ```c
   void* alloc(size_t size);    // Implement a simple allocator
   ```

3. **Interrupts** (requires GDT/IDT setup):
   ```c
   void setup_idt(void);
   void register_interrupt_handler(int vector, void (*handler)(void));
   ```

4. **Paging**:
   ```c
   void setup_paging(void);
   ```

## QEMU Exit Codes

- Exit code 0: Normal shutdown (after `hlt` instruction)
- Exit code 1: Invalid instruction or general fault

## Troubleshooting

### QEMU shows nothing
- Ensure QEMU is built with VGA support
- Check that serial output is enabled: `make run` should show serial output

### Compiler errors about stdint.h
- Install 32-bit development headers: `sudo apt-get install gcc-multilib`

### "grub-mkrescue: command not found"
- Install GRUB utilities: `sudo apt-get install grub-pc-bin xorriso`
- Or just use `make run` which doesn't require GRUB

### QEMU hangs after boot
- This is normal! The kernel halts with an infinite loop of `hlt` instructions
- Press `Ctrl+A` then `X` to exit QEMU

## References

- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/)
- [OSDev Wiki](https://wiki.osdev.org/)
- [x86 I/O Ports](https://wiki.osdev.org/I/O_Ports)
- [VGA Text Mode](https://wiki.osdev.org/Text_mode)

## License

Public Domain - Use freely for educational purposes
