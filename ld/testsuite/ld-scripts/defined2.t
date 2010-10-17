SECTIONS {
	.text : { *(.text) sym1 = 3 - DEFINED (x); }
	.data : { *(.data) }
	.bss : { *(.bss) *(COMMON) }
}
defined1 = !DEFINED (x);
defined2 = DEFINED (defined1) + 16;
defined3 = DEFINED (defined2) * 256;
defined4 = 0x200 - DEFINED (defined3);
