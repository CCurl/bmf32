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
; numq - Check if string is a number and convert to integer
; Entry: ESI = pointer to string
; Exit: EBX = 1 if valid number, 0 if not
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
    mov edx, 1              ; Set negative flag
    inc esi
    ; Check if string is empty after prefix/minus
    mov bl, [esi]
    test bl, bl
    jz .numq_fail
    
.numq_loop:
    mov bl, [esi]
    test bl, bl
    jz .numq_done
    
.numq_char_ok:
    ; Convert char to digit value (0-9 or 10-15 for a-f)
    ; Try 0-9
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
; iToA: input:  EAX: the number to print
;       input:  EBX: the base to convert to (2, 10, 16)
;       output: ESI: the start of the string
;               ECX: the length of the string
i2aBuf: db 64 dup(0)
iToA:   push    eax
        mov     esi, i2aBuf+63  ; output string start
        xor     ecx, ecx        ; output length
        xor     edx, edx        ; isNegative flag
        mov     BYTE [esi], 0   ; NULL terminate the string
        bt      eax, 31
        jnc     .L0
        mov edx, 1             ; set isNegative flag
        neg     eax
.L0:    push    edx
.loop:  cdq
        div     ebx
        add     dl, '0'
        cmp     dl, '9'
        jle     .L1
        add     dl, 7
.L1:    dec     esi
        mov     BYTE [esi], dl
        inc     ecx
        test    eax, eax
        jnz     .loop
        pop     edx             ; get the negative flag
        test    edx, edx        ; 0 means not negative
        jz      .xit
        dec     esi
        mov     BYTE [esi], '-'
        inc     ecx
.xit:   pop     eax             ; restore original EAX
        ret
