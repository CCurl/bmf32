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
; strcpy - copy null-terminated string from ESI to EDI
; Entry: ESI = source string, EDI = destination buffer
; Exit: no registers modified, destination string is null-terminated
strcpy:
    push eax
    push ecx
    xor ecx, ecx

.loop:
    mov al, [esi+ecx]
    mov [edi+ecx], al
    inc ecx
    test al, al
    jnz .loop

    pop ecx
    pop eax
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
; numq - Convert a string to a number (base 10 only)
; Entry: ESI = pointer to string
; Exit: EBX = 1 if valid, 0 if not
;       ECX = accumulated number (if valid)
numq:
    push edx
    xor edx, edx            ; Clear negative flag
    xor ecx, ecx            ; Clear accumulated number
    mov bl, [esi]
    test bl, bl             ; Empty string? fail
    jz .numq_fail
    
.numq_check_minus:
    cmp bl, '-'
    jne .numq_loop
    inc edx                 ; Set negative flag
    inc esi
    ; Check if string is empty after prefix/minus
    mov bl, [esi]
    test bl, bl
    jz .numq_fail
    
.numq_loop:
    mov bl, [esi]
    test bl, bl
    jz .numq_done
    cmp bl, '0'
    jl .numq_fail
    cmp bl, '9'
    jg .numq_fail
    sub bl, '0'
    movzx ebx, bl
    
    ; Accumulate: n = n*base + digit
    imul ecx, ecx, 10       ; ECX = ECX * 10
    add ecx, ebx            ; ECX = ECX + digit
    inc esi
    jmp .numq_loop
    
.numq_done:
    ; Apply sign if needed
    test edx, edx
    jz .numq_good
    neg ecx
    
.numq_good:
    mov ebx, 1
    jmp .numq_return
    
.numq_fail:
    xor ecx, ecx
    xor ebx, ebx
    
.numq_return:
    pop edx
    ret

; ******************************************************************************
; dot10: input:  EAX: the number to print in base 10
;        output: none (prints to VGA serial)
dotBuf: db 64 dup(0)
dot10:  push    eax             ; save EAX
        push    ebx
        push    ecx
        push    edx
        push    edi
        push    esi
        mov     edi, dotBuf+63  ; output string start
        xor     ecx, ecx        ; output length
        push    ecx             ; save isNegative flag
        mov     ebx, 10         ; base for division
        mov     BYTE [edi], 0   ; NULL terminate the string
        bt      eax, 31
        jnc     .loop
        inc     BYTE [esp]      ; set isNegative flag
        neg     eax
.loop:  cdq
        div     ebx             ; EAX gets quotient
        add     dl, '0'         ; EDX gets remainder
        dec     edi
        mov     BYTE [edi], dl
        test    eax, eax
        jnz     .loop
        pop     eax
        test    eax, eax        ; 0 means not negative
        jz      .xit
        dec     edi
        mov     BYTE [edi], '-'
.xit:   mov     esi, edi
        call    vga_ser_write
        pop     esi
        pop     edi
        pop     edx
        pop     ecx
        pop     ebx
        pop     eax
        ret
