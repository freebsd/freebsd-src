TORS=".tors :
	{
	  ___ctors = . ;
	  *(.ctors)
	  ___ctors_end = . ;
	  ___dtors = . ;
	  *(.dtors)
	  ___dtors_end = . ;
	} > ram"

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})
ENTRY("_start")

MEMORY
{
	/* 0xc4 is a magic entry.  We should have the linker just
	   skip over it one day...  */
	vectors : o = 0x0000, l = 0xc4
	magicvectors : o = 0xc4, l = 0x3c
	ram    : o = 0x0100, l = 0xfdfc
	/* The stack starts at the top of main ram.  */
	topram : o = 0xfefc, l = 0x4
	/* At the very top of the address space is the 8-bit area.  */
	eight : o = 0xff00, l = 0x100
}

SECTIONS
{
.vectors :
	{
	  /* Use something like this to place a specific
	     function's address into the vector table.

	     SHORT (ABSOLUTE (_foobar)).  */

	  *(.vectors)
	} ${RELOCATING+ > vectors}

.init :
	{ 
	  *(.init)
	} ${RELOCATING+ > ram}

.text :
	{
	  *(.rodata)
	  *(.text)
	  *(.text.*)
	  *(.strings)
   	  ${RELOCATING+ _etext = . ; }
	} ${RELOCATING+ > ram}

${CONSTRUCTING+${TORS}}

.data :
	{
	  *(.data)
	  *(.data.*)
	  *(.tiny)
	  ${RELOCATING+ _edata = . ; }
	} ${RELOCATING+ > ram}

.bss :
	{
	  ${RELOCATING+ _bss_start = . ;}
	  *(.bss)
	  *(COMMON)
	  ${RELOCATING+ _end = . ;  }
	} ${RELOCATING+ >ram}

.stack :
	{
	  ${RELOCATING+ _stack = . ; }
	  *(.stack)
	} ${RELOCATING+ > topram}

.eight :
	{
	  *(.eight)
	} ${RELOCATING+ > eight}

.stab 0 ${RELOCATING+(NOLOAD)} :
	{
	  [ .stab ]
	}

.stabstr 0 ${RELOCATING+(NOLOAD)} :
	{
	  [ .stabstr ]
	}
}
EOF
