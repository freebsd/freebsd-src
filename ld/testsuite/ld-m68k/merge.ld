OUTPUT_FORMAT("elf32-m68k", "elf32-m68k",
              "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)

SECTIONS {
 
  .text : {_start = .; *(.text)}
}
