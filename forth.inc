; ============================================================================
; This Forth system is inspired by Tachyon
; ============================================================================

; Note: EBP = data stack pointer (grows downward)
; Note: ESP = return stack pointer (grows downward)
; Note: ESI = instruction pointer (points to next instruction to execute)

CELL_SZ equ 4
TOS equ [ebp]          ; Top of stack
NOS equ [ebp+CELL_SZ]  ; Next on stack
R1 equ EAX
R2 equ EBX
R3 equ ECX
R4 equ EDX
R5 equ ESI
R6 equ EDI

; ============================================================================
; SECTION: MACROS
; ============================================================================

; Read top of stack into a register (non-destructive)
; Usage: getTOS EAX     (or any 32-bit register)
; Note: 'MOV DWORD EAX, [EBP]' is machine code '$8B $45 $00'
; Note: 'MOV DWORD EBX, [EBP]' is machine code '$8B $5D $00'
macro getTOS reg {
    mov reg, TOS
}

; Write to top of stack (non-destructive to SP)
; Usage: setTOS EAX     (or any 32-bit value)
; Note: 'MOV DWORD [ebp], EAX' is machine code '$89 $45 $00'
; Note: 'MOV DWORD [ebp], EBX' is machine code '$89 $5D $00'
; Note: 'MOV DWORD [ebp], $12345678' is machine code '$C7 $45 $00 $78 $56 $34 $12'
macro setTOS regVal {
    mov DWORD TOS, regVal
}

; Push a value onto the data stack (grows downward, EBP = data stack pointer)
; Usage: dPush EAX    (or any 32-bit value)
; Note: 'SUB EBP, 4' is machine code '$83 $ED $04'
macro dPush regVal {
    lea ebp, [ebp-CELL_SZ]
    setTOS regVal
}

; Drop the value from the data stack
; Usage: dDrop (no operands)
; Note: 'ADD EBP, 4' is machine code '$83 $C5 $04'
macro dDrop {
    lea ebp, [ebp+CELL_SZ]
}

; Pop a value from the data stack into a register
; Usage: dPop EAX     (or any 32-bit register)
macro dPop reg {
    getTOS reg
    dDrop
}

; Read next on stack (2nd element) into a register (non-destructive)
; Usage: getNOS EAX     (or any 32-bit register)
; Note: 'MOV DWORD EAX, [EBP+4]' is machine code '$8B $45 $04'
macro getNOS reg {
    mov reg, [ebp+CELL_SZ]
}

; Write to next on stack (2nd element, non-destructive to SP)
; Usage: setNOS EAX     (or any 32-bit register)
; Note: 'MOV DWORD [EBP+4], EAX' is machine code '$89 $45 $04'
; Note: 'MOV DWORD [EBP+4], $12345678' is machine code '$C7 $45 $04 $78 $56 $34 $12'
macro setNOS regVal {
    mov DWORD [ebp+CELL_SZ], regVal
}

; Macro to print a char to the serial port (for debugging)
macro dbgPC ch {
    push eax
    mov al, ch
    call ser_emit
    pop eax
}

; Macro to create a dictionary entry
last_de equ 0
macro dictEntry nameId, name, len, flags {
dict_##nameId:
    dd last_de
    dd XT_##nameId
    db flags, len, name, 0
    last_de equ dict_##nameId
XT_##nameId:
}

; ============================================================================
; Forth Primitives - Start
; ============================================================================

; Name     Offset     Length
; ----------------------------
; Link     0 -> 3     4
; XT       4 -> 7     4
; Flags    8          1
; Len      9          1
; Name     10 -> n    variable
; NULL     n+1        1
; Code/XT  n+2        variable

; DUP primitive - Duplicate top of stack
; ( a -- a a )
dictEntry dup, "DUP", 3, 0
    getTOS R2
    dPush R2
    ret

; DROP primitive - Remove top of stack
; ( a b -- a )
dictEntry drop, "DROP", 4, 0
    cmp ebp, DATA_STK_BASE
    jge .underflow         ; Data stack underflow?
    dDrop
    ret
.underflow:
    mov ebp, DATA_STK_BASE
    ret

; KEY? primitive - Check if keyboard buffer has data
; Returns 1 (true) or 0 (false) on data stack
; ( -- flag )
dictEntry keyq, "KEY?", 4, 0
    dPush 0                  ; Default to 0 (no data)
    mov R2, [keyboard_tail]
    cmp R2, [keyboard_head]
    je .keyq_exit            ; If tail == head, buffer is empty
    setTOS 1                 ; Return 1 if data available
.keyq_exit:                  ; If tail == head, buffer is empty
    ret

; SWAP primitive - Exchange top two stack elements
; ( a b -- b a )
dictEntry swap, "SWAP", 4, 0
    getTOS R2
    getNOS R3
    setTOS R3
    setNOS R2
    ret

; OVER primitive - push NOS
; ( a b -- a b a )
dictEntry over, "OVER", 4, 0
    getNOS R2
    dPush R2
    ret

; TIMER primitive - Get the current TIMER value
; ( -- ticks )
dictEntry timer, "TIMER", 5, 0
    mov R2, [timer_ticks]
    dPush R2
    ret

; ADD primitive - Add top two stack elements
; ( a b -- sum )
dictEntry add, "+", 1, 0
    dPop R2
    getTOS R2
    add R2, R3
    setTOS R2
    ret

; SUB primitive - Subtract top two stack elements
; ( a b -- diff )
dictEntry sub, "-", 1, 0
    dPop R2
    getTOS R2
    sub R2, R3
    setTOS R2
    ret

; MULT primitive - Multiply top two stack elements
; ( a b -- product )
dictEntry mult, "*", 1, 0
    dPop R2
    getTOS R1
    imul R1, R2
    setTOS R1
    ret

; DIV primitive - Divide top two stack elements
; ( a b -- quotient )
dictEntry div, "/", 1, 0
    dPop R2
    getTOS R1
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv R2
    setTOS R1
    ret

; /MOD primitive - Divide top two stack elements and return remainder
; ( a b -- quotient remainder )
dictEntry mod, "/MOD", 4, 0
    call XT_div
    dPush R4               ; Remainder
    ret

; = primitive - Compare top two stack elements for equality
; ( a b -- flag )
dictEntry eq, "=", 1, 0
    dPop R2
    getTOS R3
    cmp R3, R2
    sete bl
    movzx R3, bl
    setTOS R3
    ret

; AND primitive - Bitwise AND top two stack elements
; ( a b -- a&b )
dictEntry and, "AND", 3, 0
    dPop R2
    getTOS R1
    and R1, R2
    setTOS R1
    ret

; OR primitive - Bitwise OR top two stack elements
; ( a b -- a|b )
dictEntry or, "OR", 2, 0
    dPop R2
    getTOS R1
    or R1, R2
    setTOS R1
    ret

; XOR primitive - Bitwise XOR top two stack elements
; ( a b -- a^b )
dictEntry xor, "XOR", 3, 0
    dPop R2
    getTOS R1
    xor R1, R2
    setTOS R1
    ret

; COM primitive - Bitwise complement (NOT) of top of stack
; ( a -- ~a )
dictEntry com, "COM", 3, 0
    getTOS R1
    not R1
    setTOS R1
    ret

; NUMBER? primitive - Check if string is a number
; ( str -- (num 1) | 0 )
dictEntry numq, "NUMBER?", 7, 0
    dPop R4
    push esi                ; Save instruction pointer
    mov esi, R4
    call numq               ; Check if string in ESI is a number
    pop esi                 ; Restore instruction pointer
    test R1, R1             ; Check if numq returned 0 (not a number)
    jz .no
    dPush R3                ; Push the parsed number
.no:                        ; If not a number, push 0 as the flag
    dPush R1
    ret

; WORD primitive - parse the next word from >IN
; ( --a len )
dictEntry word, "WORD", 4, 0
    xor R3, R3              ; Length
    mov R2, [TO_IN]         ; Load TO_IN into R2
    mov R5, WORD_START
.skip_ws:                   ; Skip leading whitespace
    mov dl, [R2]
    test dl, dl             ; end of string?
    jz .done
    cmp dl, 32
    jg .collect_wd
    inc R2
    jmp .skip_ws
.collect_wd:                ; Collect characters until whitespace or null
    mov [R5+R3], dl
    inc R2
    inc R3
    mov dl, [R2]
    cmp dl, 32
    jg .collect_wd
.done:
    mov [TO_IN], R2
    mov BYTE [R5+R3], 0     ; NULL terminate the word
    dPush R5
    dPush R3
    ret

; S-LEN primitive - Get string length
; ( str -- len )
dictEntry strlen, "S-LEN", 5, 0
    getTOS R4               ; String pointer
    xor R3, R3              ; R3 = length counter
.loop:
    mov bl, [R4+R3]
    test bl, bl
    jz .done
    inc R3
    jmp .loop
.done:
    setTOS R3
    ret

; S-EQ primitive - Compare two strings for equality
; ( s1 s2 -- f )
dictEntry streq, "S-EQ", 4, 0
    dPop esi                 ; string 2
    getTOS edi               ; string 1
    setTOS 0                 ; Default to 0 (not equal)
    call streq
    jnz .exit
    inc byte TOS
.exit:
    ret

; S-CPY primitive - Copy string from src to dst
; ( dst src -- dst )
dictEntry strcpy, "S-CPY", 5, 0
    dPop esi                 ; src
    getTOS edi               ; dst
    jmp strcpy

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry emit, "EMIT", 4, 0
    dPop R1
    jmp vga_ser_emit

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry comma, ",", 1, 0
    dPop R1
    mov R4, [HERE]          ; Get current HERE address
    mov [R4], R1            ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    ret

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry ccomma, "C,", 2, 0
    dPop R1
    mov R4, [HERE]          ; Get current HERE address
    mov [R4], al            ; Store TOS byte at HERE
    add dword [HERE], 1     ; Increment HERE by 1 (byte size)
    ret

; WORDS primitive - output the words in the dictionary
; ( -- )
dictEntry words, "WORDS", 5, 0
    mov R1, [LAST]          ; Start at most recently defined word
.words_loop:
    cmp R1, 0               ; End of dictionary?
    je .words_done
    push R1                 ; Print the word name
    mov esi, R1             ; Set ESI: word name address
    add esi, 10
    call vga_ser_write
    mov al, 32              ; Space
    call vga_ser_emit
    pop R1
    mov R1, [R1]            ; Next word in dictionary
    jmp .words_loop
.words_done:
    ret

; Dictionary lookup - find a word by name
; (str -- addr | 0)
dictEntry find, "FIND", 4, 0
    push esi                ; Save Forth instruction pointer
    call XT_dup             ; Get length of search string in R3
    call XT_strlen
    dPop R3                 ; R3 = length of search string
    getTOS esi              ; Get pointer to name string from TOS
    push R3                 ; Save length on stack
    mov R1, [LAST]          ; Start at most recently defined word
    
.search_loop:
    test R1, R1             ; End of dictionary?
    je .find_exit
    
    ; Check length first: compare CL with length byte at [EAX + 9]
    mov cl, [esp]           ; Restore length into CL
    mov bl, [R1 + 9]        ; Length byte
    cmp bl, cl              ; Compare lengths
    jne .name_no_match      ; Length mismatch, try next entry
    
    ; Length matches, now compare name: [EAX + 10] is the name field
    mov edi, R1
    add edi, 10             ; Point to name in entry

    call streq              ; Compare search string (ESI) with entry name (EDI)
    jz .find_exit           ; If equal (ZF=1), found the word

.name_no_match:
    mov R1, [R1]          ; Follow link to previous entry
    jmp .search_loop
    
; EAX already contains either the address or 0 (not found)
.find_exit:           
    setTOS R1
    pop R3                 ; Clean up stack
    pop esi                 ; Restore Forth instruction pointer
    ret

; CELL primitive - Push cell size onto stack
; (-- cell-size)
dictEntry cell, "CELL", 4, 0
    dPush 4
    ret

; 1+ primitive - Increment TOS by 1
; (n -- n+1)
dictEntry onePlus, "1+", 2, 0
    inc DWORD [ebp]
    ret

; 1- primitive - Decrement TOS by 1
; (n -- n-1)
dictEntry oneMinus, "1-", 2, 0
    dec DWORD [ebp]
    ret

; FETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry fetch, "@", 1, 0
    getTOS R1
    mov R2, [R1]
    setTOS R2
    ret

; STORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry store, "!", 1, 0
    dPop R2                ; Address to store into
    dPop R1                ; Value to store
    mov [R2], R1
    ret

; CFETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry cFetch, "C@", 2, 0
    getTOS R1
    movzx R2, byte [R1]
    setTOS R2
    ret

; CSTORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry cStore, "C!", 2, 0
    dPop R2                ; Address to store into
    dPop R1                ; Value to store
    mov [R2], al
    ret

; X@ primitive - Fetch X
; ( -- value )
dictEntry xFetch, "X@", 2, 0
    mov R2, [xVal]
    dPush R2
    ret
xVal:  dd 0

; X! primitive - Store X
; ( value -- )
dictEntry xStore, "X!", 2, 0
    dPop R2
    mov [xVal], R2
    ret

; Y@ primitive - Fetch Y
; ( -- value )
dictEntry yFetch, "Y@", 2, 0        
    mov R2, [yVal]
    dPush R2
    ret
yVal:  dd 0

; Y! primitive - Store Y
; ( value -- )
dictEntry yStore, "Y!", 2, 0
    dPop R2
    mov [yVal], R2
    ret

; Z@ primitive - Fetch Z
; ( -- value )
dictEntry zFetch, "Z@", 2, 0
    mov R2, [zVal]
    dPush R2
    ret
zVal:  dd 0

; Z! primitive - Store Z
; ( value -- )
dictEntry zStore, "Z!", 2, 0
    dPop R2
    mov [zVal], R2
    ret

; >L primitive - Move to local stack
; ( value -- )
dictEntry toLStk, ">L", 2, 0
    dPop R3
    add DWORD [LSP], CELL_SZ
    mov R2, [LSP]
    mov [R2], R3
    ret

; L@ primitive - Fetch from local stack
; ( -- value )
dictEntry atLStk, "L@", 2, 0
    mov R2, [LSP]
    mov R3, [R2]
    dPush R3
    ret

; L> primitive - Move from local stack
; ( -- value )
dictEntry fromLStk, "L>", 2, 0
    call XT_atLStk
    cmp R2, LSTK
    jle .under
    sub DWORD [LSP], CELL_SZ
    ret
.under:
    mov DWORD [LSP], LSTK
    ret

dictEntry lspAddr, "(LSP)", 5, 0
    mov R2, LSP
    dPush R2
    ret

dictEntry zType, "ZTYPE", 5, 0
    push esi
    dPop esi
    call vga_ser_write
    pop esi
    ret

; I>A primitive - Convert integer to ASCII string
; ( n -- addr len )
dictEntry iToA, "I>A", 3, 0
    dPop R1
    mov R2, [BASE]
    call iToA
    dPush R3
    dPush R4
    ret

; ============================================================================
; Forth Primitives - End
; ============================================================================

; ============================================================================
; FORTH system words
; ============================================================================
dictEntry toIn, ">IN", 3, 0
    mov R2, TO_IN
    dPush R2
    ret

dictEntry base, "BASE", 4, 0
    dPush BASE
    ret

dictEntry state, "STATE", 5, 0
    dPush STATE
    ret

dictEntry last, "(L)", 4, 0
    dPush LAST
    ret

dictEntry here, "(H)", 4, 0
    dPush HERE
    ret

; ============================================================================
; Other FORTH words
; ============================================================================
dictEntry cr, "CR", 2, 0
    dPush 10
    jmp XT_emit

dictEntry space, "SPACE", 5, 0
    dPush 32
    jmp XT_emit

dictEntry pDot, "(.)", 3, 0
    call XT_iToA
    call XT_drop
    jmp XT_zType

; ============================================================================
; The outer interpreter
; ============================================================================
; : outer ( str -- )
dictEntry outer, "OUTER", 5, 0
    mov R2, [TO_IN]         ; Save >IN
    push R2
    dPop R2                 ; Get input string
    mov [TO_IN], R2         ; Set >IN
.loop:
    call XT_word            ; Parse next word into buffer at WORD_START
    dPop R2                 ; Get length of parsed word
    test R2, R2             ; If length is 0, we're done
    jz .done
    ; TODO: Check if word is a number
    ; TODO: Check if word is in dictionary
    ; TODO: If not found, print error message
    ; TODO: Execute the word if found
    ; TEMP: Output the parsed word
    call XT_zType           ; TEMP
    call XT_space           ; TEMP
    jmp .loop
.done:
    pop R2                  ; Restore >IN
    mov [TO_IN], R2
    ret

; ============================================================================
; COLD Start
; ============================================================================
;   : COLD ... ;
dictEntry cold, "COLD", 4, 0
    mov ebp, DATA_STK_BASE      ; Initialize data stack pointer (EBP)
    mov R1, LSTK               ; Initialize local stack pointer (LSP)
    mov [LSP], R1
    mov dword [BASE], 10
    mov dword [STATE], 0
    ; just run the test suite for now
    call XT_tests
    dPush FORTH_SRC
    call XT_outer
    ret

include 'tests.inc'
; ============================================================================
; Init/ Run
; ============================================================================
init_forth:
    ; Set LAST to the last defined word
    mov [Start_ESP], esp        ; Save the start of the return stack for bounds checking
    mov dword [LAST], last_de
    mov dword [HERE], DICT_START
    ret

run_forth:
    jmp XT_cold

; ============================================================================
; FORTH variables
; ============================================================================

Start_ESP: dd 0                 ; Start of return stack (for bounds checking)
HERE:  dd DICT_START            ; Next free address (grows UP)
LAST:  dd last_de               ; Most recent word (0 initially, will point to last defined)
BASE:  dd 10                    ; Number base (default 10)
STATE: dd 0                     ; FORTH state (0 = interpreting, 1 = compiling)
TO_IN: dd 0                     ; >IN - address of current input stream
LSTK:  dd 32 dup(0)
LSP:   dd LSTK

FORTH_SRC: db ": test1 235 ;", 10
           db ": test2 334 ;", 10
           db 0
