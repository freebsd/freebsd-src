DATA_ADDR=0x40000000
test "$LD_FLAG" = "N" && DATA_ADDR=.
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})
ENTRY("\$START\$")
${RELOCATING+${LIB_SEARCH_DIRS}}
SECTIONS
{
  .text 0x1000 ${RELOCATING++${TEXT_START_ADDR}}:
  {
    ${RELOCATING+__text_start = .};
    CREATE_OBJECT_SYMBOLS
    *(.PARISC.stubs)
    *(.text)
    ${RELOCATING+etext = .};
    ${RELOCATING+_etext = .};
  }
  ${RELOCATING+. = ${DATA_ADDR};}
  .data :
  {
    ${RELOCATING+ . = . + 0x1000 };
    ${RELOCATING+__data_start = .};
    *(.data)
    ${CONSTRUCTING+CONSTRUCTORS}
    ${RELOCATING+edata = .};
    ${RELOCATING+_edata = .};
  }
  ${RELOCATING+. = ${DATA_ADDR} + SIZEOF(.data);}
  .bss :
  {
   *(.bss)
   *(COMMON)
   ${RELOCATING+end = . };
   ${RELOCATING+_end = . };
  }
}
EOF
