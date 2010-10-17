cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

${RELOCATING+${LIB_SEARCH_DIRS}}
${STACKZERO+${RELOCATING+${STACKZERO}}}
SECTIONS
{
  ${RELOCATING+. = ${TEXT_START_ADDR};}
  .text :
  {
    CREATE_OBJECT_SYMBOLS
    *(.text)
    ${RELOCATING+etext = .;}
    ${RELOCATING+_etext = .;}
    ${RELOCATING+__etext = .;}
  }
  .data :
  {
    *(.rodata)
    *(.data)
    ${CONSTRUCTING+CONSTRUCTORS}
    ${RELOCATING+edata  =  .;}
    ${RELOCATING+_edata  =  .;}
    ${RELOCATING+__edata  =  .;}
  }
  .bss :
  {
   ${RELOCATING+ _bss_start = .};
   ${RELOCATING+ __bss_start = .};
   *(.bss)
   *(COMMON)
   ${RELOCATING+end = ALIGN(4) };
   ${RELOCATING+_end = ALIGN(4) };
   ${RELOCATING+__end = ALIGN(4) };
  }
}
EOF
