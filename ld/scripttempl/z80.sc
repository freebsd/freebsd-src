if [ x${LD_FLAG} = x ]
then
cat << EOF
/* Create a cp/m executable; load and execute at 0x100.  */
OUTPUT_FORMAT("binary")
. = 0x100;
__Ltext = .;
ENTRY (__Ltext)
EOF
else 
    echo "OUTPUT_FORMAT(\"${OUTPUT_FORMAT}\")"
fi
cat <<EOF
OUTPUT_ARCH("${OUTPUT_ARCH}")
SECTIONS
{
.text :	{
	*(.text)
	*(text)
	${RELOCATING+ __Htext = .;}
	}
.data :	{
	${RELOCATING+ __Ldata = .;}
	*(.data)
	*(data)
	${RELOCATING+ __Hdata = .;}
	}
.bss :	{
	${RELOCATING+ __Lbss = .;}
	*(.bss)
	*(bss)
	${RELOCATING+ __Hbss = .;}
	}
}
EOF
