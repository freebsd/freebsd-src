# default linker script for c54x, TI COFF(1).
# patterned after description in TI Assembler Tools PDF, SPRU102C, 7-53
test -z "$ENTRY" && ENTRY=_c_int00

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH("${OUTPUT_ARCH}")

MEMORY
{
	/*PAGE 0 : */ prog (RXI) : ORIGIN = 0x00000080, LENGTH = 0xFF00
	/*PAGE 1 : */ data (W) : ORIGIN = 0x01000080, LENGTH = 0xFF80
}

ENTRY(${ENTRY})

SECTIONS 				
{ 					
	.text : 
	{
		___text__ = .;
		*(.text)
		etext = .;
		___etext__ = .;
	} > prog
	.data : 
	{
		___data__ = .;
		__data = .;
		*(.data)
		__edata = .;
		edata = .;
		___edata__ = .;
	} > prog
	/* all other initialized sections should be allocated here */
	.cinit : 
	{
		*(.cinit)
	} > prog
	.bss : 
	{
		___bss__ = .;
		__bss = .;
		*(.bss)
		*(COMMON)
		__ebss = .;
		end = .;
		___end__ = .;
	} > data
	/* all other uninitialized sections should be allocated here */
}
EOF
