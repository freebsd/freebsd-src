# First set some configuration-specific variables
. ${srcdir}/emulparams/xtensa-config.sh

# See genscripts.sh and ../scripttempl/elfxtensa.sc for the meaning of these.
SCRIPT_NAME=elfxtensa
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=xtensaelf
OUTPUT_FORMAT=undefined
BIG_OUTPUT_FORMAT="elf32-xtensa-be"
LITTLE_OUTPUT_FORMAT="elf32-xtensa-le"
TEXT_START_ADDR=0x400000
NONPAGED_TEXT_START_ADDR=0x400000
ARCH=xtensa
MACHINE=
GENERATE_SHLIB_SCRIPT=yes
GENERATE_COMBRELOC_SCRIPT=yes
NO_SMALL_DATA=yes
PLT="/* .plt* sections are embedded in .text */"
GOT=".got          ${RELOCATING-0} : { *(.got) }"
OTHER_READONLY_SECTIONS="
  .got.loc      ${RELOCATING-0} : { *(.got.loc) }
  .xt_except_table ${RELOCATING-0} : { KEEP (*(.xt_except_table)) }
"
OTHER_READWRITE_SECTIONS="
  .xt_except_desc ${RELOCATING-0} :
  {
    *(.xt_except_desc${RELOCATING+ .gnu.linkonce.h.*})
    ${RELOCATING+*(.xt_except_desc_end)}
  }
"
OTHER_SECTIONS="
  .xt.lit         0 : { *(.xt.lit${RELOCATING+ .xt.lit.* .gnu.linkonce.p.*}) }
  .xt.insn        0 : { *(.xt.insn${RELOCATING+ .gnu.linkonce.x.*}) }
  .xt.prop        0 : { *(.xt.prop${RELOCATING+ .gnu.linkonce.prop.*}) }
"
