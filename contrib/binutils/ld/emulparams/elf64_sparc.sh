SCRIPT_NAME=elf
ELFSIZE=64
TEMPLATE_NAME=elf32
OUTPUT_FORMAT="elf64-sparc"
TEXT_START_ADDR=0x100000
MAXPAGESIZE=0x100000
NONPAGED_TEXT_START_ADDR=0x100000
ARCH="sparc:v9"
MACHINE=
DATA_PLT=
GENERATE_SHLIB_SCRIPT=yes
NOP=0x01000000

if [ "x${host}" = "x${target}" ]; then
  case " $EMULATION_LIBPATH " in
    *" ${EMULATION_NAME} "*)
      # Native, and default or emulation requesting LIB_PATH.

      # Linux and Solaris modify the default library search path
      # to first include a 64-bit specific directory.  It's put
      # in slightly different places on the two systems.
      case "$target" in
        sparc*-linux*)
          suffix=64 ;;
        sparc*-solaris*)
          suffix=/sparcv9 ;;
      esac

      if [ -n "${suffix}" ]; then

	LIB_PATH=/lib${suffix}:/lib
	LIB_PATH=${LIB_PATH}:/usr/lib${suffix}:/usr/lib
	if [ -n "${NATIVE_LIB_DIRS}" ]; then
	  LIB_PATH=${LIB_PATH}:`echo ${NATIVE_LIB_DIRS} | sed s/:/${suffix}:/g`${suffix}:${NATIVE_LIB_DIRS}
	fi
	if [ "${libdir}" != /usr/lib ]; then
	  LIB_PATH=${LIB_PATH}:${libdir}${suffix}:${libdir}
	fi
	if [ "${libdir}" != /usr/local/lib ]; then
	  LIB_PATH=${LIB_PATH}:/usr/local/lib${suffix}:/usr/local/lib
	fi

      fi
    ;;
  esac
fi
