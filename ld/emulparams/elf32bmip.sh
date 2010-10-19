# If you change this file, please also look at files which source this one:
# elf32b4300.sh elf32bsmip.sh elf32btsmip.sh elf32ebmip.sh elf32lmip.sh

SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-bigmips"
BIG_OUTPUT_FORMAT="elf32-bigmips"
LITTLE_OUTPUT_FORMAT="elf32-littlemips"
TEXT_START_ADDR=0x0400000
test -n "${EMBEDDED}" || DATA_ADDR=0x10000000
MAXPAGESIZE=0x40000
COMMONPAGESIZE=0x1000
NONPAGED_TEXT_START_ADDR=0x0400000
SHLIB_TEXT_START_ADDR=0x5ffe0000
TEXT_DYNAMIC=
INITIAL_READONLY_SECTIONS="
  .reginfo      ${RELOCATING-0} : { *(.reginfo) }
"
OTHER_TEXT_SECTIONS='*(.mips16.fn.*) *(.mips16.call.*)'
# If the output has a GOT section, there must be exactly 0x7ff0 bytes
# between .got and _gp.  The ". = ." below stops the orphan code from
# inserting other sections between the assignment to _gp and the start
# of .got.
OTHER_GOT_SYMBOLS='
  . = .;
  _gp = ALIGN(16) + 0x7ff0;
'
OTHER_SDATA_SECTIONS="
  .lit8         ${RELOCATING-0} : { *(.lit8) }
  .lit4         ${RELOCATING-0} : { *(.lit4) }
"
TEXT_START_SYMBOLS='_ftext = . ;'
DATA_START_SYMBOLS='_fdata = . ;'
OTHER_BSS_SYMBOLS='_fbss = .;'
OTHER_SECTIONS='
  .gptab.sdata : { *(.gptab.data) *(.gptab.sdata) }
  .gptab.sbss : { *(.gptab.bss) *(.gptab.sbss) }
  .mdebug.abi32 : { KEEP(*(.mdebug.abi32)) }
  .mdebug.abiN32 : { KEEP(*(.mdebug.abiN32)) }
  .mdebug.abi64 : { KEEP(*(.mdebug.abi64)) }
  .mdebug.abiO64 : { KEEP(*(.mdebug.abiO64)) }
  .mdebug.eabi32 : { KEEP(*(.mdebug.eabi32)) }
  .mdebug.eabi64 : { KEEP(*(.mdebug.eabi64)) }
  .gcc_compiled_long32 : { KEEP(*(.gcc_compiled_long32)) }
  .gcc_compiled_long64 : { KEEP(*(.gcc_compiled_long64)) }
'
ARCH=mips
MACHINE=
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
