defined6 = DEFINED (sym2) ? 1 : 0;
SECTIONS {
	.text : { *(.text) sym2 = .; }
	.data : { *(.data) }
	.bss : { *(.bss) *(COMMON) }
}
defined4 = DEFINED (sym1) ? 1 : 0;
sym1 = 42;
defined3 = DEFINED (defined1) ? defined1 + 1 : 256;
defined1 = DEFINED (defined1) ? defined1 + 1 : 512;
defined2 = DEFINED (defined1) ? defined1 + 1 : 1024;
defined5 = DEFINED (sym1) ? sym1 : 0;
defined7 = DEFINED (sym2);
defined8 = !DEFINED (defined8);
defined = DEFINED (defined) ? defined : 42;
