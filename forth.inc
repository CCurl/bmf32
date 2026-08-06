; ============================================================================
; This Forth system is inspired by Tachyon
; ============================================================================

; Note: EBP = data stack pointer (grows downward)
; Note: ESP = return stack pointer (grows downward)
; Note: ESI = instruction pointer (points to next instruction to execute)

CELL_SZ equ 4

; ============================================================================
; SECTION: MACROS
; ============================================================================

; Read top of stack into a register (non-destructive)
; Usage: getTOS eax     (or any 32-bit register)
; Note: 'MOV DWORD EAX, [EBP]' is machine code '$8B $45 $00'
; Note: 'MOV DWORD EBX, [EBP]' is machine code '$8B $5D $00'
macro getTOS reg {
    mov reg, [ebp]
}

; Write to top of stack (non-destructive to SP)
; Usage: setTOS eax     (or any 32-bit value)
; Note: 'MOV DWORD [ebp], eax' is machine code '$89 $45 $00'
; Note: 'MOV DWORD [ebp], ebx' is machine code '$89 $5D $00'
; Note: 'MOV DWORD [ebp], $12345678' is machine code '$C7 $45 $00 $78 $56 $34 $12'
macro setTOS regVal {
    mov DWORD [ebp], regVal
}

; Push a value onto the data stack (grows downward, EBP = data stack pointer)
; Usage: dPush eax    (or any 32-bit value)
; Note: 'SUB EBP, 4' is machine code '$83 $ED $04'
macro dPush regVal {
    sub ebp, CELL_SZ
    setTOS regVal
}

; Drop the value from the data stack
; Usage: dDrop (no operands)
; Note: 'ADD EBP, 4' is machine code '$83 $C5 $04'
macro dDrop {
    add ebp, CELL_SZ
}

; Pop a value from the data stack into a register
; Usage: dPop eax     (or any 32-bit register)
macro dPop reg {
    getTOS reg
    dDrop
}

; Read next on stack (2nd element) into a register (non-destructive)
; Usage: getNOS eax     (or any 32-bit register)
; Note: 'MOV DWORD EAX, [EBP+4]' is machine code '$8B $45 $04'
macro getNOS reg {
    mov reg, [ebp+CELL_SZ]
}

; Write to next on stack (2nd element, non-destructive to SP)
; Usage: setNOS eax     (or any 32-bit register)
; Note: 'MOV DWORD [EBP+4], EAX' is machine code '$89 $45 $04'
; Note: 'MOV DWORD [EBP+4], $12345678' is machine code '$C7 $45 $04 $78 $56 $34 $12'
macro setNOS regVal {
    mov DWORD [ebp+CELL_SZ], regVal
}

macro dbgPC ch {
    push eax
    mov al, ch
    call ser_emit
    pop eax
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

; >IN primitive - Push the address of >IN onto the stack
; ( -- a )
dictEntry toIn, ">IN", 3, 0
    mov eax, TO_IN
    dPush eax
    ret

; DUP primitive - Duplicate top of stack
; ( a -- a a )
dictEntry dup, "DUP", 3, 0
    getTOS eax
    dPush eax
    ret

; ?DUP primitive - Duplicate top of stack if not ZERO
; ( a -- a | (a a) )
dictEntry qDup, "?DUP", 4, 0
    getTOS eax
    test eax, eax
    jz .noDup
    dPush eax
.noDup:
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
    mov ebx, [keyboard_tail]
    cmp ebx, [keyboard_head]
    je .keyq_exit            ; If tail == head, buffer is empty
    setTOS 1                 ; Return 1 if data available
.keyq_exit:                  ; If tail == head, buffer is empty
    ret

; SWAP primitive - Exchange top two stack elements
; ( a b -- b a )
dictEntry swap, "SWAP", 4, 0
    getTOS eax
    getNOS ebx
    setTOS ebx
    setNOS eax
    ret

; OVER primitive - push NOS
; ( a b -- a b a )
dictEntry over, "OVER", 4, 0
    getNOS eax
    dPush eax
    ret

; TIMER primitive - Get the current TIMER value
; ( -- ticks )
dictEntry timer, "TIMER", 5, 0
    mov eax, [timer_ticks]
    dPush eax
    ret

; ADD primitive - Add top two stack elements
; ( a b -- sum )
dictEntry add, "+", 1, 0
    dPop ebx
    getTOS eax
    add eax, ebx
    setTOS eax
    ret

; SUB primitive - Subtract top two stack elements
; ( a b -- diff )
dictEntry sub, "-", 1, 0
    dPop ebx
    getTOS eax
    sub eax, ebx
    setTOS eax
    ret

; MULT primitive - Multiply top two stack elements
; ( a b -- product )
dictEntry mult, "*", 1, 0
    dPop ebx
    getTOS eax
    imul eax, ebx
    setTOS eax
    ret

; DIV primitive - Divide top two stack elements
; ( a b -- quotient )
dictEntry div, "/", 1, 0
    dPop ebx
    getTOS eax
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv ebx
    setTOS eax
    ret

; /MOD primitive - Divide top two stack elements and return remainder
; ( a b -- quotient remainder )
dictEntry mod, "/MOD", 4, 0
    call XT_div
    dPush edx               ; Remainder
    ret

; AND primitive - Bitwise AND top two stack elements
; ( a b -- a&b )
dictEntry and, "AND", 3, 0
    dPop ebx
    getTOS eax
    and eax, ebx
    setTOS eax
    ret

; OR primitive - Bitwise OR top two stack elements
; ( a b -- a|b )
dictEntry or, "OR", 2, 0
    dPop ebx
    getTOS eax
    or eax, ebx
    setTOS eax
    ret

; XOR primitive - Bitwise XOR top two stack elements
; ( a b -- a^b )
dictEntry xor, "XOR", 3, 0
    dPop ebx
    getTOS eax
    xor eax, ebx
    setTOS eax
    ret

; COM primitive - Bitwise complement (NOT) of top of stack
; ( a -- ~a )
dictEntry com, "COM", 3, 0
    getTOS eax
    not eax
    setTOS eax
    ret

; NUMBER? primitive - Check if string is a number
; ( str -- (num 1) | 0 )
dictEntry numq, "NUMBER?", 7, 0
    dPop edx
    push esi                ; Save instruction pointer
    mov esi, edx
    call numq               ; Check if string in ESI is a number
    pop esi                 ; Restore instruction pointer
    test eax, eax           ; Check if numq returned 0 (not a number)
    jz .no
    dPush ecx               ; Push the parsed number
.no:                        ; If not a number, push 0 as the flag
    dPush eax
    ret

; WORD primitive - parse the next word from >IN
; ( --a len )
dictEntry word, "WORD", 4, 0
    push ebx                ; Save EBX
    mov ecx, 0              ; Length
    mov ebx, [TO_IN]        ; Load TO_IN into EBX
    mov edi, WORD_START
.skip_ws:                   ; Skip leading whitespace
    mov al, [ebx]
    test al, al             ; end of string?
    jz .done
    cmp al, 32
    jg .collect_wd
    inc ebx
    jmp .skip_ws
.collect_wd:                ; Collect characters until whitespace or null
    mov [edi+ecx], al
    inc ebx
    inc ecx
    mov al, [ebx]
    cmp al, 32
    jg .collect_wd
.done:
    mov [TO_IN], ebx
    dPush edi
    dPush ecx
    pop ebx                 ; Restore EBX
    ret

; S-LEN primitive - Get string length
; ( str -- len )
dictEntry strlen, "S-LEN", 5, 0
    getTOS edx
    xor ecx, ecx            ; ECX = length counter
.loop:
    mov bl, [edx+ecx]
    test bl, bl
    jz .done
    inc ecx
    jmp .loop
.done:
    setTOS ecx
    ret

; S-EQ primitive - Compare two strings for equality
; ( s1 s2 -- f )
dictEntry streq, "S-EQ", 4, 0
    push esi                 ; Save instruction pointer
    dPop esi                 ; string 2
    getTOS edi               ; string 1
    setTOS 0                 ; Default to 0 (not equal)
    call streq
    jnz .exit
    setTOS 1                 ; Strings are equal
.exit:
    pop esi                  ; Restore instruction pointer
    ret

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry emit, "EMIT", 4, 0
    dPop eax
    call vga_ser_emit
    ret

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry comma, ",", 1, 0
    dPop eax
    mov edx, [HERE]         ; Get current HERE address
    mov [edx], eax          ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    ret

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry ccomma, "C,", 2, 0
    dPop eax
    mov edx, [HERE]          ; Get current HERE address
    mov [edx], al            ; Store TOS byte at HERE
    add dword [HERE], 1      ; Increment HERE by 1 (byte size)
    ret

; WORDS primitive - output the words in the dictionary
; ( -- )
dictEntry words, "WORDS", 5, 0
    mov eax, [LAST]         ; Start at most recently defined word
    push esi                ; Save ESI for later
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
    pop esi                 ; Restore ESI
    ret

; Dictionary lookup - find a word by name
; (str -- addr | 0)
dictEntry find, "FIND", 4, 0
    push esi                ; Save Forth instruction pointer
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
    getTOS eax
    mov ebx, [eax]
    setTOS ebx
    ret

; STORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry store, "!", 1, 0
    dPop ebx                ; Address to store into
    dPop eax                ; Value to store
    mov [ebx], eax
    ret

; CFETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry cFetch, "C@", 2, 0
    getTOS eax
    movzx ebx, byte [eax]
    setTOS ebx
    ret

; CSTORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry cStore, "C!", 2, 0
    dPop ebx                ; Address to store into
    dPop eax                ; Value to store
    mov [ebx], al
    ret

; X@ primitive - Fetch X
; ( -- value )
dictEntry xFetch, "X@", 2, 0
    mov eax, [xVal]
    dPush eax
    ret
xVal:  dd 0

; X! primitive - Store X
; ( value -- )
dictEntry xStore, "X!", 2, 0
    dPop eax
    mov [xVal], eax
    ret

; Y@ primitive - Fetch Y
; ( -- value )
dictEntry yFetch, "Y@", 2, 0        
    mov eax, [yVal]
    dPush eax
    ret
yVal:  dd 0

; Y! primitive - Store Y
; ( value -- )
dictEntry yStore, "Y!", 2, 0
    dPop eax
    mov [yVal], eax
    ret

; Z@ primitive - Fetch Z
; ( -- value )
dictEntry zFetch, "Z@", 2, 0
    mov eax, [zVal]
    dPush eax
    ret
zVal:  dd 0

; Z! primitive - Store Z
; ( value -- )
dictEntry zStore, "Z!", 2, 0
    dPop eax
    mov [zVal], eax
    ret

; >L primitive - Move to local stack
; ( value -- )
dictEntry toLStk, ">L", 2, 0
    dPop eax
    add DWORD [LSP], CELL_SZ
    mov ebx, [LSP]
    mov [ebx], eax
    ret

; L@ primitive - Fetch from local stack
; ( -- value )
dictEntry atLStk, "L@", 2, 0
    mov ebx, [LSP]
    mov eax, [ebx]
    dPush eax
    ret

; L> primitive - Move from local stack
; ( -- value )
dictEntry fromLStk, "L>", 2, 0
    call XT_atLStk
    cmp ebx, LSTK
    jle .under
    sub DWORD [LSP], CELL_SZ
    ret
.under:
    mov DWORD [LSP], LSTK
    ret

dictEntry lspAddr, "(LSP)", 5, 0
    mov eax, LSP
    dPush eax
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
    dPop eax
    mov ebx, [BASE]
    call iToA
    dPush ecx
    dPush edx
    ret

; ============================================================================
; Forth Primitives - End
; ============================================================================

; ============================================================================
; FORTH words
; ============================================================================
dictEntry base, "BASE", 4, 0
    dPush BASE
    ret

dictEntry dec, "DECIMAL", 7, 0
    dPush 10
    call XT_base
    jmp XT_store

dictEntry hex, "HEX", 3, 0
    dPush 16
    call XT_base
    jmp XT_store

dictEntry cr, "CR", 2, 0
    dPush 10
    jmp XT_emit

dictEntry space, "SPACE", 5, 0
    dPush 32
    jmp XT_emit

dictEntry cells, "CELLS", 5, 0
    dPush CELL_SZ
    jmp XT_mult

;   : X@+ ( -- value )  X@ DUP 1+ X! ;
dictEntry xAtPlus, "X@+", 3, 0
    call XT_xFetch
    call XT_dup
    call XT_onePlus
    jmp XT_xStore

;   : C@X+ ( -- value )  X@+ C@ ;
dictEntry cAtXPlus, "C@X+", 4, 0
    call XT_xAtPlus
    jmp XT_cFetch

;  : X>L ( -- )  X@ >L ;
dictEntry xToL, "X>L", 3, 0
    call XT_xFetch
    jmp XT_toLStk

;  : L>X ( -- )  L> X! ;
dictEntry lToX, "L>X", 3, 0
    call XT_fromLStk
    jmp XT_xStore

dictEntry pDot, "(.)", 3, 0
    call XT_iToA
    call XT_drop
    jmp XT_zType

;   : COLD TESTS ;
dictEntry cold, "COLD", 4, 0
    mov ebp, DATA_STK_BASE      ; Initialize data stack pointer (EBP)
    mov eax, LSTK               ; Initialize local stack pointer (LSP)
    mov [LSP], eax
    mov dword [BASE], 10
    mov dword [STATE], 0
    ; just run the test suite for now
    call XT_tests
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
