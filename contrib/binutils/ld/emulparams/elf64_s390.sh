SCRIPT_NAME=elf
ELFSIZE=64
OUTPUT_FORMAT="elf64-s390"
TEXT_START_ADDR=0x80000000
MAXPAGESIZE=0x1000
NONPAGED_TEXT_START_ADDR=0x80000000
ARCH="s390:64-bit"
MACHINE=
NOP=0x07070707
TEMPLATE_NAME=elf32
GENERATE_SHLIB_SCRIPT=yes 

# Treat a host that matches the target with the possible exception of "x"
# in the name as if it were native.
if test `echo "$host" | sed -e s/390x/390/` \
   = `echo "$target" | sed -e s/390x/390/`; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      LIB_PATH=${libdir}
      for lib in ${NATIVE_LIB_DIRS}; do
	case :${LIB_PATH}: in
	  *:${lib}:*) ;;
	  *) LIB_PATH=${LIB_PATH}:${lib} ;;
	esac
      done

      case "$target" in
	s390*-linux*)
	  suffix=64 ;;
      esac

      # Look for 64 bit target libraries in /lib64, /usr/lib64 etc., first
      # on Linux.
      if [ -n "$suffix" ]; then
	case "$EMULATION_NAME" in
	  *64*)
	    LIB_PATH=`echo ${LIB_PATH}: | sed -e s,:,$suffix:,g`$LIB_PATH ;;
	esac
      fi ;;
  esac
fi
