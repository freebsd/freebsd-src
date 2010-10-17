cat << EOF
OUTPUT_FORMAT("elf32-ip2k", "elf32-ip2k", "elf32-ip2k")
OUTPUT_ARCH(ip2k)
ENTRY(_start)
SEARCH_DIR(.);

/* IP2022 default linker script.  */

MEMORY
{
	D_GPR    : org = 0x01000080, len = 128
        D_RAM    : org = 0x01000100, len = 4K - 256
        P_RAM    : org = 0x02000000, len = 16K
        P_ROM    : org = 0x02010000, len = 64K - 32
        P_RESET  : org = 0x0201FFE0, len = 32
        P_CONFIG : org = 0x02020000, len = 128
}

SECTIONS
{
	/* Allocated memory end markers
	   (initialized to start of appropiate memory address).  */
	__data_end  = 0x01000100;
	__pram_end  = 0x02000000;
	__flash_end = 0x02010000;

	/* Global general purpose registers in direct addressing range.  */
	.gpr 0x01000080 :
	{
		*(.gpr)
	} >D_GPR

	/* Pre-allocated, pre-initialized data memory.  */
	__data_run_begin = __data_end;
	__data_load_begin = (__flash_end + 1) & 0xFFFFFFFE;
	.data __data_run_begin : AT (__data_load_begin)
	{
		* (.data);
		* (.rodata)
	} >D_RAM
	__data_run_end  = __data_run_begin  + SIZEOF(.data);
	__data_load_end = __data_load_begin + SIZEOF(.data);
	__data_end      = __data_run_end;
	__flash_end     = __data_load_end;

	/* Pre-allocated, uninitialized data memory.  */
	__bss_begin = __data_end;
	.bss __bss_begin :
	{
		* (.bss)
	} >D_RAM
	__bss_end  = __bss_begin + SIZEOF(.bss);
	__data_end = __bss_end;

	/* Pre-allocated PRAM data memory.  */
	__pram_data_begin = (__pram_end + 1) & 0xFFFFFFFE;
	.pram_data __pram_data_begin :
	{
		* (.pram_data)
	} >P_RAM
	__pram_data_end = __pram_data_begin + SIZEOF(.pram_data);
	__pram_end      = __pram_data_end;

	/* PRAM code.  */
	__pram_run_begin  = (__pram_end + 1) & 0xFFFFFFFE;
	__pram_load_begin = (__flash_end + 1) & 0xFFFFFFFE;
	.pram __pram_run_begin : AT (__pram_load_begin)
	{
		* (.pram)
	} >P_RAM
	__pram_run_end  = __pram_run_begin  + SIZEOF(.pram);
	__pram_load_end = __pram_load_begin + SIZEOF(.pram);

	__pram_load_shift = ((__pram_run_begin - __pram_load_begin) & 0x1FFFF) | 0x02000000;
	__pram_end  = __pram_run_end;
	__flash_end = __pram_load_end;

	/* PRAM overlay code.  */
	__pram_overlay_run_start  = (__pram_end  + 1) & 0xFFFFFFFE;
	__pram_overlay_load_start = (__flash_end + 1) & 0xFFFFFFFE;	
	OVERLAY __pram_overlay_run_start : AT (__pram_overlay_load_start)
	{
		.pram1 { */overlay1/* (.pram); * (.pram1) }
		.pram2 { */overlay2/* (.pram); * (.pram2) }
	} >P_RAM
	__pram_overlay_run_end = .;
	__pram_overlay_load_end = __pram_overlay_load_start + SIZEOF(.pram1) + SIZEOF(.pram2);
	__pram_end  = __pram_overlay_run_end;
	__flash_end = __pram_overlay_load_end;

	/* Flash code.  */
	__text_begin = (__flash_end + 1) & 0xFFFFFFFE;
	.text __text_begin :
	{
		* (.text);
		* (.text.libgcc)
	} >P_ROM = 0xffff
	__text_end  = __text_begin + SIZEOF(.text);
	__flash_end = __text_end;

	/* Strings.  */
	__strings_begin = (__flash_end + 1) & 0xFFFFFFFE;
	.strings __strings_begin :
	{
		* (strings);
		* (.progmem.data)
	} >P_ROM = 0xffff
	__strings_end = __strings_begin + SIZEOF (.strings);
	__flash_end   = __strings_end;

	.ctors : { * (.ctors) } > P_ROM
	.dtors : { * (.dtors) } > P_ROM

	/* Reset code.  */
	.reset  : { * (.reset)  } >P_RESET  = 0xffff

	/* Configuration block.  */
	.config : { * (.config) } >P_CONFIG = 0xffff

	/* Stack.  */
	PROVIDE (__stack = 0x01000FFF);

	/* Stabs debugging sections.  */
	.stab           0 : { *(.stab) }
	.stabstr        0 : { *(.stabstr) }
	.stab.excl      0 : { *(.stab.excl) }
	.stab.exclstr   0 : { *(.stab.exclstr) }
	.stab.index     0 : { *(.stab.index) }
	.stab.indexstr  0 : { *(.stab.indexstr) }
	.comment        0 : { *(.comment) }

	/* DWARF 1.  */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions.  */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2.  */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2.  */
	.debug_info     0 : { *(.debug_info) *(.gnu.linkonce.wi.*) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
}
EOF
