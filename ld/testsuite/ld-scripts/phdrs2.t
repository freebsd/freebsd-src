PHDRS
{
  Foo PT_LOAD ;
  Bar PT_LOAD ;
}

SECTIONS
{
  . = 0x800000 - 1;
  /* The PHDRS generated should start at the aligned .foo section
     address, not the unaligned .empty section address */
  .empty : { 
	EMPTY_START = ABSOLUTE(.) ;
	*(.empty) 
	EMPTY_END = ABSOLUTE(.) ;
	} : Foo
  .foo : { *(.foo) } : Foo
  .bar : { *(.bar)
	LONG(EMPTY_START) ;
	 } : Bar
	
  /DISCARD/ : { *(.*) }
}
