# Linker script for PE.

if test -z "${RELOCATEABLE_OUTPUT_FORMAT}"; then
  RELOCATEABLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
fi

# We can't easily and portably get an unquoted $ in a shell
# substitution, so we do this instead.
if test "${RELOCATING}"; then
  R_TEXT='*(.text$*)'
  R_DATA='*(.data$*)'
  R_RDATA='*(.rdata$*)'
  R_IDATA='
    *(.idata$2)
    *(.idata$3)
    /* These zeroes mark the end of the import list.  */
    LONG (0); LONG (0); LONG (0); LONG (0); LONG (0);
    *(.idata$4)
    *(.idata$5)
    *(.idata$6)
    *(.idata$7)'
  R_CRT='*(.CRT$*)'
  R_RSRC='*(.rsrc$*)'
  R_EXC='*(.exc$*)'
else
  R_TEXT=
  R_DATA=
  R_RDATA=
  R_IDATA=
  R_CRT=
  R_RSRC=
  R_EXC=
fi

cat <<EOF
${RELOCATING+OUTPUT_FORMAT(${OUTPUT_FORMAT})}
${RELOCATING-OUTPUT_FORMAT(${RELOCATEABLE_OUTPUT_FORMAT})}

${LIB_SEARCH_DIRS}

ENTRY(__start)
${RELOCATING+header = .;}
${RELOCATING+__fltused = .; /* set up floating pt for MS .obj\'s */}
${RELOCATING+__ldused = .;}
SECTIONS
{
  .text ${RELOCATING+ __image_base__ + __section_alignment__ } : 
  {
    ${RELOCATING+ __text_start__ = . ;}
    ${RELOCATING+ *(.init)}
    *(.text)
    ${R_TEXT}
    *(.glue_7t)
    *(.glue_7)
    ${CONSTRUCTING+ ___CTOR_LIST__ = .; __CTOR_LIST__ = . ; 
			LONG (-1); *(.ctors); *(.ctor); LONG (0); }
    ${CONSTRUCTING+ ___DTOR_LIST__ = .; __DTOR_LIST__ = . ; 
			LONG (-1); *(.dtors); *(.dtor);  LONG (0); }
    ${RELOCATING+ *(.fini)}
    /* ??? Why is .gcc_exc here?  */
    ${RELOCATING+ *(.gcc_exc)}
    ${RELOCATING+ etext = .;}
    ${RELOCATING+ __text_end__ = .;}
    *(.gcc_except_table)
  }

  /* The Cygwin32 library uses a section to avoid copying certain data
     on fork.  This used to be named ".data$nocopy".  The linker used
     to include this between __data_start__ and __data_end__, but that
     breaks building the cygwin32 dll.  Instead, we name the section
     ".data_cygwin_nocopy" and explictly include it after __data_end__. */

  .data ${RELOCATING+BLOCK(__section_alignment__)} : 
  {
    ${RELOCATING+__data_start__ = . ;}
    *(.data)
    *(.data2)
    ${R_DATA}
    ${RELOCATING+__data_end__ = . ;}
    ${RELOCATING+*(.data_cygwin_nocopy)}
  }

  .bss ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    ${RELOCATING+__bss_start__ = . ;}
    *(.bss)
    *(COMMON)
    /* link.exe apparently pulls in .obj's because of UNDEF common
	symbols, which is not the coff way, but that's MS for you. */
    *(.CRT\$XCA)
    *(.CRT\$XCZ)
    *(.CRT\$XIA)
    *(.CRT\$XIZ)
    ${RELOCATING+__bss_end__ = . ;}
  }

  .rdata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    *(.rdata)
    ${R_RDATA}
    *(.eh_frame)
  }

  .edata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    *(.edata)
  }

  /DISCARD/ :
  {
    *(.debug\$S)
    *(.debug\$T)
    *(.debug\$F)
    *(.drectve)
    *(.debug*)
  }

  .idata ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    /* This cannot currently be handled with grouped sections.
	See pe.em:sort_sections.  */
    ${R_IDATA}
  }
  .CRT ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    ${R_CRT}
  }

  .endjunk ${RELOCATING+BLOCK(__section_alignment__)} :
  {
    /* end is deprecated, don't use it */
    ${RELOCATING+ end = .;}
    ${RELOCATING+ _end = .;}
    ${RELOCATING+ __end__ = .;}
  }

  .reloc ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    *(.reloc)
  }

  .rsrc ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    *(.rsrc)
    ${R_RSRC}
  }

  .exc ${RELOCATING+BLOCK(__section_alignment__)} :
  { 					
    *(.exc)
    ${R_EXC}
  }

  .stab ${RELOCATING+BLOCK(__section_alignment__)} ${RELOCATING+(NOLOAD)} :
  {
    [ .stab ]
  }

  .stabstr ${RELOCATING+BLOCK(__section_alignment__)} ${RELOCATING+(NOLOAD)} :
  {
    [ .stabstr ]
  }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug          0 ${RELOCATING+(NOLOAD)} : { *(.debug) }
  .line           0 ${RELOCATING+(NOLOAD)} : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 ${RELOCATING+(NOLOAD)} : { *(.debug_srcinfo) }
  .debug_sfnames  0 ${RELOCATING+(NOLOAD)} : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 ${RELOCATING+(NOLOAD)} : { *(.debug_aranges) }
  .debug_pubnames 0 ${RELOCATING+(NOLOAD)} : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 ${RELOCATING+(NOLOAD)} : { *(.debug_info) *(.gnu.linkonce.wi.*) }
  .debug_abbrev   0 ${RELOCATING+(NOLOAD)} : { *(.debug_abbrev) }
  .debug_line     0 ${RELOCATING+(NOLOAD)} : { *(.debug_line) }
  .debug_frame    0 ${RELOCATING+(NOLOAD)} : { *(.debug_frame) }
  .debug_str      0 ${RELOCATING+(NOLOAD)} : { *(.debug_str) }
  .debug_loc      0 ${RELOCATING+(NOLOAD)} : { *(.debug_loc) }
  .debug_macinfo  0 ${RELOCATING+(NOLOAD)} : { *(.debug_macinfo) }

  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 ${RELOCATING+(NOLOAD)} : { *(.debug_weaknames) }
  .debug_funcnames 0 ${RELOCATING+(NOLOAD)} : { *(.debug_funcnames) }
  .debug_typenames 0 ${RELOCATING+(NOLOAD)} : { *(.debug_typenames) }
  .debug_varnames  0 ${RELOCATING+(NOLOAD)} : { *(.debug_varnames) }
}
EOF
