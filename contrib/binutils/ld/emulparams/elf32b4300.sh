SCRIPT_NAME=elfmips
OUTPUT_FORMAT="elf32-bigmips"
BIG_OUTPUT_FORMAT="elf32-bigmips"
LITTLE_OUTPUT_FORMAT="elf32-littlemips"
TEXT_START_ADDR=0xa0020000
DATA_ADDR=.
MAXPAGESIZE=0x40000
OTHER_READONLY_SECTIONS='.reginfo : { *(.reginfo) }'
OTHER_GOT_SYMBOLS='
  _gp = ALIGN(16) + 0x7ff0;
'
OTHER_READWRITE_SECTIONS='
  .lit8 : { *(.lit8) }
  .lit4 : { *(.lit4) }
'
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'
EXECUTABLE_SYMBOLS='_DYNAMIC_LINK = 0;'
OTHER_SECTIONS='
  .gptab.sdata : { *(.gptab.data) *(.gptab.sdata) }
  .gptab.sbss : { *(.gptab.bss) *(.gptab.sbss) }
'
ARCH=mips
MACHINE=4000
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
DYNAMIC_LINK=false
EMBEDDED=yes
