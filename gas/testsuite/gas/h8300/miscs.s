	.h8300s
	.text
h8300s_misc:
	eepmov.b
 	eepmov.w
	ldc.b #0,ccr
	ldc.b r0l,ccr
	ldc.b #0,exr
	ldc.b r0l,exr
	ldc.w @er0,ccr
	ldc.w @(16:16,er0),ccr
	ldc.w @(32:32,er0),ccr
	ldc.w @er0+,ccr
	ldc.w @h8300s_misc:16,ccr
	ldc.w @h8300s_misc:32,ccr
	ldc.w @er0,exr
	ldc.w @(16:16,er0),exr
	ldc.w @(32:32,er0),exr
	ldc.w @er0+,exr
	ldc.w @h8300s_misc:16,exr
	ldc.w @h8300s_misc:32,exr
;	movfpe 16:16,r0l
;	movtpe r0l,16:16
	nop
	rte
	rts
	sleep
	stc.b ccr,r0l
	stc.b exr,r0l
	stc.w ccr,@er0
	stc.w ccr,@(16:16,er0)
	stc.w ccr,@(32:32,er0)
	stc.w ccr,@-er0
	stc.w ccr,@h8300s_misc:16
	stc.w ccr,@h8300s_misc:32
	stc.w exr,@er0
	stc.w exr,@(16:16,er0)
	stc.w exr,@(32:32,er0)
	stc.w exr,@-er0
	stc.w exr,@h8300s_misc:16
	stc.w exr,@h8300s_misc:32
