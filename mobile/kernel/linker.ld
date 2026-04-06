OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    .text : {
        *(.text.entry)
        *(.text)
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        bss_start = .;
        *(.bss)
        bss_end = .;
    }

    . = ALIGN(8);
}