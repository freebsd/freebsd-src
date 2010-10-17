# If you change this file, please also look at files which source this one:
# shelf_linux.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-sh-linux"
TEXT_START_ADDR=0x400000
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x1000
ARCH=sh
MACHINE=
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes

DATA_START_SYMBOLS='__data_start = . ;';

OTHER_READWRITE_SECTIONS="
  .note.ABI-tag ${RELOCATING-0} : { *(.note.ABI-tag) }"
