cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

${RELOCATING+${LIB_SEARCH_DIRS}}
${RELOCATING+__DYNAMIC  =  0;}
${STACKZERO+${RELOCATING+${STACKZERO}}}
${SHLIB_PATH+${RELOCATING+${SHLIB_PATH}}}
SECTIONS
{
  .text ${RELOCATING+${TEXT_START_ADDR}}:
  {
    CREATE_OBJECT_SYMBOLS
    *(.text)
    ${PAD_TEXT+${RELOCATING+. = ${DATA_ALIGNMENT};}}
    ${RELOCATING+_etext = ${DATA_ALIGNMENT};}
    ${RELOCATING+__etext = ${DATA_ALIGNMENT};}
  }
  .data  ${RELOCATING+${DATA_ALIGNMENT}} :
  {
    *(.data)
    ${CONSTRUCTING+CONSTRUCTORS}
    ${RELOCATING+_edata  =  .;}
    ${RELOCATING+__edata =  .;}
  }
  .bss ${RELOCATING+SIZEOF(.data) + ADDR(.data)} :
  {
   ${RELOCATING+ __bss_start = .};
   *(.bss)
   *(COMMON)
   ${RELOCATING+_end = ALIGN(4) };
   ${RELOCATING+__end = ALIGN(4) };
  }
}
EOF
