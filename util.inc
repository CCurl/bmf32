; ============================================================================
; Utility functions
; ============================================================================

; streq - compare two null-terminated strings
; Entry: ESI = address of first string, EDI = address of second string
; Exit: ZF = 1 if equal, ZF = 0 if not equal
streq:
    push ebx
    push ecx
    xor ecx, ecx            ; ECX = index
    
.compare:
    mov bl, [esi+ecx]
    cmp bl, [edi+ecx]
    jne .exit               ; Bytes don't match

    test bl, bl             ; Check for null terminator
    je .exit
    
    inc ecx
    jmp .compare
    
.exit:
    pop ecx
    pop ebx
    ret

; hex_to_string - convert a number to a hex string
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
