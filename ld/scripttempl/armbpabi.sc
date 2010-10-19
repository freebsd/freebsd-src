# This variant of elf.sc is used for ARM BPABI platforms, like Symbian
# OS, where a separate postlinker will operated on the generated
# executable or shared object.  See elf.sc for configuration variables
# that apply; only BPABI-specific variables will be noted here.

test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test -z "${ELFSIZE}" && ELFSIZE=32
test -z "${ALIGNMENT}" && ALIGNMENT="${ELFSIZE} / 8"
test "$LD_FLAG" = "N" && DATA_ADDR=.
test -n "$CREATE_SHLIB$CREATE_PIE" && test -n "$SHLIB_DATA_ADDR" && COMMONPAGESIZE=""
test -z "$CREATE_SHLIB$CREATE_PIE" && test -n "$DATA_ADDR" && COMMONPAGESIZE=""
test -n "$RELRO_NOW" && unset SEPARATE_GOTPLT
DATA_SEGMENT_ALIGN="ALIGN(${SEGMENT_SIZE}) + (. & (${MAXPAGESIZE} - 1))"
DATA_SEGMENT_RELRO_END=""
DATA_SEGMENT_RELRO_GOTPLT_END=""
DATA_SEGMENT_END=""
if test -n "${COMMONPAGESIZE}"; then
  DATA_SEGMENT_ALIGN="ALIGN (${SEGMENT_SIZE}) - ((${MAXPAGESIZE} - .) & (${MAXPAGESIZE} - 1)); . = DATA_SEGMENT_ALIGN (${MAXPAGESIZE}, ${COMMONPAGESIZE})"
  DATA_SEGMENT_END=". = DATA_SEGMENT_END (.);"
  if test -n "${SEPARATE_GOTPLT}"; then
    DATA_SEGMENT_RELRO_GOTPLT_END=". = DATA_SEGMENT_RELRO_END (. + ${SEPARATE_GOTPLT});"
  else
    DATA_SEGMENT_RELRO_END=". = DATA_SEGMENT_RELRO_END (.);"
  fi
fi
INTERP=".interp       0 : { *(.interp) }"
PLT=".plt          ${RELOCATING-0} : { *(.plt) }"
RODATA=".rodata       ${RELOCATING-0} : { *(.rodata${RELOCATING+ .rodata.* .gnu.linkonce.r.*}) }"
DATARELRO=".data.rel.ro : { *(.data.rel.ro.local) *(.data.rel.ro*) }"
STACKNOTE="/DISCARD/ : { *(.note.GNU-stack) }"
if test -z "${NO_SMALL_DATA}"; then
  SBSS=".sbss         ${RELOCATING-0} :
  {
    ${RELOCATING+PROVIDE (__sbss_start = .);}
    ${RELOCATING+PROVIDE (___sbss_start = .);}
    *(.dynsbss)
    *(.sbss${RELOCATING+ .sbss.* .gnu.linkonce.sb.*})
    *(.scommon)
    ${RELOCATING+PROVIDE (__sbss_end = .);}
    ${RELOCATING+PROVIDE (___sbss_end = .);}
  }"
  SBSS2=".sbss2        ${RELOCATING-0} : { *(.sbss2${RELOCATING+ .sbss2.* .gnu.linkonce.sb2.*}) }"
  SDATA="/* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata        ${RELOCATING-0} : 
  {
    ${RELOCATING+${SDATA_START_SYMBOLS}}
    *(.sdata${RELOCATING+ .sdata.* .gnu.linkonce.s.*})
  }"
  SDATA2=".sdata2       ${RELOCATING-0} : { *(.sdata2${RELOCATING+ .sdata2.* .gnu.linkonce.s2.*}) }"
  REL_SDATA=".rel.sdata    ${RELOCATING-0} : { *(.rel.sdata${RELOCATING+ .rel.sdata.* .rel.gnu.linkonce.s.*}) }
  .rela.sdata   ${RELOCATING-0} : { *(.rela.sdata${RELOCATING+ .rela.sdata.* .rela.gnu.linkonce.s.*}) }"
  REL_SBSS=".rel.sbss     ${RELOCATING-0} : { *(.rel.sbss${RELOCATING+ .rel.sbss.* .rel.gnu.linkonce.sb.*}) }
  .rela.sbss    ${RELOCATING-0} : { *(.rela.sbss${RELOCATING+ .rela.sbss.* .rela.gnu.linkonce.sb.*}) }"
  REL_SDATA2=".rel.sdata2   ${RELOCATING-0} : { *(.rel.sdata2${RELOCATING+ .rel.sdata2.* .rel.gnu.linkonce.s2.*}) }
  .rela.sdata2  ${RELOCATING-0} : { *(.rela.sdata2${RELOCATING+ .rela.sdata2.* .rela.gnu.linkonce.s2.*}) }"
  REL_SBSS2=".rel.sbss2    ${RELOCATING-0} : { *(.rel.sbss2${RELOCATING+ .rel.sbss2.* .rel.gnu.linkonce.sb2.*}) }
  .rela.sbss2   ${RELOCATING-0} : { *(.rela.sbss2${RELOCATING+ .rela.sbss2.* .rela.gnu.linkonce.sb2.*}) }"
else
  NO_SMALL_DATA=" "
fi
test -n "$SEPARATE_GOTPLT" && SEPARATE_GOTPLT=" "
CTOR=".ctors        ${CONSTRUCTING-0} : 
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
       the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend*.o $OTHER_EXCLUDE_FILES) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ${CONSTRUCTING+${CTOR_END}}
  }"
DTOR=".dtors        ${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+${DTOR_START}}
    KEEP (*crtbegin*.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend*.o $OTHER_EXCLUDE_FILES) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ${CONSTRUCTING+${DTOR_END}}
  }"
STACK="  .stack        ${RELOCATING-0}${RELOCATING+${STACK_ADDR}} :
  {
    ${RELOCATING+_stack = .;}
    *(.stack)
  }"

TEXT_START_ADDR="SEGMENT_START(\"text\", ${TEXT_START_ADDR})"
SHLIB_TEXT_START_ADDR="SEGMENT_START(\"text\", ${SHLIB_TEXT_START_ADDR:-0})"
DATA_ADDR="SEGMENT_START(\"data\", ${DATA_ADDR-${DATA_SEGMENT_ALIGN}})"
SHLIB_DATA_ADDR="SEGMENT_START(\"data\", ${SHLIB_DATA_ADDR-${DATA_SEGMENT_ALIGN}})"

# if this is for an embedded system, don't add SIZEOF_HEADERS.
if [ -z "$EMBEDDED" ]; then
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR} + SIZEOF_HEADERS"
   SHLIB_BASE_ADDRESS="${SHLIB_TEXT_START_ADDR} + SIZEOF_HEADERS"
else
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR}"
   SHLIB_BASE_ADDRESS="${SHLIB_TEXT_START_ADDR}"
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
${RELOCATING+${INPUT_FILES}}
${RELOCATING- /* For some reason, the Solaris linker makes bad executables
  if gld -r is used and the intermediate file has sections starting
  at non-zero addresses.  Could be a Solaris ld bug, could be a GNU ld
  bug.  But for now assigning the zero vmas works.  */}

/* ARM's proprietary toolchain generate these symbols to match the start 
   and end of particular sections of the image.  SymbianOS uses these
   symbols.  We provide them for compatibility with ARM's toolchains.  
   These symbols should be bound locally; each shared object may define 
   its own version of these symbols.  */ 
	
VERSION
{ 
  /* Give these a dummy version to work around linker lameness.
     The name used shouldn't matter as these are all local symbols.  */
  __GNU { 
    local: 
      Image\$\$ER_RO\$\$Base;
      Image\$\$ER_RO\$\$Limit;
      SHT\$\$INIT_ARRAY\$\$Base;
      SHT\$\$INIT_ARRAY\$\$Limit;
      .ARM.exidx\$\$Base;	
      .ARM.exidx\$\$Limit;
  };
}

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  ${CREATE_SHLIB-${CREATE_PIE-${RELOCATING+PROVIDE (__executable_start = ${TEXT_START_ADDR});}}}

  ${CREATE_SHLIB-${CREATE_PIE-${RELOCATING+ . = ${TEXT_BASE_ADDRESS};}}}
  ${CREATE_SHLIB+${RELOCATING+. = ${SHLIB_BASE_ADDRESS};}}
  ${CREATE_PIE+${RELOCATING+. = ${SHLIB_BASE_ADDRESS};}}

  /* Define Image\$\$ER_RO\$\$Base.  */
  ${RELOCATING+PROVIDE (Image\$\$ER_RO\$\$Base = .);}

  ${INITIAL_READONLY_SECTIONS}

EOF
cat <<EOF
  .init         ${RELOCATING-0} : 
  { 
    ${RELOCATING+${INIT_START}}
    KEEP (*(.init))
    ${RELOCATING+${INIT_END}}
  } =${NOP-0}
  .text         ${RELOCATING-0} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text .stub${RELOCATING+ .text.* .gnu.linkonce.t.*})
    KEEP (*(.text.*personality*))
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    ${RELOCATING+${OTHER_TEXT_SECTIONS}}
  } =${NOP-0}
  .fini         ${RELOCATING-0} :
  {
    ${RELOCATING+${FINI_START}}
    KEEP (*(.fini))
    ${RELOCATING+${FINI_END}}
  } =${NOP-0}
  /* The SymbianOS kernel requires that the PLT go at the end of the
     text section.  */
  ${DATA_PLT-${BSS_PLT-${PLT}}}
  ${RELOCATING+PROVIDE (__etext = .);}
  ${RELOCATING+PROVIDE (_etext = .);}
  ${RELOCATING+PROVIDE (etext = .);}

  /* Define Image\$\$ER_RO\$\$Limit.  */
  ${RELOCATING+PROVIDE (Image\$\$ER_RO\$\$Limit = .);}

  ${WRITABLE_RODATA-${RODATA}}
  .rodata1      ${RELOCATING-0} : { *(.rodata1) }
  ${CREATE_SHLIB-${SDATA2}}
  ${CREATE_SHLIB-${SBSS2}}

  /* On SymbianOS, put  .init_array and friends in the read-only
     segment; there is no runtime relocation applied to these
     arrays.  */

  .preinit_array   ${RELOCATING-0} :
  {
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__preinit_array_start = .);}}
    KEEP (*(.preinit_array))
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__preinit_array_end = .);}}
  }
  .init_array   ${RELOCATING-0} :
  {
    /* SymbianOS uses this symbol.  */
    ${RELOCATING+PROVIDE (SHT\$\$INIT_ARRAY\$\$Base = .);}
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__init_array_start = .);}}
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array))
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__init_array_end = .);}}
    /* SymbianOS uses this symbol.  */
    ${RELOCATING+PROVIDE (SHT\$\$INIT_ARRAY\$\$Limit = .);}
  }
  .fini_array   ${RELOCATING-0} :
  {
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__fini_array_start = .);}}
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    ${RELOCATING+${CREATE_SHLIB-PROVIDE_HIDDEN (__fini_array_end = .);}}
  }

  ${OTHER_READONLY_SECTIONS}
  .eh_frame_hdr : { *(.eh_frame_hdr) }
  .eh_frame     ${RELOCATING-0} : ONLY_IF_RO { KEEP (*(.eh_frame)) }
  .gcc_except_table ${RELOCATING-0} : ONLY_IF_RO { KEEP (*(.gcc_except_table)) *(.gcc_except_table.*) }

  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  ${CREATE_SHLIB-${CREATE_PIE-${RELOCATING+. = ${DATA_ADDR};}}}
  ${CREATE_SHLIB+${RELOCATING+. = ${SHLIB_DATA_ADDR};}}
  ${CREATE_PIE+${RELOCATING+. = ${SHLIB_DATA_ADDR};}}

  /* Exception handling  */
  .eh_frame     ${RELOCATING-0} : ONLY_IF_RW { KEEP (*(.eh_frame)) }
  .gcc_except_table ${RELOCATING-0} : ONLY_IF_RW { KEEP (*(.gcc_except_table)) *(.gcc_except_table.*) }

  /* Thread Local Storage sections  */
  .tdata	${RELOCATING-0} : { *(.tdata${RELOCATING+ .tdata.* .gnu.linkonce.td.*}) }
  .tbss		${RELOCATING-0} : { *(.tbss${RELOCATING+ .tbss.* .gnu.linkonce.tb.*})${RELOCATING+ *(.tcommon)} }

  ${RELOCATING+${CTOR}}
  ${RELOCATING+${DTOR}}
  .jcr          ${RELOCATING-0} : { KEEP (*(.jcr)) }

  ${RELOCATING+${DATARELRO}}
  ${OTHER_RELRO_SECTIONS}
  ${RELOCATING+${DATA_SEGMENT_RELRO_END}}

  ${DATA_PLT+${PLT_BEFORE_GOT-${PLT}}}

  .data         ${RELOCATING-0} :
  {
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data${RELOCATING+ .data.* .gnu.linkonce.d.*})
    KEEP (*(.gnu.linkonce.d.*personality*))
    ${CONSTRUCTING+SORT(CONSTRUCTORS)}
  }
  .data1        ${RELOCATING-0} : { *(.data1) }
  ${WRITABLE_RODATA+${RODATA}}
  ${OTHER_READWRITE_SECTIONS}
  ${DATA_PLT+${PLT_BEFORE_GOT+${PLT}}}
  ${CREATE_SHLIB+${SDATA2}}
  ${CREATE_SHLIB+${SBSS2}}
  ${SDATA}
  ${OTHER_SDATA_SECTIONS}
  ${RELOCATING+_edata = .;}
  ${RELOCATING+PROVIDE (edata = .);}
  ${RELOCATING+. = DEFINED(__bss_segment_start) ? __bss_segment_start : .;}
  ${RELOCATING+__bss_start = .;}
  ${RELOCATING+${OTHER_BSS_SYMBOLS}}
  ${SBSS}
  ${BSS_PLT+${PLT}}
  .bss          ${RELOCATING-0} :
  {
   *(.dynbss)
   *(.bss${RELOCATING+ .bss.* .gnu.linkonce.b.*})
   *(COMMON)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections.  */
   ${RELOCATING+. = ALIGN(${ALIGNMENT});}
  }
  ${RELOCATING+${OTHER_BSS_END_SYMBOLS}}
  ${RELOCATING+. = ALIGN(${ALIGNMENT});}
  ${RELOCATING+${OTHER_END_SYMBOLS}}
  ${RELOCATING+_end = .;}
  ${RELOCATING+PROVIDE (end = .);}
  ${RELOCATING+${DATA_SEGMENT_END}}

  /* These sections are not mapped under the BPABI.  */
  .dynamic      0 : { *(.dynamic) }
  .hash         0 : { *(.hash) }
  .dynsym       0 : { *(.dynsym) }
  .dynstr       0 : { *(.dynstr) }
  .gnu.version  0 : { *(.gnu.version) }
  .gnu.version_d 0: { *(.gnu.version_d) }
  .gnu.version_r 0: { *(.gnu.version_r) }
  ${CREATE_SHLIB-${INTERP}}

  /* Stabs debugging sections.  */
  .stab          0 : { *(.stab) }
  .stabstr       0 : { *(.stabstr) }
  .stab.excl     0 : { *(.stab.excl) }
  .stab.exclstr  0 : { *(.stab.exclstr) }
  .stab.index    0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment       0 : { *(.comment) }

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
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
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

  ${STACK_ADDR+${STACK}}
  ${OTHER_SECTIONS}
  ${RELOCATING+${OTHER_SYMBOLS}}
  ${RELOCATING+${STACKNOTE}}
EOF

# These relocations sections are part of the read-only segment in SVR4
# executables, but are not mapped in BPABI executables.
if [ "x$COMBRELOC" = x ]; then
  COMBRELOCCAT=cat
else
  COMBRELOCCAT="cat > $COMBRELOC"
fi
eval $COMBRELOCCAT <<EOF
  .rel.init     0 : { *(.rel.init) }
  .rela.init    0 : { *(.rela.init) }
  .rel.text     0 : { *(.rel.text${RELOCATING+ .rel.text.* .rel.gnu.linkonce.t.*}) }
  .rela.text    0 : { *(.rela.text${RELOCATING+ .rela.text.* .rela.gnu.linkonce.t.*}) }
  .rel.fini     0 : { *(.rel.fini) }
  .rela.fini    0 : { *(.rela.fini) }
  .rel.rodata   0 : { *(.rel.rodata${RELOCATING+ .rel.rodata.* .rel.gnu.linkonce.r.*}) }
  .rela.rodata  0 : { *(.rela.rodata${RELOCATING+ .rela.rodata.* .rela.gnu.linkonce.r.*}) }
  ${OTHER_READONLY_RELOC_SECTIONS}
  .rel.data.rel.ro 0 : { *(.rel.data.rel.ro${RELOCATING+*}) }
  .rela.data.rel.ro 0 : { *(.rel.data.rel.ro${RELOCATING+*}) }
  .rel.data     0 : { *(.rel.data${RELOCATING+ .rel.data.* .rel.gnu.linkonce.d.*}) }
  .rela.data    0 : { *(.rela.data${RELOCATING+ .rela.data.* .rela.gnu.linkonce.d.*}) }
  .rel.tdata	0 : { *(.rel.tdata${RELOCATING+ .rel.tdata.* .rel.gnu.linkonce.td.*}) }
  .rela.tdata	0 : { *(.rela.tdata${RELOCATING+ .rela.tdata.* .rela.gnu.linkonce.td.*}) }
  .rel.tbss	0 : { *(.rel.tbss${RELOCATING+ .rel.tbss.* .rel.gnu.linkonce.tb.*}) }
  .rela.tbss	0 : { *(.rela.tbss${RELOCATING+ .rela.tbss.* .rela.gnu.linkonce.tb.*}) }
  .rel.ctors    0 : { *(.rel.ctors) }
  .rela.ctors   0 : { *(.rela.ctors) }
  .rel.dtors    0 : { *(.rel.dtors) }
  .rela.dtors   0 : { *(.rela.dtors) }
  ${REL_SDATA}
  ${REL_SBSS}
  ${REL_SDATA2}
  ${REL_SBSS2}
  .rel.bss      0 : { *(.rel.bss${RELOCATING+ .rel.bss.* .rel.gnu.linkonce.b.*}) }
  .rela.bss     0 : { *(.rela.bss${RELOCATING+ .rela.bss.* .rela.gnu.linkonce.b.*}) }
  .rel.init_array  0 : { *(.rel.init_array) }
  .rela.init_array 0 : { *(.rela.init_array) }
  .rel.fini_array  0 : { *(.rel.fini_array) }
  .rela.fini_array 0 : { *(.rela.fini_array) }
EOF
if [ -n "$COMBRELOC" ]; then
cat <<EOF
  .rel.dyn      0 :
    {
EOF
sed -e '/^[ 	]*[{}][ 	]*$/d;/:[ 	]*$/d;/\.rela\./d;s/^.*: { *\(.*\)}$/      \1/' $COMBRELOC
cat <<EOF
    }
  .rela.dyn     0 :
    {
EOF
sed -e '/^[ 	]*[{}][ 	]*$/d;/:[ 	]*$/d;/\.rel\./d;s/^.*: { *\(.*\)}/      \1/' $COMBRELOC
cat <<EOF
    }
EOF
fi
cat <<EOF
  .rel.plt      0 : { *(.rel.plt) }
  .rela.plt     0 : { *(.rela.plt) }
  ${OTHER_PLT_RELOC_SECTIONS}
  .rel.other    0 : { *(.rel.*) }
  .rela.other   0 : { *(.rela.*) }
  .reli.other   0 : { *(.reli.*) }
}
EOF
