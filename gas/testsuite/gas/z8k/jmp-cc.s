.text

	jr	f,dd
	jr lt,dd
	jr le,dd
	jp	 ule ,  dd
	jp  ov, dd
	jr pe,	dd
	jr	 mi , dd
	jr	 eq	,	dd
	jr	z ,dd
	jr	 c,dd
	jr ult,dd
jr	 t   , dd
	jr	 ge,dd
	jr gt,dd
	jr ugt,dd
	jp	 nov   ,	dd
	jr po ,dd
	jr pl,dd
	jr ne,dd
	JR NE,dd
	jr ov/pe,dd
	jr c/ult,dd
	jr nov/po,dd
	jr nc/uge,dd
	jr	 nz,	dd
	jr nc,dd   ! ssss
	jr uge ,dd   ! dddd
	 jr dd
	jr	dd 
dd:
	jr	t,dd
	jr	dd

	nop
	nop

