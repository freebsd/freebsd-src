test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test "$LD_FLAG" = "N" && DATA_ADDR=.
INTERP=".interp   ${RELOCATING-0} : { *(.interp) 	}"
PLT=".plt    ${RELOCATING-0} : { *(.plt)	}"


CTOR=".ctors ${CONSTRUCTING-0} : 
  {
    ${CONSTRUCTING+${CTOR_START}}
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin*.o(.ctors))

    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend*.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ${CONSTRUCTING+${CTOR_END}}
  }"

DTOR=" .dtors       ${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+${DTOR_START}}
    KEEP (*crtbegin*.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend*.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ${CONSTRUCTING+${DTOR_END}}
  }"

STACK=" .stack : { _stack = .; *(.stack) } >STACK "

# if this is for an embedded system, don't add SIZEOF_HEADERS.
if [ -z "$EMBEDDED" ]; then
   test -z "${READONLY_BASE_ADDRESS}" && READONLY_BASE_ADDRESS="${READONLY_START_ADDR} + SIZEOF_HEADERS"
else
   test -z "${READONLY_BASE_ADDRESS}" && READONLY_BASE_ADDRESS="${READONLY_START_ADDR}"
fi

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}", "${BIG_OUTPUT_FORMAT}",
	      "${LITTLE_OUTPUT_FORMAT}")
OUTPUT_ARCH(${OUTPUT_ARCH})
ENTRY(${ENTRY})

${RELOCATING+${LIB_SEARCH_DIRS}}
${RELOCATING+/* Do we need any of these for elf?
   __DYNAMIC = 0; ${STACKZERO+${STACKZERO}} ${SHLIB_PATH+${SHLIB_PATH}}  */}
${RELOCATING+${EXECUTABLE_SYMBOLS}}

MEMORY
{
  /* These are the values for the D10V-TS3 board.
     There are other memory regions available on
     the TS3 (eg ROM, FLASH, etc) but these are not
     used by this script.  */
     
  INSN       : org = 0x01000000, len = 256K
  DATA       : org = 0x02000000, len = 48K

  /* This is a fake memory region at the top of the
     on-chip RAM, used as the start of the
     (descending) stack.  */
     
  STACK      : org = 0x0200BFFC, len = 4
}

SECTIONS
{
  .text ${RELOCATING+${TEXT_START_ADDR}} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    KEEP (*(.init))
    KEEP (*(.init.*))
    KEEP (*(.fini))
    KEEP (*(.fini.*))
    *(.text)
    *(.text.*)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    *(.gnu.linkonce.t*)
    ${RELOCATING+_etext = .;}
    ${RELOCATING+PROVIDE (etext = .);}
  } ${RELOCATING+ >INSN} =${NOP-0}

  .rodata ${RELOCATING+${READONLY_START_ADDR}} : {
    *(.rodata)
    *(.gnu.linkonce.r*)
    *(.rodata.*)
  } ${RELOCATING+ >DATA}

  .rodata1 ${RELOCATING-0} : {
    *(.rodata1)
    *(.rodata1.*)
   } ${RELOCATING+ >DATA}

  .data  ${RELOCATING-0} :
  {
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d*)
    ${CONSTRUCTING+CONSTRUCTORS}
  } ${RELOCATING+ >DATA}

  .data1 ${RELOCATING-0} : {
    *(.data1)
    *(.data1.*)
  } ${RELOCATING+ >DATA}

  ${RELOCATING+${CTOR} >DATA}
  ${RELOCATING+${DTOR} >DATA}

  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata   ${RELOCATING-0} : {
    *(.sdata)
    *(.sdata.*)
  } ${RELOCATING+ >DATA}

  ${RELOCATING+_edata = .;}
  ${RELOCATING+PROVIDE (edata = .);}
  ${RELOCATING+__bss_start = .;}
  .sbss    ${RELOCATING-0} : { *(.sbss) *(.scommon) } ${RELOCATING+ >DATA}
  .bss     ${RELOCATING-0} :
  {
   *(.dynbss)
   *(.dynbss.*)
   *(.bss)
   *(.bss.*)
   *(COMMON)
  } ${RELOCATING+ >DATA}

  ${RELOCATING+_end = . ;}
  ${RELOCATING+PROVIDE (end = .);}

  ${RELOCATING+$STACK}

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment 0 : { *(.comment) }

  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) *(.gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }

  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
}
EOF
