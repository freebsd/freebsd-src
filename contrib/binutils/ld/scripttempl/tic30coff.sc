cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH("${OUTPUT_ARCH}")

MEMORY
{
	rom : ORIGIN = 0x00000300, LENGTH = 16k
	ram : ORIGIN = 0x00000300 + 16k, LENGTH = 16k
	ramblk0 : ORIGIN = 0x02026000, LENGTH = 0x1000
	ramblk1 : ORIGIN = 0x02027000, LENGTH = 0x1000
}

SECTIONS 				
{ 					
.vectors 0x00000000 :
{
	*(vectors)
}

.text : 
{
	*(.text)
} > rom

.const :
{
	*(.const)
	__etext = . ;
} > rom

.mdata : AT( ADDR(.const) + SIZEOF(.const) )
{
	__data = . ;
	*(.data);
	__edata = . ;
} > ram

.bss :
{
	__bss = . ;
	*(.bss);
	*(COMMON);
	__ebss = . ;
} > ram

.ram0 :
{
	*(ram0)
} > ramblk0

.ram1 :
{
	*(ram1)
} > ramblk1

}

EOF
