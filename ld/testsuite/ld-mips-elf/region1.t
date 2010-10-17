MEMORY
{
  TEXTMEM : ORIGIN = 0x10000, LENGTH = 32K
  DATAMEM : ORIGIN = 0x20000, LENGTH = 32K
}

SECTIONS
{
  .text : { *(.text) } > TEXTMEM
  .data : { *(.data) } > DATAMEM
  .bss  : { *(.bss)  } > DATAMEM
}
