.text
.proc personality
personality:
	br.ret.sptk	rp
.endp personality

.proc full1
full1:

.prologue
.spill 0
.save.g 0x1
	nop		0
.save.f 0x1
	nop		0
.save.b 0x01
	nop		0
.save.g 0x8
	nop		0
.save.f 0x8
	nop		0
.save.b 0x10
	nop		0
.altrp b7
	nop		0
.unwabi @svr4, 0
	nop		0

.body
.spillreg r4, r2
	nop		0
.spillreg.p p1, r7, r127
	nop		0
.spillsp b1, 0x08
	nop		0
.spillsp.p p2, b5, 0x10
	nop		0
.spillpsp f2, 0x18
	nop		0
.spillpsp.p p4, f5, 0x20
	nop		0
.restorereg f16
	nop		0
.restorereg.p p8, f31
	nop		0

.spillreg ar.bsp, r16
	nop		0
.spillreg ar.bspstore, r17
	nop		0
.spillreg ar.fpsr, r18
	nop		0
.spillreg ar.lc, r19
	nop		0
.spillreg ar.pfs, r20
	nop		0
.spillreg ar.rnat, r21
	nop		0
.spillreg ar.unat, r22
	nop		0
.spillreg psp, r23
	nop		0
.spillreg pr, r24
	nop		0
.spillreg rp, r25
	nop		0
.spillreg @priunat, r26
	nop		0

.label_state 1
	nop		0
.restore sp
	nop.x		0
.copy_state 1
	br.ret.sptk	rp

.personality personality
.handlerdata
	data4		-1
	data4		0

.endp full1

.proc full2
full2:

.prologue 0xb, r8
.spill 0
.save.gf 0x1, 0x00001
	nop		0
	nop		0
.save.b 0x11, r32
	nop		0
	nop		0
.save.gf 0x8, 0x80000
	nop		0
	nop		0
.spillreg f31, f127
	nop		0
.spillreg.p p63, f16, f32
	nop		0
.spillsp f5, 0x20
	nop		0
.spillsp.p p31, f2, 0x18
	nop		0
.spillpsp b5, 0x10
	nop		0
.spillpsp.p p15, b1, 0x08
	nop		0
.restorereg r7
	nop		0
.restorereg.p p7, r4
	nop		0

.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue
.body; .prologue; .body; .prologue; .body; .prologue; .body; .prologue

.body
.label_state 32
	nop		0
.restore sp, 32
	nop.x		0
.copy_state 32
	br.ret.sptk	rp
.endp full2

.proc full3
full3:

.prologue
.spill 0
.save.g 0x3, r32
	nop		0
	nop		0
.save.b 0x03, r34
	nop		0
	nop		0
.save.g 0xc, r124
	nop		0
	nop		0
.save.b 0x18, r126
	nop		0
	nop		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
.body
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	nop.x		0
	br.ret.sptk	rp
.endp full3

.proc fframe
fframe:
.prologue
.fframe 0
	nop		0
.body
	br.ret.sptk	rp
.endp fframe

.proc vframe
vframe:
.prologue
.vframe r16
	nop		0
.save ar.bsp, r17
	nop		0
.save ar.bspstore, r18
	nop		0
.save ar.fpsr, r19
	nop		0
.save ar.lc, r20
	nop		0
.save ar.pfs, r21
	nop		0
.save ar.rnat, r22
	nop		0
.save ar.unat, r23
	nop		0
.save pr, r24
	nop		0
.save @priunat, r25
	nop		0
.save rp, r26
	nop		0
.body
	br.ret.sptk	rp
.endp vframe

.proc vframesp
vframesp:
.prologue
.vframesp 0
	nop		0
.savesp ar.bsp, 0x08
	nop		0
.savesp ar.bspstore, 0x10
	nop		0
.savesp ar.fpsr, 0x18
	nop		0
.savesp ar.lc, 0x20
	nop		0
.savesp ar.pfs, 0x28
	nop		0
.savesp ar.rnat, 0x30
	nop		0
.savesp ar.unat, 0x38
	nop		0
.savesp pr, 0x40
	nop		0
.savesp @priunat, 0x48
	nop		0
.savesp rp, 0x50
	nop		0
.body
	br.ret.sptk	rp
.endp vframesp

.proc psp
psp:
.prologue
.vframesp 0
	nop		0
.savepsp ar.bsp, 0x08
	nop		0
.savepsp ar.bspstore, 0x10
	nop		0
.savepsp ar.fpsr, 0x18
	nop		0
.savepsp ar.lc, 0x20
	nop		0
.savepsp ar.pfs, 0x28
	nop		0
.savepsp ar.rnat, 0x30
	nop		0
.savepsp ar.unat, 0x38
	nop		0
.savepsp pr, 0x40
	nop		0
.savepsp @priunat, 0x48
	nop		0
.savepsp rp, 0x50
	nop		0
.body
	br.ret.sptk	rp
.endp psp

.proc simple
simple:
.unwentry
	br.ret.sptk	rp
.endp simple
