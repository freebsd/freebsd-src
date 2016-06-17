/* ld script to make m68k Linux kernel */
OUTPUT_FORMAT("elf32-m68k", "elf32-m68k", "elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(_start)
SECTIONS
{
  . = 0xE004000;
  _text = .;			/* Text and read-only data */
  .text : {
	*(.head)
	*(.text)
	*(.fixup)
	*(.gnu.warning)
	} = 0x4e75
  .kstrtab : { *(.kstrtab) }


  _etext = .;			/* End of text section */

  .data : {			/* Data */
	*(.rodata)
	*(.rodata.*)
	*(.data)
	CONSTRUCTORS
  	. = ALIGN(16);		/* Exception table */
  	__start___ex_table = .;
  	*(__ex_table) 
  	__stop___ex_table = .;
  	__start___ksymtab = .;	/* Kernel symbol table */
  	*(__ksymtab) 
  	__stop___ksymtab = .;
	}
  /* End of data goes *here* so that freeing init code works properly. */
  _edata = .;

  . = ALIGN(8192);	/* Init code and data */
__init_begin = .;
  	.text.init : { *(.text.init) }
  	.data.init : { *(.data.init) }
	. = ALIGN(16);
	__setup_start = .;
	.setup.init : { *(.setup.init) }
	__setup_end = .;
	__initcall_start = .;
	.initcall.init : { *(.initcall.init) }
	__initcall_end = .;
	. = ALIGN(8192);
	__init_end = .;
	.init.task : { *(init_task) }
	

  .bss : { *(.bss) }		/* BSS */

  _end = . ;

  .crap : {
  	/* Stabs debugging sections.  */
	*(.stab)
	*(.stabstr)
	*(.stab.excl)
	*(.stab.exclstr)
	*(.stab.index)
	*(.stab.indexstr)
	*(.comment)
	*(.note)
  }

}
