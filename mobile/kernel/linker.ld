OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* Kernel loaded at 0x8020_0000 in physical memory */
    . = 0x80200000;

    .text.entry : {
        *(.text.entry)
    }

    .text : {
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(0x1000);
}
