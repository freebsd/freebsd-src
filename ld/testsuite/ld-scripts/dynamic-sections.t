SECTIONS
{
  .data : { *(.data) }
  .rodata : { *(.rodata) }

  /* The .rel* sections are typically placed here, because of the way
     elf32.em handles orphaned sections.  A bug introduced on 2002-06-10
     would cause . to be 0 at this point.  */

  _bar = ASSERT (. > 0, "Bad . value");
}
