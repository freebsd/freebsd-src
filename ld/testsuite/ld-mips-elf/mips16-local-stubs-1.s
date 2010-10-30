	.macro	makestub,type,func,section
	.text
	.set	\type
	.type	\func,@function
	.ent	\func
\func:
	jr	$31
	.end	\func

	.section \section,"ax",@progbits
	.set	nomips16
	.type	stub_for_\func,@function
	.ent	stub_for_\func
stub_for_\func:
	.set	noat
	la	$1,\func
	jr	$1
	.set	at
	.end	stub_for_\func
	.endm

	.macro	makestubs,id
	makestub nomips16,f\id,.mips16.call.F\id
	makestub nomips16,g\id,.mips16.call.fp.G\id
	makestub mips16,h\id,.mips16.fn.H\id
	.endm

	.macro	makecaller,type,func
	.text
	.set	\type
	.globl	\func
	.type	\func,@function
	.ent	\func
\func:
	jal	f1
	jal	f2
	jal	g1
	jal	g2
	jal	h1
	jal	h2
	.end	\func
	.endm

	makestubs 1
	makestubs 2
	makestubs 3

	makecaller nomips16,caller1
	makecaller mips16,caller2
