; ============================================================================
; This Forth system is subroutine threaded
; ============================================================================

; Note: EAX is the top of the stack (TOS) (future)
; Note: EBP is the data stack pointer (grows downward)
; Note: ESP is the return stack pointer (grows downward)

CELL_SZ equ "4"
TOS equ [ebp]          ; Top of stack
NOS equ [ebp+CELL_SZ]  ; Next on stack
; TOS equ EAX
R1 equ EBX
R2 equ ECX
R3 equ EDX
R4 equ ESI
R5 equ EDI

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
    getTOS R1
    dPush R1
    ret

; DROP primitive - Remove top of stack
; ( a b -- a )
dictEntry drop, "DROP", 4, 0
    cmp ebp, DATA_STK_TOP
    jge .underflow         ; Data stack underflow?
    dDrop
    ret
.underflow:
    mov ebp, DATA_STK_TOP
    ret

; KEY? primitive - Check if keyboard buffer has data
; Returns 1 (true) or 0 (false) on data stack
; ( -- flag )
dictEntry keyq, "KEY?", 4, 0
    dPush 0                  ; Default to 0 (no data)
    mov R1, [keyboard_tail]
    cmp R1, [keyboard_head]
    je .keyq_exit            ; If tail == head, buffer is empty
    setTOS 1                 ; Return 1 if data available
.keyq_exit:                  ; If tail == head, buffer is empty
    ret

; SWAP primitive - Exchange top two stack elements
; ( a b -- b a )
dictEntry swap, "SWAP", 4, 0
    getTOS R1
    getNOS R2
    setTOS R2
    setNOS R1
    ret

; OVER primitive - push NOS
; ( a b -- a b a )
dictEntry over, "OVER", 4, 0
    getNOS R1
    dPush R1
    ret

; TIMER primitive - Get the current TIMER value
; ( -- ticks )
dictEntry timer, "TIMER", 5, 0
    mov R1, [timer_ticks]
    dPush R1
    ret

; ADD primitive - Add top two stack elements
; ( a b -- sum )
dictEntry add, "+", 1, 0
    dPop R1
    getTOS EAX
    add EAX, R1
    setTOS EAX
    ret

; SUB primitive - Subtract top two stack elements
; ( a b -- diff )
dictEntry sub, "-", 1, 0
    dPop R1
    getTOS EAX
    sub EAX, R1
    setTOS EAX
    ret

; MULT primitive - Multiply top two stack elements
; ( a b -- product )
dictEntry mult, "*", 1, 0
    dPop R1
    getTOS EAX
    imul EAX, R1
    setTOS EAX
    ret

; DIV primitive - Divide top two stack elements
; ( a b -- quotient )
dictEntry div, "/", 1, 0
    dPop R1
    getTOS EAX
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv R1
    setTOS EAX
    ret

; /MOD primitive - Divide top two stack elements and return remainder
; ( a b -- quotient remainder )
dictEntry mod, "/MOD", 4, 0
    call XT_div
    dPush R3               ; Remainder
    ret

; = primitive - Compare top two stack elements for equality
; ( a b -- flag )
dictEntry eq, "=", 1, 0
    dPop R1
    getTOS R2
    cmp R2, R1
    sete bl
    movzx R2, bl
    setTOS R2
    ret

; AND primitive - Bitwise AND top two stack elements
; ( a b -- a&b )
dictEntry and, "AND", 3, 0
    dPop R1
    getTOS EAX
    and EAX, R1
    setTOS EAX
    ret

; OR primitive - Bitwise OR top two stack elements
; ( a b -- a|b )
dictEntry or, "OR", 2, 0
    dPop R1
    getTOS EAX
    or EAX, R1
    setTOS EAX
    ret

; XOR primitive - Bitwise XOR top two stack elements
; ( a b -- a^b )
dictEntry xor, "XOR", 3, 0
    dPop R1
    getTOS EAX
    xor EAX, R1
    setTOS EAX
    ret

; COM primitive - Bitwise complement (NOT) of top of stack
; ( a -- ~a )
dictEntry com, "COM", 3, 0
    getTOS EAX
    not EAX
    setTOS EAX
    ret

; NUMBER? primitive - Check if string is a number
; ( str -- (num 1) | 0 )
dictEntry numq, "NUMBER?", 7, 0
    dPop esi
    call numq               ; Check if string in ESI is a number
    test EAX, EAX             ; Check if numq returned 0 (not a number)
    jz .no
    dPush R2                ; Push the parsed number
.no:                        ; If not a number, push 0 as the flag
    dPush EAX
    ret

; WORD primitive - parse the next word from >IN
; ( --len )
dictEntry word, "WORD", 4, 0
    xor R2, R2              ; Length
    mov R1, [TO_IN]         ; Load TO_IN into R1
    mov R4, WORD_START
.skip_ws:                   ; Skip leading whitespace
    mov dl, [R1]
    test dl, dl             ; end of string?
    jz .done
    cmp dl, 32
    jg .collect_wd
    inc R1
    jmp .skip_ws
.collect_wd:                ; Collect characters until whitespace or null
    mov [R4+R2], dl
    inc R1
    inc R2
    mov dl, [R1]
    cmp dl, 32
    jg .collect_wd
.done:
    mov [TO_IN], R1
    mov BYTE [R4+R2], 0     ; NULL terminate the word
    dPush R2
    ret

; S-LEN primitive - Get string length
; ( str -- len )
dictEntry strlen, "S-LEN", 5, 0
    getTOS R3               ; String pointer
    xor R2, R2              ; R2 = length counter
.loop:
    mov bl, [R3+R2]
    test bl, bl
    jz .done
    inc R2
    jmp .loop
.done:
    setTOS R2
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
    dPop EAX
    jmp vga_ser_emit

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry comma, ",", 1, 0
    dPop EAX
    mov R3, [HERE]          ; Get current HERE address
    mov [R3], EAX            ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    ret

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry ccomma, "C,", 2, 0
    dPop EAX
    mov R3, [HERE]          ; Get current HERE address
    mov [R3], al            ; Store TOS byte at HERE
    add dword [HERE], 1     ; Increment HERE by 1 (byte size)
    ret

; WORDS primitive - output the words in the dictionary
; ( -- )
dictEntry words, "WORDS", 5, 0
    mov R1, [LAST]          ; Start at most recently defined word
.words_loop:
    test R1, R1             ; End of dictionary?
    jz .words_done
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
    call XT_dup             ; Get length of search string in R2
    call XT_strlen
    dPop R2                 ; R2 = length of search string
    getTOS esi              ; Get pointer to name string from TOS
    push R2                 ; Save length on stack
    mov EAX, [LAST]          ; Start at most recently defined word
    
.search_loop:
    test EAX, EAX             ; End of dictionary?
    jz .find_exit
    
    ; Check length first: compare CL with length byte at [EAX + 9]
    mov cl, [esp]           ; Restore length into CL
    mov bl, [EAX + 9]        ; Length byte
    cmp bl, cl              ; Compare lengths
    jne .name_no_match      ; Length mismatch, try next entry
    
    ; Length matches, now compare name: [EAX + 10] is the name field
    mov edi, EAX
    add edi, 10             ; Point to name in entry

    call streq              ; Compare search string (ESI) with entry name (EDI)
    jz .find_exit           ; If equal (ZF=1), found the word

.name_no_match:
    mov EAX, [EAX]          ; Follow link to previous entry
    jmp .search_loop
    
; EAX already contains either the address or 0 (not found)
.find_exit:           
    setTOS EAX
    pop R2                 ; Clean up stack
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
    getTOS EAX
    mov R1, [EAX]
    setTOS R1
    ret

; STORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry store, "!", 1, 0
    dPop R1                ; Address to store into
    dPop EAX                ; Value to store
    mov [R1], EAX
    ret

; CFETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry cFetch, "C@", 2, 0
    getTOS EAX
    movzx R1, byte [EAX]
    setTOS R1
    ret

; CSTORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry cStore, "C!", 2, 0
    dPop R1                ; Address to store into
    dPop EAX                ; Value to store
    mov [R1], al
    ret

; X@ primitive - Fetch X
; ( -- value )
dictEntry xFetch, "X@", 2, 0
    mov R1, [xVal]
    dPush R1
    ret
xVal:  dd 0

; X! primitive - Store X
; ( value -- )
dictEntry xStore, "X!", 2, 0
    dPop R1
    mov [xVal], R1
    ret

; Y@ primitive - Fetch Y
; ( -- value )
dictEntry yFetch, "Y@", 2, 0        
    mov R1, [yVal]
    dPush R1
    ret
yVal:  dd 0

; Y! primitive - Store Y
; ( value -- )
dictEntry yStore, "Y!", 2, 0
    dPop R1
    mov [yVal], R1
    ret

; Z@ primitive - Fetch Z
; ( -- value )
dictEntry zFetch, "Z@", 2, 0
    mov R1, [zVal]
    dPush R1
    ret
zVal:  dd 0

; Z! primitive - Store Z
; ( value -- )
dictEntry zStore, "Z!", 2, 0
    dPop R1
    mov [zVal], R1
    ret

; >L primitive - Move to local stack
; ( value -- )
dictEntry toLStk, ">L", 2, 0
    dPop R2
    add DWORD [LSP], CELL_SZ
    mov R1, [LSP]
    mov [R1], R2
    ret

; L@ primitive - Fetch from local stack
; ( -- value )
dictEntry atLStk, "L@", 2, 0
    mov R1, [LSP]
    mov R2, [R1]
    dPush R2
    ret

; L> primitive - Move from local stack
; ( -- value )
dictEntry fromLStk, "L>", 2, 0
    call XT_atLStk
    cmp R1, LSTK
    jle .under
    sub DWORD [LSP], CELL_SZ
    ret
.under:
    mov DWORD [LSP], LSTK
    ret

dictEntry lspAddr, "(LSP)", 5, 0
    mov R1, LSP
    dPush R1
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
    dPop EAX
    mov EBX, [BASE]
    call iToA
    dPush ECX
    dPush EDX
    ret

; ============================================================================
; Forth Primitives - End
; ============================================================================

; ============================================================================
; FORTH system words
; ============================================================================
dictEntry wordAddr, "(WORD)", 6, 0
    dPush WORD_START
    ret

dictEntry toIn, ">IN", 3, 0
    mov R1, TO_IN
    dPush R1
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
    mov R1, [TO_IN]         ; Save >IN
    push R1
    dPop R1                 ; Get input string
    mov [TO_IN], R1         ; Set >IN
.loop:
    call XT_word            ; Parse next word into buffer at WORD_START
    dPop R1                 ; Get length of parsed word
    call XT_drop            ; Drop the address of the word buffer - we know it
    test R1, R1             ; If length is 0, we're done
    jz .done
    ; TODO: Check if word is a number
    mov ESI, WORD_START     ; Push address of parsed word
    call numq               ; Check if it's a number
    test EAX, EAX           ; Check if numq returned 0 (not a number)
    jz .not_number
    dPush R2                ; Push the parsed number
    dbgPC '-'               ; Print the number
    call XT_pDot
    dbgPC '-'
    call XT_space           ; Print a space
    jmp .loop
.not_number:
    ; TODO: Check if word is in dictionary
    ; TODO: If not found, print error message
    ; TODO: Execute the word if found
    ; TEMP: Output the parsed word
    call XT_wordAddr        ; TEMP
    call XT_zType           ; TEMP
    call XT_space           ; TEMP
    jmp .loop
.done:
    pop R1                  ; Restore >IN
    mov [TO_IN], R1
    ret

; ============================================================================
; COLD Start
; ============================================================================
;   : COLD ... ;
dictEntry cold, "COLD", 4, 0
    mov ebp, DATA_STK_TOP      ; Initialize data stack pointer (EBP)
    mov EAX, LSTK               ; Initialize local stack pointer (LSP)
    mov [LSP], EAX
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
           db ": cell 4 ;", 10
           db 0
