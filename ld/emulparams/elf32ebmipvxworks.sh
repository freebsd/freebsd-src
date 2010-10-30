. ${srcdir}/emulparams/elf32bmip.sh

OUTPUT_FORMAT="elf32-bigmips-vxworks"
BIG_OUTPUT_FORMAT="elf32-bigmips-vxworks"
LITTLE_OUTPUT_FORMAT="elf32-littlemips-vxworks"
# VxWorks .rdata sections are normally read-only, but one of the objects
# in libdl.a (the dynamic loader) is actually read-write.  Explicitly
# place the section in the appropriate segment for its flags.
OTHER_READONLY_SECTIONS="
  .rdata ${RELOCATING-0} : ONLY_IF_RO { *(.rdata) }
"
OTHER_READWRITE_SECTIONS="
  .rdata ${RELOCATING-0} : ONLY_IF_RW { *(.rdata) }
"
unset OTHER_GOT_SYMBOLS
SHLIB_TEXT_START_ADDR=0
unset TEXT_DYNAMIC
unset DATA_ADDR

. ${srcdir}/emulparams/vxworks.sh
