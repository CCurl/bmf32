; Multiboot-compliant 32-bit bootloader for FWC bare metal OS
; Assembled with FASM: fasm boot.asm boot.o
; This is linked at the beginning of the kernel by linker.ld

format ELF
use32

; Multiboot header must be within first 8KB and 4-byte aligned
; See: https://www.gnu.org/software/grub/manual/multi

MBOOT_MAGIC     = 0x1BADB002          ; Magic number for multiboot
MBOOT_FLAGS     = 0x00000003          ; Flags: bit0=align modules, bit1=pass memory info
MBOOT_CHECKSUM  = -(MBOOT_MAGIC + MBOOT_FLAGS)

section '.boot' align 4
mboot:
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM
    dd 0            ; header_addr (0 = bootloader uses ELF header)
    dd 0            ; load_addr (0 = use ELF header)
    dd 0            ; load_end_addr (0 = load entire file)
    dd 0            ; bss_end_addr (0 = no BSS)
    dd start        ; Entry point

; Kernel stack: 16 KB allocated in BSS, grows downward

section '.bss' align 16
stack_bottom: rb 0x4000   ; 16 KB stack space
stack_top:

section '.text' align 4

public start

start:
    ; Multiboot bootloader passes:
    ; EAX = magic number (0x2BADB002)
    ; EBX = address of Multiboot information structure
    
    ; Set up the stack
    mov esp, stack_top
    
    ; Clear the direction flag
    cld
    
    ; Push multiboot info for kmain
    push ebx        ; Multiboot info pointer
    push eax        ; Magic number
    
    ; Zero out BSS (if needed - bootloader may have done this)
    ; xor eax, eax
    ; mov ecx, (__bss_end - __bss_start) / 4
    ; mov edi, __bss_start
    ; rep stosd
    
    ; Call C kernel entry point
    extrn kmain
    call kmain
    
    ; If kmain returns, halt the CPU
    cli
    hlt
    jmp $           ; Infinite loop
