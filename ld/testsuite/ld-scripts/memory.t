MEMORY
{
  TEXTMEM (ARX) : ORIGIN = 0x100, LENGTH = 32K
  DATAMEM (AW)  : org = 0x1000, l = (64 * 1024)
}

SECTIONS
{
  . = 0;
  .text :
  {
    /* The value returned by the ORIGIN operator is a constant.
       However it is being assigned to a symbol declared within
       a section.  Therefore the symbol is section-relative and
       its value will include the offset of that section from
       the start of memory.  ie the declaration:
          text_start = ORIGIN (TEXTMEM);
       here will result in text_start having a value of 0x200.
       Hence we need to subtract the absolute value of the
       location counter at this point in order to give text_start
       a value that is truely absolute, and which coincidentally
       will allow the tests in script.exp to work.  */
 	
    text_start = ORIGIN(TEXTMEM) - ABSOLUTE (.);
    *(.text)
    *(.pr)
    text_end = .;
  } > TEXTMEM
  
  data_start = ORIGIN (DATAMEM);
  .data :
  {
    *(.data)
    *(.rw)
    data_end = .;
  } >DATAMEM

  fred = ORIGIN(DATAMEM) + LENGTH(DATAMEM);  
}
