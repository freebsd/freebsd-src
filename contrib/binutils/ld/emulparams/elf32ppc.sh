# If you change this file, please also look at files which source this one:
# elf32lppc.sh elf32ppclinux.sh elf32ppcsim.sh

TEMPLATE_NAME=elf32
EXTRA_EM_FILE=ppc32elf
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-powerpc"
TEXT_START_ADDR=0x01800000
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x1000
ARCH=powerpc:common
MACHINE=
BSS_PLT=
EXECUTABLE_SYMBOLS='PROVIDE (__stack = 0); PROVIDE (___stack = 0);'
OTHER_BSS_END_SYMBOLS='__end = .;'
OTHER_READWRITE_SECTIONS="
  .fixup        ${RELOCATING-0} : { *(.fixup) }
  .got1         ${RELOCATING-0} : { *(.got1) }
  .got2         ${RELOCATING-0} : { *(.got2) }
"
OTHER_GOT_RELOC_SECTIONS="
  .rela.got1         ${RELOCATING-0} : { *(.rela.got1) }
  .rela.got2         ${RELOCATING-0} : { *(.rela.got2) }
"

# Treat a host that matches the target with the possible exception of "64"
# in the name as if it were native.
if test `echo "$host" | sed -e s/64//` = `echo "$target" | sed -e s/64//`; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      NATIVE=yes
      ;;
  esac
fi

# Look for 64 bit target libraries in /lib64, /usr/lib64 etc., first.
case "$EMULATION_NAME" in
  *64*) LIBPATH_SUFFIX=64 ;;
esac
