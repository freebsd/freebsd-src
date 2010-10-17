# Linker script for TI TMS320C80 (tic80) COFF.
#
# Besides the shell variables set by the emulparams script, and the LD_FLAG
# variable, the genscripts.sh script will set the following variables for each
# time this script is run to generate one of the linker scripts for ldscripts:
#
# RELOCATING: Set to a non-empty string when the linker is going to be doing
# a final relocation.
#
# CONSTRUCTING: Set to a non-empty string when the linker is going to be
# building global constructor and destructor tables.
#
# DATA_ALIGNMENT: Set to an ALIGN expression when the output should be page
# aligned, or to "." when generating the -N script.
#
# CREATE_SHLIB: Set to a non-empty string when generating a script for
# the -shared linker arg.

test -z "$TEXT_START_ADDR" && TEXT_START_ADDR="0x80000 + SIZEOF_HEADERS"
test -z "$ENTRY" && ENTRY=__start

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}

ENTRY(${ENTRY})

SECTIONS
{
  .text ${RELOCATING+ $TEXT_START_ADDR} : {
    *(.init)
    *(.fini)
    *(.text)
  }
  .const ALIGN(4) : {
    *(.const)
  }
  .ctors ALIGN(4) : {
    ${CONSTRUCTING+ . = ALIGN(4);}
    ${CONSTRUCTING+ ___CTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG(-1)}
    *(.ctors)
    ${CONSTRUCTING+ ___CTOR_END__ = .;}
    ${CONSTRUCTING+ LONG(0)}
  }
  .dtors ALIGN(4) : {
    ${CONSTRUCTING+ ___DTOR_LIST__ = .;}
    ${CONSTRUCTING+ LONG(-1)}
    ${CONSTRUCTING+ *(.dtors)}
    ${CONSTRUCTING+ ___DTOR_END__ = .;}
    ${CONSTRUCTING+ LONG(0)}
  }
  ${RELOCATING+ etext  =  .;}
  .data : {
    *(.data)
    ${RELOCATING+ __edata  =  .};
  }
  .bss : { 					
    ${RELOCATING+ __bss_start = .};
    *(.bss)
    *(COMMON)
     ${RELOCATING+ _end = ALIGN(0x8)};
     ${RELOCATING+ __end = ALIGN(0x8)};
  }
  .stab  0 ${RELOCATING+(NOLOAD)} : 
  {
    [ .stab ]
  }
  .stabstr  0 ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }
}
EOF
