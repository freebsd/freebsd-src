ARCH=arm
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-littlearm"
BIG_OUTPUT_FORMAT="elf32-bigarm"
LITTLE_OUTPUT_FORMAT="elf32-littlearm"
MAXPAGESIZE=0x8000
TEMPLATE_NAME=elf32
EXTRA_EM_FILE=armelf
GENERATE_SHLIB_SCRIPT=yes

DATA_START_SYMBOLS='__data_start = . ;';
OTHER_TEXT_SECTIONS='*(.glue_7t) *(.glue_7)'
OTHER_BSS_SYMBOLS='__bss_start__ = .;'
OTHER_BSS_END_SYMBOLS='_bss_end__ = . ; __bss_end__ = . ; __end__ = . ;'

TEXT_START_ADDR=0x00008000
