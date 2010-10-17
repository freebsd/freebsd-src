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

/* Compact model - code < 64k, data > 64k */

SECTIONS
{
.text 0x10000 :
	{
	  *(.text)
	  *(.strings)
	  ${RELOCATING+ _etext = . ; }
	} ${RELOCATING+ > ram}

.data 0x20000 :
	{
	  *(.data)
	  ${RELOCATING+ _edata = . ; }
	} ${RELOCATING+ > ram}

.rdata 0x30000  :
	{
	  *(.rdata);

	  ${CONSTRUCTING+${TORS}}
	}  ${RELOCATING+ > ram}

.bss  0x40000 :
	{
	  ${RELOCATING+ __start_bss = . ; }
	  *(.bss)
	  *(COMMON)
	  ${RELOCATING+ _end = . ;  }
	} ${RELOCATING+ >ram}

.stack 0x5fff0 :
	{
	  ${RELOCATING+ _stack = . ; }
	  *(.stack)
	} ${RELOCATING+ > topram}

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
