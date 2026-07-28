; ============================================================================
; Utility functions
; ============================================================================

; strlen - determine the length of a null-terminated string
; Entry: ESI = pointer to string
; Exit: ECX = length of string (not including null terminator)
strlen:
    xor ecx, ecx            ; ECX = length counter
.strlen_loop:
    mov al, [esi+ecx]
    cmp al, 0
    je .strlen_done
    inc ecx
    jmp .strlen_loop
.strlen_done:
    ret
    