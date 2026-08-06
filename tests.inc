; ============================================================================
; Tests
; ============================================================================
;    : test1 1 LIT 16 CELLS + EMIT ;
dictEntry test1, "TEST1", 5, 0
    dd 0x80000001, XT_lit, 16, XT_cells, XT_add, XT_emit, XT_exit

dictEntry test2, "TEST2", 5, 0
    dd XT_test1, XT_cr, XT_words, XT_cr, XT_exit

;    : test3 IF 'Y' EMIT EXIT THEN 'N' EMIT ;
dictEntry test3, "TEST3", 5, 0
    dd XT_jmpz, t3_no
       dd $80000059, XT_emit, XT_exit
t3_no:
    dd $8000004E, XT_emit, XT_exit

;    : test4 'N' X! IF 'Y' X! THEN X@ EMIT ;
dictEntry test4, "TEST4", 5, 0
    dd $8000004E, XT_xStore, XT_jmpz, t4_no
        dd $80000059, XT_xStore
t4_no:
    dd XT_xFetch, XT_emit, XT_exit

;    : test5 'N' SWAP IF DROP 'Y' THEN EMIT ;
dictEntry test5, "TEST5", 5, 0
    dd $8000004E, XT_swap, XT_jmpz, t5_no
        dd XT_drop, $80000059
t5_no:
    dd XT_emit, $80000041, XT_xStore
    dd XT_xAtPlus, XT_emit, XT_xAtPlus, XT_emit, XT_xAtPlus, XT_emit
    dd XT_exit

dictEntry testHW, "TEST-HW", 7, 0
    dd XT_lit, helloWorld, XT_zType
    dd XT_hex, XT_lit, 12345, XT_iToA, XT_drop, XT_space, XT_zType
    dd XT_dec, XT_lit, 12345, XT_iToA, XT_drop, XT_space, XT_zType
    dd XT_cr, XT_exit

; TEST-NUMQ - Test NUMBER? primitive
; Tests parsing various number formats: decimal, hex, binary, char literals
dictEntry testNumq, "TEST-NUMQ", 9, 0
    dd XT_exit                ; TODO: implement number parsing tests

; TEST-DICT - Test FIND primitive
; Tests dictionary lookup with various word names
dictEntry testDict, "TEST-DICT", 9, 0
    dd XT_exit                ; TODO: implement dictionary lookup tests

; TEST-WORD - Test WORD primitive
; Tests word parsing from input stream
dictEntry testWord, "TEST-WORD", 9, 0
    dd XT_exit                ; TODO: implement word parsing tests

dictEntry tests, "TESTS", 5, 0
    dd XT_testHW, XT_test1, XT_test2
    dd XT_lit, 1, XT_test3, XT_lit, 0, XT_test3, XT_space
    dd XT_lit, 1, XT_test4, XT_lit, 0, XT_test4, XT_space
    dd XT_lit, 1, XT_test5, XT_lit, 0, XT_test5, XT_space
    dd XT_testNumq, XT_testDict, XT_testWord
    dd XT_cr, XT_exit

; ============================================================================
; Test data
; ============================================================================

section '.data'

; Test strings for numq
msg_test: db 10, "Testing numq:", 10, 0
msg_test_pass: db " => ", 0
msg_test_fail: db " => FAIL", 10, 0

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

; Test strings for WORD primitive
msg_word_test: db 10, "Testing WORD:", 10, 0
word_test_input_1: db "  hello  world!", 0 ; Leading spaces, should parse "hello"
helloWorld: db "Hello, World! ", 0
