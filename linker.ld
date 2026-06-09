/* Linker script for FWC bare metal 32-bit kernel
 * Positions kernel at 1MB (0x100000) standard QEMU/multiboot load address
 */

ENTRY(start)

SECTIONS
{
    . = 0x100000;       /* Load kernel at 1MB */
    
    __bootstart = .;
    
    .boot ALIGN(0x1000) : {
        *(.boot)        /* Multiboot header */
    }
    
    .text ALIGN(0x1000) : {
        *(.text)
        *(.text.*)
    }
    
    .rodata ALIGN(0x1000) : {
        *(.rodata)
        *(.rodata.*)
    }
    
    .data ALIGN(0x1000) : {
        *(.data)
        *(.data.*)
    }
    
    __bootend = .;
    
    .bss ALIGN(0x1000) : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu_debugdata)
    }
}
