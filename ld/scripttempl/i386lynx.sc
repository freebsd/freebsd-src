test -z "$ENTRY" && ENTRY=_start
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

ENTRY(${ENTRY})

SECTIONS
{
  .text ${RELOCATING+ SIZEOF_HEADERS} : {
    *(.init)
    *(.text)
    ${RELOCATING+ etext  =  .;}
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
    *(.fini)
    ${RELOCATING+ etext  =  .};
  }
  .data ${RELOCATING+ 0x400000 + (. & 0xffc00fff)} : {
    *(.data)
    ${RELOCATING+ edata  =  .};
  }
  .bss ${RELOCATING+ SIZEOF(.data) + ADDR(.data)} :
  { 					
    *(.bss)
    *(COMMON)
    ${RELOCATING+ end = .};
  }
  .stab  0 ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }
  .stabstr  0 ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }
}
EOF
