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
NO_SMALL_DATA=yes

if [ "x${host}" = "x${target}" ]; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      LIB_PATH=${libdir}
      for lib in ${NATIVE_LIB_DIRS}; do
	case :${LIB_PATH}: in
	  *:${lib}:*) ;;
	  *) LIB_PATH=${LIB_PATH}:${lib} ;;
	esac
      done

      # Linux modify the default library search path to first include
      # a 64-bit specific directory.
      case "$target" in
	x86_64*-linux*)
	  suffix=64 ;;
      esac

      # Look for 64 bit target libraries in /lib64, /usr/lib64 etc., first.
      if [ -n "$suffix" ]; then
	case "$EMULATION_NAME" in
	  *64*)
	    LIB_PATH=`echo ${LIB_PATH}: | sed -e s,:,$suffix:,g`$LIB_PATH ;;
	esac
      fi ;;
  esac
fi
