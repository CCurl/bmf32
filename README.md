# A 32-bit Bare Metal FORTH OS/Kernel

A minimal 32-bit x86 bare metal operating system kernel written entirely in **pure assembly (FASM)**.<br/>
This is intended to be a foundation for a subroutine threaded FORTH system.<br/>
Currently runs under QEMU (the 32-bit x86 emulator) using the `-kernel` option.

## Features

- **32-bit x86 protected mode**: Full x86-32 architecture support
- **Pure Assembly (FASM)**: Entire kernel in single `.asm` file
- **VGA text console**: 80×25 text mode output (0xB8000)
- **Serial output**: COM1 (0x3F8) for debugging/secondary output
- **Interrupt system**: IDT + 8259 PIC with PS/2 keyboard handler
- **PS/2 keyboard**: Ring buffer for scancode capture (IRQ1/INT 0x21)
- **Memory layout**: 32 MB for FORTH (stacks, dictionary, graphics buffer)
- **Direct kernel loading**: Boots with QEMU `-kernel` flag

## Quick Start

```bash
# Prerequisites
sudo apt-get install fasm qemu qemu-system-x86

# Clone the repository
git clone https://github.com/CCurl/bmf32.git

# Build and run
make run
```

QEMU window will open. You'll see boot messages. PS/2 keyboard input is buffered and ready for FORTH interpreter.

## Project Structure

```
.
├── kernel.asm       # Bootloader + kernel + drivers
├── linker.ld        # Memory layout script
├── Makefile         # Build automation
├── LICENSE          # License (MIT)
└── README.md        # This file
```

## Building

```bash
make          # Full build
make clean    # Remove artifacts
make run      # Build and run in QEMU window
```

**Toolchain:**
- FASM 1.73.30+ (assembler)
- GNU ld (linker, elf_i386 format)

## Memory Layout (32 MB)

```
0x01FFFFFF  ┌─────────────────────────────┐
            │ User Dictionary (grows UP)  │ ~15 MB free
0x00600500  ├─────────────────────────────┤
            │ Buffer                      │ 256 bytes
0x00600400  ├─────────────────────────────┤
            │ Data stack (grows DOWN)     │ 1 KB, 256 entries
0x00600000  ├─────────────────────────────┤
            │ Graphics buffer             │ 4 MB
0x00200000  ├─────────────────────────────┤
            │ Kernel + ESP stack          │ 1 MB (16 KB stack)
0x00100000  ├─────────────────────────────┤
            │ VGA text (HW)               │ 4 KB
0x000B8000  ├─────────────────────────────┤
            │ BIOS / System               │
0x00000000  └─────────────────────────────┘
```

**Dictionary Entry Format:**
```
[Offset 0:3]   Link pointer to previous entry (4 bytes)
[Offset 4:7]   Execution Token (XT) (4 bytes)
[Offset 8:8]   Flags (1 byte)  
[Offset 9:9]   Length (1 byte)  
[Offset 10:n]  Name, NULL-terminated (variable length)
[Offset n+1:m] Inline code (XT, variable size)
```

## Kernel Components

### Bootloader (_start)
- Stack setup (ESP -- stack_top, 16 KB kernel stack)
- Calls kernel_main

### IDT & PIC (Interrupt Handling)
- **IDT**: 256-entry interrupt descriptor table
- **PIC**: Master/Slave programmable interrupt controller
  - Maps IRQ0-7 -- INT 0x20-0x27
  - Maps IRQ8-15 -- INT 0x28-0x2F
  - IRQ1 (keyboard) enabled by default

### VGA Driver
- `kernel_clear()` - Clear screen, reset cursor
- `vga_putchar(AL)` - Write char at cursor, advance, wrap, scroll
- `vga_write(ESI)` - Write null-terminated string
- Text mode: 80×25 @ 0xB8000

### Serial Driver (COM1)
- `ser_write(ESI)` - Write null-terminated string to serial port
- Port: 0x3F8 (COM1)
- Used for debugging output

### Timer (IRQ0)
- **Handler**: `timer_handler()` (INT 0x20/IRQ0)
- **Counter**: `timer_ticks` - Incremented on each timer tick
- **Reader**: `timer_get_ticks()` - Non-blocking, returns current tick count
- **Init**: IRQ0 enabled by default, PIC configured

### PS/2 Keyboard
- **Handler**: `keyboard_handler()` (INT 0x21/IRQ1)
- **Ring buffer**: 32 scancodes, power-of-2 wrap with AND
- **Reader**: `keyboard_read()` - Non-blocking, returns scancode or 0
- **Data check**: `keyboard_has_data()` - Non-blocking, returns 1 if buffer has data
- **Status check**: Port 0x64 bit 0 before reading 0x60
- **Init**: Disables/re-enables controller, enables IRQ1

### Utility Functions
- `hex_to_string(EAX, ESI)` - Convert 32-bit to "0xXXXXXXXX"
- `idt_set_entry(EAX, BL, CL)` - Configure IDT entry
- `init_idt()` - Initialize IDT, load with LIDT
- `init_pic()` - Configure PIC for IRQ remapping
- `init_ps2()` - Initialize PS/2 keyboard hardware
- `pic_enable_irq(AL)` - Enable timer interrupt (clear PIC mask bit AL)
- `timer_get_ticks()` - Read current timer tick count
- `keyboard_read()` - Non-blocking read from keyboard buffer
- `keyboard_has_data()` - Check if keyboard buffer has pending scancodes

### FORTH Dictionary & Primitives
- **Dict pointer**: EBP (data stack pointer, grows downward from DATA_STK_BASE)
- **Entry format**: [Link(4)] [XT(4)] [Flags/Len(1)] [Name(variable)] [NULL] [Code]
- **Stack macros**:
  - `dPush reg` - Push register onto data stack
  - `dPop reg` - Pop from data stack into register
  - `getTOS reg` - Read top of stack (non-destructive)
  - `getNOS reg` - Read 2nd element (non-destructive)
  - `setTOS reg` - Write top of stack
  - `setNOS reg` - Write 2nd element

## Running

```bash
# Build and run (serial output to terminal)
make qemu

# Or directly:
qemu-system-i386 -kernel kernel.elf -m 32M -serial stdio

# Without serial output:
qemu-system-i386 -kernel kernel.elf -m 32M
```

## Implemented Primitives

**Stack Manipulation:**
- [x] CELL - Push cell size (4)
- [x] DUP - Duplicate TOS
- [x] DROP - Remove TOS
- [x] SWAP - Exchange TOS and NOS
- [x] OVER - Copy NOS to TOS

**Arithmetic:**
- [x] \+ (ADD) - Add TOS and NOS
- [x] \- (SUB) - Subtract TOS from NOS
- [x] \* (MULT) - Multiply TOS and NOS
- [x] / (DIV) - Signed divide NOS by TOS

**I/O & Utility:**
- [x] EMIT - Output character (to VGA + serial)
- [x] KEY? - Check if keyboard buffer has data
- [x] TIMER - Get current timer tick count
- [x] WORDS - List all dictionary words
- [x] NUMBER? - Parse string to integer ($hex, %binary, #decimal, 'char', -negative)
- [x] WORD - Parse next word from input stream
- [x] STRLEN - Get length of null-terminated string
- [x] C, - Store byte at HERE, increment by 1
- [x] , - Store cell at HERE, increment by 4

## Next Steps (FORTH Implementation)

**Roadmap for remaining FORTH:**

1. **More stack primitives** - ROT, -ROT, NIP, TUCK, DEPTH, PICK, ROLL
2. **Comparison** - <, >, =, <>, <=, >=, 0<, 0>, 0=
3. **Memory access** - @, !, C@, C!, +!
4. **Control flow** - IF, THEN, ELSE, BEGIN, UNTIL, LOOP, DO
5. **FORTH I/O** - KEY, CR, SPACES, ACCEPT (read line)
6. **Interpreter loop** - Token parsing, execute from dictionary
7. **Word definition** - Colon definitions (: name ... ;)
8. **Graphics** - PIXEL drawing using 4MB buffer
9. **Optimizations** - JIT compilation, tail call optimization

## Debug Commands

```bash
# Inspect binary
file kernel.elf
readelf -l kernel.elf        # Program headers
readelf -S kernel.elf        # Section headers
nm kernel.elf                 # Symbols

# Disassemble
objdump -d kernel.elf | less
objdump -M intel -d kernel.elf  # Intel syntax

# Check multiboot magic
objdump -s -j .multiboot kernel.elf | head -5
```

## Known Limitations / TODOs

- [x] Stack abstraction (EBP-based data stack)
- [x] Dictionary infrastructure
- [x] Core primitives (17 implemented)
- [x] Number parsing (numq with multiple bases)
- [x] Dictionary lookup (case-insensitive)
- [ ] FORTH interpreter loop not yet implemented
- [ ] No scancode→ASCII conversion (raw scancodes in buffer)
- [ ] Graphics buffer allocated but unused
- [ ] No disk support

## Architecture Notes

**Why pure assembly?**
- No dependency on a 3rd party compiler
- Total control over memory layout and execution
- Minimal overhead (very small kernel!)
- Single executable file, no dependencies
- Perfect for bare metal + FORTH experimentation

**Register conventions:**
- EAX, EDX: Return values / scratch
- ESI: String pointer (calls)
- EBX, ECX: General purpose
- ESP: Return stack (Forth and x86 stack calls/returns)
- EBP: FORTH data stack pointer (grows downward, initialized to `DATA_STK_BASE`)

**Calling convention:**
- Return via RET (pops EIP)
- No STDCALL (manual stack management)

## Tools Used

- **FASM** (v1.73.30) - Compact, elegant, open-source assembler
- **GNU ld** - Linker with custom script
- **QEMU** - Machine emulator (i386 mode)
- **readelf/objdump** - ELF inspection

## References

- [OSDev.org Wiki](https://wiki.osdev.org/)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/)
- [x86 Instruction Set](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-manual-combined-volumes-1-2a-2b-2c-2d.pdf)
- [FASM Documentation](https://flatassembler.net/)
- [FORTH Standards](https://forth-standard.org/)

## License

MIT License
