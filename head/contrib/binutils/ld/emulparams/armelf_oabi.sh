MACHINE=
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-littlearm-oabi"
BIG_OUTPUT_FORMAT="elf32-bigarm-oabi"
LITTLE_OUTPUT_FORMAT="elf32-littlearm-oabi"
TEXT_START_ADDR=0x8000
TEMPLATE_NAME=armelf_oabi
OTHER_TEXT_SECTIONS='*(.glue_7t) *(.glue_7)'
OTHER_BSS_SYMBOLS='__bss_start__ = .;'
OTHER_BSS_END_SYMBOLS='_bss_end__ = . ; __bss_end__ = . ;'
 
 
ARCH=arm
MACHINE=
MAXPAGESIZE=256
ENTRY=_start
EMBEDDED=yes
 
# This sets the stack to the top of the simulator memory (2^19 bytes).
STACK_ADDR=0x80000

# ARM does not support .s* sections.
NO_SMALL_DATA=yes
