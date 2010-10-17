cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
ENTRY(start)

SECTIONS {
  .text ${RELOCATING+${TEXT_START_ADDR}} : 
    {
	*(.text);
	${RELOCATING+_etext = .};
    }
  data ${RELOCATING+0x80002000} :
    {
	*(.data);
	*(.mstack); 
	*(.shbss);
	*(.rstack);
	*(.mstack);
	${CONSTRUCTING+CONSTRUCTORS}
    }
  .bss  . :
    { 
	*(COMMON) 	
	*(.bss);
	${RELOCATING+_end = .};
    } 
}
EOF
