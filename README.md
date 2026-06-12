# BMF: a very minimal Word-Code Forth

BMF is a minimal Forth system that can run stand-alone or be embedded into another program.

BMF is implemented in modular source files:
- **Core VM:** `bmf-vm.c` and `bmf-vm.h` (Forth virtual machine, ~200 lines)
- **Hardware Drivers:** `drivers.c` and `drivers.h` (consolidated: serial, timer, PIC, PS/2, IDT, string utilities)
- **System Layer:** `system.c` (bare metal I/O, interrupt handling, REPL)
- **Bootloader:** `boot.asm` and `linker.ld` (FASM, Multiboot-compliant)
- **Interrupts:** `idt.asm` (FASM, ISR stubs and handlers)

BMF has 64 primitives, all implemented as a high-performance threaded code interpreter.
The primitives are quite complete and any Forth system can be built from them.
Bootstrap files `bmf-boot.fth` and `block-01.fth` provide higher-level vocabulary.

## Bare Metal 32-bit OS Mode

BMF can also run as a **bare metal operating system** on 32-bit x86 QEMU or real hardware.
See [BARE_METAL.md](BARE_METAL.md) for details on building and running the bare metal kernel.

**Current Status**: ✅ Stable and deterministic - all interrupt-related non-determinism fixed, reliable interactive Forth execution verified.

**Quick start:**
```bash
make kernel.elf          # Build bare metal kernel (31 KB)
make qemu-run            # Run in QEMU emulator
```

In a BMF program, each instruction is a single CELL.
- A CELL is either a QWord (64-bits), or a DWord (32-bits).
- If <= the last primitive (system), then it is a primitive.
- Else, if it is in the range from `0` to `LIT_MASK`, then it is a literal.
- Else, it is the XT (code address) of a word in the dictionary.

### STATES in BMF
Setting `STATE` to 999 signals BMF to exit.

### BMF hard-codes the following IMMEDIATE words:

| Word | Behavior |
|:--   |:-- |
|  :   | Add the next word to the dictionary, set `STATE` to COMPILE (1). |
|  ;   | Compile EXIT and change `STATE` to INTERPRET (0). |
|  (   | Skips words until the next ')' word. |
|  \\  | Skips words until the end next new-line character ($0A). |

### ColorForth influences

BMF will change the state depending on embedded bytes in the whitespace.<br/>
NOTE: I cannot use '$00' for INTERPRET because that is the line terminator.<br/>

| Byte | Behavior                      |
|:--   |:--                            |
| $01  | Set `STATE` to INTERPRET (0). |
| $02  | Set `STATE` to COMPILE (1).   |

## INLINE words

An INLINE word is somewhat similar to a macro in other languages.<br/>
When a word is INLINE, its definition is copied to the target, up to the first `EXIT`.<br/>
When not INLINE, a call is made to the word instead.<br/>
**NOTE**: if the next instruction is `EXIT`, it becomes a `JUMP` instead (the tail-call optimization).<br/>

## Transient words

Words 't0' through 't9' are transient and are not added to the dictionary.<br/>
They are **case sensitive**; 't0' is a transient word, 'T0' is not.<br/>
They help with factoring code and keep the dictionary uncluttered.<br/>
They can be reused as many times as desired.

## Built-in variables

There are 3 built-in variables: `x`, `y`, and `z`.<br/>
Use `+L` to create new versions of the variables.<br/>
Use `-L` to destroy the most recently created variables.<br/>
`+L` and `-L` can be used at any time for any reason.

## Building BMF (Hosted Mode)

### Linux
There is a makefile for hosted builds (Linux/Windows targets).
- **Default (64-bit):** `make`
- **32-bit:** `BITS=32 make`
- **Run:** `./bmf` or `make run`

### Windows
There is a .SLN file with configurations for 32-bit and 64-bit builds.

### Bare Metal (32-bit QEMU/x86)
For bare metal kernel building, see [BARE_METAL.md](BARE_METAL.md).

## BMF Startup Behavior

On startup, BMF does the following:
- Create 'argc' with the count of command-line arguments.
- For each argument, create 'argX' with the address of th1.e argument string
- For example, `arg0 ztype` will print `bmf`.
- If arg1 exists and names a file that can be opened, load that file.
- Else, try to load file 'bmf-boot.fth' in the current folder.
- Else, try to load file 'bmf-boot.fth' in the `BIN_DIR` folder.
- On Linux, `BIN_DIR` is "/home/chris/bin/".
- On Windows, `BIN_DIR` is "D:\\bin\\".
- `BIN_DIR` is defined in bmf-vm.h. Adjust it in `bmf-vm.h` for your system as desired.

**Note:** In bare metal mode, file I/O is not available. BMF runs from an embedded bootstrap in kernel memory.

## The VM Primitives

| Primitive | Op/Word  | Stack        | Description |
|:--        |:--       |:--           |:-- |
|   0       | exit     | (--)         | PC = R-TOS. Discard R-TOS. If (PC=0) then stop. |
|   1       | lit      | (--)         | Push code[PC]. Increment PC. |
|   2       | jmp      | (--)         | PC = code[PC]. |
|   3       | jmpz     | (n--)        | If (n==0) then PC = code[PC] else PC = PC+1. |
|   4       | jmpnz    | (n--)        | If (n!=0) then PC = code[PC] else PC = PC+1. |
|   5       | njmpz    | (n--n)       | If (n==0) then PC = code[PC] else PC = PC+1. |
|   6       | njmpnz   | (n--n)       | If (n!=0) then PC = code[PC] else PC = PC+1. |
|   7       | dup      | (n--n n)     | Duplicate `n`. |
|   8       | drop     | (n--)        | Discard `n`. |
|   9       | swap     | (a b--b a)   | Swap `a` and `b`. |
|  10       | over     | (a b--a b a) | Push `a`. |
|  11       | !        | (n a--)      | CELL store `n` through `a`. |
|  12       | @        | (a--n)       | CELL fetch `n` through `a`. |
|  13       | c!       | (b a--)      | BYTE store `b` through `a`. |
|  14       | c@       | (a--b)       | BYTE fetch `b` through `a`. |
|  15       | >r       | (n--)        | Move `n` to the return stack. |
|  16       | r@       | (--n)        | Copy `n` from the return stack. |
|  17       | r>       | (--n)        | Move `n` from the return stack. |
|  18       | +L       | (--)         | Create new versions of variables (x,y,z). |
|  19       | -L       | (--)         | Restore the last set of variables. |
|  20       | x!       | (n--)        | Set local variable X to `n`. |
|  21       | y!       | (n--)        | Set local variable Y to `n`. |
|  22       | z!       | (n--)        | Set local variable Z to `n`. |
|  23       | x@       | (--n)        | Push local variable X. |
|  24       | y@       | (--n)        | Push local variable Y. |
|  25       | z@       | (--n)        | Push local variable Z. |
|  26       | x@+      | (--n)        | Push local variable X, then increment it. |
|  27       | y@+      | (--n)        | Push local variable Y, then increment it. |
|  28       | z@+      | (--n)        | Push local variable Z, then increment it. |
|  29       | *        | (a b--c)     | `c` = `a`*`b`. |
|  30       | +        | (a b--c)     | `c` = `a`+`b`. |
|  31       | -        | (a b--c)     | `c` = `a`-`b`. |
|  32       | /mod     | (a b--r q)   | `q` = `a`/`b`. `r` = `a` modulo `b`. |
|  33       | 1+       | (a--b)       | `b` = `a`+1. |
|  34       | 1-       | (a--b)       | `b` = `a`-1. |
|  35       | <        | (a b--f)     | If (`a`<`b`) then `f` = 1 else `f` = 0. |
|  36       | =        | (a b--f)     | If (`a`=`b`) then `f` = 1 else `f` = 0. |
|  37       | >        | (a b--f)     | If (`a`>`b`) then `f` = 1 else `f` = 0. |
|  38       | 0=       | (n--f)       | If (n==0) then `f` = 1 else `f` = 0. |
|  39       | min      | (a b--c)     | If (`a` < `b`) `c` = `a` else `b`. |
|  40       | max      | (a b--c)     | If (`a` > `b`) `c` = `a` else `b`. |
|  41       | +!       | (n a--)      | Add `n` to the cell at `a`. |
|  42       | for      | (C--)        | Start a FOR loop starting at 0. Upper limit is `C`. |
|  43       | i        | (--I)        | Push current loop index `I`. |
|  44       | next     | (--)         | Increment I. If (I < C) then jump to loop start. |
|  45       | and      | (a b--c)     | `c` = `a` and `b`. |
|  46       | or       | (a b--c)     | `c` = `a` or  `b`. |
|  47       | xor      | (a b--c)     | `c` = `a` xor `b`. |
|  48       | ztype    | (a--)        | Output the null-terminated string `a`. |
|  49       | find     | (--a)        | Push the dictionary address `a` of the next word. |
|  50       | key      | (--n)        | Push the next keypress `n`. Wait if necessary. |
|  51       | key?     | (--f)        | Push 1 if a keypress is available, else 0. |
|  52       | emit     | (c--)        | Output char `c`. |
|  53       | fopen    | (nm md--fh)  | Open file `nm` using mode `md` (`fh`=0 if error). |
|  54       | fclose   | (fh--)       | Close file `fh`. |
|  55       | fread    | (a sz fh--n) | Read `sz` chars from file `fh` to `a`. |
|  56       | fwrite   | (a sz fh--n) | Write `sz` chars to file `fh` from `a`. |
|  57       | ms       | (n--)        | Wait/sleep for `n` milliseconds |
|  58       | timer    | (--n)        | Push the current system time `n`. |
|  59       | add-word | (--)         | Add the next word to the dictionary. |
|  60       | outer    | (str--)      | Run the outer interpreter on `str`. |
|  61       | cmove    | (f t n--)    | Copy `n` bytes from `f` to `t`. |
|  62       | s-len    | (str--n)     | Determine the length `n` of string `str`. |
|  63       | system   | (str--)      | Execute system(`str`). |

## Other built-in words

| Word      | Stack | Description |
|:--        |:--    |:-- |
| version   | (--n) | Current version number. |
| WINDOWS   | (--n) | If the system is Windows, 1 Else 0. |
| LINUX     | (--n) | If the system is Linux, 1 Else 0. |
| output-fp | (--a) | Address of the output file handle. 0 means STDOUT. |
| (h)       | (--a) | Address of HERE. |
| (l)       | (--a) | Address of LAST. |
| (lsp)     | (--a) | Address of the loop stack pointer. |
| lstk      | (--a) | Address of the loop stack. |
| (rsp)     | (--a) | Address of the return stack pointer. |
| rstk      | (--a) | Address of the return stack. |
| (tsp)     | (--a) | Address of the x/y/z stack pointer. |
| tstk      | (--a) | Address of the x/y/z stack. |
| (sp)      | (--a) | Address of the data stack pointer. |
| stk       | (--a) | Address of the data stack. |
| state     | (--a) | Address of STATE. |
| base      | (--a) | Address of BASE. |
| mem       | (--a) | Address of the beginning of the memory area. |
| mem-sz    | (--n) | The number of BYTEs in the memory area. |
| >in       | (--a) | Address of the text input buffer pointer. |
| de-sz     | (--n) | The size of a dictionary in bytes (32). |
| cell      | (--n) | The size of a CELL in bytes (4 or 8). |

##   Embedding BMF in your C or C++ project

For **bare metal builds**, see [BARE_METAL.md](BARE_METAL.md).

For **hosted mode** (Linux/Windows), modify `system.c` to provide the I/O primitives. The VM core files `bmf-vm.c/h` are portable and require only standard C library functions.
#include "bmf-vm.h"
// ... implement the functions bmf-vm.c needs
bmfInit();
outer(".\" Hello World!\"");
```
