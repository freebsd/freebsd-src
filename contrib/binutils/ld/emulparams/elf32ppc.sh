# If you change this file, please also look at files which source this one:
# elf32lppc.sh elf32ppclinux.sh elf32ppcsim.sh

TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-powerpc"
TEXT_START_ADDR=0x01800000
MAXPAGESIZE=0x10000
ARCH=powerpc
MACHINE=
BSS_PLT=
EXECUTABLE_SYMBOLS='PROVIDE (__stack = 0); PROVIDE (___stack = 0);'
OTHER_BSS_END_SYMBOLS='__end = .;'
OTHER_READWRITE_SECTIONS="
  .fixup        ${RELOCATING-0} : { *(.fixup) }
  .got1         ${RELOCATING-0} : { *(.got1) }
  .got2         ${RELOCATING-0} : { *(.got2) }
"
