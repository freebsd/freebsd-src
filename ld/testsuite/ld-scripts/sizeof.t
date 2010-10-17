SECTIONS {
	.text :
	  {
	    text_start = .;
	    tmpdir/sizeof.o 
	    text_end = .;
	  }
	.data : 
	  { 
	    data_start = .;
	    . = . + SIZEOF(.text);
	    data_end = .;
	  }
}	

sizeof_text = SIZEOF(.text);
sizeof_data = SIZEOF(.data);
