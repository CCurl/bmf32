; ============================================================================
; This Forth system is subroutine threaded
; ============================================================================

; Notes: ESI is the Inner Instruction Pointer (IP)
;      - EAX is the Top Of Stack (TOS)
;      - EBP is the data stack pointer (grows downward)
;      - ESP is the return stack pointer (grows downward)

CELL_SZ equ 4
TOS equ EAX     ; Top of stack
NOS equ [EBP]   ; Next on stack
IIP equ ESI     ; ???
R1  equ EBX     ; General purpose register 1
R2  equ ECX     ; General purpose register 2
R3  equ EDX     ; General purpose register 3
R4  equ EDI     ; General purpose register 4

; ============================================================================
macro getTOS regVal { mov regVal, TOS }
macro setTOS regVal { mov TOS, regVal }
macro getNOS regVal { mov regVal, NOS }
macro setNOS regVal { mov NOS, regVal }
macro dDrop { call fDrop }
macro NEXT { ret }

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
; Forth initialize and run
; ============================================================================
run_forth:
    call pMain
    jmp run_forth

pMain:
    mov R1, xtDup
    call doExec
    mov esi, ok1
    call vga_write
    mov R1, xtDrop
    call doExec
    mov esi, ok2
    call ser_write
    ret

dictEntry Exec, "EXECUTE", 3, 0
    mPop R1
doExec:
    jmp R1

dictEntry Dup, "DUP", 3, 0
    mDup
    ret

dictEntry Drop, "DROP", 4, 0
    mDrop
    ret

dictEntry Swap, "SWAP", 4, 0
    mov R1, TOS
    mov TOS, NOS
    mov NOS, R1
    ret

ok1: db "O", 0
ok2: db "K", 0

theRom:
    ; dd xtDup, xtDrop, xtExit