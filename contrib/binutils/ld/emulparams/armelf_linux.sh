ARCH=arm
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-littlearm"
BIG_OUTPUT_FORMAT="elf32-bigarm"
LITTLE_OUTPUT_FORMAT="elf32-littlearm"
MAXPAGESIZE=0x8000
TEMPLATE_NAME=armelf
GENERATE_SHLIB_SCRIPT=yes

DATA_START_SYMBOLS='__data_start = . ;';
OTHER_TEXT_SECTIONS='*(.glue_7t) *(.glue_7)'
OTHER_BSS_SYMBOLS='__bss_start__ = .;'
OTHER_BSS_END_SYMBOLS='_bss_end__ = . ; __bss_end__ = . ; __end__ = . ;'

# This needs to be high enough so that we can load ld.so below it,
# yet low enough to stay away from the mmap area at 0x40000000.
# Also, it is small enough so that relocs which are pointing
# at absolute 0 will still be fixed up.
TEXT_START_ADDR=0x02000000
