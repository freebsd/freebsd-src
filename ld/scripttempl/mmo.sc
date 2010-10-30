cat <<EOF
OUTPUT_FORMAT("mmo")
OUTPUT_ARCH(mmix)
ENTRY(Main)
SECTIONS
{
  .text ${RELOCATING+ ${TEXT_START_ADDR}}:
  {
    *(.text)
    ${RELOCATING+*(.text.*)}
    ${RELOCATING+*(.gnu.linkonce.t*)}
    ${RELOCATING+*(.rodata)}
    ${RELOCATING+*(.rodata.*)}
    ${RELOCATING+*(.gnu.linkonce.r*)}

    /* FIXME: Move .init, .fini, .ctors and .dtors to their own sections.  */
    ${RELOCATING+ PROVIDE (_init_start = .);}
    ${RELOCATING+ PROVIDE (_init = .);}
    ${RELOCATING+ KEEP (*(.init))}
    ${RELOCATING+ PROVIDE (_init_end = .);}

    ${RELOCATING+ PROVIDE (_fini_start = .);}
    ${RELOCATING+ PROVIDE (_fini = .);}
    ${RELOCATING+ KEEP (*(.fini))}
    ${RELOCATING+ PROVIDE (_fini_end = .);}

    /* FIXME: Align ctors, dtors, ehframe.  */
    ${RELOCATING+ PROVIDE (_ctors_start = .);}
    ${RELOCATING+ PROVIDE (__ctors_start = .);}
    ${RELOCATING+ PROVIDE (_ctors = .);}
    ${RELOCATING+ PROVIDE (__ctors = .);}
    ${RELOCATING+ KEEP (*crtbegin.o(.ctors))}
    ${RELOCATING+ KEEP (*crtbegin?.o(.ctors))}
    ${RELOCATING+ KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o) .ctors))}
    ${RELOCATING+ KEEP (*(SORT(.ctors.*)))}
    ${RELOCATING+ KEEP (*(.ctors))}
    ${RELOCATING+ PROVIDE (_ctors_end = .);}
    ${RELOCATING+ PROVIDE (__ctors_end = .);}

    ${RELOCATING+ PROVIDE (_dtors_start = .);}
    ${RELOCATING+ PROVIDE (__dtors_start = .);}
    ${RELOCATING+ PROVIDE (_dtors = .);}
    ${RELOCATING+ PROVIDE (__dtors = .);}
    ${RELOCATING+ KEEP (*crtbegin.o(.dtors))}
    ${RELOCATING+ KEEP (*crtbegin?.o(.dtors))}
    ${RELOCATING+ KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o) .dtors))}
    ${RELOCATING+ KEEP (*(SORT(.dtors.*)))}
    ${RELOCATING+ KEEP (*(.dtors))}
    ${RELOCATING+ PROVIDE (_dtors_end = .);}
    ${RELOCATING+ PROVIDE (__dtors_end = .);}

    ${RELOCATING+KEEP (*(.jcr))}
    ${RELOCATING+KEEP (*(.eh_frame))}
    ${RELOCATING+*(.gcc_except_table)}

    ${RELOCATING+ PROVIDE(etext = .);}
    ${RELOCATING+ PROVIDE(_etext = .);}
    ${RELOCATING+ PROVIDE(__etext = .);}
  }
  ${RELOCATING+Main = DEFINED (Main) ? Main : (DEFINED (_start) ? _start : ADDR (.text));}

  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_info     0 : { *(.debug_info) *(.gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  .debug_ranges   0 : { *(.debug_ranges) }

  .data ${RELOCATING+ ${DATA_ADDR}}:
  {
    ${RELOCATING+ PROVIDE(__Sdata = .);}

    *(.data);
    ${RELOCATING+*(.data.*)}
    ${RELOCATING+*(.gnu.linkonce.d*)}

    ${RELOCATING+ PROVIDE(__Edata = .);}

    /* Deprecated, use __Edata.  */
    ${RELOCATING+ PROVIDE(edata = .);}
    ${RELOCATING+ PROVIDE(_edata = .);}
    ${RELOCATING+ PROVIDE(__edata = .);}
  }

  /* At the moment, although perhaps we should, we can't map sections
     without contents to sections *with* contents due to FIXME: a BFD bug.
     Anyway, the mmo back-end ignores sections without contents when
     writing out sections, so this works fine.   */
  .bss :
  {
    ${RELOCATING+ PROVIDE(__Sbss = .);}
    ${RELOCATING+ PROVIDE(__bss_start = .);}
    ${RELOCATING+ *(.sbss);}
    ${RELOCATING+ *(.bss);}
    ${RELOCATING+*(.bss.*)}
    ${RELOCATING+ *(COMMON);}
    ${RELOCATING+ PROVIDE(__Ebss = .);}
  }

  /* Deprecated, use __Ebss or __Eall as appropriate.  */
  ${RELOCATING+ PROVIDE(end = .);}
  ${RELOCATING+ PROVIDE(_end = .);}
  ${RELOCATING+ PROVIDE(__end = .);}
  ${RELOCATING+ PROVIDE(__Eall = .);}

  .MMIX.reg_contents :
  {
    /* Note that this section always has a fixed VMA - that of its
       first register * 8.  */
    *(.MMIX.reg_contents.linker_allocated);
    *(.MMIX.reg_contents);
  }

  /* By default, put the high end of the stack where the register stack
     begins.  They grow in opposite directions.  */
  PROVIDE (__Stack_start = 0x6000000000000000);

  /* Unfortunately, stabs are not mappable from ELF to MMO.
     It can probably be fixed with some amount of work.  */
  /DISCARD/ :
  { *(.gnu.warning.*); }
}
EOF
