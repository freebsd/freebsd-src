TORS="
    ___ctors = . ;
    *(.ctors)
    ___ctors_end = . ;
    ___dtors = . ;
    *(.dtors)
    ___dtors_end = . ;"

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}")
OUTPUT_ARCH(${ARCH})

/* Code and data, both 64k */

SECTIONS
{
.text ${RELOCATING+ 0x10000} :
	{
	  *(.text)
	  ${RELOCATING+ _etext = . ; }
	}

.rdata  ${RELOCATING+ 0x20000} :
	{
	  *(.rdata);
	  *(.strings)

	  ${CONSTRUCTING+${TORS}}
	}

.data  ${RELOCATING+ . } :
	{
	  *(.data)
	  ${RELOCATING+ _edata = . ; }
	}

.bss  ${RELOCATING+ .} :
	{
	  ${RELOCATING+ __start_bss = . ; }
	  *(.bss)
	  *(COMMON)
	  ${RELOCATING+ _end = . ;  }
	}

.stack  ${RELOCATING+ 0x2fff0} :
	{
	  ${RELOCATING+ _stack = . ; }
	  *(.stack)
	}

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
