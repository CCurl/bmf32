; ============================================================================
; Forth dictionary entries - Start
; ============================================================================
section '.data'
; Format for a dictionary entry: [Link:0-3][XT:4-7][Flags:8][Len:9][Name:10-?][NULL][Code(XT)]

; Macro to create a dictionary entry
macro dictEntry prev, nameId, nameStr, len, flgs {
dict_##nameId:
    if prev eq 0
        dd 0   ; No previous word, link = 0
    else
        dd dict_##prev   ; Link to previous word, XT
    end if
    dd XT_##nameId   ; Link to previous word, XT
    db flgs, len
    db nameStr, 0       ; Flags, Length, Name, NULL
XT_##nameId:
}

; CELL primitive - Push cell size onto stack
; (-- cell-size)
dictEntry 0, cell, "CELL", 4, 0
    dPush eax
    mov eax, 4              ; Cell size is 4 bytes
    ret

; DUP primitive - Duplicate top of stack
; (a -- a a)
dictEntry cell, dup, "DUP", 3, 0
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
    dPush eax
    xor eax, eax             ; Default to 0 (no data)
    mov ebx, [keyboard_tail]
    cmp ebx, [keyboard_head]
    je .keyq_exit            ; If tail == head, buffer is empty
    inc eax                  ; Return 1 if data available
.keyq_exit:                  ; If tail == head, buffer is empty
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
    dPush eax
    mov eax, [timer_ticks]
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
    dPush edi
.skip_ws:                   ; Skip leading whitespace
    mov al, [esi]
    cmp al, 0               ; end of string?
    je .done
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
    dPush ecx
    ret

; STRLEN primitive - Get string length
; ( str -- len )
dictEntry word, slen, "STRLEN", 6, 0
    getTOS esi
    call strlen
    setTOS ecx
    ret

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry slen, emit, "EMIT", 4, 0
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

; TODO: Add more primitives

; ============================================================================
; Forth dictionary entries - End
; ============================================================================
