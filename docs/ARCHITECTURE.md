# FWC Bare Metal Implementation - Architecture Plan

## Overview
Transform FWC from a hosted application into a minimal bare metal 32-bit OS that boots directly in QEMU.

**Core Strategy**: Keep FWC VM unchanged, create a thin x86 bootloader & kernel HAL layer to replace POSIX system.c, and provide Forth primitives for hardware access (serial, keyboard, timer, interrupts).

**Recommended Approach**: Multiboot-compatible bootloader (grub-friendly) + minimal kernel stub (300-500 lines) + restructured system layer that maps Forth primitives to bare metal hardware.

---

## Architecture Decisions

### Bootloader: Multiboot (not UEFI/BIOS)
- **Rationale**: QEMU supports it natively, works on real x86 hardware, simpler than UEFI
- **Format**: ELF 32-bit, loaded at 1MB memory address
- **Assembly**: FASM syntax for maximum compatibility

### Memory Model: Flat 32-bit (no paging)
- **Rationale**: Simpler for MVP, sufficient for QEMU test
- **Layout**: Kernel at 1MB, FWC VM at 2MB, 4GB total addressable (32-bit limit)
- **Caveat**: Limits real hardware to 4GB, no memory protection between processes

### I/O: Serial Console (COM1) + PS/2 Keyboard
- **Serial**: Main console (0x3F8 port, 115200 baud)
- **Keyboard**: PS/2 scan codes from port 0x60
- **Rationale**: QEMU default, reliable, no video driver needed

### Interrupt Model: PIC (not APIC)
- **Rationale**: Simpler, works on all x86, sufficient for MVP
- **Caveat**: Limits scalability to single core; APIC needed for SMP

### Bootstrap: Embedded in Kernel
- **Approach**: Embed fwc-boot.fth as kernel .rodata section
- **Advantage**: Simplifies initial testing, no filesystem needed
- **Alternative**: Load from disk (requires ATA driver, deferred)

---

## Implementation Phases

### ✅ Phase 1: Bootloader & Kernel Entry (COMPLETE)
1. **boot.asm** (FASM, ~70 lines)
   - ✅ Multiboot header (magic, flags, checksum)
   - ✅ GDT setup with flat 32-bit memory model
   - ✅ Protected mode enabled
   - ✅ Stack setup at 0x8000
   - ✅ Jump to C kernel entry (kmain)

2. **linker.ld** (~40 lines)
   - ✅ Kernel loaded at 1MB (0x100000)
   - ✅ Sections: .boot, .text, .rodata, .data, .bss
   - ✅ ELF executable format

3. **system.c** (kmain + REPL, ~205 lines)
   - ✅ Initialize serial port (COM1)
   - ✅ Initialize PIC and IDT
   - ✅ Initialize FWC VM
   - ✅ Enter REPL loop with line buffering

### ✅ Phase 2: Hardware Abstraction Layer (COMPLETE)

All drivers consolidated into **drivers.c** and **drivers.h** (~1000 lines combined):

1. **Serial Driver** (~200 lines)
   - ✅ Serial output: `zType()`, `emit()` → serial_write_string()
   - ✅ Serial input: `key()` → serial_getc() (blocking)
   - ✅ Non-blocking: `qKey()` → serial_has_data()
   - ✅ Baud rate 115200, 8-N-1 format
   - ✅ Replaces stdout/stdin entirely

2. **Timer Driver** (~150 lines)
   - ✅ PIT initialization at 0x40-0x43
   - ✅ `timer()` → timer_get_ticks()
   - ✅ `ms()` → timer_sleep_ms()
   - ✅ Tick interrupt support

3. **PIC Driver** (~80 lines)
   - ✅ PIC initialization (remap IRQ0-7 to INT32-39)
   - ✅ `pic_init()`, `pic_send_eoi()`, `pic_enable_irq()`
   - ✅ EOI signaling for hardware interrupts

4. **PS/2 Keyboard Driver** (~200 lines)
   - ✅ PS/2 keyboard at port 0x60 (data), 0x64 (status)
   - ✅ Scan code → ASCII conversion
   - ✅ `ps2_has_key()`, `ps2_getc()`, `ps2_interrupt_handler()`
   - ✅ Support for shift, ctrl, alt modifiers

5. **IDT & ISR Stubs** (~200 lines combined)
   - ✅ **idt.c** - Interrupt Descriptor Table setup
   - ✅ **idt.asm** (FASM) - ISR stubs for exceptions (INT 0-14) and hardware IRQs (INT 32-47)
   - ✅ Specific handlers: timer (INT32), keyboard (INT33), serial (INT36)

6. **String Library** (~100 lines)
   - ✅ Implements: `strcasecmp()`, `strlen()`, `strcpy()`, `memcpy()`, `memmove()`, `memset()`
   - ✅ Bare metal libc replacements

### ✅ Phase 3: Bootable Kernel (COMPLETE)
1. **Build and test**
   - ✅ Assemble boot.asm with FASM → boot.o
   - ✅ Compile drivers and system layer
   - ✅ Link all components → kernel.elf (31 KB)
   - ✅ Test with QEMU: `qemu-system-i386 -kernel kernel.elf`

2. **QEMU test script**
   - ✅ test_qemu.sh - Runs kernel with serial I/O capture
   - ✅ Full debugging output available

3. **Boot sequence verified**
   - ✅ "FWC Bare Metal Kernel" prints to serial
   - ✅ All drivers init successfully, no crashes
   - ✅ REPL prompt appears and accepts input
   - ✅ Forth arithmetic tested and working

### ⏳ Phase 4: Bootstrap File (READY)
1. **fwc-boot.fth** (Bootstrap vocabulary)
   - Ready to load as embedded kernel .rodata
   - Contains Forth control structures (if/then, begin/until, etc.)
   - Ready for testing in live kernel

2. **block-01.fth** (Additional vocabulary)
   - Supplemental Forth words
   - Ready for integration

### ⏳ Phase 5: Advanced Features (Future)
1. **Block disk I/O** - Requires ATA/IDE driver
2. **Advanced keyboard** - Extended scan codes, key remapping
3. **Additional memory management** - Heap allocation, garbage collection

---

## Current Architecture

## File Structure

### Core VM (Portable)
```
fwc-vm.c           - Forth VM implementation (threaded code interpreter, 64 primitives)
fwc-vm.h           - VM definitions and declarations
fwc-boot.fth       - Bootstrap Forth vocabulary
block-01.fth       - Additional Forth definitions
```

### Bare Metal Kernel
```
boot.asm           - Multiboot bootloader (FASM, 70 lines)
idt.asm            - Interrupt handlers (FASM, ~150 lines)
linker.ld          - ELF linker script
```

### Drivers & System (Consolidated)
```
drivers.c          - Unified driver implementation (~1000 lines)
                     • Serial (COM1): serial_init(), serial_write_string(), serial_getc()
                     • Timer (PIT): timer_init(), timer_get_ticks(), timer_sleep_ms()
                     • PIC: pic_init(), pic_send_eoi(), pic_enable_irq()
                     • PS/2: ps2_init(), ps2_getc(), ps2_has_key()
                     • IDT: idt_init(), interrupt dispatcher
                     • String: strcpy(), strlen(), memcpy(), memmove(), etc.

drivers.h          - Unified driver interface

system.c           - System layer & REPL (~205 lines)
                     • kmain() - Kernel entry point
                     • Hardware initialization sequence
                     • zType(), emit(), key(), qKey() - I/O primitives
                     • timer(), ms() - Time primitives
                     • repl() - Interactive Read-Eval-Print Loop
```

### Build System
```
makefile           - Build targets and rules
                     • make kernel.elf - Build bare metal kernel
                     • make qemu-run - Run in QEMU
                     • make clean-bare - Clean build artifacts

test_qemu.sh       - QEMU launcher with output logging
```

---

## Key Abstractions

| Component | File(s) | Purpose |
|-----------|---------|---------|
| **Bootloader** | boot.asm | Multiboot header, GDT, protected mode entry, stack setup |
| **Linker** | linker.ld | Memory layout, section placement, 1MB load address |
| **Kernel Entry** | system.c::kmain() | Hardware init sequence, REPL |
| **Serial Driver** | drivers.c | Replace stdout/stdin with COM1 (0x3F8, 115200 baud) |
| **Timer Driver** | drivers.c | PIT initialization, ticks, sleep function |
| **PIC Driver** | drivers.c | Interrupt controller, IRQ remap, EOI signaling |
| **PS/2 Driver** | drivers.c | Keyboard input via interrupts, scan→ASCII |
| **IDT/ISR Stubs** | drivers.c, idt.asm | Interrupt dispatch, exception handling |
| **String Utilities** | drivers.c | strcpy, strlen, memcpy, etc. (bare metal libc) |
| **Forth VM** | fwc-vm.c/h | Threaded code interpreter (unchanged, 32-bit compatible) |
| **I/O Primitives** | system.c | zType(), emit(), key(), qKey() - ties VM to hardware |

---

## Testing Strategy

### Phase 1-2 Verification
1. kernel.elf produces valid ELF binary
2. QEMU boots without crash: `qemu-system-i386 -kernel kernel.elf`
3. Serial output visible: "FWC Bare Metal Kernel starting..."
4. No exceptions/page faults in QEMU debug

### Phase 3 Verification
1. Forth prompt appears: "ok"
2. Simple Forth works: `1 2 +` → stack shows 3
3. Loops work: `1 10 for i next` completes without hang
4. Strings work: `z" hello" ztype` prints "hello"

### Phase 4-5 Verification
1. Bootstrap loads successfully
2. High-level Forth words available (if/then, begin/again, etc)
3. Arithmetic, loops, conditions work
4. Keyboard input responds
5. Timer ticks increase

### Integration Test
```forth
: test-math 10 20 + . ;
test-math  ( prints 30 )

: test-loop 5 0 for i . next ;
test-loop  ( prints 0 1 2 3 4 )

: test-cond 5 3 > if ." yes" then ;
test-cond  ( prints yes )
```
