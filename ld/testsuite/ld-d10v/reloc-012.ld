SECTIONS
{
  .text 0x01014000 :
  { 
	foo = .;
	. = (. + 0x80004);
	*(.text)   
  }
}


