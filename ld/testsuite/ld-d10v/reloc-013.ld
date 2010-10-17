SECTIONS
{
  .text 0x01014000 :
  { 
	foo = .;
	. = (. + 0x400);
	*(.text)   
  }
}



