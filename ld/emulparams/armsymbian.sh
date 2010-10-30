. ${srcdir}/emulparams/armelf.sh
SCRIPT_NAME="armbpabi"
GENERATE_COMBRELOC_SCRIPT=1
OUTPUT_FORMAT="elf32-littlearm-symbian"
BIG_OUTPUT_FORMAT="elf32-bigarm-symbian"
LITTLE_OUTPUT_FORMAT="$OUTPUT_FORMAT"
TARGET1_IS_REL=1
TARGET2_TYPE=abs
# On BPABI systems, program headers should not be mapped.
EMBEDDED=yes

# As for armelf.sh, but add the SymbianOS-specific
# .ARM.exidx$${Base,Limit} symbols.
OTHER_READONLY_SECTIONS="
  .ARM.extab ${RELOCATING-0} : { *(.ARM.extab${RELOCATING+* .gnu.linkonce.armextab.*}) }
  ${RELOCATING+ .ARM.exidx\$\$Base = . ; }
  ${RELOCATING+ __exidx_start = .; }
  .ARM.exidx ${RELOCATING-0} : { *(.ARM.exidx${RELOCATING+* .gnu.linkonce.armexidx.*}) }
  ${RELOCATING+ __exidx_end = .; }
  ${RELOCATING+ .ARM.exidx\$\$Limit = . ; }"

MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
