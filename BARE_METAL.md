# FWC Bare Metal 32-bit OS

FWC can run as a **bare metal operating system** on 32-bit x86 processors, tested with QEMU emulator and designed for real hardware compatibility.

## Quick Start

### Build the Kernel
```bash
make kernel.elf          # Compile bootloader, drivers, and kernel (~31 KB)
```

### Run in QEMU
```bash
make qemu-run            # Launch in QEMU with serial console
# or: bash test_qemu.sh  # Run test script with output logging
```

### Expected Output
```
Booting from ROM..FWC Bare Metal Kernel starting...
Timer initialized
PIC initialized
PS/2 Keyboard initialized
IDT initialized
Interrupts enabled
FWC VM initialized
FWC Bare Metal v1.0
 ok
```

## Architecture

### Memory Layout
- **0x00000000 - 0x00100000**: BIOS/bootloader, reserved
- **0x00100000 (1MB)**: Kernel entry point (Multiboot load address)
- **0x00200000 (2MB)**: FWC VM memory (16 MB, configurable)
- **Flat 32-bit addressing**: No paging or virtual memory (MVP)

### Boot Process
1. **Bootloader** (`boot.asm`): FASM-assembled, Multiboot-compliant
   - Multiboot header within first 8KB
   - GDT setup for protected mode
   - Stack initialization (16 KB in BSS)
   - Jump to `kmain()` in system.c

2. **Hardware Initialization** (system.c::kmain):
   - Serial port (COM1) for console
   - Timer (PIT) for clock ticks
   - Programmable Interrupt Controller (PIC)
   - Interrupt Descriptor Table (IDT)
   - PS/2 Keyboard
   - Enable interrupts with STI

3. **FWC VM Initialization**:
   - `fwcInit()` - Setup primitives, stacks, dictionary
   - Load bootstrap (currently minimal, ready for fwc-boot.fth)
   - Enter REPL loop

### Hardware Drivers

All drivers are consolidated in **drivers.c** and **drivers.h** (~1000 lines combined):

#### Serial I/O (COM1, Port 0x3F8)
- Used for console input/output
- Replaces stdout/stdin from hosted mode
- Functions: `serial_init()`, `serial_write_string()`, `serial_getc()`, `serial_has_data()`
- Baud rate: 115200 (default, configurable)

#### Timer (PIT, Ports 0x40-0x43)
- Programmable Interval Timer (8253/8254)
- Provides tick counter and sleep function
- Functions: `timer_init()`, `timer_get_ticks()`, `timer_increment_ticks()`, `timer_sleep_ms()`
- Default frequency: ~1193 Hz (PIT divisor ~11932)

#### Interrupt Controller (PIC, Ports 0x20/0xA0)
- 8259 Programmable Interrupt Controller
- Remaps IRQ0-15 to INT32-47 (standard x86 mapping)
- Functions: `pic_init()`, `pic_send_eoi()`, `pic_enable_irq()`, `pic_disable_irq()`

#### PS/2 Keyboard (Ports 0x60/0x64)
- Reads scan codes from keyboard port 0x60
- Status register at port 0x64
- Functions: `ps2_init()`, `ps2_has_key()`, `ps2_getc()`, `ps2_interrupt_handler()`
- Supports shift, ctrl, alt for modified keys

#### Interrupt Dispatcher (IDT)
- **idt.c** + **idt.asm**
- Interrupt Descriptor Table setup
- ISR stubs for exceptions (INT 0-14) and hardware IRQs (INT 32-47)
- Specific handlers:
  - **INT32 (IRQ0/Timer)**: Calls `timer_increment_ticks()`
  - **INT33 (IRQ1/Keyboard)**: Calls `ps2_interrupt_handler()`
  - **INT36 (IRQ4/Serial)**: Reads from serial port (optional)
- Functions: `idt_init()`, `load_idt()`

#### String Library (libc Replacements)
- Implements: `strcasecmp()`, `strlen()`, `strcpy()`, `memcpy()`, `memmove()`, `memset()`
- Functions in drivers.c for bare metal C code
- Needed because bare metal cannot link standard libc

## Build System

### Files Structure
```

  boot.asm          - Multiboot bootloader (FASM syntax)
  boot.o            - Compiled bootloader (600 bytes)
  linker.ld         - ELF linker script, kernel at 1MB


  serial.c, h       - Serial COM1 driver
  timer.c, h        - PIT timer driver
  pic.c, h          - PIC interrupt controller
  ps2.c, h          - PS/2 keyboard driver
  idt.c, h          - IDT setup and registration
  idt.asm           - ISR stubs and dispatcher (FASM)
  string.c          - String function implementations
  drivers.h         - Unified driver interface

system.c - HAL layer (replaces POSIX system.c)
fwc-vm.c/h         - Forth VM (unchanged from hosted, 32-bit compatible)
kernel.elf         - Final bare metal kernel binary (~31 KB)

Makefile           - Build rules for bare metal targets
test_qemu.sh       - QEMU test launcher script
```

### Makefile Targets

```bash
# Build
make kernel.elf      # Compile and link bare metal kernel

# Run
make qemu-run        # Launch in QEMU with -d int,guest_errors debugging
make qemu-run-debug  # As above, plus -S -gdb tcp::1234 for GDB debugging

# Clean
make clean-bare      # Remove .o files and kernel.elf
make clean           # Clean hosted build only
```

### Build Requirements
- **gcc -m32** (32-bit cross-compiler)
- **ld** (linker, usually bundled with gcc)
- **fasm** (Flat Assembler, version 1.73+)

### Compiler Flags
```
-m32                    # 32-bit output
-ffreestanding          # No hosted runtime
-fno-stack-protector    # No stack canary (not needed for bare metal)
-fno-builtin            # Don't assume builtin functions
-nostdlib               # Don't link libc
```

## Porting to Real Hardware

### Multiboot Bootloader Compatibility
The kernel is **Multiboot-compliant**, so it can be loaded by:
- **GRUB 2** (Linux bootloader, supports Multiboot)
- **Any Multiboot-compatible bootloader**

### Steps for Real Hardware
1. Compile kernel as described above
2. Create GRUB menu entry pointing to kernel.elf
3. Boot with GRUB on x86-32 machine
4. Should see same "FWC Bare Metal Kernel starting..." message on serial console

Example GRUB menu entry:
```
menuentry 'FWC Bare Metal' {
    multiboot /boot/kernel.elf
    boot
}
```

### Serial Console on Real Hardware
- Serial port typically COM1 (0x3F8) - same as QEMU
- Use a null modem cable and serial terminal (minicom, picocom, etc.)
  ```bash
  minicom -D /dev/ttyS0 -b 115200  # Linux
  ```

## Testing

### Automated Test Script
```bash
bash test_qemu.sh
```
Runs QEMU with serial output to `/tmp/qemu_serial.log` and error log to `/tmp/qemu_errors.log`.

### Manual Testing
```bash
# Terminal 1: Run QEMU
make qemu-run

# Terminal 2: Send input to serial (if using stdio redirection)
# Type Forth commands and press Enter
```

### Expected Behavior
1. Kernel prints initialization messages
2. REPL loop responds with " ok\n" prompt
3. Keyboard input works (serial or PS/2)
4. No exceptions or page faults in QEMU debug output

### Debugging with GDB
```bash
# Terminal 1: Start QEMU with GDB stub
make qemu-run-debug

# Terminal 2: Connect with GDB
gdb kernel.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) c
```

## HAL Interface (system.c)

The bare metal HAL provides these functions to FWC VM:

### Console I/O
- `void zType(const char *str)` - Output string to serial
- `void emit(char ch)` - Output single character to serial
- `int key(void)` - Read character from keyboard (blocking)
- `int qKey(void)` - Check if key available (non-blocking)

### Timer
- `cell timer(void)` - Get current tick count
- `void ms(cell ms)` - Sleep for milliseconds

### Stubs (Not Implemented in MVP)
- `cell fOpen()`, `fClose()`, `fRead()`, `fWrite()` - Return 0 (no filesystem)
- `void ttyMode()` - No-op (no terminal mode switching)

## Differences from Hosted Mode

| Feature | Hosted | Bare Metal |
|---------|--------|------------|
| **I/O** | stdout/stdin | Serial COM1 |
| **Terminal** | ANSI codes, raw mode | Plain text only |
| **Filesystem** | Full POSIX | None (MVP) |
| **Memory** | Unlimited | Flat 32-bit (4GB max) |
| **Concurrency** | OS threads | Single task |
| **Real Time** | ~Millisecond precision | Microsecond precision (timer) |

## Known Limitations (MVP)

1. **No filesystem** - Cannot load files from disk
   - Solution: Pre-embed fwc-boot.fth in kernel memory or implement ATA driver

2. **Single task** - No process isolation or concurrency
   - Solution: Implement cooperative multitasking via Forth TASK word

3. **No paging** - Flat memory, no virtual memory
   - Solution: Implement page tables and MMU for multi-task or memory protection

4. **Limited hardware** - Only basic x86 features
   - Solution: Add APIC, IOAPIC, ACPI, PCI drivers as needed

5. **Serial console only** - No graphics or video
   - Solution: Implement framebuffer driver for VGA/UEFI GOP

## Future Enhancements

- [ ] Filesystem (FAT32, ext2, or custom block format)
- [ ] ATA/SATA disk driver
- [ ] Memory paging and virtual memory
- [ ] Cooperative multitasking
- [ ] Graphics/framebuffer support
- [ ] Network stack (if needed)
- [ ] ACPI for real hardware power management

## Troubleshooting

### Kernel won't boot in QEMU
- Verify kernel.elf is valid ELF: `file kernel.elf`
- Check serial output: `bash test_qemu.sh` and check `/tmp/qemu_serial.log`
- Enable QEMU debug output: `make qemu-run` (uses `-d int,guest_errors`)

### No serial output
- Ensure serial driver initializes without crash
- Check QEMU command line includes `-serial stdio` or `-serial file:...`
- Verify /dev/ttyS0 is available on real hardware

### Keyboard input not working
- PS/2 driver may need interrupt handler implementation
- Fallback to serial input (already implemented)
- Check `/tmp/qemu_errors.log` for interrupt-related messages

### GDB won't connect
- Verify QEMU runs with `-S` (pause at startup) and `-gdb tcp::1234`
- GDB target: `target remote :1234`
- Ensure no other process uses port 1234

## References

- **x86 Architecture**: Intel/AMD i386 manuals
- **Multiboot**: GNU Multiboot 2.0 specification
- **QEMU**: QEMU x86 system emulator documentation
- **FASM**: Flat Assembler documentation
- **Serial Port**: 16550 UART standard (COM1 at 0x3F8)
- **PIC**: 8259 Programmable Interrupt Controller
- **PIT**: 8253/8254 Timer specifications
- **PS/2**: AT Keyboard interface documentation
