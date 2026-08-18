; ============================================================================
; This Forth system is subroutine threaded
; ============================================================================

; Note: ESI is the Inner Instruction Pointer (IP)
; Note: EAX is the Top Of Stack (TOS)
; Note: EBP is the data stack pointer (grows downward)
; Note: ESP is the return stack pointer (grows downward)

CELL_SZ equ 4
TOS equ EAX
NOS equ [EBP]  ; Next on stack
IIP equ ESI
R1  equ EBX
R2  equ ECX
R3  equ EDX
R4  equ EDI

; ============================================================================
; SECTION: MACROS
; ============================================================================

macro getTOS regVal {
    mov regVal, TOS
}

macro setTOS regVal {
    mov TOS, regVal
}

macro getNOS regVal {
    mov regVal, NOS
}

macro setNOS regVal {
    mov NOS, regVal
}

macro dPush regVal {
    call fDup
    mov TOS, regVal
}

macro dDrop {
    call fDrop
}

macro dPop regMem {
    mov regMem, TOS
    call fDrop
}

macro NEXT {
    jmp inner
}

; Create a dictionary entry (link, xt, flags, len, name, null, code/words)
last_de equ 0
macro dictEntry nameId, name, len, flags {
dict_##nameId:
    dd last_de
    dd xt##nameId
    db flags, len, name, 0
    last_de equ dict_##nameId
xt##nameId:
}

; ============================================================================
; Forth initialize and run
; ============================================================================
run_forth:
    ; Set LAST to the last defined word
    mov [Start_ESP], esp           ; Save the start of the return stack for bounds checking
    mov ebp, DATA_STK_TOP          ; Save the start of the return stack for bounds checking
    mov DWORD [HERE], DICT_START   ; Set HERE to the start of the dictionary
    mov DWORD [BASE], 10           ; Set BASE to 10
    mov DWORD [STATE], 0           ; Set STATE to interpreting mode
    mov DWORD [LSP], LSTK          ; Set LSP to the start of the return stack
    mov IIP, TheROM
    jmp inner

; ============================================================================
; Forth inner interpreter
; ============================================================================
doWord:
    push IIP            ; Save the current IIP on the return stack
    mov IIP, R1         ; "Call" that word
inner:
    mov R1, [IIP]       ; Get the next instruction
    add IIP, 4          ; Increment the instruction pointer
    cmp R1, primsEnd    ; Is it a primitive?
    jg doWord           ; If not, must be a word
    jmp R1              ; Jump to the primitive's code

; ============================================================================
; Forth Primitives - Start
; ============================================================================
fDup:
    lea ebp, [ebp - CELL_SZ]
    mov DWORD [ebp], EAX
    ret

fDrop:
    mov EAX, DWORD [ebp]
    lea ebp, [ebp + CELL_SZ]
    ret

; SPACE primitive - output a space character
; ( -- )
dictEntry Space, "SPACE", 5, 0
    push EAX
    mov EAX, 32
    call vga_ser_emit
    pop EAX
    NEXT

; EXIT primitive - Push literal onto the stack
; ( -- )
dictEntry Ret, "EXIT", 4, 0
    cmp esp, [Start_ESP]  ; Check if we are at the bottom of the return stack
    jge .exit             ; If at the bottom, exit
    pop IIP               ; Restore the IIP
    NEXT
.exit:
    ret

; LIT primitive - Push literal onto the stack
; ( -- n )
dictEntry Lit, "LIT", 3, 0
    dPush [IIP]         ; Push the literal value onto the stack
    add IIP, 4          ; Increment the instruction pointer
    NEXT

; JMP primitive - Unconditional jump
; ( -- )
dictEntry Jmp, "JMP", 3, 0
    mov IIP, [IIP]      ; Load the target address into IIP
    NEXT                ; Continue execution

; JMPZ primitive - Jump if top of stack is zero
; ( n -- )
dictEntry Jmpz, "JMPZ", 4, 0
    dPop R1             ; Pop the top of stack into ebx
    test R1, R1         ; Test if it's zero
    jz xtJmp            ; If zero, jump to the target address
    add IIP, 4          ; If not zero, skip the target address
    NEXT                ; Continue execution

; JMPNZ primitive - Jump if top of stack is not zero
; ( n -- )
dictEntry Jmpnz, "JMPNZ", 5, 0
    dPop R1             ; Pop the top of stack into ebx
    test R1, R1         ; Test if it's zero
    jnz xtJmp           ; If not zero, jump to the target address
    add IIP, 4          ; If zero, skip the target address
    NEXT                ; Continue execution

; DUP primitive - Duplicate top of stack
; ( a -- a a )
dictEntry Dup, "DUP", 3, 0
    call fDup
    NEXT

; ?DUP primitive - Duplicate top of stack if not 0
; ( a -- a | a a )
dictEntry QDup, "?DUP", 4, 0
    test TOS, TOS
    jnz xtDup
    NEXT

; DROP primitive - Remove top of stack
; ( a b -- a )
dictEntry Drop, "DROP", 4, 0
    cmp ebp, DATA_STK_TOP
    jge .underflow         ; Data stack underflow?
    dDrop
    NEXT
.underflow:
    mov ebp, DATA_STK_TOP
    NEXT

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
    NEXT

; SWAP primitive - Exchange top two stack elements
; ( a b -- b a )
dictEntry Swap, "SWAP", 4, 0
    getTOS R1
    setTOS NOS
    setNOS R1
    NEXT

; OVER primitive - push NOS
; ( a b -- a b a )
dictEntry Over, "OVER", 4, 0
    getNOS R1
    dPush R1
    NEXT

; TIMER primitive - Get the current TIMER value
; ( -- ticks )
dictEntry Timer, "TIMER", 5, 0
    mov R1, [timer_ticks]
    dPush R1
    NEXT

; ADD primitive - Add top two stack elements
; ( a b -- sum )
dictEntry Add, "+", 1, 0
    dPop R1
    getTOS EAX
    add EAX, R1
    setTOS EAX
    NEXT

; SUB primitive - Subtract top two stack elements
; ( a b -- diff )
dictEntry Sub, "-", 1, 0
    dPop R1
    getTOS EAX
    sub EAX, R1
    setTOS EAX
    NEXT

; MULT primitive - Multiply top two stack elements
; ( a b -- product )
dictEntry Mult, "*", 1, 0
    dPop R1
    getTOS EAX
    imul EAX, R1
    setTOS EAX
    NEXT

; DIV primitive - Divide top two stack elements
; ( a b -- quotient )
dictEntry Div, "/", 1, 0
    dPop R1
    getTOS EAX
    cdq                     ; Sign-extEnd EAX into EDX:EAX
    idiv R1
    setTOS EAX
    NEXT

; /MOD primitive - Divide top two stack elements and return remainder
; ( a b -- quotient remainder )
dictEntry Mod, "/MOD", 4, 0
    dPop R1
    getTOS EAX
    cdq                     ; Sign-extEnd EAX into EDX:EAX
    idiv R1
    setTOS EAX
    dPush EDX               ; Push the remainder
    NEXT

; = primitive - Compare top two stack elements for equality
; ( a b -- flag )
dictEntry Eq, "=", 1, 0
    dPop R1
    getTOS R2
    cmp R2, R1
    sete bl
    movzx R2, bl
    setTOS R2
    NEXT

; AND primitive - Bitwise AND top two stack elements
; ( a b -- a&b )
dictEntry And, "AND", 3, 0
    dPop R1
    getTOS EAX
    and EAX, R1
    setTOS EAX
    NEXT

; OR primitive - Bitwise OR top two stack elements
; ( a b -- a|b )
dictEntry Or, "OR", 2, 0
    dPop R1
    getTOS EAX
    or EAX, R1
    setTOS EAX
    NEXT

; XOR primitive - Bitwise XOR top two stack elements
; ( a b -- a^b )
dictEntry Xor, "XOR", 3, 0
    dPop R1
    getTOS EAX
    xor EAX, R1
    setTOS EAX
    NEXT

; COM primitive - Bitwise complement (NOT) of top of stack
; ( a -- ~a )
dictEntry Com, "COM", 3, 0
    getTOS EAX
    not EAX
    setTOS EAX
    NEXT

; NUMBER? primitive - Check if string is a number
; ( str -- (num 1) | 0 )
dictEntry NumQ, "NUMBER?", 7, 0
    push IIP                ; Save IIP
    dPop ESI
    call numq               ; Check if string in ESI is a number
    test EBX, EBX           ; Check if numq returned 0 (not a number)
    jz .no
    dPush ECX               ; Push the parsed number
.no:                        ; If not a number, push 0 as the flag
    dPush EBX
    pop IIP                 ; Restore IIP
    NEXT

; WORD primitive - parse the next word from >IN
; ( --len )
dictEntry Word, "WORD", 4, 0
    mov R1, [TO_IN]         ; Load TO_IN into R1
    xor R2, R2              ; Length
    ; NOTE R3 is EDX; dl is used for character transfer
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
    inc R1                  ; Next character
    inc R2                  ; Increment length
    mov dl, [R1]
    cmp dl, 32
    jg .collect_wd
.done:
    mov [TO_IN], R1
    mov BYTE [R4+R2], 0     ; NULL terminate the word
    dPush R2
    NEXT

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
    NEXT

; S-EQ primitive - Compare two strings for equality
; ( s1 s2 -- f )
dictEntry streq, "S-EQ", 4, 0
    push ESI                 ; save IIP
    dPop ESI                 ; string 2
    getTOS EDI               ; string 1
    setTOS 0                 ; Default to 0 (not equal)
    call streq
    jnz .exit
    inc TOS
.exit:
    pop ESI                 ; restore IIP
    NEXT

; S-CPY primitive - Copy string from src to dst
; ( dst src -- dst )
dictEntry strcpy, "S-CPY", 5, 0
    push ESI                 ; save IIP
    dPop ESI                 ; src
    getTOS EDI               ; dst
    pop ESI                  ; restore IIP
    NEXT

; EMIT primitive - Output character on TOS
; ( ch -- )
dictEntry Emit, "EMIT", 4, 0
    call vga_ser_emit
    call fDrop
    NEXT

; COMMA primitive - store TOS value at HERE, increment HERE by 4
; ( N-- )
dictEntry comma, ",", 1, 0
    dPop R1
    mov R3, [HERE]          ; Get current HERE address
    mov [R3], R1            ; Store TOS value at HERE
    add dword [HERE], 4     ; Increment HERE by 4 (cell size)
    NEXT

; CCOMMA primitive - store TOS byte at HERE, increment HERE by 1
; ( B-- )
dictEntry ccomma, "C,", 2, 0
    dPop EBX
    mov R3, [HERE]          ; Get current HERE address
    mov [R3], bl            ; Store TOS byte at HERE
    add dword [HERE], 1     ; Increment HERE by 1 (byte size)
    NEXT

; WORDS primitive - output the words in the dictionary
; ( -- )
dictEntry Words, "WORDS", 5, 0
    push ESI                ; save IIP
    mov R1, [LAST]          ; Start at most recently defined word
.words_loop:
    test R1, R1             ; End of dictionary?
    jz .words_done
    push R1                 ; Save current word address
    mov esi, R1             ; Set ESI: word name address
    add esi, 10             ; Name in entry is at offset 10
    call vga_ser_write
    dPush 32                ; Name in entry is at offset 10
    call vga_ser_emit       ; Print a space after the word
    pop R1                  ; Restore current word address
    mov R1, [R1]            ; Next word in dictionary
    jmp .words_loop
.words_done:
    pop ESI                 ; restore IIP
    NEXT

; Dictionary lookup - find a word by name
; (str -- addr | 0)
dictEntry Find, "FIND", 4, 0
    push ESI                ; save IIP
    getTOS ESI              ; Get pointer to name string from TOS
    mov TOS, [LAST]         ; Start at most recently defined word
    
.search_loop:
    test TOS, TOS           ; End of dictionary?
    jz .find_exit
    
    mov edi, TOS            ; Set EDI to address of the entry's name
    add edi, 10             ; Name in entry is at offset 10

    call streq              ; Compare search string (ESI) with entry name (EDI)
    jz .find_exit           ; If equal (ZF=1), found the word

.name_no_match:
    mov TOS, [TOS]          ; Follow link to previous entry
    jmp .search_loop
    
.find_exit:                 ; TOS already contains either the address or 0 (not found)
    pop ESI                 ; restore IIP
    NEXT

; 1+ primitive - Increment TOS by 1
; (n -- n+1)
dictEntry OnePlus, "1+", 2, 0
    inc TOS
    NEXT

; 1- primitive - Decrement TOS by 1
; (n -- n-1)
dictEntry OneMinus, "1-", 2, 0
    dec TOS
    NEXT

; FETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry Fetch, "@", 1, 0
    mov TOS, [TOS]
    NEXT

; STORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry Store, "!", 1, 0
    dPop R1                  ; Address to store into
    dPop R2                  ; Value to store
    mov [R1], R2
    NEXT

; CFETCH primitive - Fetch a cell from memory
; ( addr -- value )
dictEntry CFetch, "C@", 2, 0
    movzx R1, byte [TOS]
    setTOS R1
    NEXT

; CSTORE primitive - Store a cell into memory
; ( value addr -- )
dictEntry CStore, "C!", 2, 0
    dPop R1                  ; Address to store into
    dPop ECX                 ; Value to store
    mov [R1], CL
    NEXT

; >R primitive - Store a cell into memory
; ( value -- )
dictEntry RTo, ">R", 2, 0
    dPop R1                  ; Pop the value from the data stack
    push R1                  ; Push it onto return stack
    NEXT

; R@ primitive - Get the top of the return stack
; ( -- value )
dictEntry RFetch, "R@", 2, 0
    pop R1                  ; Pop the value from return stack
    push R1                 ; Push it back on
    dPush R1                ; Push the value onto data stack
    NEXT

; R> primitive - Fetch a cell from the return stack
; ( -- value )
dictEntry RFrom, "R>", 2, 0
    pop R1                  ; Pop the value from return stack
    dPush R1                ; Push the value onto data stack
    NEXT

; 3 free variables
xVal:  dd 0
yVal:  dd 0
zVal:  dd 0

; X@ primitive - Fetch X
; ( -- value )
dictEntry xFetch, "X@", 2, 0
    dPush [xVal]
    NEXT

; X! primitive - Store X
; ( value -- )
dictEntry xStore, "X!", 2, 0
    dPop [xVal]
    NEXT

; Y@ primitive - Fetch Y
; ( -- value )
dictEntry yFetch, "Y@", 2, 0        
    dPush [yVal]
    NEXT

; Y! primitive - Store Y
; ( value -- )
dictEntry yStore, "Y!", 2, 0
    dPop [yVal]
    NEXT

; Z@ primitive - Fetch Z
; ( -- value )
dictEntry zFetch, "Z@", 2, 0
    dPush [zVal]
    NEXT

; Z! primitive - Store Z
; ( value -- )
dictEntry zStore, "Z!", 2, 0
    dPop [zVal]
    NEXT

; >L primitive - Move to local stack
; ( value -- )
dictEntry toLStk, ">L", 2, 0
    add DWORD [LSP], CELL_SZ
    mov R1, [LSP]
    mov [R1], TOS
    call fDrop
    NEXT

; L@ primitive - Fetch from local stack
; ( -- value )
dictEntry atLStk, "L@", 2, 0
    mov R1, [LSP]
    dPush [R1]
    NEXT

; L> primitive - Move from local stack
; ( -- value )
dictEntry fromLStk, "L>", 2, 0
    mov R1, [LSP]
    mov R2, [R1]
    dPush R2
    cmp R1, LSTK
    jna .under
    sub R1, CELL_SZ
    mov [LSP], R1
    NEXT
.under:
    mov DWORD [LSP], LSTK
    setTOS 0
    NEXT

dictEntry lspAddr, "(LSP)", 5, 0
    dPush LSP
    NEXT

dictEntry zType, "ZTYPE", 5, 0
    push IIP                ; save IIP
    dPop ESI
    call vga_ser_write
    pop IIP                 ; restore IIP
    NEXT

; I>A primitive - Convert integer to ASCII string
; ( n -- addr len )
dictEntry iToA, "I>A", 3, 0
    mov EBX, [BASE]
    call iToA
    setTOS EDI
    dPush ECX
    NEXT

primsEnd:
; ============================================================================
; Forth Primitives - End
; ============================================================================

; ============================================================================
; FORTH system words
; ============================================================================
dictEntry WordAddr, "(WORD)", 6, 0
    dd xtLit, WORD_START
    dd xtRet

dictEntry ToIn, ">IN", 3, 0
    dd xtLit, TO_IN
    dd xtRet

dictEntry base, "BASE", 4, 0
    dd xtLit, BASE
    dd xtRet

dictEntry state, "STATE", 5, 0
    dd xtLit, STATE
    dd xtRet

dictEntry last, "(L)", 4, 0
    dd xtLit, LAST
    dd xtRet

dictEntry here, "(H)", 4, 0
    dd xtLit, HERE
    dd xtRet

; ============================================================================
; Other FORTH words
; ============================================================================
dictEntry CR, "CR", 2, 0
    dd xtLit, 0x0A, xtEmit
    dd xtRet

dictEntry PDot, "(.)", 3, 0
    dd xtiToA
    dd xtDrop             ; Drop the length of the string
    dd xtzType
    dd xtRet

dictEntry Dot, ".", 1, 0
    dd xtPDot
    dd xtSpace            ; Print a space after the number
    dd xtRet

; ============================================================================
; The outer interpreter
; ============================================================================
; : outer ( str -- )
dictEntry Outer, "OUTER", 5, 0
    ; TODO: Implement the outer interpreter logic here
    dd xtToIn , xtFetch, xtRTo                  ; Save >IN on return stack
    dd xtToIn , xtStore                         ; Set >IN to the input string
.oLoop:
    dd xtWord, xtJmpz, .oXit
    dd xtLit, WORD_START, xtNumQ, xtJmpz, .isWordQ
        dd xtLit, '.', xtEmit, xtPDot, xtLit, '.', xtEmit
        dd xtJmp, .oLoop
.isWordQ:
    dd xtLit, WORD_START, xtFind, xtQDup, xtJmpz, .notFound
        dd xtLit, WORD_START, xtzType, xtFound, xtJmp, .oLoop
.notFound:
    dd xtLit, WORD_START, xtzType, xtLit, oErr, xtzType, xtSpace
    dd xtJmp, .oLoop
.oXit:
    dd xtRFrom, xtToIn, xtStore                  ; Restore >IN
    dd xtRet

; ============================================================================
; COLD Start
; ============================================================================
include 'tests.inc'            ; Include the test suite for now

dictEntry Cold, "COLD", 4, 0
    ; TODO: Implement the cold start logic here
    dd xtRet

; ============================================================================
; FORTH variables
; ============================================================================

Start_ESP: dd 0                 ; Start of return stack (for bounds checking)
HERE:  dd DICT_START            ; Next free address (grows UP)
LAST:  dd last_de               ; Most recent word (0 initially, will point to last defined)
BASE:  dd 10                    ; Number base (default 10)
STATE: dd 0                     ; FORTH state (0 = interpreting, 1 = compiling)
TO_IN: dd 0                     ; >IN - address of current input stream
LSTK:  dd 32 dup(0)             ; Local stack (32 cells)
LSP:   dd LSTK                  ; Local stack pointer (grows UP)
Hello: db "Hello.", 0
Bye:   db "Bye.", 0
oErr:  db "??", 0

FORTH_SRC: db ": test1 123 ;", 10
           db ": test2 -00045 ;", 10
           db " WORDS : CELL 4 ;", 10
           db 0

; ============================================================================
; Init / Run
; ============================================================================
TheROM:
    dd xtLit, Hello, xtzType, xtCR
    dd xtWords, xtCR
    dd xtTests
    dd xtLit, FORTH_SRC, xtOuter
    dd xtLit, Bye, xtzType, xtCR
    dd xtRet
