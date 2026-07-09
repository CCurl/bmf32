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
;   Dictionary (grows UP from DICT_START, code + data)
;   Each entry: [Link(4)] [XT(4)] [Flags|Len(1)] [Name(variable)] [NULL(1)] [Code]
; Starts at 0x00600500
;
;   Data stack (1 KB, grows DOWN) 
; Starts at 0x00600400
;
;   Graphics buffer (4 MB, 1280×1024@32-bit)
; Ends at 0x00600000
; Starts at 0x00200000
;
;   Kernel code + data + kernel stack (16 KB, ESP points here)
; Starts at 0x00100000

; Kernel entry point (GRUB multiboot)
KERNEL_START  = 0x00100000

; Graphics buffer (4 MB for VESA 1280×1024@32-bit)
GRAPHICS_START = 0x00200000
GRAPHICS_END = 0x00600000
GRAPHICS_SIZE  = 0x00400000  ; 4 MB

; Data stack (grows downward from here)
DATA_STK_BASE = 0x00600400

; Dictionary + Code (grows upward from here, code and data)
DICT_START    = 0x00600500

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

; Timer interrupt counter
timer_ticks: dd 0               ; Incremented on each timer interrupt (IRQ0)

; FORTH variables
HERE: dd DICT_START             ; Next free address (grows UP)
LAST: dd 0                      ; Most recent word (0 initially, will point to last defined)
BASE: dd 10                     ; Number base (default 10)
STATE: dd 0                     ; FORTH state (0 = interpreting, 1 = compiling)
TO_IN: dd 0                     ; >IN - address of current input stream
TIB: db 256 dup(0)              ; TIB - the Text Input Buffer
PARSED_WORD: db 32

; Note: EBP = data stack pointer (grows downward)

; IDT (Interrupt Descriptor Table) - 256 entries × 8 bytes
section '.bss'
idt: rb IDT_SIZE * IDT_ENTRY_SIZE

; IDTR descriptor (for LIDT instruction)
section '.data'
idtr:
    dw IDT_SIZE * IDT_ENTRY_SIZE - 1  ; limit (size - 1)
    dd idt                            ; base address

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
vga_emit:
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
    
    call vga_emit
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

; Write char in AL to serial port (COM1)
; Entry: AL = character to send
; Exit: character sent to serial port
ser_emit:
    push edx
    mov dx, SERIAL_PORT
    out dx, al
    pop edx
    ret

; Write null-terminated string to serial port (COM1)
; Entry: ESI = pointer to string
; Exit: string sent to serial port
ser_write:
    push eax
    push edx
    push esi
    
    mov dx, SERIAL_PORT
.ser_write_loop:
    mov al, [esi]
    test al, al
    jz .ser_write_done
    out dx, al
    inc esi
    jmp .ser_write_loop

.ser_write_done:
    pop esi
    pop edx
    pop eax
    ret

; Write null-terminated string to VGA and serial port (COM1)
; Entry: ESI = pointer to string
vga_ser_write:
    call vga_write
    call ser_write
    ret

; EMIT a char to both VGA and serial port (COM1)
; Entry: AL = character to send
vga_ser_emit:
    call vga_emit
    call ser_emit
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
    
    ; Mask all interrupts except timer (IRQ0) and keyboard (IRQ1)
    mov al, 0xFC                ; 11111100 (enable IRQ0 and IRQ1 on master)
    out PIC_MASTER_DATA, al
    
    mov al, 0xFF                ; disable all slave interrupts
    out PIC_SLAVE_DATA, al
    
    pop edx
    pop eax
    ret

; Initialize the IDT (Interrupt Descriptor Table) with default handlers
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
    
    ; Set timer handler (IRQ0 = INT 0x20)
    mov eax, timer_handler
    mov bl, 0x20                ; INT 0x20
    mov cl, 0x8E                ; interrupt gate, present, ring 0
    call idt_set_entry
    
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

; Timer interrupt handler (IRQ0)
; Increments timer_ticks counter
timer_handler:
    ; Save EAX
    push eax
    
    ; Increment timer counter
    inc dword [timer_ticks]
    
    ; Send EOI (End Of Interrupt) to master PIC
    mov al, PIC_EOI
    out PIC_MASTER_CMD, al
    
    ; Restore EAX and return
    pop eax
    iret

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

; Enable IRQ (parameterized)
; Entry: AL = IRQ bit mask (0x01 for IRQ0, 0x02 for IRQ1, etc.)
; Example: mov al, 0x01; call pic_enable_irq  (enables IRQ0)
;          mov al, 0x02; call pic_enable_irq  (enables IRQ1)
pic_enable_irq:
    push edx
    
    ; Save the bit mask to DL first
    mov dl, al              ; DL = bit mask
    
    ; Read current mask from master PIC
    mov dx, PIC_MASTER_DATA
    in al, dx               ; AL = current mask
    
    ; Invert the bit mask and AND with current mask to clear that bit
    not dl                  ; DL = ~bit_mask (inverted)
    and al, dl              ; AL = current_mask & ~bit_mask
    
    ; Write back
    out dx, al
    
    pop edx
    ret

; Get current timer ticks (non-blocking)
; Exit: EAX = number of timer ticks since boot
timer_get_ticks:
    mov eax, [timer_ticks]
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

; numq - Check if string is a number and convert to integer
; Entry: ESI = pointer to string
; Exit: EAX = 1 if valid number, 0 if not
;       Pushes parsed integer to data stack if valid
numq:
    push ebx
    push ecx
    push edx
    push edi
    
    ; Check for char literal: 'c' format
    mov al, [esi]
    cmp al, 39              ; 39 = ' (single quote)
    jne .numq_check_prefix
    
    mov al, [esi + 2]
    cmp al, 39              ; Check closing quote
    jne .numq_check_prefix
    
    mov al, [esi + 3]
    cmp al, 0               ; Check null terminator
    jne .numq_check_prefix
    
    ; Valid char literal: push the character value
    mov al, [esi + 1]
    movzx eax, al           ; Zero-extend AL to EAX
    dPush eax
    mov eax, 1              ; Return success
    jmp .numq_return
    
.numq_check_prefix:
    cmp al, 0
    je .numq_fail           ; Empty string, fail

    ; Load base (default from BASE variable)
    mov ebx, [BASE]         ; EBX = base
    xor edx, edx            ; EDX = 0 (1 => isNeg)
    
    ; Check for % prefix (binary)
    cmp al, '%'
    jne .numq_check_hash
    mov ebx, 2
    inc esi
    jmp .numq_parse_start   ; No negative sign for binary
    
.numq_check_hash:
    ; Check for # prefix (decimal)
    cmp al, '#'
    jne .numq_check_dollar
    mov ebx, 10
    inc esi
    jmp .numq_check_minus   ; Decimal allows negative sign
    
.numq_check_dollar:
    ; Check for $ prefix (hex)
    cmp al, '$'
    jne .numq_check_minus
    mov ebx, 16
    inc esi
    jmp .numq_parse_start   ; No negative sign check for hex
    
.numq_check_minus:
    ; Check for minus sign (only if base == 10)
    cmp ebx, 10
    jne .numq_parse_start
    
    mov al, [esi]
    cmp al, '-'
    jne .numq_parse_start
    mov edx, 1              ; Set negative flag
    inc esi
    
.numq_parse_start:
    ; Check if string is empty after prefix/minus
    mov al, [esi]
    cmp al, 0
    je .numq_fail
    
    ; Initialize accumulator
    xor ecx, ecx            ; ECX = accumulated number
    
.numq_loop:
    mov al, [esi]
    cmp al, 0
    je .numq_done
    
    ; Convert uppercase to lowercase
    cmp al, 'A'
    jl .numq_char_ok
    cmp al, 'Z'
    jg .numq_char_ok
    add al, 32              ; Convert to lowercase
    
.numq_char_ok:
    ; Convert char to digit value (0-9 or 10-15 for a-f)
    xor edi, edi            ; EDI = digit value
    
    ; Try 0-9
    cmp al, '0'
    jl .numq_fail
    cmp al, '9'
    jg .numq_try_letters
    
    sub al, '0'
    movzx edi, al
    jmp .numq_check_range
    
.numq_try_letters:
    ; Try a-f
    cmp al, 'a'
    jl .numq_fail
    cmp al, 'f'
    jg .numq_fail
    
    sub al, 'a'
    add al, 10
    movzx edi, al
    
.numq_check_range:
    ; Check if digit < base
    cmp edi, ebx
    jge .numq_fail
    
    ; Accumulate: n = n*base + digit
    imul ecx, ebx           ; ECX = ECX * base
    add ecx, edi            ; ECX = ECX + digit
    
    inc esi
    jmp .numq_loop
    
.numq_done:
    ; Apply sign if needed
    test edx, edx
    jz .numq_push_result
    neg ecx
    
.numq_push_result:
    dPush ecx
    mov eax, 1
    jmp .numq_return
    
.numq_fail:
    xor eax, eax
    
.numq_return:
    pop edi
    pop edx
    pop ecx
    pop ebx
    ret

; ============================================================================
; SECTION: FORTH DICTIONARY & LOOKUP
; ============================================================================

; Dictionary lookup - find a word by name
; Entry: ESI = pointer to name string
;        CL = string length
; Exit: EAX = address of dictionary entry (or 0 if not found)
; NOTE: Overwrites EAX, EBX, EDX, EDI
dict_lookup:
    push ecx                ; Save length on stack
    mov eax, [LAST]         ; Start at most recently defined word
    
.search_loop:
    cmp eax, 0              ; End of dictionary?
    je .lookup_not_found
    
    ; Check length first: compare CL with length byte at [eax + 9]
    mov cl, [esp]           ; Restore length into CL
    mov bl, [eax + 9]       ; Length byte
    cmp bl, cl              ; Compare lengths
    jne .name_no_match      ; Length mismatch, try next entry
    
    ; Length matches, now compare name: [eax + 10] is the name field
    mov ebx, eax
    add ebx, 10             ; Point to name in entry
    mov edi, esi            ; EDI is our search string pointer
    
.name_compare:
    mov dl, [ebx]           ; DL = dict char
    mov cl, [edi]           ; CL = search char
    
    ; Convert dict char (DL)
    cmp dl, 'a'
    jl .skip_dict_upper
    cmp dl, 'z'
    jg .skip_dict_upper
    sub dl, 32              ; Convert to uppercase
.skip_dict_upper:
    
    ; Convert search char (CL)
    cmp cl, 'a'
    jl .skip_search_upper
    cmp cl, 'z'
    jg .skip_search_upper
    sub cl, 32              ; Convert to uppercase
.skip_search_upper:
    
    cmp dl, cl
    jne .name_no_match      ; Bytes don't match
    
    test dl, dl             ; Check for null terminator
    je .lookup_found
    
    inc edi
    inc ebx
    jmp .name_compare
    
.name_no_match:
    mov eax, [eax]          ; Follow link to previous entry
    jmp .search_loop
    
.lookup_found:              ; EAX already contains 0 (not found)
.lookup_not_found:          ; EAX already contains the entry address
    pop ecx                 ; Clean up stack
    ret
    
; ============================================================================
; DICTIONARY ENTRIES - Primitives
; ============================================================================
section '.data'
; Entry format: [Link:0-3][XT:4-7][Flags:8][Len:9][Name:10][NULL][Code]

; CELL primitive - Push cell size onto stack
; (-- cell-size)
dict_cell:
    dd 0, XT_cell           ; Link, XT
    db 0, 0x04, "CELL", 0   ; Flags, Len, Name
XT_cell:
    mov eax, 4              ; Cell size is 4 bytes
    dPush eax
    ret

; DUP primitive - Duplicate top of stack
; (a -- a a)
dict_dup:
    dd dict_cell, XT_dup    ; Link, XT
    db 0, 0x03, "DUP", 0    ; Flags, Len, Name
XT_dup:
    getTOS eax
    dPush eax
    ret

; DROP primitive - Remove top of stack
; (a b -- a)
dict_drop:
    dd dict_dup, XT_drop    ; Link, XT
    db 0, 0x04, "DROP", 0   ; Flags, Len, Name
XT_drop:
    dPop eax
    ret

; KEY? primitive - Check if keyboard buffer has data
; Returns 1 (true) or 0 (false) on data stack
; (-- flag)
dict_keyq:
    dd dict_drop, XT_keyq   ; Link, XT
    db 0, 0x04, "KEY?", 0   ; Flags, Len, Name
XT_keyq:
    call keyboard_has_data  ; Sets AL = 1 if data available, else 0
    movzx eax, al           ; Zero-extend AL to EAX
    dPush eax
    ret

; SWAP primitive - Exchange top two stack elements
; (a b -- b a)
dict_swap:
    dd dict_keyq, XT_swap   ; Link, XT
    db 0, 0x04, "SWAP", 0   ; Flags, Len, Name
XT_swap:
    getTOS eax
    getNOS ebx
    setTOS ebx
    setNOS eax
    ret

; OVER primitive - push NOS
; (a b -- a b a)
dict_over:
    dd dict_swap, XT_over   ; Link, XT
    db 0, 0x04, "OVER", 0   ; Flags, Len, Name
XT_over:
    getNOS eax
    dPush eax
    ret

; TIMER primitive - Get the current TIMER value
; (-- ticks)
dict_timer:
    dd dict_over, XT_timer  ; Link, XT
    db 0, 0x05, "TIMER", 0  ; Flags, Len, Name
XT_timer:
    call timer_get_ticks
    dPush eax
    ret

; ADD primitive - Add top two stack elements
; (a b -- sum)
dict_add:
    dd dict_timer, XT_add   ; Link, XT
    db 0, 0x01, "+", 0      ; Flags, Len, Name
XT_add:
    dPop ebx
    getTOS eax
    add eax, ebx
    setTOS eax
    ret

; SUB primitive - Subtract top two stack elements
; (a b -- diff)
dict_sub:
    dd dict_add, XT_sub     ; Link, XT
    db 0, 0x01, "-", 0      ; Flags, Len, Name
XT_sub:
    dPop ebx
    getTOS eax
    sub eax, ebx
    setTOS eax
    ret

; MULT primitive - Multiply top two stack elements
; (a b -- product)
dict_mult:
    dd dict_sub, XT_mult    ; Link, XT
    db 0, 0x01, "*", 0      ; Flags, Len, Name
XT_mult:
    dPop ebx
    getTOS eax
    imul eax, ebx
    setTOS eax
    ret

; DIV primitive - Divide top two stack elements
; (a b -- quotient)
dict_div:
    dd dict_mult, XT_div    ; Link, XT
    db 0, 0x01, "/", 0      ; Flags, Len, Name
XT_div:
    dPop ebx
    getTOS eax
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv ebx
    setTOS eax
    ret

; NUMBER? primitive - Check if string is a number
; (str -- (num 1) | 0 )
dict_numq:
    dd dict_div, XT_numq    ; Link, XT
    db 0, 7, "NUMBER?", 0   ; Flags, Len, Name
XT_numq:
    dPop esi
    call numq               ; Check if string in ESI is a number
    dPush eax               ; numq pushes the parsed number if valid
    ret

; WORD primitive - parse the next word from >IN
; (--a len)
dict_word:
    dd dict_numq, XT_word
    db 0, 4, "WORD", 0      ; Parse the next word from >IN
XT_word:
    mov ecx, 0              ; Length
    mov esi, [TO_IN]
    lea edi, [PARSED_WORD]
    dPush edi
.skip_ws:
    mov al, [esi]
    cmp al, 0
    je .done
    cmp al, 32
    jg .collect_wd
    inc esi
    jmp .skip_ws
.collect_wd:
    mov [edi+ecx], al
    inc esi
    inc ecx
    jmp .collect_wd
.done:
    dPush ecx               ; TODO
    ret


; EMIT primitive - Output character on TOS
; (ch -- )
dict_emit:
    dd dict_word, XT_emit   ; Link, XT
    db 0, 0x04, "EMIT", 0   ; Flags, Len, Name
XT_emit:
    dPop eax
    call vga_ser_emit
    ret

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dict_comma:
    dd dict_emit, XT_comma  ; Link, XT
    db 0, 0x01, ",", 0      ; Flags, Len, Name
XT_comma:
    dPop eax
    mov edx, [HERE]         ; Get current HERE address
    mov [edx], eax          ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    ret

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dict_ccomma:
    dd dict_comma, XT_ccomma ; Link, XT
    db 0, 0x02, "C,", 0      ; Flags, Len, Name
XT_ccomma:
    dPop eax
    mov edx, [HERE]          ; Get current HERE address
    mov [edx], al            ; Store TOS byte at HERE
    add dword [HERE], 1      ; Increment HERE by 1 (byte size)
    ret

; WORDS primitive - output the words in the dictionary
; ( -- )
dict_words:
    dd dict_ccomma, XT_words ; Link, XT
    db 0, 0x05, "WORDS", 0  ; Flags, Len, Name
XT_words:
    mov eax, [LAST]         ; Start at most recently defined word
.words_loop:
    cmp eax, 0              ; End of dictionary?
    je .words_done
    push eax                ; Print the word name
    mov esi, eax            ; Set ESI: word name address
    add esi, 10
    call vga_ser_write
    mov al, 32              ; Space
    call vga_ser_emit
    pop eax
    mov eax, [eax]          ; Next word in dictionary
    jmp .words_loop
.words_done:
    ret

; TODO: Add more primitives

; ============================================================================
; SECTION: MAIN KERNEL FUNCTION
; ============================================================================

kernel_main:
    call init_idt            ; Initialize interrupt system
    call init_pic            ; Initialize PIC
    call init_ps2            ; Initialize PS/2 keyboard hardware
    
    mov al, 0x01             ; Enable IRQ0 (timer) - bit 0
    call pic_enable_irq
    
    mov al, 0x02             ; Enable IRQ1 (keyboard) - bit 1
    call pic_enable_irq
    
    sti                      ; Enable interrupts
    
    ; Initialize other stuff
    mov ebp, DATA_STK_BASE   ; Data stack pointer (EBP)
    call init_serial         ; Serial port
    
    ; Set LAST to the last defined word
    mov dword [LAST], dict_words
    mov dword [BASE], 10
    mov dword [STATE], 0

    call kernel_clear        ; Clear VGA screen
    mov esi, msg_started     ; Print startup message
    call vga_ser_write
    
    mov esi, msg_magic       ; Print magic number
    call vga_ser_write

    ; Load and display the multiboot magic number
    mov eax, [multiboot_magic]
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write

    mov al, 10
    call vga_ser_emit
    call vga_ser_emit
    call XT_words            ; Print dictionary words
    mov al, 10
    call vga_ser_emit

    ; Test numq function
    mov esi, msg_test
    call vga_ser_write
    
    ; Test 1: 'A' (char literal)
    mov esi, test_str_1
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_1_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_2
.test_1_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_2:
    ; Test 2: $FF (hex)
    mov esi, test_str_2
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_2_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_3
.test_2_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_3:
    ; Test 3: %1010 (binary)
    mov esi, test_str_3
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_3_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_4
.test_3_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_4:
    ; Test 4: #42 (decimal)
    mov esi, test_str_4
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_4_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_5
.test_4_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_5:
    ; Test 5: -99 (negative)
    mov esi, test_str_5
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_5_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_6
.test_5_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_6:
    ; Test 6: 0 (zero)
    mov esi, test_str_6
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_6_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_7
.test_6_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_7:
    ; Test 7: #XYZ (invalid)
    mov esi, test_str_7
    call vga_ser_write
    call numq
    test eax, eax
    jnz .test_7_fail
    mov esi, msg_test_fail
    call vga_ser_write
    jmp .tests_done
.test_7_fail:
    mov esi, msg_test_pass
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    
.tests_done:
    ; Print completion message
    mov esi, msg_complete
    call vga_ser_write
    
    ; Test dict_lookup function
    mov esi, msg_dict_test
    call vga_ser_write
    
    ; Test 1: Look up "DUP"
    mov esi, lookup_str_1
    call vga_ser_write
    mov esi, lookup_str_1
    mov cl, 3               ; "DUP" length
    call dict_lookup
    test eax, eax
    jz .dict_test_1_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_2
.dict_test_1_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_2:
    ; Test 2: Look up "dup" (case-insensitive)
    mov esi, lookup_str_2
    call vga_ser_write
    mov esi, lookup_str_2
    mov cl, 3               ; "dup" length
    call dict_lookup
    test eax, eax
    jz .dict_test_2_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_3
.dict_test_2_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_3:
    ; Test 3: Look up "SWAP"
    mov esi, lookup_str_3
    call vga_ser_write
    mov esi, lookup_str_3
    mov cl, 4               ; "SWAP" length
    call dict_lookup
    test eax, eax
    jz .dict_test_3_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_4
.dict_test_3_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_4:
    ; Test 4: Look up "NOTAWORD" (should not exist)
    mov esi, lookup_str_4
    call vga_ser_write
    mov esi, lookup_str_4
    mov cl, 8               ; "NOTAWORD" length
    call dict_lookup
    test eax, eax
    jnz .dict_test_4_fail
    mov esi, msg_dict_notfound
    call vga_ser_write
    jmp .dict_tests_done
.dict_test_4_fail:
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    
.dict_tests_done:

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

; Test strings for numq
msg_test: db 10, "Testing numq:", 10, 0
msg_test_pass: db " => ", 0
msg_test_fail: db " => FAIL", 10, 0

test_str_1: db "'A'", 0           ; Char literal: should be 65
test_str_2: db "$FF", 0          ; Hex: should be 255
test_str_3: db "%1010", 0        ; Binary: should be 10
test_str_4: db "#42", 0          ; Decimal: should be 42
test_str_5: db "-99", 0          ; Negative: should be -99
test_str_6: db "0", 0            ; Zero
test_str_7: db "#XYZ", 0         ; Invalid: should fail

; Test strings for dict_lookup
msg_dict_test: db 10, "Testing dict_lookup:", 10, 0
msg_dict_found: db " => FOUND at 0x", 0
msg_dict_notfound: db " => NOT FOUND", 10, 0

lookup_str_1: db "DUP", 0         ; Should find DUP
lookup_str_2: db "dup", 0         ; Should find dup (case-insensitive)
lookup_str_3: db "SWAP", 0        ; Should find SWAP
lookup_str_4: db "NOTAWORD", 0    ; Should NOT find this
