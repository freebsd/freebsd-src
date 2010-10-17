# If you change this file, please also look at files which source this one:
# elf64hppa.sh

SCRIPT_NAME=elf
ELFSIZE=64
# FIXME: this output format is for hpux.
OUTPUT_FORMAT="elf64-hppa-linux"
TEXT_START_ADDR=0x10000
TARGET_PAGE_SIZE=0x10000
MAXPAGESIZE=0x10000
ARCH=hppa
MACHINE=hppa2.0w
ENTRY="main"
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes

# We really want multiple .stub sections, one for each input .text section,
# but for now this is good enough.
OTHER_READONLY_SECTIONS="
  .PARISC.unwind ${RELOCATING-0} : { *(.PARISC.unwind) }"

# The PA64 ELF port treats .plt sections differently than most.  We also have
# to create a .opd section.  What most systems call the .got, we call the .dlt
OTHER_READWRITE_SECTIONS="
  .opd          ${RELOCATING-0} : { *(.opd) }
  ${RELOCATING+PROVIDE (__gp = .);}
  .plt          ${RELOCATING-0} : { *(.plt) }
  .dlt          ${RELOCATING-0} : { *(.dlt) }"

# The PA64 ELF port has two additional bss sections. huge bss and thread bss.
# Make sure they end up in the appropriate location.  We also have to set
# __TLS_SIZE to the size of the thread bss section.
OTHER_BSS_SECTIONS="
  .hbss         ${RELOCATING-0} : { *(.hbss) }
  .tbss         ${RELOCATING-0} : { *(.tbss) }
"
#OTHER_BSS_END_SYMBOLS='PROVIDE (__TLS_SIZE = SIZEOF (.tbss));'
OTHER_BSS_END_SYMBOLS='
  PROVIDE (__TLS_SIZE = 0);
  PROVIDE (__TLS_INIT_SIZE = 0);
  PROVIDE (__TLS_INIT_START = 0);
  PROVIDE (__TLS_INIT_A = 0);
  PROVIDE (__TLS_PREALLOC_DTV_A = 0);'

# HPs use .dlt where systems use .got.  Sigh.
OTHER_GOT_RELOC_SECTIONS="
  .rela.dlt     ${RELOCATING-0} : { *(.rela.dlt) }
  .rela.opd     ${RELOCATING-0} : { *(.rela.opd) }"

# We're not actually providing a symbol anymore (due to the inability to be
# safe in regards to shared libraries). So we just allocate the hunk of space
# unconditionally, but do not mess around with the symbol table.
DATA_START_SYMBOLS='. += 16;'

DATA_PLT=

# .dynamic should be at the start of the .text segment.
TEXT_DYNAMIC=
