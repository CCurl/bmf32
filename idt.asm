; Interrupt handling assembly code for 32-bit protected mode
; Defines ISR stubs and common interrupt handling dispatcher

format ELF
use32

; Declare external symbols
extrn handle_interrupt
extrn timer_increment_ticks
extrn ps2_interrupt_handler

section '.data' align 16
    ; GDT entry for code segment (selector 0x08)
    gdt_kernel_code = 0x08
    
section '.text'

; Interrupt dispatcher - called from ISR stubs
; Common handler for exceptions (doesn't call PIC_EOI)
public common_interrupt_handler
common_interrupt_handler:
    push eax            ; Push exception number as argument
    call handle_interrupt
    add esp, 4
    popad
    iret

; Common handler for hardware IRQs (calls PIC_EOI)
public common_irq_handler
common_irq_handler:
    pushad              ; Save all registers
    push dword [esp+32] ; Push IRQ number (in EAX from caller)
    call handle_interrupt
    add esp, 4
    popad
    iret

; Load IDT
; void load_idt(IDT_Ptr *ptr)
; Parameter: [esp+4] = pointer to IDT_Ptr structure
public load_idt
load_idt:
    mov eax, [esp+4]
    lidt [eax]
    ret

; Timer interrupt handler (IRQ0, maps to INT32)
public isr_32
isr_32:
    cli
    pushad              ; Save all registers
    
    ; Call timer tick increment
    call timer_increment_ticks
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popad               ; Restore all registers
    sti
    iret

; Keyboard interrupt handler (IRQ1, maps to INT33)
public isr_33
isr_33:
    cli
    pushad              ; Save all registers
    
    ; Call PS/2 handler
    call ps2_interrupt_handler
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popad               ; Restore all registers
    sti
    iret

; Serial interrupt handler (IRQ4, maps to INT36)
public isr_36
isr_36:
    cli
    pushad              ; Save all registers
    
    ; Read from serial port (COM1 data at 0x3F8)
    mov dx, 0x3F8
    in al, dx
    
    ; For now, just acknowledge and ignore
    ; In full implementation, would buffer the character
    
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    
    popad               ; Restore all registers
    sti
    iret

; Generic exception handlers (for debugging)
public isr_0
isr_0:
    pushad
    mov eax, 0
    jmp common_interrupt_handler

public isr_1
isr_1:
    pushad
    mov eax, 1
    jmp common_interrupt_handler

public isr_2
isr_2:
    pushad
    mov eax, 2
    jmp common_interrupt_handler

public isr_3
isr_3:
    pushad
    mov eax, 3
    jmp common_interrupt_handler

public isr_4
isr_4:
    pushad
    mov eax, 4
    jmp common_interrupt_handler

public isr_5
isr_5:
    pushad
    mov eax, 5
    jmp common_interrupt_handler

public isr_6
isr_6:
    pushad
    mov eax, 6
    jmp common_interrupt_handler

public isr_7
isr_7:
    pushad
    mov eax, 7
    jmp common_interrupt_handler

public isr_8
isr_8:
    pushad
    mov eax, 8
    jmp common_interrupt_handler

public isr_10
isr_10:
    pushad
    mov eax, 10
    jmp common_interrupt_handler

public isr_11
isr_11:
    pushad
    mov eax, 11
    jmp common_interrupt_handler

public isr_12
isr_12:
    pushad
    mov eax, 12
    jmp common_interrupt_handler

public isr_13
isr_13:
    pushad
    mov eax, 13
    jmp common_interrupt_handler

public isr_14
isr_14:
    pushad
    mov eax, 14
    jmp common_interrupt_handler
