/* ld script to make s390 Linux kernel
 * Written by Martin Schwidefsky (schwidefsky@de.ibm.com)
 */
OUTPUT_FORMAT("elf32-s390", "elf32-s390", "elf32-s390")
OUTPUT_ARCH(s390)
ENTRY(_start)
SECTIONS
{
  . = 0x00000000;
  _text = .;			/* Text and read-only data */
  .text : {
	*(.text)
	*(.fixup)
	*(.gnu.warning)
	} = 0x0700
  .rodata : { *(.rodata) }
  .kstrtab : { *(.kstrtab) }

  . = ALIGN(16);		/* Exception table */
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __start___ksymtab = .;	/* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;

  __start___kallsyms = .;       /* All kernel symbols */
  __kallsyms : { *(__kallsyms) }
  __stop___kallsyms = .;

  . = ALIGN(1048576);		/* VM shared segments are 1MB aligned */

  _etext = .;			/* End of text section */

  .data : {			/* Data */
	*(.data)
	CONSTRUCTORS
	}

  _edata = .;			/* End of data section */

  . = ALIGN(8192);		/* init_task */
  .data.init_task : { *(.data.init_task) }

  . = ALIGN(4096);		/* Init code and data */
  __init_begin = .;
  .text.init : { *(.text.init) }
  .data.init : { *(.data.init) }
  . = ALIGN(4096);
  __init_end = .;

  __setup_start = .;
  .setup.init : { *(.setup.init) }
  __setup_end = .;
  __initcall_start = .;
  .initcall.init : { *(.initcall.init) }
  __initcall_end = .;
  . = ALIGN(4096);
  __init_end = .;

  . = ALIGN(32);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  . = ALIGN(4096);
  .data.page_aligned : { *(.data.idt) }


  __bss_start = .;		/* BSS */
  .bss : {
	*(.bss)
	}
  _end = . ;

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }
  .comment 0 : { *(.comment) }
}
