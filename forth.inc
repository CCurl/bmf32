; ============================================================================
; This Forth system is subroutine threaded
; ============================================================================

; Notes: ESI is the Inner Instruction Pointer (IIP)
;      - EDI is the Top Of Stack (TOS)
;      - EBP is the data stack pointer (grows downward)
;      - ESP is the return stack pointer (grows downward)

CELL_SZ equ 4
TOS equ EDI     ; Top of stack
NOS equ [EBP]   ; Next on stack
R0  equ EAX     ; General purpose register 0
R1  equ EBX     ; General purpose register 1
R2  equ ECX     ; General purpose register 2
R3  equ EDX     ; General purpose register 3

; ============================================================================
macro getTOS regVal { mov regVal, TOS }
macro setTOS regVal { mov TOS, regVal }
macro getNOS regVal { mov regVal, NOS }
macro setNOS regVal { mov NOS, regVal }
macro dDrop { call fDrop }
macro NEXT { jmp inner }

macro mDup {
    lea ebp, [ebp - CELL_SZ]
    mov DWORD [ebp], TOS
}

macro mDrop {
    mov TOS, DWORD [ebp]
    lea ebp, [ebp + CELL_SZ]
}

macro mPush regVal {
    mDup
    mov TOS, regVal
}

macro mPop regMem {
    mov regMem, TOS
    mDrop
}

; ============================================================================
; Create a dictionary entry (link, xt, flags, len, name, null, code/words)
last_de equ 0
macro dictEntry nameId, name, len, flags {
d_##nameId:
    dd last_de
    dd xt##nameId
    db flags, len, name, 0
    last_de equ d_##nameId
xt##nameId:
}

; ============================================================================
; Forth inner loop and primitives
; ============================================================================
doCall:
    push esi
    mov esi, eax
inner:
    lodsd
    cmp eax, primsEnd
    jae doCall
    jmp eax

; ============================================================================
dictEntry Exit, "EXIT", 4, 0
    pop esi
    test esi, esi
    jz .done
    NEXT
.done:
    ret

dictEntry Lit, "LIT", 3, 0
    lodsd
    mPush eax
    NEXT

dictEntry Jmp, "JMP", 3, 0
    lodsd
doJmp:
    mov esi, eax
    NEXT

dictEntry JmpZ, "JMPZ", 4, 0
    lodsd
    mPop R2
    test R2, R2
    je doJmp
    NEXT

dictEntry JmpNZ, "JMPNZ", 5, 0
    lodsd
    mPop R2
    test R2, R2
    jne doJmp
    NEXT

dictEntry NJmpZ, "NJMPZ", 5, 0
    lodsd
    test TOS, TOS
    je doJmp
    NEXT

dictEntry NJmpNZ, "NJMPNZ", 6, 0
    lodsd
    test TOS, TOS
    jne doJmp
    NEXT

dictEntry Emit, "EMIT", 4, 0
    mPop EAX
    call vga_ser_emit
    NEXT

dictEntry Dup, "DUP", 3, 0
    mDup
    NEXT

dictEntry Drop, "DROP", 4, 0
    mDrop
    NEXT

dictEntry Swap, "SWAP", 4, 0
    mov R1, TOS
    mov TOS, NOS
    mov NOS, R1
    NEXT

dictEntry Over, "OVER", 4, 0
    mov R1, NOS
    mPush R1
    NEXT

dictEntry Dec, "DEC", 3, 0
    dec TOS
    NEXT

dictEntry Inc, "INC", 3, 0
    inc TOS
    NEXT

dictEntry IsNum, "NUMBER?", 7, 0
    push esi
    mPop esi
    call numq
    test ebx, ebx
    jz .notNum
    mPush ecx
.notNum:
    mPush ebx
    pop esi
    NEXT

dictEntry Dot10, ".10", 3, 0
    mPop eax
    call dot10
    mov eax, 32
    call vga_ser_emit
    NEXT

primsEnd:
; ============================================================================

dictEntry Space, "SPACE", 5, 0
    dd xtLit, 32, xtEmit, xtExit

dictEntry CR, "CR", 2, 0
    dd xtLit, 10, xtEmit, xtExit

dictEntry OK, "OK", 2, 0
    dd xtSpace, xtLit, 'O', xtEmit, xtLit, 'K', xtEmit
    dd xtLit, 10, xtEmit, xtExit

LAST: dd 0
dictEntry Last, "LAST", 4, 0
    dd xtLit, LAST, xtExit

HERE: dd 0
dictEntry Here, "HERE", 4, 0
    dd xtLit, HERE, xtExit

BASE: dd 0
dictEntry Base, "BASE", 4, 0
    dd xtLit, BASE, xtExit

STATE: dd 0
dictEntry State, "STATE", 5, 0
    dd xtLit, STATE, xtExit

dictEntry Wd, "WORD", 4, 0
    dd xtLit, WORD_START, xtExit

dictEntry Outer, "OUTER", 5, 0
    dd xtLit, 12345, xtDot10
    dd xtLit, TestN1, xtIsNum, xtDot10, xtDot10
    dd xtLit, TestN2, xtIsNum, xtDot10
    dd xtOK, xtExit
TestN1: db "223344", 0
TestN2: db "223abc", 0
; ============================================================================
theRom:
     dd xtOuter, xtLit, 10000000
.L1: dd xtDec, xtNJmpNZ, .L1
     dd xtDrop, xtJmp, theRom

; ============================================================================
run_forth:
    cld
    mov DWORD [LAST], last_de
    mov DWORD [HERE], DICT_START
    mov ebp, DATA_STK_TOP
    mov esi, theRom
    push DWORD 0
    jmp inner
