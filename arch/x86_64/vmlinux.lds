/* ld script to make x86-64 Linux kernel
 * Written by Martin Mares <mj@atrey.karlin.mff.cuni.cz>;
 */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
  . = 0xffffffff80100000;
  _text = .;			/* Text and read-only data */
  .text : {
	*(.text)
	*(.fixup)
	*(.gnu.warning)
	} = 0x9090
  .text.lock : { *(.text.lock) }	/* out-of-line lock text */

  _etext = .;			/* End of text section */

  .rodata : { *(.rodata) *(.rodata.*) }
  .kstrtab : { *(.kstrtab) }

  . = ALIGN(16);		/* Exception table */
  __start___ex_table = .;
  __ex_table : { *(__ex_table) }
  __stop___ex_table = .;

  __start___ksymtab = .;	/* Kernel symbol table */
  __ksymtab : { *(__ksymtab) }
  __stop___ksymtab = .;

  __start___kallsyms = .;	/* All kernel symbols */
  __kallsyms : { *(__kallsyms) }
  __stop___kallsyms = .;

  .data : {			/* Data */
	*(.data)
	CONSTRUCTORS
	}

  _edata = .;			/* End of data section */

  __bss_start = .;		/* BSS */
  .bss : {
	*(.bss)
	}
  __bss_end = .;

  . = ALIGN(64);
  .data.cacheline_aligned : { *(.data.cacheline_aligned) }

  .vsyscall_0 -10*1024*1024: AT ((LOADADDR(.data.cacheline_aligned) + SIZEOF(.data.cacheline_aligned) + 4095) & ~(4095)) { *(.vsyscall_0) }
  __vsyscall_0 = LOADADDR(.vsyscall_0);
  . = ALIGN(64);
  .vxtime_sequence : AT ((LOADADDR(.vsyscall_0) + SIZEOF(.vsyscall_0) + 63) & ~(63)) { *(.vxtime_sequence) }
  vxtime_sequence = LOADADDR(.vxtime_sequence);
  . = ALIGN(16);
  .vxtime : AT ((LOADADDR(.vxtime_sequence) + SIZEOF(.vxtime_sequence) + 15) & ~(15)) { *(.vxtime) }
  vxtime = LOADADDR(.vxtime);
  . = ALIGN(16);
  .wall_jiffies : AT ((LOADADDR(.vxtime) + SIZEOF(.vxtime) + 15) & ~(15)) { *(.wall_jiffies) }
  wall_jiffies = LOADADDR(.wall_jiffies);
  . = ALIGN(16);
  .sys_tz : AT ((LOADADDR(.wall_jiffies) + SIZEOF(.wall_jiffies) + 15) & ~(15)) { *(.sys_tz) }
  sys_tz = LOADADDR(.sys_tz);
  . = ALIGN(16);
  .jiffies : AT ((LOADADDR(.sys_tz) + SIZEOF(.sys_tz) + 15) & ~(15)) { *(.jiffies) }
  jiffies = LOADADDR(.jiffies);
  . = ALIGN(16);
  .xtime : AT ((LOADADDR(.jiffies) + SIZEOF(.jiffies) + 15) & ~(15)) { *(.xtime) }
  xtime = LOADADDR(.xtime);
  .vsyscall_1 ADDR(.vsyscall_0) + 1024: AT (LOADADDR(.vsyscall_0) + 1024) { *(.vsyscall_1) }
  . = LOADADDR(.vsyscall_0) + 4096;

  . = ALIGN(8192);		/* init_task */
  .data.init_task : { *(.data.init_task) }

  . = ALIGN(4096); 
  .data.boot_pgt : { *(.data.boot_pgt) }

  . = ALIGN(4096);		/* Init code and data */
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
  . = ALIGN(4096);
  __init_end = .;

  _end = . ;

  /* Sections to be discarded */
  /DISCARD/ : {
	*(.text.exit)
	*(.data.exit)
	*(.exitcall.exit)
	*(.eh_frame)
	}

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
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


  .comment 0 : { *(.comment) }
}
