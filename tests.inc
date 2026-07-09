run_tests:
    call kernel_clear        ; Clear VGA screen
    mov esi, msg_started     ; Print startup message
    call vga_ser_write

    mov esi, msg_magic       ; Print magic number
    call vga_ser_write

    ; Load and display the multiboot magic number
    mov eax, [multiboot_magic]
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write

    mov al, 10
    call vga_ser_emit
    call vga_ser_emit
    call XT_words            ; Print dictionary words
    mov al, 10
    call vga_ser_emit

    ; Test numq function
    mov esi, msg_test
    call vga_ser_write
    
    ; Test 1: 'A' (char literal)
    mov esi, test_str_1
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_1_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_2
.test_1_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_2:
    ; Test 2: $FF (hex)
    mov esi, test_str_2
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_2_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_3
.test_2_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_3:
    ; Test 3: %1010 (binary)
    mov esi, test_str_3
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_3_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_4
.test_3_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_4:
    ; Test 4: #42 (decimal)
    mov esi, test_str_4
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_4_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_5
.test_4_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_5:
    ; Test 5: -99 (negative)
    mov esi, test_str_5
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_5_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_6
.test_5_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_6:
    ; Test 6: 0 (zero)
    mov esi, test_str_6
    call vga_ser_write
    call numq
    test eax, eax
    jz .test_6_fail
    dPop eax
    mov esi, hex_buffer
    mov ecx, 8
    call hex_to_string
    mov esi, msg_test_pass
    call vga_ser_write
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .test_7
.test_6_fail:
    mov esi, msg_test_fail
    call vga_ser_write
    
.test_7:
    ; Test 7: #XYZ (invalid)
    mov esi, test_str_7
    call vga_ser_write
    call numq
    test eax, eax
    jnz .test_7_fail
    mov esi, msg_test_fail
    call vga_ser_write
    jmp .tests_done
.test_7_fail:
    mov esi, msg_test_pass
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    
.tests_done:
    ; Test dict_lookup function
    mov esi, msg_dict_test
    call vga_ser_write
    
    ; Test 1: Look up "DUP"
    mov esi, lookup_str_1
    call vga_ser_write
    mov esi, lookup_str_1
    call dict_lookup
    test eax, eax
    jz .dict_test_1_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_2
.dict_test_1_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_2:
    ; Test 2: Look up "dup" (case-insensitive)
    mov esi, lookup_str_2
    call vga_ser_write
    mov esi, lookup_str_2
    call dict_lookup
    test eax, eax
    jz .dict_test_2_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_3
.dict_test_2_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_3:
    ; Test 3: Look up "SWAP"
    mov esi, lookup_str_3
    call vga_ser_write
    mov esi, lookup_str_3
    call dict_lookup
    test eax, eax
    jz .dict_test_3_fail
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    jmp .dict_test_4
.dict_test_3_fail:
    mov esi, msg_dict_notfound
    call vga_ser_write
    
.dict_test_4:
    ; Test 4: Look up "NOTAWORD" (should not exist)
    mov esi, lookup_str_4
    call vga_ser_write
    mov esi, lookup_str_4
    call dict_lookup
    test eax, eax
    jnz .dict_test_4_fail
    mov esi, msg_dict_notfound
    call vga_ser_write
    jmp .dict_tests_done
.dict_test_4_fail:
    mov esi, msg_dict_found
    call vga_ser_write
    mov ecx, eax
    mov esi, hex_buffer
    mov eax, ecx
    mov ecx, 8
    call hex_to_string
    mov esi, hex_buffer
    call vga_ser_write
    mov al, 10
    call vga_ser_emit
    
.dict_tests_done:
    ; Print completion message
    mov esi, msg_complete
    call vga_ser_write
ret
