cat <<EOF
OUTPUT_FORMAT(${OUTPUT_FORMAT})
OUTPUT_ARCH(${ARCH})
${RELOCATING+${LIB_SEARCH_DIRS}}

SECTIONS
{
  .text ${RELOCATING:-0} ${RELOCATING+${TEXT_START_ADDR}} : {
    ${RELOCATING+ start = DEFINED(_START) ? _START : DEFINED(_start) ? _start : .;}
    ${RELOCATING+ PROVIDE(__text = .);}
    *(.text);
    *(code);
    *(const);
    *(strings);
    *(pSOS);
    *(pROBE);
    *(pNA);
    *(pHILE);
    *(pREPC);
    *(pRPC);
    ${CONSTRUCTING+ ___CTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((___CTOR_END__ - ___CTOR_LIST__) / 4 - 2)}
    ${CONSTRUCTING+ *(.ctors)}
    ${CONSTRUCTING+ LONG(0);}
    ${CONSTRUCTING+ ___CTOR_END__ = .;}
    ${CONSTRUCTING+ ___DTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG((___DTOR_END__ - ___DTOR_LIST__) / 4 - 2);}
    ${CONSTRUCTING+ *(.dtors);}
    ${CONSTRUCTING+ LONG(0);}
    ${CONSTRUCTING+ ___DTOR_END__ = .;}
    ${RELOCATING+ PROVIDE(__etext = .);}
    ${RELOCATING+ PROVIDE(_etext = .);}
  }
  .data ${RELOCATING:-0} : ${RELOCATING+ AT(ADDR(.text) + SIZEOF(.text))} {
    ${RELOCATING+ PROVIDE(__data = .);}
    *(.data);
    *(vars);
    ${RELOCATING+ PROVIDE(__edata = .);}
    ${RELOCATING+ PROVIDE(_edata = .);}
  }
  .bss ${RELOCATING:-0} :
  { 					
    ${RELOCATING+ PROVIDE(__bss = .);}
    *(.bss);
    *(zerovars);
    *(COMMON);
    ${RELOCATING+ PROVIDE(__ebss = .);}
    ${RELOCATING+ PROVIDE(__end = .);}
    ${RELOCATING+ PROVIDE(_end = .);}
    ${RELOCATING+ PROVIDE(_FreeMemStart = .);}
  }
  .stab 0 ${RELOCATING+(NOLOAD)} : 
  {
    *(.stab);
  }
  .stabstr 0 ${RELOCATING+(NOLOAD)} :
  {
    *(.stabstr);
  }
}
EOF
