TEMPLATE_NAME=elf32
EXTRA_EM_FILE=ppc64elf
ELFSIZE=64
GENERATE_SHLIB_SCRIPT=yes
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf64-powerpc"
TEXT_START_ADDR=0x10000000
#DATA_ADDR="ALIGN (0x10000000) + (. & (${MAXPAGESIZE} - 1))"
MAXPAGESIZE=0x10000
COMMONPAGESIZE=0x1000
ARCH=powerpc:common64
MACHINE=
NOP=0x60000000
EXECUTABLE_SYMBOLS='PROVIDE (__stack = 0); PROVIDE (___stack = 0);'
OTHER_BSS_END_SYMBOLS='__end = .;'
CTOR_START='PROVIDE (__CTOR_LIST__ = .); PROVIDE (___CTOR_LIST__ = .);'
CTOR_END='PROVIDE (__CTOR_END__ = .); PROVIDE (___CTOR_END__ = .);'
DTOR_START='PROVIDE (__DTOR_LIST__ = .); PROVIDE (___DTOR_LIST__ = .);'
DTOR_END='PROVIDE (__DTOR_END__ = .); PROVIDE (___DTOR_END__ = .);'
OTHER_TEXT_SECTIONS="*(.sfpr .glink)"
BSS_PLT=
OTHER_BSS_SYMBOLS="
  .tocbss	${RELOCATING-0}${RELOCATING+ALIGN(8)} : { *(.tocbss)}"
OTHER_PLT_RELOC_SECTIONS="
  .rela.tocbss	${RELOCATING-0} : { *(.rela.tocbss) }"
OTHER_GOT_SECTIONS="
  .toc		${RELOCATING-0}${RELOCATING+ALIGN(8)} : { *(.toc) }"
OTHER_GOT_RELOC_SECTIONS="
  .rela.toc	${RELOCATING-0} : { *(.rela.toc) }"
OTHER_READWRITE_SECTIONS="
  .toc1		${RELOCATING-0}${RELOCATING+ALIGN(8)} : { *(.toc1) }
  .opd		${RELOCATING-0}${RELOCATING+ALIGN(8)} : { KEEP (*(.opd)) }"

# Treat a host that matches the target with the possible exception of "64"
# in the name as if it were native.
if test `echo "$host" | sed -e s/64//` = `echo "$target" | sed -e s/64//`; then
    case " $EMULATION_LIBPATH " in
      *" ${EMULATION_NAME} "*)
	LIB_PATH=${libdir}
	for lib in ${NATIVE_LIB_DIRS}; do
	  case :${LIB_PATH}: in
	    *:${lib}:*) ;;
	    *) LIB_PATH=${LIB_PATH}:${lib} ;;
	  esac
	done
	# Look for 64 bit target libraries in /lib64, /usr/lib64 etc., first.
    	case "$EMULATION_NAME" in
    	  *64*) LIB_PATH=`echo ${LIB_PATH}: | sed -e s,:,64:,g`$LIB_PATH
    	esac
    esac
fi
