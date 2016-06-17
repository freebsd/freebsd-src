/* ld script to make UltraLinux kernel */
OUTPUT_FORMAT("elf64-sparc", "elf64-sparc", "elf64-sparc")
OUTPUT_ARCH(sparc:v9a)
ENTRY(_start)

SECTIONS
{
  swapper_pmd_dir = 0x0000000000402000;
  empty_pg_dir = 0x0000000000403000;
  . = 0x4000;
  .text 0x0000000000404000 :
  {
    *(.text)
    *(.gnu.warning)
  } =0
  _etext = .;
  PROVIDE (etext = .);
  .rodata    : { *(.rodata) *(.rodata.*) }
  .rodata1   : { *(.rodata1) }
  .data    :
  {
    *(.data)
    CONSTRUCTORS
  }
  .data1   : { *(.data1) }
  _edata  =  .;
  PROVIDE (edata = .);
  .fixup   : { *(.fixup) }
  . = ALIGN(16);
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;
  __start___ksymtab = .;
  __ksymtab  : { *(__ksymtab) }
  __stop___ksymtab = .;
  __kstrtab  : { *(.kstrtab) }
  __start___kallsyms = .;	/* All kernel symbols */
  __kallsyms : { *(__kallsyms) }
  __stop___kallsyms = .;
  . = ALIGN(8192);
  __init_begin = .;
  .text.init : { *(.text.init) }
  .data.init : { *(.data.init) }
  . = ALIGN(16);
  __setup_start = .;
  .setup_init : { *(.setup.init) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : { *(.initcall.init) }
  __initcall_end = .;
  . = ALIGN(8192);
  __init_end = .;
  . = ALIGN(64);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }
  __bss_start = .;
  .sbss      : { *(.sbss) *(.scommon) }
  .bss       :
  {
   *(.dynbss)
   *(.bss)
   *(COMMON)
  }
  _end = . ;
  PROVIDE (end = .);
  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
  .debug          0 : { *(.debug) }
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  .line           0 : { *(.line) }
  /DISCARD/ : { *(.text.exit) *(.data.exit) *(.exitcall.exit) }
}
