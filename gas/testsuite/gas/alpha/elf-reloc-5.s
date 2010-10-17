	.text
	.align 2
_start:
	br $31, nopv
	br $31, nopv	!samegp
	br $31, stdgp
	br $31, stdgp	!samegp

	br $31, undef	!samegp
	br $31, extern	!samegp

.ent nopv
nopv:
	.prologue 0
	nop
.end nopv

.ent stdgp
stdgp:
	ldgp $29,0($27)
	.prologue 1
	nop
.end stdgp

.globl extern
.ent extern
extern:
	.prologue 0
	nop
.end extern

