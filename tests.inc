; ============================================================================
; Tests
; ============================================================================
dictEntry Found, "FOUND", 5, 0
    dd xtLit, msgFF, xtZType, xtRet

dictEntry NotFound, "NOT-FOUND", 9, 0
    dd xtLit, msgNF, xtZType, xtRet

;    : test1 1 LIT 16 CELLS + EMIT ;
dictEntry Test1, "TEST1", 5, 0
    dd xtLit, 12345, xtDup, xtDot, xtDot
    dd xtRet

dictEntry Test2, "TEST2", 5, 0
    dd xtLit, 1, xtLit, 2, xtLit, 3, xtAdd, xtAdd, xtDot
    dd xtRet

;    : test3 IF 'Y' EMIT EXIT THEN 'N' EMIT ;
dictEntry Test3, "TEST3", 5, 0
    dd xtJmpz, .is0
        dd xtLit, 'Y', xtEmit, xtRet
.is0:
    dd xtLit, 'N', xtEmit
    dd xtRet

;    : test4  ;
dictEntry Test4, "TEST4", 5, 0
    dd xtLit, 222332, xtOnePlus, xtxStore, xtxFetch, xtDot
    dd xtRet

;    : test5 'N' SWAP IF DROP 'Y' THEN EMIT ;
dictEntry Test5, "TEST5", 5, 0
    dd xtLit, 'N', xtSwap, xtJmpz, .emit
        dd xtDrop, xtLit, 'Y'
.emit:
    dd xtEmit
    dd xtRet

dictEntry TestHW, "TEST-HW", 7, 0
    dd xtLit, helloWorld, xtZType, xtCR, xtRet

dictEntry TDS, "TDS", 3, 0
    dd xtDup, xtZType, xtFind, xtJmpz, .nf1
        dd xtFound, xtRet
.nf1:
    dd xtNotFound, xtRet

; TEST-DICT - Test FIND primitive
; Tests dictionary lookup with various word names
dictEntry TestDict, "TEST-DICT", 9, 0
    dd xtLit, lookup_str_1, xtTDS
    dd xtLit, lookup_str_2, xtTDS
    dd xtLit, lookup_str_3, xtTDS
    dd xtLit, lookup_str_4, xtTDS
    dd xtRet

dictEntry Tests, "TESTS", 5, 0
    dd xtTestHW
    dd xtTest1
    dd xtTest2
    dd xtLit, 1, xtTest3
    dd xtLit, 0, xtTest3
    dd xtSpace
    dd xtTest4
    dd xtLit, 123, xtTest5
    dd xtLit,   0, xtTest5, xtCR
    dd xtTestDict
    dd xtRet

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
