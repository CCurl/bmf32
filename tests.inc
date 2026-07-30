; ============================================================================
; Tests
; ============================================================================

run_tests:
    call XT_cr
    call XT_words            ; Print dictionary words
    call XT_cr

    ; Run individual test suites
    call test_numq
    call test_dict_lookup
    call test_word
    
    ret

; ============================================================================
; TEST HELPER FUNCTIONS
; ============================================================================

; Helper: Print test result in hex (with " => 0x..." format)
; Entry: EAX = value to print
; Exit: Prints result and newline
print_hex_result:
    push ecx
    push esi
    
    mov esi, msg_test_pass
    call vga_ser_write
    
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    
    call XT_cr
    
    pop esi
    pop ecx
    ret

; Helper: Print test failure message (" => FAIL")
print_fail:
    push esi
    mov esi, msg_test_fail
    call vga_ser_write
    pop esi
    ret

; Helper: Run a numq test case (ESI = test string)
; Prints string, calls numq, prints result or failure
test_numq_case:
    push eax
    push esi
    
    call vga_ser_write          ; Print test string
    call numq                   ; Parse string
    test eax, eax
    jz .numq_case_fail
    
    dPop eax                    ; Get parsed value
    call print_hex_result
    jmp .numq_case_done
    
.numq_case_fail:
    call print_fail
    
.numq_case_done:
    pop esi
    pop eax
    ret

; Helper: Run a dict_lookup test case (ESI = search string)
; Prints string, calls dict_lookup, prints result or failure
test_dict_case:
    push eax
    push esi
    
    call vga_ser_write          ; Print test string
    dPush esi                   ; Push string pointer for dict_lookup
    call XT_find                ; Lookup word
    dPop eax                    ; Get result
    test eax, eax
    jz .dict_case_not_found
    
    ; Found: print address
    mov esi, msg_dict_found
    call vga_ser_write
    call print_hex_result
    jmp .dict_case_done
    
.dict_case_not_found:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_case_done:
    pop esi
    pop eax
    ret

; ============================================================================
; TEST SUITES
; ============================================================================

; Test numq function with 7 test cases
test_numq:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov esi, msg_test
    call vga_ser_write
    
    ; Test cases 1-6: should succeed
    mov esi, test_str_1
    call test_numq_case
    
    mov esi, test_str_2
    call test_numq_case
    
    mov esi, test_str_3
    call test_numq_case
    
    mov esi, test_str_4
    call test_numq_case
    
    mov esi, test_str_5
    call test_numq_case
    
    mov esi, test_str_6
    call test_numq_case
    
    ; Test 7: #XYZ (invalid - should fail)
    mov esi, test_str_7
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_numq_correctly_failed
    call print_fail              ; Incorrectly succeeded
    jmp .test_numq_done
    
.test_numq_correctly_failed:
    call print_fail              ; Correctly failed
    
.test_numq_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Test dict_lookup function with 4 test cases
test_dict_lookup:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov esi, msg_dict_test
    call vga_ser_write
    
    ; Tests 1-3: should find
    mov esi, lookup_str_1
    call test_dict_case
    
    mov esi, lookup_str_2
    call test_dict_case
    
    mov esi, lookup_str_3
    call test_dict_case
    
    mov esi, lookup_str_4
    call test_dict_case
    
.test_dict_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Test WORD primitive
test_word:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    mov esi, msg_word_test
    call vga_ser_write
    
    ; Test: Parse "hello" from "  hello  world" (skip leading spaces)
    mov esi, word_test_input_1
    call vga_ser_write
    mov esi, word_test_input_1
    mov [TO_IN], esi            ; Set input stream to test string
    call XT_word                ; Parse the word
    
    ; Check length (should be 5 for "hello")
    dPop ecx                    ; ECX = length
    dPop esi                    ; ESI = address of parsed word
    cmp ecx, 5
    je .test_word_pass

    call XT_word                ; Parse the word
    ; Check length (should be 5 for "hello")
    dPop ecx                    ; ECX = length
    dPop esi                    ; ESI = address of parsed word
    cmp ecx, 5
    je .test_word_pass

.test_word_fail:
    call print_fail
    jmp .test_word_done
    
.test_word_pass:
    push esi
    mov esi, msg_test_pass
    call vga_ser_write
    pop esi
    call vga_ser_write
    
    mov al, ','
    dPush eax
    call XT_emit
    call XT_word                ; Parse the word
    ; Check length (should be 5 for "world!")
    dPop ecx                    ; ECX = length
    dPop esi                    ; ESI = address of parsed word
    cmp ecx, 6
    jne .test_word_fail
    call vga_ser_write

    call XT_cr
    
.test_word_done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

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
