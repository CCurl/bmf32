/* Linker script for bare metal OS */

ENTRY(kernel_main)

SECTIONS
{
    /* Start at 1MB (standard location for kernel) */
    . = 0x100000;

    /* Text section */
    .text : {
        *(.multiboot)  /* Multiboot header must be first */
        *(.text)
    }

    /* Read-only data section */
    .rodata : {
        *(.rodata)
    }

    /* Data section */
    .data : {
        *(.data)
    }

    /* BSS section (uninitialized data) */
    .bss : {
        *(.bss)
    }
}
