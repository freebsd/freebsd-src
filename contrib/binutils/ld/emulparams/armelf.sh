MACHINE=
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-littlearm"
BIG_OUTPUT_FORMAT="elf32-bigarm"
LITTLE_OUTPUT_FORMAT="elf32-littlearm"
TEXT_START_ADDR=0x8000
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=armelf
OTHER_TEXT_SECTIONS='*(.glue_7t) *(.glue_7)'
OTHER_BSS_SYMBOLS='__bss_start__ = .;'
OTHER_BSS_END_SYMBOLS='_bss_end__ = . ; __bss_end__ = . ; __end__ = . ;'

GENERATE_SHLIB_SCRIPT=yes

ARCH=arm
MACHINE=
MAXPAGESIZE=256
ENTRY=_start
EMBEDDED=yes

# Hmmm, there's got to be a better way.  This sets the stack to the
# top of the simulator memory (2^19 bytes).
OTHER_RELOCATING_SECTIONS='.stack 0x80000 : { _stack = .; *(.stack) }'
