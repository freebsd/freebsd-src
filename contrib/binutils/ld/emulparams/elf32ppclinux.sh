TEMPLATE_NAME=elf32
# If you change this, please also look at:
# elf32ppc.sh elf32ppcsim.sh elf32lppc.sh elf32lppcsim.sh elf32ppclinux.sh
GENERATE_SHLIB_SCRIPT=yes
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-powerpc"
TEXT_START_ADDR=0x10000000
MAXPAGESIZE=0x10000
ARCH=powerpc
MACHINE=
BSS_PLT=
OTHER_RELOCATING_SECTIONS='
  /DISCARD/	: { *(.fixup) }
'
OTHER_READWRITE_SECTIONS='
  .got1		: { *(.got1) }
  .got2		: { *(.got2) }
'
