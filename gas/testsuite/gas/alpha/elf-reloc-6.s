	br $31, undef
	br $31, undef	!samegp
	br $31, nopv
	br $31, nopv	!samegp
	br $31, stdgp
	br $31, stdgp	!samegp
	br $31, nopro
	br $31, nopro	!samegp
	br $31, 1f
	br $31, 1f	!samegp

1:	nop

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

.ent nopro
nopro:
	nop
.end nopro

