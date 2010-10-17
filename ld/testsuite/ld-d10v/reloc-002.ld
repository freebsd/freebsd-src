SECTIONS
{
  .text 0x01014000 :
  { 
	*(.text)   
	foo = (. + 0x1F8);
  }
}


