cat <<EOF
SECTIONS
{ 
    .text : 
    { 
	${GLD_STYLE+ CREATE_OBJECT_SYMBOLS}
	*(.text) 
	${RELOCATING+ _etext = .};
	${CONSTRUCTING+${COFF_CTORS}}
    }  
    .data :
    { 
 	*(.data) 
	${CONSTRUCTING+CONSTRUCTORS}
	${RELOCATING+ _edata = .};
    }  
    .bss :
    { 
	${RELOCATING+ _bss_start = .};
	*(.bss)	 
	*(COMMON) 
	${RELOCATING+ _end = .};
    } 
} 
EOF
