.text

.proc full1
full1:

.prologue
.spill 0
.save.g 0
	nop		0
.save.g 0x10
	nop		0
.save.g -1
	nop		0
.save.g 0x3
	nop		0
.save.g 0x4
	nop		0
.save.g 0x1
	nop		0
.save.f 0
	nop		0
.save.f 0x100000
	nop		0
.save.f -1
	nop		0
.save.f 0x3
	nop		0
.save.f 0x4
	nop		0
.save.f 0x1
	nop		0
.save.b 0
	nop		0
.save.b 0x20
	nop		0
.save.b -1
	nop		0
.save.b 0x3
	nop		0
.save.b 0x4
	nop		0
.save.b 0x1
	nop		0
.spillreg r4, r0
	nop		0
.spillreg r3, r2
	nop		0
.spillreg r8, r9
	nop		0
.spillreg b6, r10
	nop		0
.spillreg f2, f0
	nop		0
.spillreg f3, f1
	nop		0
.spillreg f6, f7
	nop		0
.spillreg f4, r11
	nop		0
.spillreg f5, b0
	nop		0
.spillreg.p p0, r4, r3
	nop		0
.spillreg.p p1, r4, r0
	nop		0
.spillreg.p p1, f16, f0
	nop		0
.restorereg.p p0, r4
	nop		0
.body
	br.ret.sptk	rp
.endp full1

.proc full2
full2:
.prologue
.spill 0
.save.gf 0, 0
	nop		0
.save.gf 0x10, 0
	nop		0
.save.gf 0, 0x100000
	nop		0
.save.gf ~0, 0
	nop		0
.save.gf 0, ~0
	nop		0
.save.gf 1, 1
	nop		0
.save.gf 2, 0
	nop		0
.save.gf 1, 0
	nop		0
.save.gf 0, 1
	nop		0
.body
.label_state 1
.restore sp, 1
	nop.x		0
.copy_state 2
	br.ret.sptk	rp
.endp full2

.proc full3
full3:
.prologue
.spill 0
.save.g 0x10, r16
	nop		0
.save.g 0x01, r0
	nop		0
.save.g 0x06, r127
	nop		0
	nop		0
.save.b 0x20, r16
	nop		0
.save.b 0x01, r0
	nop		0
.save.b 0x18, r127
	nop		0
	nop		0
.body
	br.ret.sptk	rp
.endp full3

.proc simple1
simple1:
.prologue 0x10, r2
	br.ret.sptk	rp
.endp simple1

.proc simple2
simple2:
.prologue 0, r2
	br.ret.sptk	rp
.endp simple2

.proc simple3
simple3:
.prologue -1, r2
.vframe r0
	br.ret.sptk	rp
.endp simple3

.proc simple4
simple4:
.prologue 0x1, r0
	br.ret.sptk	rp
.endp simple4

.proc simple5
simple5:
.prologue 0xc, r127
	br.ret.sptk	rp
.endp simple5
