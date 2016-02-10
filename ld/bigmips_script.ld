/* $Id$ */

OUTPUT_FORMAT("elf32-bigmips")
ENTRY(_start)
SEARCH_DIR("/lib"); SEARCH_DIR("/usr/lib");
SECTIONS {
	PROVIDE (__executable_start = 0x00400000);
	. = 0x00400000 + SIZEOF_HEADERS;
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
		*(.rel.sdata .rel.sdata.* .rel.gnu.linkonce.s.*)
		*(.rel.data .rel.data.* .rel.gnu.linkonce.d.*)
		*(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*)
		*(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*)
		*(.rel.ctors)
		*(.rel.dtors)
		*(.rel.got)
		*(.rel.sbss .rel.sbss.* .rel.gnu.linkonce.sb.*)
		*(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*)
	}
	.rela.dyn	:
	{
		*(.rela.init)
		*(.rela.text .rela.text.* .rela.gnu.linkonce.t.*)
		*(.rela.fini)
		*(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*)
		*(.rela.sdata .rela.sdata.* .rela.gnu.linkonce.s.*)
		*(.rela.data .rela.data.* .rela.gnu.linkonce.d.*)
		*(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*)
		*(.rela.tbss .rela.tbss.* .rela.gnu.linkonce.tb.*)
		*(.rela.ctors)
		*(.rela.dtors)
		*(.rela.got)
		*(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*)
		*(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*)
	}
	.rel.plt	: { *(.rel.plt) }
	.rela.plt	: { *(.rela.plt) }
	.init		:
	{
		KEEP(*(.init))
	} = 0x00000000
	.plt		: { *(.plt) }
	.text		:
	{
		_ftext = .;
		*(.text .stub .text.* .gnu.linkonce.t.*)
	} = 0x00000000
	.fini		:
	{
		KEEP(*(.fini))
	} = 0x00000000
	PROVIDE(__etext = .);
	PROVIDE(_etext = .);
	PROVIDE(etext = .);
	.rodata		: { *(.rodata .rodata.* .gnu.linkonce.r.*) }
	.rodata1	: { *(.rodata1) }
	.eh_frame_hdr	: { *(.eh_frame_hdr) }
	.eh_frame	: { KEEP(*(.eh_frame)) }
	.gcc_except_table   : { *(.gcc_except_table) }

	.tdata		: ALIGN(4096) { *(.tdata .tdata.* .gnu.linkonce.td.*) }
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
	.dynamic	: { *(.dynamic) }
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
	.got		: { *(.got.plt) *(.got) }
	.data		:
	{
		_fdata = .;
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
	}
	.data1		: { *(.data1) }
	_gp = ALIGN(16) + 0x7ff0;
	.sdata          :
	{
		*(.sdata .sdata.* .gnu.linkonce.s.*)
	}
	/* Align the end of data segment to page boundary. */
	. = ALIGN(. != 0 ? 4096 : 1);
	_edata = .;
	PROVIDE(edata = .);
	__bss_start = .;
	_fbss = .;
	.sbss           : ALIGN(8)
	{
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
	}
	.bss		:
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(32 / 8);
	}
	. = ALIGN(32 / 8);
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

	/DISCARD/	: { *(.note.GNU-stack) *(.reginfo) }
}
