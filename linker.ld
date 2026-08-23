/* Linker script for bare metal OS */

ENTRY(kernel_main)

SECTIONS
{
    /* Start at 1MB (standard location for kernel) */
    . = 0x100000;

    /* Flat kernel image: all sections are RWX */
    .text : {
        *(.multiboot)  /* Multiboot header must be first */
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
