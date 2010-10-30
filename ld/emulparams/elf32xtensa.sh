SCRIPT_NAME=elfxtensa
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=xtensaelf
OUTPUT_FORMAT=undefined
BIG_OUTPUT_FORMAT="elf32-xtensa-be"
LITTLE_OUTPUT_FORMAT="elf32-xtensa-le"
TEXT_START_ADDR=0x400000
NONPAGED_TEXT_START_ADDR=0x400000
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
ARCH=xtensa
MACHINE=
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
GENERATE_COMBRELOC_SCRIPT=yes
NO_SMALL_DATA=yes
TEXT_PLT=yes
PLT="/* .plt* sections are embedded in .text */"
GOT=".got          ${RELOCATING-0} : { *(.got) }"
OTHER_READONLY_SECTIONS="
  .got.loc      ${RELOCATING-0} : { *(.got.loc) }
  .xt_except_table ${RELOCATING-0} : ONLY_IF_RO { KEEP (*(.xt_except_table${RELOCATING+ .xt_except_table.* .gnu.linkonce.e.*})) }
"
OTHER_RELRO_SECTIONS="
  .xt_except_table ${RELOCATING-0} : ONLY_IF_RW { KEEP (*(.xt_except_table${RELOCATING+ .xt_except_table.* .gnu.linkonce.e.*})) }
"
OTHER_READWRITE_SECTIONS="
  .xt_except_desc ${RELOCATING-0} :
  {
    *(.xt_except_desc${RELOCATING+ .xt_except_desc.* .gnu.linkonce.h.*})
    ${RELOCATING+*(.xt_except_desc_end)}
  }
"
OTHER_SDATA_SECTIONS="
  .lit4         ${RELOCATING-0} :
  {
    ${RELOCATING+PROVIDE (_lit4_start = .);}
    *(.lit4${RELOCATING+ .lit4.* .gnu.linkonce.lit4.*})
    ${RELOCATING+PROVIDE (_lit4_end = .);}
  }
"
OTHER_SECTIONS="
  .xt.lit       0 : { KEEP (*(.xt.lit${RELOCATING+ .xt.lit.* .gnu.linkonce.p.*})) }
  .xt.insn      0 : { KEEP (*(.xt.insn${RELOCATING+ .gnu.linkonce.x.*})) }
  .xt.prop      0 : { KEEP (*(.xt.prop${RELOCATING+ .xt.prop.* .gnu.linkonce.prop.*})) }
"
