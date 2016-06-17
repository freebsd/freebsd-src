OUTPUT_ARCH(powerpc:common64)
SEARCH_DIR(/lib); SEARCH_DIR(/usr/lib); SEARCH_DIR(/usr/local/lib); SEARCH_DIR(/usr/local/powerpc-any-elf/lib);
/* Do we need any of these for elf?
   __DYNAMIC = 0;    */
SECTIONS
{
  /* Read-only sections, merged into text segment: */
  . = + SIZEOF_HEADERS;
  .interp : { *(.interp) }
  .hash          : { *(.hash)		}
  .dynsym        : { *(.dynsym)		}
  .dynstr        : { *(.dynstr)		}
  .rel.text      : { *(.rel.text)		}
  .rela.text     : { *(.rela.text) 	}
  .rel.data      : { *(.rel.data)		}
  .rela.data     : { *(.rela.data) 	}
  .rel.rodata    : { *(.rel.rodata) 	}
  .rela.rodata   : { *(.rela.rodata) 	}
  .rel.got       : { *(.rel.got)		}
  .rela.got      : { *(.rela.got)		}
  .rel.ctors     : { *(.rel.ctors)	}
  .rela.ctors    : { *(.rela.ctors)	}
  .rel.dtors     : { *(.rel.dtors)	}
  .rela.dtors    : { *(.rela.dtors)	}
  .rel.bss       : { *(.rel.bss)		}
  .rela.bss      : { *(.rela.bss)		}
  .rel.plt       : { *(.rel.plt)		}
  .rela.plt      : { *(.rela.plt)		}
/*  .init          : { *(.init)	} =0*/
  .plt : { *(.plt) }
  .text      :
  {
    *(.text)
    *(.fixup)
    *(.got1)
  }
  . = ALIGN(4096);
  _etext = .;
  PROVIDE (etext = .);
  .rodata    :
  {
    *(.rodata)
    *(.rodata1)
  }
  .fini      : { *(.fini)    } =0
  .ctors     : { *(.ctors)   }
  .dtors     : { *(.dtors)   }
  /* Read-write section, merged into data segment: */
  . = (. + 0x0FFF) & 0xFFFFFFFFFFFFF000;
  .data    :
  {
    *(.data)
    *(.data1)
    *(.sdata)
    *(.sdata2)
    *(.got.plt) *(.got)
    *(.dynamic)
    CONSTRUCTORS
  }
  . = ALIGN(4096);
  _edata  =  .;
  PROVIDE (edata = .);

  .fixup   : { *(.fixup) }
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __start___ksymtab = .;        /* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;
  __start___kallsyms = .;	/* All kernel symbols */
  __kallsyms : { *(__kallsyms) }
  __stop___kallsyms = .;

  . = ALIGN(16);
  __start___ftr_fixup = .;
  __ftr_fixup : { *(__ftr_fixup) }
  __stop___ftr_fixup = .;

  . = ALIGN(4096);
  .data.page_aligned : { *(.data.page_aligned) }

  . = ALIGN(128);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  . = ALIGN(4096);
  __init_begin = .;
  .text.init : { *(.text.init) }
  .data.init : { 
    *(.data.init);
    __vtop_table_begin = .;
    *(.vtop_fixup);
    __vtop_table_end = .;
    __ptov_table_begin = .;
    *(.ptov_fixup);
    __ptov_table_end = .;
  }
  . = ALIGN(16);
  __setup_start = .;
  .setup.init : { *(.setup.init) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : { *(.initcall.init) }
  __initcall_end = .;


  . = ALIGN(4096);
  __init_end = .;

  __chrp_begin = .;
  .text.chrp : { *(.text.chrp) }
  .data.chrp : { *(.data.chrp) }
  . = ALIGN(4096);
  __chrp_end = .;

  . = ALIGN(4096);
  __openfirmware_begin = .;
  .text.openfirmware : { *(.text.openfirmware) }
  .data.openfirmware : { *(.data.openfirmware) }
  . = ALIGN(4096);
  __openfirmware_end = .;

  __toc_start = .;
  .toc       :
  {
   *(.toc)
  }
  . = ALIGN(4096);
  __toc_end = .;

  __bss_start = .;
  .bss       :
  {
   *(.sbss) *(.scommon)
   *(.dynbss)
   *(.bss)
   *(COMMON)
  }

  . = ALIGN(4096);
  _end = . ;
  PROVIDE (end = .);
}
