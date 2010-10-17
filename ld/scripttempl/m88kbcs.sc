# These are substituted in as variables in order to get '}' in a shell
# conditional expansion.
INIT='.init : { *(.init) }'
FINI='.fini : { *(.fini) }'
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})
ENTRY(__start)
${RELOCATING+${LIB_SEARCH_DIRS}}

SECTIONS 				
{ 					
  .text ${RELOCATING+ (0x20007 + SIZEOF_HEADERS) &~ 7} :
    {
      ${RELOCATING+ __.text.start = .};
      ${RELOCATING+ __.init.start = .};
      ${RELOCATING+ *(.init)}
      ${RELOCATING+ __.init.end = .};
      *(.text) 				
      ${RELOCATING+ __.tdesc_start = .};
      ${RELOCATING+ *(.tdesc)}
      ${RELOCATING+ __.text_end = .}	;
      ${RELOCATING+ __.initp.start = .};
      ${RELOCATING+ __.initp.end = .};
      ${RELOCATING+ __.fini_start = .};
      ${RELOCATING+ *(.fini) }
      ${RELOCATING+ __.fini_end = .};
      ${RELOCATING+_etext = .};
    }  					
  .data ${RELOCATING+ NEXT (0x400000) + ((SIZEOF(.text) + ADDR(.text)) % 0x2000)} :
    { 					
      *(.data)
      ${RELOCATING+_edata  =  .};
    }  					
  .bss ${RELOCATING+ SIZEOF(.data) + ADDR(.data)} :
    { 		
      *(.bss)	
      *(COMMON) 	
      ${RELOCATING+ _end = .};
      ${RELOCATING+ __end = .};
    }
  ${RELOCATING- ${INIT}}
  ${RELOCATING- ${FINI}}
  .comment  0 ${RELOCATING+(NOLOAD)} : 
  {
    *(.comment)
  }
}
EOF
