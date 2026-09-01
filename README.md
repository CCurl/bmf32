# Bare Metal OS - QEMU

A minimal bare-metal x86 kernel written in pure C and booted under QEMU. It includes a simple text-mode VGA console, serial output, interrupt-driven keyboard input, a PIT-based tick counter, and a small Forth-like VM for experimentation.

## Features

- **Pure C kernel**: written without assembly for the main runtime logic
- **Multiboot1 support**: kernel entry is recognized by a multiboot header
- **Single-kernel flat image**: the linked ELF is the OS image for this project
- **VGA text mode**: console output with scrolling support
- **Serial output**: debug messages through COM1
- **Interrupts**: GDT, IDT, PIC, keyboard, and timer handling
- **Keyboard input**: buffered PS/2 keyboard support
- **Tick counter**: PIT-driven 50 Hz timer used by the VM and timing helpers
- **DWC VM**: the in-kernel Forth-style interpreter is compiled and linked
- **QEMU compatible**: boots directly with the `-kernel` flag or via the generated ISO

## Architecture

```text
block-10.fth    - translates boot.f -> boot.h using fwc
boot.f          - Forth source code for base forth system
dwc-vm.c        - Forth-style VM implementation
dwc-vm.h        - VM interface and memory layout declarations
kernel.c        - kernel core: VGA, serial, PIC, keyboard, timer, IRQ setup
kernel.h        - extern functions for kernel.c
LICENSE         - MIT license
linker.ld       - memory layout and ELF placement
Makefile        - build system
os.c            - OS/runtime support layer and freestanding compatibility helpers
README.md       - this file
```

## Building

### Prerequisites

You will need fwc, a 32-bit toolchain and QEMU.

```bash
# Ubuntu/Debian
sudo apt-get install build-essential qemu-system-x86 gcc-multilib grub-pc-bin

# Fedora/RHEL
sudo dnf install gcc gcc-multilib glibc-devel.i686 qemu-system-x86 grub2-tools
```

### Build the kernel

```bash
make
```

This produces `build/kernel.elf`, which is the complete kernel/OS image for this project. There is no separate `boot.bin` in this build flow.

## Running

### Direct boot

```bash
make run
```

This builds the kernel and runs it in QEMU.

### ISO boot

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

## Project structure

### kernel.c

The kernel implements:

- **Multiboot header**
- **GDT / IDT initialization**
- **PIC setup**
- **Keyboard interrupt processing**
- **PIT timer tick counting**
- **Serial I/O**
- **VGA text-mode console**
- **kernel_main()** entry point

### dwc-vm.c and dwc-vm.h

This is the Forth-like VM used by the project. It includes:

- a dictionary and primitive table
- stack operations and compiled words
- VM entry points like `outer()`, `inner()`, and `dwcInit()`
- primitive hooks for `emit`, `ztype`, `key`, `key?`, and `timer`

### os.c

This file provides the minimal runtime glue needed for a freestanding build, including:

- libc-like string/memory helpers
- keyboard and timer wrappers used by the VM
- stubbed `emit` / `ztype` output support

### linker.ld

Defines the memory layout:

- code starts at `0x100000` (1 MB)
- a flat single-segment kernel image is used

## Important notes

- **No full standard library**: the kernel is built with `-ffreestanding`
- **No dynamic memory**: the project is still intentionally minimal
- **Interrupts are active**: GDT/IDT, keyboard, and timer are implemented
- **Timer rate**: the PIT is configured for 50 Hz, so `system_ticks` advances at roughly 20 ms per tick
- **The VM is linked into the kernel**: the build includes [dwc-vm.c](dwc-vm.c)

## Troubleshooting

### QEMU shows nothing

- ensure QEMU is installed
- check that VGA and serial output are enabled
- run `make run` for the direct boot path

### Compiler errors about stdint.h

- install 32-bit development headers:

```bash
sudo apt-get install gcc-multilib
```

### `grub-mkrescue: command not found`

- install GRUB tools:

```bash
sudo apt-get install grub-pc-bin xorriso
```

### QEMU hangs after boot

- this may happen if the kernel is waiting for input or interrupts are not configured properly
- use `Ctrl+A` then `X` to exit QEMU

## References

- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/)
- [OSDev Wiki](https://wiki.osdev.org/)
- [x86 I/O Ports](https://wiki.osdev.org/I/O_Ports)
- [VGA Text Mode](https://wiki.osdev.org/Text_mode)

## License

Public Domain - use freely for educational purposes.
