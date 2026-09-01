# Memory Map

This project is a 32-bit bare-metal x86 kernel running under QEMU. The linker script places the kernel image at 1 MB and uses a small set of fixed hardware addresses for devices and I/O.

## 1. Kernel image at 1 MB

The kernel entry point is defined in [linker.ld](linker.ld):

- Entry point: `kernel_main`
- Kernel load address: `0x100000`

The linker script currently uses a flat single-segment layout. The sections are placed contiguously:

- `.multiboot`
- `.text`
- `.rodata`
- `.data`
- `.bss`

Because the whole image is one load segment, the linker emits a warning that the segment is `RWX`. This is intentional for the current minimal kernel design.

## 2. Segment layout

The current layout is intentionally simple:

- one load segment contains the whole kernel image
- that segment is `RWX`
- the linker warning is expected in this setup

This is not a protected-mode split between kernel and user memory. It is a minimal flat kernel layout.

## 3. Hardware memory and I/O regions

The kernel manually accesses specific hardware addresses in [kernel.c](kernel.c):

- VGA text memory: `0xB8000`
  - used for 80x25 text mode output
- PIC master controller: `0x20`, `0x21`
- PIC slave controller: `0xA0`, `0xA1`
- PS/2 keyboard data port: `0x60`
- PIT port: `0x40`, `0x41`, `0x43`

These addresses are outside the ELF image and are treated as direct hardware interfaces.

## 4. Runtime and VM memory

The OS support layer in [os.c](os.c) owns runtime-side storage and compatibility glue for freestanding code. The Forth VM in [dwc-vm.c](dwc-vm.c) and [dwc-vm.h](dwc-vm.h) use a flat in-memory dictionary and stack region, and the OS layer provides the missing freestanding helpers such as string operations and keyboard/timer primitives.

This separates:

- kernel core: boot, interrupts, VGA, serial, PIC, keyboard, timer
- runtime support: VM memory, libc-like shims, and OS glue

## 5. Interrupt and timer model

The current kernel includes:

- GDT and IDT setup
- PIC initialization
- keyboard interrupt handling
- timer interrupt handling via the PIT at 50 Hz
- a simple `system_ticks` counter used by the VM `timer` primitive

At 50 Hz, each tick is effectively 20 ms, so `system_ticks` is advanced in coarse steps rather than every 1 ms.

This means the current system is no longer a purely passive boot-only kernel; it includes basic interrupt-driven behavior.

## 6. Mental model

A useful way to think about the layout is:

- `0x00000000` to `0x000FFFFF`: low memory / reserved / bootstrap area
- `0x00100000`: kernel image start
- kernel image: flat `RWX` ELF image
- `0xB8000`: VGA text memory
- keyboard and timer interrupts: IRQ-driven behavior through PIC and port I/O
- VM/runtime memory: in-kernel storage used by the Forth interpreter

## 7. Summary

This system currently follows a flat bare-metal design:

- the kernel is loaded at `0x100000`
- the image is intentionally a single `RWX` load segment
- the linked ELF is the complete OS image for this project; there is no separate `boot.bin`
- hardware devices are addressed via fixed memory and port locations
- interrupts are active for keyboard and timer support
- the Forth VM runs inside the same minimal kernel environment

This is a deliberately simple early-stage kernel, not a hardened, split-user-space design.
