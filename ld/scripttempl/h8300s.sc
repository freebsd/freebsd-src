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
OUTPUT_ARCH(h8300s)
ENTRY("_start")

/* The memory size is 256KB to coincide with the simulator.
   Don't change either without considering the other.  */

MEMORY
{
	/* 0xc4 is a magic entry.  We should have the linker just
	   skip over it one day...  */
	vectors : o = 0x0000, l = 0xc4
	magicvectors : o = 0xc4, l = 0x3c
	/* We still only use 256k as the main ram size.  */
	ram    : o = 0x0100, l = 0x3fefc
	/* The stack starts at the top of main ram.  */
	topram : o = 0x3fffc, l = 0x4
	/* This holds variables in the "tiny" sections.  */
	tiny   : o = 0xff8000, l = 0x7f00
	/* At the very top of the address space is the 8-bit area.  */
	eight  : o = 0xffff00, l = 0x100
}

SECTIONS
{
.vectors :
	{
	  /* Use something like this to place a specific
	     function's address into the vector table.

	     LONG (ABSOLUTE (_foobar)).  */

	  *(.vectors)
	} ${RELOCATING+ > vectors}

.text :
	{
	  *(.rodata)
	  *(.text)
	  *(.strings)
   	  ${RELOCATING+ _etext = . ; }
	} ${RELOCATING+ > ram}

${CONSTRUCTING+${TORS}}

.data :
	{
	  *(.data)
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

.tiny :
	{
	  *(.tiny)
	} ${RELOCATING+ > tiny}

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
