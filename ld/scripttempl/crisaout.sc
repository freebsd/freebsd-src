cat <<EOF
OUTPUT_FORMAT("a.out-cris")
OUTPUT_ARCH(cris)
ENTRY(__start)
SECTIONS
{
  .text ${RELOCATING+ ${TEXT_START_ADDR}}:
  {
   CREATE_OBJECT_SYMBOLS;
    ${CONSTRUCTING+ __Stext = .;}
    ${RELOCATING+*(.startup)}
    *(.text)
    ${CONSTRUCTING+__start = DEFINED(__start) ? __start : 
		   DEFINED(_start) ? _start :
		     DEFINED(start) ? start :
		        DEFINED(.startup) ? .startup + 2 : 2;}
    ${RELOCATING+*(.text.*)}
    ${RELOCATING+*(.gnu.linkonce.t*)}
    ${RELOCATING+*(.rodata)}
    ${RELOCATING+*(.rodata.*)}
    ${RELOCATING+*(.gnu.linkonce.r*)}

    /* Do not "provide" init-start and fini-start symbols; they might be
       referred to weakly, so the linker would not override the zero
       default.
       FIXME: It's somewhat unexpected to have code emitted by the linker
       script.  Some other mechanism could probably do better.  */
    ${CONSTRUCTING+ . = ALIGN (2);}
    ${CONSTRUCTING+  ___init__start = .;}
    ${CONSTRUCTING+ PROVIDE (___do_global_ctors = .);}
    ${CONSTRUCTING+ SHORT (0xe1fc); /* push srp */}
    ${CONSTRUCTING+ SHORT (0xbe7e);}
    ${CONSTRUCTING+ *(.init)}
    ${CONSTRUCTING+ SHORT (0x0d3e); /* jump [sp+] */}
    ${CONSTRUCTING+ PROVIDE (__init__end = .);}
    ${CONSTRUCTING+ PROVIDE (___init__end = .);}

    ${CONSTRUCTING+ . = ALIGN (2);}
    ${CONSTRUCTING+  ___fini__start = .;}
    ${CONSTRUCTING+ PROVIDE (___do_global_dtors = .);}
    ${CONSTRUCTING+ SHORT (0xe1fc); /* push srp */}
    ${CONSTRUCTING+ SHORT (0xbe7e);}
    ${CONSTRUCTING+ *(.fini)}
    ${CONSTRUCTING+ SHORT (0x0d3e); /* jump [sp+] */}
    ${CONSTRUCTING+ PROVIDE (__fini__end = .);}
    ${CONSTRUCTING+  ___fini__end = .;}

    /* Cater to linking from ELF.  */
    ${CONSTRUCTING+ PROVIDE(___ctors = .);}
    ${CONSTRUCTING+ ___elf_ctors_dtors_begin = .;}
    ${CONSTRUCTING+ KEEP (*crtbegin*.o(.ctors))}
    ${CONSTRUCTING+ KEEP (*(EXCLUDE_FILE (*crtend*.o) .ctors))}
    ${CONSTRUCTING+ KEEP (*(SORT(.ctors.*)))}
    ${CONSTRUCTING+ KEEP (*(.ctors))}
    ${CONSTRUCTING+ PROVIDE(___ctors_end = .);}

    ${CONSTRUCTING+ PROVIDE(___dtors = .);}
    ${CONSTRUCTING+ KEEP (*crtbegin*.o(.dtors))}
    ${CONSTRUCTING+ KEEP (*(EXCLUDE_FILE (*crtend*.o) .dtors))}
    ${CONSTRUCTING+ KEEP (*(SORT(.dtors.*)))}
    ${CONSTRUCTING+ KEEP (*(.dtors))}
    ${CONSTRUCTING+ PROVIDE(___dtors_end = .);}
    ${CONSTRUCTING+ ___elf_ctors_dtors_end = .;}

    /* We include objects that force alignment of the data segment.
       Unfortunately that sometimes causes a gap between .text and .data,
       which is not detectable since .data does not have a start address
       of itself in the a.out header.  This should only matter for
       testing; for production use, .data is at a "known" location.
       We assume .data does not get an alignment larger than 32 bytes.  */
    ${CONSTRUCTING+. = ALIGN (32);}

    ${CONSTRUCTING+ __Etext = .;}

    /* Deprecated, use __Etext.  */
    ${CONSTRUCTING+ PROVIDE(_etext = .);}
  }

  /* Any dot-relative start-expression (such as "ALIGN(2)", also including
     the "default" .data alignment expression) will use the initial, raw
     size of .text and will be incorrect if the alignment used is less
     than the alignment for .text (which might depend on input and obj
     format).  FIXME: Seems like a bug in ld.  Seems hard to fix.  Seems
     unimportant.  */
  .data :
  {
    ${CONSTRUCTING+ __Sdata = .;}
    *(.data);
    ${RELOCATING+*(.data.*)}
    ${RELOCATING+*(.gnu.linkonce.d*)}
    ${RELOCATING+*(.eh_frame) /* FIXME: Make .text */}
    ${RELOCATING+*(.gcc_except_table)}

    /* See comment at ALIGN before __Etext.  */
    ${CONSTRUCTING+. = ALIGN (32);}

    ${CONSTRUCTING+ __Edata = .;}

    /* Deprecated, use __Edata.  */
    ${CONSTRUCTING+ PROVIDE(_edata = .);}
  }

  .bss :
  {
    /* Deprecated, use __Sbss.  */
    ${CONSTRUCTING+ PROVIDE(_bss_start = .);}

    ${CONSTRUCTING+ __Sbss = .;}
    *(.bss)
    ${RELOCATING+*(.bss.*)}
    *(COMMON)
    ${CONSTRUCTING+ __Ebss = .;}

    /* Deprecated, use __Ebss or __Eall as appropriate.  */
    ${CONSTRUCTING+ PROVIDE(_end = .);}
    ${CONSTRUCTING+ PROVIDE(__end = .);}
  }
  ${CONSTRUCTING+ __Eall = .;}

  /* Unfortunately, stabs are not mappable from ELF to a.out.
     It can probably be fixed with some amount of work.  */
  /DISCARD/ :
  { *(.stab) *(.stab*) *(.debug) *(.debug*) *(.comment) *(.gnu.warning.*) }

  /* For the rsim and xsim simulators.  */
  ${CONSTRUCTING+ PROVIDE(__Endmem = 0x10000000);}

  /* For elinux.  */
  ${CONSTRUCTING+ PROVIDE(__Stacksize = 0);}
}
EOF
