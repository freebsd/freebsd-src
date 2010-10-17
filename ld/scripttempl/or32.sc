cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

SECTIONS
{
  .text : {
    *(.text)
    ${RELOCATING+ __etext  =  .};
    ${CONSTRUCTING+ __CTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)}
    ${CONSTRUCTING+ *(.ctors)}
    ${CONSTRUCTING+ LONG(0)}
    ${CONSTRUCTING+ __CTOR_END__ = .;}
    ${CONSTRUCTING+ __DTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)}
    ${CONSTRUCTING+ *(.dtors)}
    ${CONSTRUCTING+ LONG(0)}
    ${CONSTRUCTING+ __DTOR_END__ = .;}
    *(.lit)
    *(.shdata)
  }
  .shbss SIZEOF(.text) + ADDR(.text) :	{
    *(.shbss)
  } 
  .data  : {
    *(.data)
    ${RELOCATING+ __edata  =  .};
  }
  .bss   SIZEOF(.data) + ADDR(.data) :
  { 					
   *(.bss)
   *(COMMON)
     ${RELOCATING+ __end = ALIGN(0x8)};
  }
}
EOF
