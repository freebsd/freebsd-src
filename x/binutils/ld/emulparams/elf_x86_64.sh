SCRIPT_NAME=elf
ELFSIZE=64
OUTPUT_FORMAT="elf64-x86-64"
TEXT_START_ADDR=0x400000
MAXPAGESIZE=0x100000
COMMONPAGESIZE=0x1000
NONPAGED_TEXT_START_ADDR=0x400000
ARCH="i386:x86-64"
MACHINE=
NOP=0x90909090
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
NO_SMALL_DATA=yes

if [ "x${host}" = "x${target}" ]; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      NATIVE=yes
  esac
fi

# Linux modify the default library search path to first include
# a 64-bit specific directory.
case "$target" in
  x86_64*-linux*)
    case "$EMULATION_NAME" in
      *64*) LIBPATH_SUFFIX=64 ;;
    esac
    ;;
esac
