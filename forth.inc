; ============================================================================
; Forth system
; ============================================================================

; Note: EBP = data stack pointer (grows downward)

; ============================================================================
; SECTION: MACROS
; ============================================================================

; Read top of stack into a register (non-destructive)
; Usage: getTOS eax     (or any 32-bit register)
macro getTOS reg {
    mov reg, [ebp]
}

; Write to top of stack (non-destructive to SP)
; Usage: setTOS eax     (or any 32-bit register)
macro setTOS reg {
    mov [ebp], reg
}

; Push a register onto the data stack (grows downward, EBP = data stack pointer)
; Usage: dPush eax    (or any 32-bit register)
macro dPush reg {
    sub ebp, 4
    setTOS reg
}

; Pop a value from the data stack into a register
; Usage: dPop eax     (or any 32-bit register)
macro dPop reg {
    getTOS reg
    add ebp, 4
}

; Read next on stack (2nd element) into a register (non-destructive)
; Usage: getNOS eax     (or any 32-bit register)
macro getNOS reg {
    mov reg, [ebp+4]
}

; Write to next on stack (2nd element, non-destructive to SP)
; Usage: setNOS eax     (or any 32-bit register)
macro setNOS reg {
    mov [ebp+4], reg
}

; ============================================================================
; Forth dictionary entries - Start
; ============================================================================
section '.data'

; Name     Offset     Length
; ----------------------------
; Link     0 -> 3     4
; XT       4 -> 7     4
; Flags    8          1
; Len      9          1
; Name     10 -> n    variable
; NULL     n+1        1
; Code/XT  n+2        variable

; Macro to create a dictionary entry
macro dictEntry prev, nameId, name, len, flags {
dict_##nameId:
    if prev eq 0
        dd 0   ; No previous word, link = 0
    else
        dd dict_##prev   ; Link to previous word
    end if
    dd XT_##nameId
    db flags, len, name, 0
XT_##nameId:
}

; CELL primitive - Push cell size onto stack
; (-- cell-size)
dictEntry 0, cell, "CELL", 4, 0
    mov eax, 4              ; Cell size is 4 bytes
    dPush eax
    ret

; DUP primitive - Duplicate top of stack
; (a -- a a)
dictEntry cell, dup, "DUP", 3, 0
    getTOS eax
    dPush eax
    ret

; DROP primitive - Remove top of stack
; (a b -- a)
dictEntry dup, drop, "DROP", 4, 0
    dPop eax
    ret

; KEY? primitive - Check if keyboard buffer has data
; Returns 1 (true) or 0 (false) on data stack
; (-- flag)
dictEntry drop, keyq, "KEY?", 4, 0
    xor eax, eax             ; Default to 0 (no data)
    mov ebx, [keyboard_tail]
    cmp ebx, [keyboard_head]
    je .keyq_exit            ; If tail == head, buffer is empty
    inc eax                  ; Return 1 if data available
.keyq_exit:                  ; If tail == head, buffer is empty
    dPush eax
    ret

; SWAP primitive - Exchange top two stack elements
; (a b -- b a)
dictEntry keyq, swap, "SWAP", 4, 0
    getTOS eax
    getNOS ebx
    setTOS ebx
    setNOS eax
    ret

; OVER primitive - push NOS
; (a b -- a b a)
dictEntry swap, over, "OVER", 4, 0
    getNOS eax
    dPush eax
    ret

; TIMER primitive - Get the current TIMER value
; (-- ticks)
dictEntry over, timer, "TIMER", 5, 0
    mov eax, [timer_ticks]
    dPush eax
    ret

; ADD primitive - Add top two stack elements
; (a b -- sum)
dictEntry timer, add, "+", 1, 0
    dPop ebx
    getTOS eax
    add eax, ebx
    setTOS eax
    ret

; SUB primitive - Subtract top two stack elements
; (a b -- diff)
dictEntry add, sub, "-", 1, 0
    dPop ebx
    getTOS eax
    sub eax, ebx
    setTOS eax
    ret

; MULT primitive - Multiply top two stack elements
; (a b -- product)
dictEntry sub, mult, "*", 1, 0
    dPop ebx
    getTOS eax
    imul eax, ebx
    setTOS eax
    ret

; DIV primitive - Divide top two stack elements
; (a b -- quotient)
dictEntry mult, div, "/", 1, 0
    dPop ebx
    getTOS eax
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv ebx
    setTOS eax
    ret

; NUMBER? primitive - Check if string is a number
; ( str -- (num 1) | 0 )
dictEntry div, numq, "NUMBER?", 7, 0
    dPop esi
    call numq               ; Check if string in ESI is a number
    dPush eax               ; numq pushes the parsed number if valid
    ret

; WORD primitive - parse the next word from >IN
; ( --a len )
dictEntry numq, word, "WORD", 4, 0
    mov ecx, 0              ; Length
    mov esi, [TO_IN]
    mov edi, WORD_START
.skip_ws:                   ; Skip leading whitespace
    mov al, [esi]
    test al, al             ; end of string?
    jz .done
    cmp al, 32
    jg .collect_wd
    inc esi
    jmp .skip_ws
.collect_wd:                ; Collect characters until whitespace or null
    mov [edi+ecx], al
    inc esi
    inc ecx
    mov al, [esi]
    cmp al, 32
    jg .collect_wd
.done:
    mov [TO_IN], esi
    dPush edi
    dPush ecx
    ret

; STRLEN primitive - Get string length
; ( str -- len )
dictEntry word, strlen, "S-LEN", 6, 0
    getTOS esi
    xor ecx, ecx            ; ECX = length counter
.loop:
    mov bl, [esi+ecx]
    test bl, bl
    jz .done
    inc ecx
    jmp .loop
.done:
    setTOS ecx
    ret

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry strlen, emit, "EMIT", 4, 0
    dPop eax
    call vga_ser_emit
    ret

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry emit, comma, ",", 1, 0
    dPop eax
    mov edx, [HERE]         ; Get current HERE address
    mov [edx], eax          ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    ret

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry comma, ccomma, "C,", 2, 0
    dPop eax
    mov edx, [HERE]          ; Get current HERE address
    mov [edx], al            ; Store TOS byte at HERE
    add dword [HERE], 1      ; Increment HERE by 1 (byte size)
    ret

; CR primitive - Output a carriage return/newline
; ( -- )
dictEntry ccomma, cr, "CR", 2, 0
    mov al, 10              ; Newline character
    call vga_ser_emit
    ret

; WORDS primitive - output the words in the dictionary
; ( -- )
dictEntry cr, words, "WORDS", 5, 0
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

; Dictionary lookup - find a word by name
; (str -- addr | 0)
dictEntry words, find, "FIND", 4, 0
    call XT_dup             ; Get length of search string in ECX
    call XT_strlen
    dPop ecx                ; ECX = length of search string
    getTOS esi              ; Get pointer to name string from TOS
    push ecx                ; Save length on stack
    mov eax, [LAST]         ; Start at most recently defined word
    
.search_loop:
    test eax, eax           ; End of dictionary?
    je .find_exit
    
    ; Check length first: compare CL with length byte at [eax + 9]
    mov cl, [esp]           ; Restore length into CL
    mov bl, [eax + 9]       ; Length byte
    cmp bl, cl              ; Compare lengths
    jne .name_no_match      ; Length mismatch, try next entry
    
    ; Length matches, now compare name: [eax + 10] is the name field
    mov edi, eax
    add edi, 10             ; Point to name in entry

    call streq              ; Compare search string (ESI) with entry name (EDI)
    jz .find_exit           ; If equal (ZF=1), found the word

.name_no_match:
    mov eax, [eax]          ; Follow link to previous entry
    jmp .search_loop
    
; EAX already contains either the address or 0 (not found)
.find_exit:           
    setTOS eax
    pop ecx                 ; Clean up stack
    ret

; ============================================================================
; Forth dictionary entries - End
; ============================================================================

init_forth:
    ; Set LAST to the last defined word
    mov ebp, DATA_STK_BASE   ; Data stack pointer (EBP)
    mov dword [LAST], dict_find
    mov dword [BASE], 10
    mov dword [STATE], 0
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
; Variables
; ============================================================================

section '.data'

; FORTH variables
HERE: dd DICT_START             ; Next free address (grows UP)
LAST: dd 0                      ; Most recent word (0 initially, will point to last defined)
BASE: dd 10                     ; Number base (default 10)
STATE: dd 0                     ; FORTH state (0 = interpreting, 1 = compiling)
TO_IN: dd 0                     ; >IN - address of current input stream
