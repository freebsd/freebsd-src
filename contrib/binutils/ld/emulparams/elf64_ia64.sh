# See genscripts.sh and ../scripttempl/elf.sc for the meaning of these.
SCRIPT_NAME=elf
ELFSIZE=64
TEMPLATE_NAME=elf32
OUTPUT_FORMAT="elf64-ia64-little"
ARCH=ia64
MACHINE=
MAXPAGESIZE=0x10000
TEXT_START_ADDR="0x4000000000000000"
DATA_ADDR="0x6000000000000000 + (. & (${MAXPAGESIZE} - 1))"
GENERATE_SHLIB_SCRIPT=yes
NOP=0x00300000010070000002000001000400  # a bundle full of nops
OTHER_GOT_SECTIONS='.IA_64.pltoff : { *(.IA_64.pltoff) }'
OTHER_PLT_RELOC_SECTIONS='.rela.IA_64.pltoff : { *(.rela.IA_64.pltoff) }'
OTHER_READONLY_SECTIONS='.opd : { *(.opd) }  .IA_64.unwind_info : { *(.IA_64.unwind_info*) }  .IA_64.unwind : { *(.IA_64.unwind*) }'
