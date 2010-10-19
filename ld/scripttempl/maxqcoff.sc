test -z "$ENTRY" && ENTRY=_main
cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
${LIB_SEARCH_DIRS}
ENTRY(${ENTRY})
MEMORY 
  {
  rom (rx)  : ORIGIN = 0, LENGTH = 0x7FFE
  ram (!rx) : org = 0x0A000, l = 0x5FFF
  }

SECTIONS
{
	.text  ${RELOCATING+ 0x0000}: 
	{
		*(.text) 
	} >rom

	.data ${RELOCATING}: 
	{ 
		*(.data)  
		*(.rodata)
		*(.bss)
		*(COMMON)
		${RELOCATING+ edata  =  .};
	}>ram

/*	.bss ${RELOCATING+ SIZEOF(.data) + 0x0000}  :
	{ 
		*(.bss)  
		*(COMMON)
	}
*/
	.stab  0 ${RELOCATING+(NOLOAD)} :
	{
		[ .stab ]
	}
	.stabstr  0 ${RELOCATING+(NOLOAD)} :
	{
		[ .stabstr ]
	}
}
EOF
