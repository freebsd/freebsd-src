	.text
h8300_misc:
	eepmov
	ldc #0,ccr
	ldc r0l,ccr
;	movfpe 16:16,r0l
;	movtpe r0l,16:16
	nop
	rte
	rts
	sleep
	stc ccr,r0l

