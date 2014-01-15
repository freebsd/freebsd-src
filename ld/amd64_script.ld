/* $Id: amd64_script.ld 2806 2012-12-24 08:23:59Z kaiwang27 $ */

OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
ENTRY(_start)
SEARCH_DIR("/lib"); SEARCH_DIR("/usr/lib");
SECTIONS {
	PROVIDE(__executable_start = 0x400000);
	. = 0x400000 + SIZEOF_HEADERS;
	.interp		: { *(.interp) }
	.hash		: { *(.hash) }
	.dynsym		: { *(.dynsym) }
	.dynstr		: { *(.dynstr) }
	.gnu.version	: { *(.gnu.version) }
	.gnu.version_d	: { *(.gnu.version_d) }
	.gnu.version_r	: { *(.gnu.version_r) }
	.rel.dyn        :
	{
		*(.rel.init)
		*(.rel.text .rel.text.* .rel.gnu.linkonce.t.*)
		*(.rel.fini)
		*(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*)
		*(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
		*(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
		*(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
		*(.rel.ctors)
		*(.rel.dtors)
		*(.rel.got)
		*(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
	}
	.rela.dyn	:
	{
		*(.rela.init)
		*(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
		*(.rela.fini)
		*(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
		*(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
		*(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
		*(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
		*(.rela.ctors)
		*(.rela.dtors)
		*(.rela.got)
		*(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
	}
	.rel.plt	: { *(.rel.plt) }
	.rela.plt	: { *(.rela.plt) }
	.init		:
	{
		KEEP(*(.init))
	} = 0x90909090
	.plt		: { *(.plt) }
	.text		:
	{
		*(.text .stub .text.* .gnu.linkonce.t.*)
	} = 0x90909090
	.fini		:
	{
		KEEP(*(.fini))
	} = 0x90909090
	PROVIDE(__etext = .);
	PROVIDE(_etext = .);
	PROVIDE(etext = .);
	.rodata		: { *(.rodata .rodata.* .gnu.linkonce.r.*) }
	.rodata1	: { *(.rodata1) }
	.eh_frame_hdr	: { *(.eh_frame_hdr) }
	.eh_frame	: { KEEP(*(.eh_frame)) }
	.gcc_except_table   : { *(.gcc_except_table) }
	. = ALIGN (CONSTANT(MAXPAGESIZE)) - ((CONSTANT(MAXPAGESIZE) - .) & (CONSTANT(MAXPAGESIZE) - 1));
	. = DATA_SEGMENT_ALIGN (CONSTANT(MAXPAGESIZE), CONSTANT(COMMONPAGESIZE));
	.tdata		: { *(.tdata .tdata.* .gnu.linkonce.td.*) }
	.tbss		: { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
	PROVIDE(__preinit_array_start = .);
	.preinit_array	: { *(.preinit_array) }
	PROVIDE(__preinit_array_end = .);
	PROVIDE(__init_array_start = .);
	.init_array	: { *(.init_array) }
	PROVIDE(__init_array_end = .);
	PROVIDE(__fini_array_start = .);
	.fini_array	: { *(.fini_array) }
	PROVIDE(__fini_array_end = .);
	.ctors		:
	{
		KEEP(*crtbegin*.o(.ctors))
		KEEP(*(EXCLUDE_FILE (*crtend*.o ) .ctors))
		KEEP(*(SORT(.ctors.*)))
		KEEP(*(.ctors))
	}
	.dtors		:
	{
		KEEP(*crtbegin*.o(.dtors))
		KEEP(*(EXCLUDE_FILE (*crtend*.o ) .dtors))
		KEEP(*(SORT(.dtors.*)))
		KEEP(*(.dtors))
	}
	.jcr		: { KEEP(*(.jcr)) }
	.data.rel.ro : {
		*(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*)
		*(.data.rel.ro* .gnu.linkonce.d.rel.ro.*)
	}
	.dynamic	: { *(.dynamic) }
	.got		: { *(.got) }
	.got.plt	: { *(.got.plt) }
	.data		:
	{
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
	}
	.data1		: { *(.data1) }
	_edata = .;
	PROVIDE(edata = .);
	__bss_start = .;
	.bss		:
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(64 / 8);
	}
	. = ALIGN(64 / 8);
	_end = .;
	PROVIDE(end = .);
	. = DATA_SEGMENT_END (.);

	.stab		0 : { *(.stab) }
	.stabstr	0 : { *(.stabstr) }
	.stab.excl	0 : { *(.stab.excl) }
	.stab.exclstr	0 : { *(.stab.exclstr) }
	.stab.index	0 : { *(.stab.index) }
	.stab.indexstr	0 : { *(.stab.indexstr) }
	.comment	0 : { *(.comment) }

	/* DWARF1 */
	.debug		0 : { *(.debug) }
	.line		0 : { *(.line) }

	/* GNU DWARF1 Extension */
	.debug_srcinfo	0 : { *(.debug_srcinfo) }
	.debug_sfnames	0 : { *(.debug_sfnames) }

	/* DWARF2 */
	.debug_aranges	0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_info	0 : { *(.debug_info .gnu.linkonce.wi.*) }
	.debug_abbrev	0 : { *(.debug_abbrev) }
	.debug_line	0 : { *(.debug_line) }
	.debug_frame	0 : { *(.debug_frame) }
	.debug_str	0 : { *(.debug_str) }
	.debug_loc	0 : { *(.debug_loc) }
	.debug_macinfo	0 : { *(.debug_macinfo) }

	/DISCARD/	: { *(.note.GNU-stack) }
}
