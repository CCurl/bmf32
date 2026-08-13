; ============================================================================
; Tests
; ============================================================================
dictEntry found, "FOUND", 5, 0
    dPush msgFF
    jmp XT_zType

dictEntry notFound, "NOT-FOUND", 9, 0
    dPush msgNF
    jmp XT_zType

;    : test1 1 LIT 16 CELLS + EMIT ;
dictEntry test1, "TEST1", 5, 0
    dPush 12345
    call XT_dup
    call XT_dot
    call XT_dot
    jmp XT_cr

dictEntry test2, "TEST2", 5, 0
    dPush 1
    dPush 2
    dPush 3
    call XT_add
    call XT_add
    jmp XT_dot

;    : test3 IF 'Y' EMIT EXIT THEN 'N' EMIT ;
dictEntry test3, "TEST3", 5, 0
    dPop ebx
    test ebx, ebx
    jz .is0
    dPush 'Y'
    jmp XT_emit
.is0:
    dPush 'N'
    jmp XT_emit

;    : test4  ;
dictEntry test4, "TEST4", 5, 0
    dPush 222332
    call XT_onePlus
    call XT_xStore
    call XT_xFetch
    call XT_dot
    ret

;    : test5 'N' SWAP IF DROP 'Y' THEN EMIT ;
dictEntry test5, "TEST5", 5, 0
    dPush 'N'
    call XT_swap
    dPop ebx
    test ebx, ebx
    jz .emit
    call XT_drop
    dPush 'Y'
.emit:
    jmp XT_emit

dictEntry testHW, "TEST-HW", 7, 0
    dPush helloWorld
    call XT_zType
    jmp XT_cr

td_sub:
    call XT_dup
    call XT_zType
    call XT_find
    test TOS, TOS
    jz .nf1
    call XT_found
    jmp .xit
.nf1:
    call XT_notFound
.xit:
    ret

; TEST-DICT - Test FIND primitive
; Tests dictionary lookup with various word names
dictEntry testDict, "TEST-DICT", 9, 0
    call XT_cr
    dPush lookup_str_1
    call td_sub
    dPush lookup_str_2
    call td_sub
    dPush lookup_str_3
    call td_sub
    dPush lookup_str_4
    call td_sub
    ret

dictEntry tests, "TESTS", 5, 0
    call XT_testHW
    call XT_test1
    call XT_test2
    dPush 1
    call XT_test3
    dPush 0
    call XT_test3
    call XT_space
    call XT_test4
    dPush 123
    call XT_test5
    dPush 0
    call XT_test5
    call XT_space
    call XT_testDict
    call XT_cr
    ret

; ============================================================================
; Test data
; ============================================================================


; Test strings for numq
msg_test: db 10, "Testing numq:", 10, 0
msgFF: db " - FOUND", 10, 0
msgNF: db " - NOT FOUND", 10, 0

test_str_1: db "'A'", 0          ; Char literal: should be 65
test_str_2: db "$FF", 0          ; Hex: should be 255
test_str_3: db "%1010", 0        ; Binary: should be 10
test_str_4: db "#42", 0          ; Decimal: should be 42
test_str_5: db "-99", 0          ; Negative: should be -99
test_str_6: db "0", 0            ; Zero
test_str_7: db "#XYZ", 0         ; Invalid: should fail

; Test strings for dict_lookup
msg_dict_test: db 10, "Testing dict_lookup:", 10, 0
msg_dict_found: db " => FOUND", 0
msg_dict_notfound: db " => NOT FOUND", 10, 0

lookup_str_1: db "DUP", 0         ; Should find DUP
lookup_str_2: db "dup", 0         ; Should find dup (case-insensitive)
lookup_str_3: db "SWAP", 0        ; Should find SWAP
lookup_str_4: db "NOTAWORD", 0    ; Should NOT find this

helloWorld: db "Hello, World! ", 0
