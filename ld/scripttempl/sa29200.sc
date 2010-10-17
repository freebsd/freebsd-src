cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
ENTRY(start)

SECTIONS {
  .text ${RELOCATING+${TEXT_START_ADDR}} : 
    {
	*(.text);
	*(.text1);
	*(.text2);
	${RELOCATING+_etext = .};
    }
  .lit ALIGN(4) :
   {
	*(.lit);
	${RELOCATING+_elit = .};
   }
  .data ALIGN(4) :
    {
	*(.data);
	*(.data1);
	*(.data2);
	${RELOCATING+_edata = .};
	${CONSTRUCTING+CONSTRUCTORS}
	${CONSTRUCTING+ ___CTOR_LIST__ = .;}
	${CONSTRUCTING+ LONG((___CTOR_END__ - ___CTOR_LIST__) / 4 - 2)}
	${CONSTRUCTING+ *(.ctors)}
	${CONSTRUCTING+ LONG(0)}
	${CONSTRUCTING+ ___CTOR_END__ = .;}
	${CONSTRUCTING+ ___DTOR_LIST__ = .;}
	${CONSTRUCTING+ LONG((___DTOR_END__ - ___DTOR_LIST__) / 4 - 2)}
	${CONSTRUCTING+ *(.dtors)}
	${CONSTRUCTING+ LONG(0)}
	${CONSTRUCTING+ ___DTOR_END__ = .;}
    }

  .bss  ALIGN(4) :
    { 
	*(COMMON) 	
	*(.bss)
	${RELOCATING+_end = .};
    } 
}
EOF
