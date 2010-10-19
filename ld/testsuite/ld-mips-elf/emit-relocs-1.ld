ENTRY(_start)
SECTIONS
{
  . = 0x80000;
  .text : { *(.text) }

  . = ALIGN (0x400);
  .merge1 : { *(.merge1) }

  . = ALIGN (0x400);
  .merge2 : { *(.merge2) }

  . = ALIGN (0x400);
  .merge3 : { *(.merge3) }

  . = ALIGN (0x400);
  .data : { *(.data) }

  /DISCARD/ : { *(*) }
}
