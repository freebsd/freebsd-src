SECTIONS
{
  .text 0x01014000 :
  { 
	*(.text)   
	foo = (. + 0x4000);
  }
}


