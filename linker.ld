/* Linker script for 32-bit bare metal kernel (pure assembly) */

ENTRY(_start)

SECTIONS
{
    . = 0x100000;
    
    .text ALIGN(0x1000) : {
        *(.multiboot)
        *(.text*)
    }
    
    .rodata ALIGN(0x1000) : {
        *(.rodata*)
    }
    
    .data ALIGN(0x1000) : {
        *(.data*)
    }
    
    .bss ALIGN(0x1000) : {
        *(COMMON)
        *(.bss*)
    }
    
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu_debugaltlink)
    }
}
