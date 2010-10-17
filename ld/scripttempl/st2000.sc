cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})


SECTIONS 				
{ 					
.text :
	{ 					
	  *(.text) 				
	  *(.strings)
   	  _etext = .;
	*(.data)
	_edata = .;
	*(.bss)
	*(COMMON)
	 _end = .;

}

}
EOF




