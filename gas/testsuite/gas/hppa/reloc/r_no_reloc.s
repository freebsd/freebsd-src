	.COPYRIGHT "MetaWare Incorporated, 1992"
	.VERSION	"hc2.6a -O1 t3.c\n"

	.data
	.ALIGN	8
$L00DATA:
	.ALIGN	8
	.EXPORT	s
s:
	.WORD	0x0
	.BLOCKZ	786425
	.BLOCKZ	7

	.code
L$001.3:
g:	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	;ldo	120(%r0),%r28 --> to delay slot
	bv	%r0(%r2)
	.EXIT
	ldo	120(%r0),%r28
	.PROCEND


	.data
	.ALIGN	4
	.EXPORT	l
l:
	.WORD	P'g
	.IMPORT	common,DATA	; common section, size=0
	.IMPORT	$global$,DATA
	.EXPORT	g,ENTRY,PRIV_LEV=3,RTNVAL=GR
	.END
