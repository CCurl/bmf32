; ============================================================================
; Utility functions
; ============================================================================

; streq - compare two null-terminated strings
; Entry: ESI = pointer to first string, EDI = pointer to second string
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
    jz .exit
    
    inc ecx                 ; Next character
    jmp .compare
    
.exit:
    pop ecx
    pop ebx
    ret


; ============================================================================
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

; ============================================================================
; numq - Check if string is a number and convert to integer
; Entry: ESI = pointer to string
; Exit: EAX = 1 if valid number, 0 if not
;       ECX = accumulated number (if valid)
numq:
    push ebx
    push edx
    push edi
    
    ; Check for char literal: 'c' format
    mov ax, [esi]
    mov bx, [esi+2]

    test al, al             ; Empty string? fail
    jz .numq_fail

    cmp al, 39              ; 39 = ' (single quote)
    jne .numq_check_prefix
    
    cmp bl, 39              ; Check closing quote
    jne .numq_fail          ; Not a valid char literal if not closed

    test bh, bh             ; Check null terminator
    jnz .numq_fail          ; Not a valid char literal if not null-terminated
    
    ; Valid char literal: push the character value
    movzx ecx, ah           ; Zero-extend AH to ECX
    jmp .numq_push_result
    
.numq_check_prefix:
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
    test al, al
    jz .numq_fail
    
    ; Initialize accumulator
    xor ecx, ecx            ; ECX = accumulated number
    
.numq_loop:
    mov al, [esi]
    test al, al
    jz .numq_done
    
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
    mov eax, 1
    jmp .numq_return
    
.numq_fail:
    xor ecx, ecx
    xor eax, eax
    
.numq_return:
    pop edi
    pop edx
    pop ebx
    ret

; ******************************************************************************
; iToA: input:  EAX: the number to print - destroyed
;       input:  EBX: the base to convert to (2, 10, 16)
;       output: ECX: the start of the string
;               EDX: the length of the string
i2aBuf: db 64 dup(0)
iToA:   mov     ecx, i2aBuf+63  ; output string start
        mov     edx, 0          ; output length
        mov     BYTE [ecx], 0
        push    0               ; isNegative flag
        bt      eax, 31
        jnc     .L
        inc     BYTE [esp]
        neg     eax
.L:     mov     edx, 0
        div     ebx
        add     dl, '0'
        cmp     dl, '9'
        jle     .L1
        add     dl, 7
.L1:    dec     ecx
        mov     BYTE [ecx], dl
        inc     edx
        test    eax, eax
        jnz     .L
        pop     eax             ; get the negative flag
        test    eax, eax        ; 0 means not negative
        jz      .X
        dec     ecx
        mov     BYTE [ecx], '-'
        inc     edx
.X:     ret
