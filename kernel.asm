; ============================================================================
; 32-bit Bare Metal FORTH OS Kernel
; GRUB Multiboot bootloader and kernel in one file (FASM syntax)
; ============================================================================

format ELF
use32

; ============================================================================
; SECTION: CONSTANTS & CONFIGURATION
; ============================================================================

; Multiboot header constants
MULTIBOOT_HEADER_MAGIC = 0x1BADB002
MULTIBOOT_HEADER_FLAGS = 0x00000003
MULTIBOOT_CHECKSUM = -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

; VGA Text Mode Console
VGA_ADDRESS = 0xB8000
VGA_WIDTH = 80
VGA_HEIGHT = 25

; Serial Port (COM1)
SERIAL_PORT = 0x3F8

; ============================================================================
; MEMORY LAYOUT (32 MB total)
; ============================================================================
; 0x01FFFFFF  Free space
;
;   Dictionary + Code (grow UP from DICT_START, mixed entries)
;   Each entry: [Link(4)] [XT(4)] [Flags|Len(1)] [Name(variable)] [NULL(1)] [Code]
; 0x00700800
;
;   Data stack (1 KB, grows DOWN) 
; 0x007FFC00
;
;   Graphics buffer (4 MB, 1280×1024@32-bit)
; 0x00200000
;
;   Kernel code + data + kernel stack (16 KB, ESP points here)
; 0x00100000
; 0x00000000

; Kernel entry point (GRUB multiboot)
KERNEL_START  = 0x00100000

; Graphics buffer (4 MB for VESA 1280×1024@32-bit)
GRAPHICS_START = 0x00200000
GRAPHICS_SIZE  = 0x00400000  ; 4 MB

; Data stack (1 KB, grows downward, will use memory)
DATA_STK_BASE = 0x007FFC00
DATA_STK_SIZE = 0x00000400  ; 1 KB (256 entries × 4 bytes)

; Dictionary + Code (grows upward from here, mixed entries)
DICT_START    = 0x00700800
DICT_SIZE     = 0x00F00000  ; ~15 MB available for dictionary+code

; Note: ESP (kernel stack) remains in kernel .bss section (16 KB)

; ============================================================================
; SECTION: MACROS
; ============================================================================

; Push a register onto the data stack (grows downward, EBP = data stack pointer)
; Usage: dPush eax    (or any 32-bit register)
macro dPush reg {
    sub ebp, 4
    mov [ebp], reg
}

; Pop a value from the data stack into a register
; Usage: dPop eax     (or any 32-bit register)
macro dPop reg {
    mov reg, [ebp]
    add ebp, 4
}

; Read top of stack into a register (non-destructive)
; Usage: getTOS eax     (or any 32-bit register)
macro getTOS reg {
    mov reg, [ebp]
}

; Read next on stack (2nd element) into a register (non-destructive)
; Usage: getNOS eax     (or any 32-bit register)
macro getNOS reg {
    mov reg, [ebp+4]
}

; Write to top of stack (non-destructive to SP)
; Usage: setTOS eax     (or any 32-bit register)
macro setTOS reg {
    mov [ebp], reg
}

; Write to next on stack (2nd element, non-destructive to SP)
; Usage: setNOS eax     (or any 32-bit register)
macro setNOS reg {
    mov [ebp+4], reg
}

; ============================================================================
; SECTION: MULTIBOOT HEADER
; ============================================================================

section '.multiboot' align 4
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM

; ============================================================================
; SECTION: MEMORY LAYOUT
; ============================================================================

; Kernel stack (16 KB) - grows downward
section '.bss' align 16
stack_bottom:
    rb 16384
stack_top:

; Data section - global variables
section '.data'
cursor_x: dd 0
cursor_y: dd 0
multiboot_magic: dd 0
multiboot_info: dd 0

; Keyboard ring buffer (32 scancodes)
KEYBOARD_BUFFER_SIZE = 32       ; must be a power of 2 for wrapping
keyboard_buffer: rb KEYBOARD_BUFFER_SIZE
keyboard_head: dd 0             ; Write pointer
keyboard_tail: dd 0             ; Read pointer

; FORTH Dictionary pointers
DICT_START = 0x00700800         ; Start of dictionary in memory
HERE: dd DICT_START             ; Next free address (grows UP)
LAST: dd 0                      ; Most recent word (0 initially, will point to last defined)

; Note: EBP = data stack pointer (grows downward)

; IDT (Interrupt Descriptor Table) - 256 entries × 8 bytes
section '.bss'
idt: rb IDT_SIZE * IDT_ENTRY_SIZE

; IDTR descriptor (for LIDT instruction)
section '.data'
idtr:
    dw IDT_SIZE * IDT_ENTRY_SIZE - 1  ; limit (size - 1)
    dd idt                             ; base address

; ============================================================================
; SECTION: BOOT & INITIALIZATION
; ============================================================================

section '.text'
public _start

_start:
    ; Setup the stack pointer
    mov esp, stack_top
    
    ; Save multiboot parameters to memory
    ; EAX = magic number (0x2BADB002)
    ; EBX = multiboot info structure pointer
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx
    
    ; Call main kernel function
    call kernel_main
    
    ; If kernel_main returns, halt
    cli
.hang:
    hlt
    jmp .hang

; ============================================================================
; SECTION: VGA TEXT CONSOLE DRIVER
; ============================================================================

; Clear VGA screen and reset cursor position
; Entry: none
; Exit: VGA cleared, cursor at (0,0)
kernel_clear:
    push ebx
    push ecx
    push edx
    
    xor ecx, ecx            ; counter = 0
    mov edx, VGA_ADDRESS
    
.clear_loop:
    cmp ecx, VGA_WIDTH * VGA_HEIGHT
    jge .clear_done
    
    mov word [edx + ecx * 2], 0x0720  ; space with white on black
    inc ecx
    jmp .clear_loop
    
.clear_done:
    mov dword [cursor_x], 0
    mov dword [cursor_y], 0
    
    pop edx
    pop ecx
    pop ebx
    ret

; Scroll screen up by one line (rows 1-24 move to 0-23, row 24 cleared)
; Entry: none
; Exit: screen scrolled, cursor_y stays at 24
scroll_screen:
    push eax
    push ecx
    push esi
    push edi
    
    ; Copy rows 1-24 up to rows 0-23
    ; Each row: 80 chars × 2 bytes = 160 bytes
    mov esi, VGA_ADDRESS + 160      ; Source: row 1
    mov edi, VGA_ADDRESS            ; Dest: row 0
    mov ecx, 24 * 80                ; Copy 24 rows × 80 chars
    
.scroll_copy:
    mov ax, [esi]                   ; Copy char + attr (2 bytes)
    mov [edi], ax
    add esi, 2
    add edi, 2
    dec ecx
    jnz .scroll_copy
    
    ; Clear row 24 (last row)
    mov edi, VGA_ADDRESS + 24 * 160
    mov ecx, 80
    mov ax, 0x0720                  ; space with white on black
    
.scroll_clear:
    mov [edi], ax
    add edi, 2
    dec ecx
    jnz .scroll_clear
    
    pop edi
    pop esi
    pop ecx
    pop eax
    ret

; Write single character to VGA at cursor position
; Entry: AL = character
; Exit: cursor advanced, wraps to next line
vga_putchar:
    push eax
    push ebx
    push ecx
    push edx
    
    cmp al, 10              ; newline?
    je .putchar_newline
    
    ; Calculate position: cursor_y * 80 + cursor_x
    mov ebx, [cursor_y]
    mov ecx, VGA_WIDTH
    imul ebx, ecx
    add ebx, [cursor_x]
    
    ; Write character and color to VGA
    mov ecx, VGA_ADDRESS
    mov byte [ecx + ebx * 2], al        ; character
    mov byte [ecx + ebx * 2 + 1], 0x07  ; white on black
    
    ; Advance cursor
    inc dword [cursor_x]
    cmp dword [cursor_x], VGA_WIDTH
    jl .putchar_done
    
    mov dword [cursor_x], 0
    inc dword [cursor_y]
    cmp dword [cursor_y], VGA_HEIGHT
    jl .putchar_done
    
    ; Scroll screen up
    call scroll_screen
    mov dword [cursor_y], VGA_HEIGHT - 1  ; Move cursor to last row after scroll
    
.putchar_newline:
    mov dword [cursor_x], 0
    inc dword [cursor_y]
    cmp dword [cursor_y], VGA_HEIGHT
    jl .putchar_done
    
    ; Scroll screen up
    call scroll_screen
    mov dword [cursor_y], VGA_HEIGHT - 1  ; Move cursor to last row after scroll
    
.putchar_done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Write null-terminated string to VGA
; Entry: ESI = pointer to string
; Exit: string displayed on screen
vga_write:
    push eax
    push esi
    
.write_loop:
    lodsb                   ; load byte from [esi] into al, increment esi
    test al, al             ; check for null terminator
    jz .write_done
    
    call vga_putchar
    jmp .write_loop
    
.write_done:
    pop esi
    pop eax
    ret

; ============================================================================
; SECTION: SERIAL PORT DRIVER
; ============================================================================

; Initialize serial port COM1 (0x3F8)
; Sets 115200 baud, 8 bits, 1 stop bit, no parity
init_serial:
    push eax
    push edx
    
    mov dx, SERIAL_PORT + 3     ; Line Control Register (0x3FB)
    mov al, 0x80                ; Enable Divisor Latch Access
    out dx, al
    
    mov dx, SERIAL_PORT         ; Divisor Latch Low (0x3F8)
    mov al, 1                   ; 115200 baud (divisor = 1)
    out dx, al
    
    mov dx, SERIAL_PORT + 1     ; Divisor Latch High (0x3F9)
    mov al, 0
    out dx, al
    
    mov dx, SERIAL_PORT + 3     ; Line Control Register (0x3FB)
    mov al, 0x03                ; 8 bits, 1 stop, no parity, disable latch
    out dx, al
    
    mov dx, SERIAL_PORT + 4     ; Modem Control Register (0x3FC)
    mov al, 0x0B                ; DTR, RTS, OUT2 enabled
    out dx, al
    
    pop edx
    pop eax
    ret

; Write null-terminated string to serial port (COM1)
; Entry: ESI = pointer to string
; Exit: string sent to serial port
ser_write:
    push eax
    push edx
    
    mov dx, SERIAL_PORT
.ser_write_loop:
    mov al, [esi]
    test al, al
    jz .ser_write_done
    out dx, al
    inc esi
    jmp .ser_write_loop

.ser_write_done:
    pop edx
    pop eax
    ret

; Write null-terminated string to VGA and serial port (COM1)
; Entry: ESI = pointer to string
vga_ser_write:
    call vga_write
    call ser_write
    ret

; ============================================================================
; SECTION: INTERRUPT HANDLING - IDT & PIC
; ============================================================================

; IDT Constants
IDT_SIZE = 256              ; Number of interrupt handlers
IDT_ENTRY_SIZE = 8          ; Bytes per IDT entry

; PIC ports
PIC_MASTER_CMD   = 0x20
PIC_MASTER_DATA  = 0x21
PIC_SLAVE_CMD    = 0xA0
PIC_SLAVE_DATA   = 0xA1

; PIC initialization constants
ICW1 = 0x11                 ; ICW1: 8086 mode, will send ICW4
ICW4 = 0x01                 ; ICW4: 8086 mode
PIC_MASTER_OFFSET = 0x20    ; Remap master to interrupts 0x20-0x27 (IRQ0-7)
PIC_SLAVE_OFFSET = 0x28     ; Remap slave to interrupts 0x28-0x2F (IRQ8-15)

; PIC EOI (End Of Interrupt)
PIC_EOI = 0x20

; Keyboard constants
KEYBOARD_PORT = 0x60        ; PS/2 keyboard data port
KEYBOARD_IRQ = 1            ; IRQ1 for keyboard
KEYBOARD_INT = 0x21         ; INT 0x21 (0x20 + IRQ1)

; ============================================================================
; SECTION: IDT & PIC FUNCTIONS
; ============================================================================

; Set an IDT entry
; Entry: EAX = handler offset, BL = index, CL = flags (0x8E = interrupt gate)
; Sets IDT[index] to point to handler
idt_set_entry:
    push eax
    push ebx
    push ecx
    push edx
    push edi
    
    ; Calculate offset into IDT: index * 8
    movzx edx, bl
    shl edx, 3
    lea edi, [idt + edx]
    
    ; Store offset_lo (bits 0-15 of handler address)
    mov word [edi], ax
    
    ; Store code segment selector (0x08 = kernel code segment)
    mov word [edi + 2], 0x08
    
    ; Store flags (0x8E = present, ring 0, interrupt gate 32-bit)
    mov byte [edi + 4], 0
    mov byte [edi + 5], cl          ; CL = flags (0x8E for interrupts)
    
    ; Store offset_hi (bits 16-31 of handler address)
    shr eax, 16
    mov word [edi + 6], ax
    
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Initialize the PIC (Programmable Interrupt Controller)
; Remaps IRQ0-7 to INT 0x20-0x27, IRQ8-15 to INT 0x28-0x2F
init_pic:
    push eax
    push edx
    
    ; Initialize master PIC
    mov al, ICW1
    out PIC_MASTER_CMD, al
    
    mov al, PIC_MASTER_OFFSET
    out PIC_MASTER_DATA, al
    
    mov al, 0x04                ; ICW3: master has slave on IRQ2
    out PIC_MASTER_DATA, al
    
    mov al, ICW4
    out PIC_MASTER_DATA, al
    
    ; Initialize slave PIC
    mov al, ICW1
    out PIC_SLAVE_CMD, al
    
    mov al, PIC_SLAVE_OFFSET
    out PIC_SLAVE_DATA, al
    
    mov al, 0x02                ; ICW3: slave connected to master IRQ2
    out PIC_SLAVE_DATA, al
    
    mov al, ICW4
    out PIC_SLAVE_DATA, al
    
    ; Mask all interrupts except keyboard (IRQ1)
    mov al, 0xFD                ; 11111101 (enable IRQ1 only on master)
    out PIC_MASTER_DATA, al
    
    mov al, 0xFF                ; disable all slave interrupts
    out PIC_SLAVE_DATA, al
    
    pop edx
    pop eax
    ret

; Initialize the IDT with default handlers
init_idt:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    
    ; Clear all IDT entries first
    xor edx, edx
    mov ecx, IDT_SIZE
.init_idt_clear:
    mov eax, edx
    shl eax, 3
    mov dword [idt + eax], 0
    mov dword [idt + eax + 4], 0
    inc edx
    cmp edx, ecx
    jl .init_idt_clear
    
    ; Set keyboard handler (IRQ1 = INT 0x21)
    mov eax, keyboard_handler
    mov bl, KEYBOARD_INT
    mov cl, 0x8E                ; interrupt gate, present, ring 0
    call idt_set_entry
    
    ; Load IDT
    lidt [idtr]
    
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Keyboard interrupt handler (IRQ1)
; Reads scancode, buffers it in ring buffer
keyboard_handler:
    ; Save registers
    push eax
    push ebx
    push edx
    
    ; Check PS/2 controller status port (0x64)
    ; Bit 0 = output buffer full (data from keyboard)
    in al, 0x64
    test al, 1
    jz .kb_no_data              ; If no data, skip reading
    
    ; Data available, read from PS/2 data port
    in al, 0x60
    
    ; Get write pointer (head)
    mov ebx, [keyboard_head]
    
    ; Store scancode in buffer
    mov byte [keyboard_buffer + ebx], al
    
    ; Advance head pointer
    inc ebx
    and ebx, KEYBOARD_BUFFER_SIZE-1
    mov [keyboard_head], ebx
    
.kb_no_data:
    ; Send EOI (End Of Interrupt) to master PIC
    mov al, PIC_EOI
    out PIC_MASTER_CMD, al
    
    ; Restore registers and return
    pop edx
    pop ebx
    pop eax
    iret

; Read scancode from keyboard buffer (non-blocking)
; Exit: AL = scancode (0 if buffer empty)
keyboard_read:
    push ebx
    
    mov ebx, [keyboard_tail]
    cmp ebx, [keyboard_head]
    je .kb_empty                ; If tail == head, buffer is empty
    
    ; Read byte from buffer
    mov al, [keyboard_buffer + ebx]
    
    ; Advance tail pointer
    inc ebx
    and ebx, KEYBOARD_BUFFER_SIZE-1
    mov [keyboard_tail], ebx
    jmp .kb_read_ret
    
.kb_empty:
    xor al, al                  ; Return 0 if empty
    
.kb_read_ret:
    pop ebx
    ret

; Check if keyboard buffer has data (non-blocking)
; Exit: AL = 1 if data available, 0 if empty
keyboard_has_data:
    push ebx
    
    mov al, 0                    ; Default to 0 (no data)
    mov ebx, [keyboard_tail]
    cmp ebx, [keyboard_head]
    je .kbd_exit                 ; If tail == head, buffer is empty
    inc al                       ; Return 1 if data available
    
.kbd_exit:
    pop ebx
    ret

; Initialize PS/2 keyboard hardware
init_ps2:
    push eax
    push edx
    
    ; Disable keyboard
    mov al, 0xAD
    out 0x64, al
    
    ; Flush output buffer
    in al, 0x60
    
    ; Re-enable keyboard
    mov al, 0xAE
    out 0x64, al
    
    pop edx
    pop eax
    ret

; Enable IRQ1 (keyboard)
; Reads current mask, clears bit 1, writes back
pic_enable_irq_1:
    push eax
    push edx
    
    ; Read current mask from master PIC
    mov dx, PIC_MASTER_DATA
    in al, dx
    
    ; Clear bit 1 (IRQ1) to enable keyboard
    and al, 0xFD            ; 11111101 in binary (clears bit 1)
    
    ; Write back
    out dx, al
    
    pop edx
    pop eax
    ret

; ============================================================================
; SECTION: UTILITY FUNCTIONS
; ============================================================================
; Entry: EAX = number to convert, ESI = buffer (11+ bytes), ECX, width
; Exit: ESI points to hex string "0xXXXXXXXX\0"
hex_to_string:
    push eax
    push ebx
    push ecx
    push esi
    
    mov byte [esi], '0'
    mov byte [esi + 1], 'x'
    add esi, 2

    cmp ecx, 8
    jle .hex_loop
    mov ecx, 8              ; Limit to 8 hex digits for 32-bit number
    
.hex_loop:
    mov ebx, eax
    shr ebx, 28             ; get top 4 bits
    and ebx, 0x0F
    
    cmp bl, 9
    jle .hex_digit
    add bl, 7               ; A-F
.hex_digit:
    add bl, '0'
    mov [esi], bl
    inc esi
    
    shl eax, 4              ; shift left by 4 bits
    dec ecx
    jnz .hex_loop
    
    mov byte [esi], 0       ; null terminate
    
    pop esi
    pop ecx
    pop ebx
    pop eax
    ret

; ============================================================================
; SECTION: FORTH DICTIONARY & LOOKUP
; ============================================================================

; Dictionary lookup - find a word by name
; Entry: ESI = pointer to name string (null-terminated), CL = string length
; Exit: EBX = address of dictionary entry (or 0 if not found)
dict_lookup:
    push eax
    push ecx
    push edx
    
    mov ebx, [LAST]         ; Start at most recently defined word
    
.search_loop:
    cmp ebx, 0              ; End of dictionary?
    je .lookup_not_found
    
    ; Check length first: compare CL with bottom 5 bits of flags/len byte at [ebx + 8]
    mov al, [ebx + 8]       ; Flags|Len byte
    and al, 0x1F            ; Extract length (bottom 5 bits)
    cmp al, cl              ; Compare lengths
    jne .name_no_match      ; Length mismatch, try next entry
    
    ; Length matches, now compare name: [ebx + 9] is the name field
    mov eax, ebx
    add eax, 9              ; Point to name in entry
    mov edx, esi            ; EDX is our search string pointer (preserve ESI on stack)
    
.name_compare:
    mov al, [eax]           ; Byte from dictionary entry name
    mov cl, [edx]           ; Byte from search string
    
    ; Convert both to uppercase for case-insensitive comparison
    cmp al, 'a'
    jl .al_ok
    cmp al, 'z'
    jg .al_ok
    sub al, 32              ; Convert to uppercase
.al_ok:
    cmp cl, 'a'
    jl .cl_ok
    cmp cl, 'z'
    jg .cl_ok
    sub cl, 32              ; Convert to uppercase
.cl_ok:
    
    cmp al, cl
    jne .name_no_match      ; Bytes don't match
    
    test al, al             ; Check for null terminator (both should match)
    je .lookup_found
    
    inc eax
    inc edx
    jmp .name_compare
    
.name_no_match:
    mov ebx, [ebx]          ; Follow link to previous entry
    jmp .search_loop
    
.lookup_found:
    pop edx
    pop ecx
    pop eax
    ret
    
.lookup_not_found:
    xor ebx, ebx            ; Return 0 (not found)
    pop edx
    pop ecx
    pop eax
    ret

; ============================================================================
; DICTIONARY ENTRIES - Core Primitives
; ============================================================================

; DUP primitive - Duplicate top of stack
; Entry format: [Link(4)][XT(4)][Flags|Len(1)][Name(var)][NULL][Code]
section '.data'
dict_dup:
    dd 0                    ; Link: 0 (no previous entry)
    dd dict_dup_code        ; XT: execution token (code address)
    db 0x03                 ; Flags|Len: immediate=0, length=3
    db "DUP", 0             ; Name: "DUP" with NULL
dict_dup_code:
    getTOS eax              ; Read top of stack
    dPush eax               ; Push duplicate
    ret

; DROP primitive - Remove top of stack
; Entry format: [Link(4)][XT(4)][Flags|Len(1)][Name(var)][NULL][Code]
dict_drop:
    dd dict_dup             ; Link: points to DUP (previous entry)
    dd dict_drop_code       ; XT: execution token (code address)
    db 0x04                 ; Flags|Len: immediate=0, length=4
    db "DROP", 0            ; Name: "DROP" with NULL
dict_drop_code:
    dPop eax                ; Pop and discard top of stack
    ret

; KEY? primitive - Check if keyboard buffer has data
; Returns 1 (true) or 0 (false) on data stack
dict_key_question:
    dd dict_drop            ; Link: points to DROP (previous entry)
    dd dict_key_question_code ; XT: execution token (code address)
    db 0x04                 ; Flags|Len: immediate=0, length=4
    db "KEY?", 0            ; Name: "KEY?" with NULL
dict_key_question_code:
    call keyboard_has_data  ; AL = 1 if data, 0 if empty
    movzx eax, al           ; Zero-extend to 32-bit
    dPush eax               ; Push result (1 or 0) onto stack
    ret

; SWAP primitive - Exchange top two stack elements
; a b SWAP → b a
dict_swap:
    dd dict_key_question    ; Link: points to KEY? (previous entry)
    dd dict_swap_code       ; XT: execution token (code address)
    db 0x04                 ; Flags|Len: immediate=0, length=4
    db "SWAP", 0            ; Name: "SWAP" with NULL
dict_swap_code:
    getTOS eax              ; eax = a (TOS)
    getNOS ebx              ; ebx = b (NOS)
    setNOS eax              ; write a to 2nd
    setTOS ebx              ; write b to TOS
    ret

; Bootstrap dictionary with core primitives
; Populates dictionary with basic FORTH words
bootstrap_dictionary:
    ; Initialize LAST to point to SWAP (most recently defined word)
    mov dword [LAST], dict_swap
    ; TODO: Add more primitives (+, -, *, /, etc.)
    ret

; ============================================================================
; SECTION: MAIN KERNEL FUNCTION
; ============================================================================

kernel_main:
    ; Initialize interrupt system
    call init_idt
    call init_pic
    call init_ps2            ; Initialize PS/2 keyboard hardware
    call pic_enable_irq_1    ; Enable keyboard interrupt (IRQ1)
    sti                      ; Enable interrupts
    
    ; Initialize serial port (before any serial output)
    call init_serial
    
    ; Initialize data stack pointer (EBP)
    mov ebp, DATA_STK_BASE
    
    ; Initialize FORTH dictionary
    call bootstrap_dictionary
    
    ; Clear VGA screen
    call kernel_clear
    
    ; Print startup message
    mov esi, msg_started
    call vga_ser_write
    
    ; Print magic number
    mov esi, msg_magic
    call vga_ser_write
    
    ; Load and display the multiboot magic number
    mov eax, [multiboot_magic]
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    
    ; Print completion message
    mov esi, msg_complete
    call vga_ser_write
    
    ; Halt the CPU
.kernel_halt:
    cli
    hlt
    jmp .kernel_halt

; ============================================================================
; SECTION: DATA & STRINGS
; ============================================================================

section '.data'
msg_started: db "Kernel started!", 10, 0
msg_magic: db "Magic: ", 0
msg_complete: db 10, "Boot complete! Halting...", 10, 0
hex_buffer: db "0x00000000", 0
