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
macro getTOS reg {
    mov reg, [ebp]
}

; Write to top of stack (non-destructive to SP)
; Usage: setTOS eax     (or any 32-bit value)
macro setTOS regVal {
    mov DWORD [ebp], regVal
}

; Push a value onto the data stack (grows downward, EBP = data stack pointer)
; Usage: dPush eax    (or any 32-bit value)
macro dPush regVal {
    sub ebp, CELL_SZ
    setTOS regVal
}

; Drop the value from the data stack
; Usage: dDrop (no operands)
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
macro getNOS reg {
    mov reg, [ebp+CELL_SZ]
}

; Write to next on stack (2nd element, non-destructive to SP)
; Usage: setNOS eax     (or any 32-bit register)
macro setNOS regVal {
    mov DWORD [ebp+CELL_SZ], regVal
}

macro doNEXT {
    ; ret
    jmp inner
}

macro dbgPC ch {
    push eax
    mov al, ch
    call ser_emit
    pop eax
}

; ============================================================================
; The Forth inner interpreter
; ============================================================================
doLit:
    dPush eax
    ;dbgPC 'n' ; temp
inner:
    lodsd
    cmp eax, prims_end
    jae .notPrim
    ;dbgPC 'p' ; temp
    jmp eax
.notPrim:
    btr eax, 31        ; Bit 31 on means it is a literal
    jc doLit
    ;dbgPC 'w' ; temp
    push esi           ; Tail-call optimization not worth the extra instructions
    mov esi, eax
    jmp inner

; ============================================================================
; Forth Primitives - Start
; ============================================================================
prims_start:

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

; EXIT primitive - exit a compiled word
; ( -- )
dictEntry exit, "EXIT", 4, 0
    ; dbgPC 'x' ; temp
    cmp esp, [Start_ESP]
    jge .done
    pop esi
    doNEXT
.done:
    dbgPC 'U' ; temp
    ret

; LIT primitive - Push literal onto stack
; ( -- n )
dictEntry lit, "(lit)", 5, 0
    lodsd
    dPush eax
    doNEXT

; JMPZ primitive - Jump if zero
; ( n -- )
dictEntry jmpz, "(jmpz)", 6, 0
    dPop ebx
    test ebx, ebx
    jz XT_jmp
    lodsd
    doNEXT

; JMPNZ primitive - Jump if not zero
; ( n -- )
dictEntry jmpnz, "(jmpnz)", 7, 0
    dPop ebx
    test ebx, ebx
    jnz XT_jmp
    lodsd
    doNEXT

; JMP primitive - Unconditional jump
; ( -- )
dictEntry jmp, "(jmp)", 5, 0
    lodsd
    mov esi, eax
    doNEXT

; DUP primitive - Duplicate top of stack
; ( a -- a a )
dictEntry dup, "DUP", 3, 0
    getTOS eax
    dPush eax
    doNEXT

; ?DUP primitive - Duplicate top of stack if not ZERO
; ( a -- a | (a a) )
dictEntry qDup, "?DUP", 4, 0
    getTOS eax
    test eax, eax
    jz .noDup
    dPush eax
.noDup:
    doNEXT

; DROP primitive - Remove top of stack
; ( a b -- a )
dictEntry drop, "DROP", 4, 0
    cmp ebp, DATA_STK_BASE
    jge .underflow         ; Data stack underflow?
    dDrop
    doNEXT
.underflow:
    mov ebp, DATA_STK_BASE
    doNEXT

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
    doNEXT

; SWAP primitive - Exchange top two stack elements
; ( a b -- b a )
dictEntry swap, "SWAP", 4, 0
    getTOS eax
    getNOS ebx
    setTOS ebx
    setNOS eax
    doNEXT

; OVER primitive - push NOS
; ( a b -- a b a )
dictEntry over, "OVER", 4, 0
    getNOS eax
    dPush eax
    doNEXT

; TIMER primitive - Get the current TIMER value
; ( -- ticks )
dictEntry timer, "TIMER", 5, 0
    mov eax, [timer_ticks]
    dPush eax
    doNEXT

; ADD primitive - Add top two stack elements
; ( a b -- sum )
dictEntry add, "+", 1, 0
    dPop ebx
    getTOS eax
    add eax, ebx
    setTOS eax
    doNEXT

; SUB primitive - Subtract top two stack elements
; ( a b -- diff )
dictEntry sub, "-", 1, 0
    dPop ebx
    getTOS eax
    sub eax, ebx
    setTOS eax
    doNEXT

; MULT primitive - Multiply top two stack elements
; ( a b -- product )
dictEntry mult, "*", 1, 0
    dPop ebx
    getTOS eax
    imul eax, ebx
    setTOS eax
    doNEXT

; DIV primitive - Divide top two stack elements
; ( a b -- quotient )
dictEntry div, "/", 1, 0
    dPop ebx
    getTOS eax
    cdq                     ; Sign-extend EAX into EDX:EAX
    idiv ebx
    setTOS eax
    doNEXT

; AND primitive - Bitwise AND top two stack elements
; ( a b -- a&b )
dictEntry and, "AND", 3, 0
    dPop ebx
    getTOS eax
    and eax, ebx
    setTOS eax
    doNEXT

; OR primitive - Bitwise OR top two stack elements
; ( a b -- a|b )
dictEntry or, "OR", 2, 0
    dPop ebx
    getTOS eax
    or eax, ebx
    setTOS eax
    doNEXT

; XOR primitive - Bitwise XOR top two stack elements
; ( a b -- a^b )
dictEntry xor, "XOR", 3, 0
    dPop ebx
    getTOS eax
    xor eax, ebx
    setTOS eax
    doNEXT

; COM primitive - Bitwise complement (NOT) of top of stack
; ( a -- ~a )
dictEntry com, "COM", 3, 0
    getTOS eax
    not eax
    setTOS eax
    doNEXT

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
    doNEXT

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
    doNEXT

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
    doNEXT

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
    doNEXT

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry emit, "EMIT", 4, 0
    dPop eax
    call vga_ser_emit
    doNEXT

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry comma, ",", 1, 0
    dPop eax
    mov edx, [HERE]         ; Get current HERE address
    mov [edx], eax          ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    doNEXT

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry ccomma, "C,", 2, 0
    dPop eax
    mov edx, [HERE]          ; Get current HERE address
    mov [edx], al            ; Store TOS byte at HERE
    add dword [HERE], 1      ; Increment HERE by 1 (byte size)
    doNEXT

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
    doNEXT

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
    doNEXT

; CELL primitive - Push cell size onto stack
; (-- cell-size)
dictEntry cell, "CELL", 4, 0
    dPush 4
    doNEXT

; 1+ primitive - Increment TOS by 1
; (n -- n+1)
dictEntry onePlus, "1+", 2, 0
    inc DWORD [ebp]
    doNEXT

; 1- primitive - Decrement TOS by 1
; (n -- n-1)
dictEntry oneMinus, "1-", 2, 0
    dec DWORD [ebp]
    doNEXT

; FETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry fetch, "@", 1, 0
    getTOS eax
    mov ebx, [eax]
    setTOS ebx
    doNEXT

; STORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry store, "!", 1, 0
    dPop ebx                ; Address to store into
    dPop eax                ; Value to store
    mov [ebx], eax
    doNEXT

; CFETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry cFetch, "C@", 2, 0
    getTOS eax
    movzx ebx, byte [eax]
    setTOS ebx
    doNEXT

; CSTORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry cStore, "C!", 2, 0
    dPop ebx                ; Address to store into
    dPop eax                ; Value to store
    mov [ebx], al
    doNEXT

; X@ primitive - Fetch X
; ( -- value )
dictEntry xFetch, "X@", 2, 0
    mov eax, [xVal]
    setTOS eax
    doNEXT

; X! primitive - Store X
; ( value -- )
dictEntry xStore, "X!", 2, 0
    dPop eax
    mov [xVal], eax
    doNEXT

; Y@ primitive - Fetch Y
; ( -- value )
dictEntry yFetch, "Y@", 2, 0        
    mov eax, [yVal]
    setTOS eax
    doNEXT

; Y! primitive - Store Y
; ( value -- )
dictEntry yStore, "Y!", 2, 0
    dPop eax
    mov [yVal], eax
    doNEXT

; Z@ primitive - Fetch Z
; ( -- value )
dictEntry zFetch, "Z@", 2, 0
    mov eax, [zVal]
    setTOS eax
    doNEXT

; Z! primitive - Store Z
; ( value -- )
dictEntry zStore, "Z!", 2, 0
    dPop eax
    mov [zVal], eax
    doNEXT

; >L primitive - Move to local stack
; ( value -- )
dictEntry toLStk, ">L", 2, 0
    dPop eax
    add DWORD [lsp], CELL_SZ
    mov ebx, [lsp]
    mov [ebx], eax
    setTOS eax
    doNEXT

; L@ primitive - Fetch from local stack
; ( -- value )
dictEntry atLStk, "L@", 2, 0
    mov ebx, [lsp]
    mov eax, [ebx]
    dPush eax
    doNEXT

; L> primitive - Move from local stack
; ( -- value )
dictEntry fromLStk, "L>", 2, 0
    mov ebx, [lsp]
    cmp ebx, lStk
    jle .under
    mov eax, [ebx]
    dPush eax
    sub DWORD [lsp], CELL_SZ
    doNEXT
.under:
    mov DWORD [lsp], lStk
    dPush 0
    doNEXT

dictEntry lspAddr, "(LSP)", 5, 0
    mov eax, lsp
    dPush eax
    doNEXT

dictEntry zType, "ZTYPE", 5, 0
    push esi
    dPop esi
    call vga_ser_write
    pop esi
    doNEXT

; I>A primitive - Convert integer to ASCII string
; ( n -- addr len )
dictEntry iToA, "I>A", 3, 0
    dPop eax
    mov ebx, [BASE]
    call iToA
    dPush ecx
    dPush edx
    doNEXT

prims_end:
; ============================================================================
; Forth Primitives - End
; ============================================================================

; ============================================================================
; FORTH words
; ============================================================================
dictEntry dec, "DECIMAL", 7, 0
    dd $8000000A, XT_lit, BASE, XT_store, XT_exit

dictEntry hex, "HEX", 3, 0
    dd $80000010, XT_lit, BASE, XT_store, XT_exit

dictEntry cr, "CR", 2, 0
    dd $8000000A, XT_emit, XT_exit

dictEntry space, "SPACE", 5, 0
    dd $80000020, XT_emit, XT_exit

dictEntry cells, "CELLS", 5, 0
    dd $80000004, XT_mult, XT_exit

;   : X@+ ( -- value )  X@ DUP 1+ X! ;
dictEntry xAtPlus, "X@+", 3, 0
    dd XT_xFetch, XT_dup, XT_onePlus, XT_xStore, XT_exit

;   : C@X+ ( -- value )  X@+ C@ ;
dictEntry cAtXPlus, "C@X+", 4, 0
    dd XT_xAtPlus, XT_cFetch, XT_exit

;  : X>L ( -- )  X@ >L ;
dictEntry xToL, "X>L", 3, 0
    dd XT_xFetch, XT_toLStk, XT_exit

;  : L>X ( -- )  L> X! ;
dictEntry lToX, "L>X", 3, 0
    dd XT_fromLStk, XT_xStore, XT_exit

dictEntry pDot, "(.)", 3, 0
    dd XT_iToA, XT_drop, XT_zType, XT_exit

;   : COLD TESTS ;
dictEntry cold, "COLD", 4, 0
    ; just run the test suite for now
    dd XT_tests, XT_exit

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
    mov ebp, DATA_STK_BASE      ; Initialize data stack pointer (EBP)
    mov eax, lStk               ; Initialize local stack pointer (LSP)
    mov [lsp], eax
    mov dword [BASE], 10
    mov dword [STATE], 0
    mov esi, XT_cold            ; FORTH COLD startup word
    cld
    jmp inner
    ret

; ============================================================================
; FORTH variables
; ============================================================================
; section '.data'

Start_ESP: dd 0                 ; Start of return stack (for bounds checking)
HERE:  dd DICT_START            ; Next free address (grows UP)
LAST:  dd last_de               ; Most recent word (0 initially, will point to last defined)
BASE:  dd 10                    ; Number base (default 10)
STATE: dd 0                     ; FORTH state (0 = interpreting, 1 = compiling)
TO_IN: dd 0                     ; >IN - address of current input stream
lStk:  dd 32 dup(0)
lsp:   dd lStk
xVal:  dd 0
yVal:  dd 0
zVal:  dd 0
