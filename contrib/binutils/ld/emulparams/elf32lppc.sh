TEMPLATE_NAME=elf32
# If you change this, please also look at:
# elf32ppc.sh elf32ppcsim.sh elf32lppc.sh elf32lppcsim.sh elf32ppclinux.sh
GENERATE_SHLIB_SCRIPT=yes
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-powerpcle"
TEXT_START_ADDR=0x01800000
MAXPAGESIZE=0x10000
ARCH=powerpc
MACHINE=
BSS_PLT=
EXECUTABLE_SYMBOLS='PROVIDE (__stack = 0); PROVIDE (___stack = 0);'
OTHER_BSS_END_SYMBOLS='__end = .;'
OTHER_READWRITE_SECTIONS='
  .fixup	: { *(.fixup) }
  .got1		: { *(.got1) }
  .got2		: { *(.got2) }
'
