	.h8300h
	.text
h8300h_misc:
	eepmov.b
 	eepmov.w
	ldc.b #0,ccr
	ldc.b r0l,ccr
	ldc.w @er0,ccr
	ldc.w @(16:16,er0),ccr
	ldc.w @(32:24,er0),ccr
	ldc.w @er0+,ccr
	ldc.w @h8300h_misc:16,ccr
	ldc.w @h8300h_misc:24,ccr
;	movfpe 16:16,r0l
;	movtpe r0l,16:16
	nop
	rte
	rts
	sleep
	stc.b ccr,r0l
	stc.w ccr,@er0
	stc.w ccr,@(16:16,er0)
	stc.w ccr,@(32:24,er0)
	stc.w ccr,@-er0
	stc.w ccr,@h8300h_misc:16
	stc.w ccr,@h8300h_misc:24

