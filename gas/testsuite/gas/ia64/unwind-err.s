.endp	xyz
.personality personality
.unwentry
.unwabi @svr4, 0
.handlerdata
.prologue
.body
.spillreg r4, r8
.spillreg.p p1, r4, r8
.spillsp r5, 0
.spillsp.p p2, r5, 0
.spillpsp r6, 0
.spillpsp.p p2, r6, 0
.restorereg r4
.restorereg.p p1, r4

.proc	personality
personality:
.endp	personality

.proc	start
start:

.label_state 1
.copy_state 1
.fframe 0
.vframe r0
.vframesp 0
.spill 0
.restore sp
.save rp, r0
.savesp pr, 0
.savepsp ar.fpsr, 0
.save.g 2
.save.gf 2,2
.save.f 2
.save.b 2
.altrp b7
.body


	.prologue
	.prologue
	.save		ar.lc, r31
	mov		r31 = ar.lc
	.body
	.body
	br.ret.sptk	rp
.personality personality
.handlerdata
.body

.endp	start

.proc	late_prologue
late_prologue:
	nop	0
	.prologue
	nop	0
.endp	late_prologue

.proc	late_body
late_body:
	nop	0
	.body
	nop	0
.endp	late_body
