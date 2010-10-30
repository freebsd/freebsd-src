SECTIONS {
	.text :
	  {
	    tmpdir/alignof.o (.text)
	  }
	.data : 
	  { 
	    tmpdir/alignof.o (.data)
	    LONG (ALIGNOF(.text))
	    LONG (ALIGNOF(.data))
	  }
}	

alignof_text = ALIGNOF(.text);
alignof_data = ALIGNOF(.data);
