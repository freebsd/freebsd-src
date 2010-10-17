SECTIONS {
	.text : { *(.text) }
	.data : { *(.data) }
	.bss : { *(.bss) *(COMMON) }
}
value1 = DEFINED (defined) ? 1 : 2;
value2 = DEFINED (undefined) ? 1 : 2;
