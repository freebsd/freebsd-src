OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
PHDRS {
 text PT_LOAD FLAGS(5); /* R_E */
}
SECTIONS
{
  . = 0xC0000000 + ((0x100000 + (0x100000 - 1)) & ~(0x100000 - 1));
  .bar : AT(ADDR(.bar) - 0xC0000000) { *(.bar) } :text
  .bss : AT(ADDR(.bss) - 0xC0000000) { *(.bss) }
  .foo 0 : AT(ADDR(.bss) + SIZEOF(.bss) - 0xC0000000) { *(.foo) } :text
  /DISCARD/ : { *(.*) }
}
