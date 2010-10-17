
	.space $PRIVATE$
	.subspa $GLOBAL$
	.export $global$
$global$
	.space $TEXT$
	.subspa $CODE$

	.proc
	.callinfo
ivaaddr
	nop
	nop
	addil L%ivaaddr-$global$,%dp
	ldo   R%ivaaddr-$global$(%r1),%r19
	.procend
