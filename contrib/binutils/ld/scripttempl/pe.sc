# Linker script for PE.

cat <<EOF
OUTPUT_FORMAT(${OUTPUT_FORMAT})
${LIB_SEARCH_DIRS}

ENTRY(_mainCRTStartup)

SECTIONS
{
  .text ${RELOCATING+ __image_base__ + __section_alignment__ } : 
  {
    ${RELOCATING+ *(.init)}
    *(.text)
    ${CONSTRUCTING+ ___CTOR_LIST__ = .; __CTOR_LIST__ = . ; 
			LONG (-1); *(.ctors); *(.ctor); LONG (0); }
    ${CONSTRUCTING+ ___DTOR_LIST__ = .; __DTOR_LIST__ = . ; 
			LONG (-1); *(.dtors); *(.dtor);  LONG (0); }
    ${RELOCATING+ *(.fini)}
    /* ??? Why is .gcc_exc here?  */
    ${RELOCATING+ *(.gcc_exc)}
    ${RELOCATING+ etext = .;}
    /* Grouped section support currently must be explicitly provided for
	in the linker script.  */
    *(.text\$)
    *(.gcc_except_table)
  }

  .bss BLOCK(__section_alignment__)  :
  {
    __bss_start__ = . ;
    *(.bss)
    *(COMMON)
    __bss_end__ = . ;
  }
  .data BLOCK(__section_alignment__) : 
  {
    __data_start__ = . ; 
    *(.data)
    *(.data2)
    __data_end__ = . ; 
    /* Grouped section support currently must be explicitly provided for
	in the linker script.  */
    *(.data\$)
  }

  .rdata BLOCK(__section_alignment__) :
  {
    *(.rdata)
    /* Grouped section support currently must be explicitly provided for
	in the linker script.  */
    *(.rdata\$)
  }

  .edata BLOCK(__section_alignment__) :
  {
    *(.edata)
  }

  /DISCARD/ BLOCK(__section_alignment__) :
  {
    *(.debug\$S)
    *(.debug\$T)
    *(.debug\$F)
    *(.drectve)
  }

  .idata BLOCK(__section_alignment__) :
  {
    /* This cannot currently be handled with grouped sections.
	See pe.em:sort_sections.  */
    *(.idata\$2)
    *(.idata\$3)
    *(.idata\$4)
    *(.idata\$5)
    *(.idata\$6)
    *(.idata\$7)
  }
  .CRT BLOCK(__section_alignment__) :
  { 					
    /* Grouped sections are used to handle .CRT\$foo.  */
    *(.CRT\$)
  }
  .rsrc BLOCK(__section_alignment__) :
  { 					
    /* Grouped sections are used to handle .rsrc\$0[12].  */
    *(.rsrc\$)
  }

  .endjunk BLOCK(__section_alignment__) :
  {
    /* end is deprecated, don't use it */
    ${RELOCATING+ end = .;}
    ${RELOCATING+ __end__ = .;}
  }

  .stab BLOCK(__section_alignment__)  ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }

  .stabstr BLOCK(__section_alignment__) ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }

  .reloc BLOCK(__section_alignment__) :
  { 					
    *(.reloc)
  }
}
EOF
