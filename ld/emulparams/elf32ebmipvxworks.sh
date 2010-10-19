. ${srcdir}/emulparams/elf32bmip.sh

OUTPUT_FORMAT="elf32-bigmips-vxworks"
BIG_OUTPUT_FORMAT="elf32-bigmips-vxworks"
LITTLE_OUTPUT_FORMAT="elf32-littlemips-vxworks"
unset OTHER_GOT_SYMBOLS
SHLIB_TEXT_START_ADDR=0
unset TEXT_DYNAMIC
unset DATA_ADDR

. ${srcdir}/emulparams/vxworks.sh
