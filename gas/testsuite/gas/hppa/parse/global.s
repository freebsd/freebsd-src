	.code
	.IMPORT foo,data

	.align 4
; Official gas code will not accept sym-$global$.
	addil L%foo-$global$,%r27

