# MIPS ELF GOT reloc n32

	.data
	.align	2
sp1:
	.space	60
	.globl	dg1
dg1:
dl1:
	.space	60


	.text

	.globl	fn
	.ent	fn
	.type	fn,@function
fn:
.Lfn:
	la	$5,dg1+0
	la	$5,dg1+12
	la	$5,dg1+123456
	la	$5,dg1+0($17)
	la	$5,dg1+12($17)
	la	$5,dg1+123456($17)

	lw	$5,dg1+0
	lw	$5,dg1+12
	lw	$5,dg1+0($17)
	lw	$5,dg1+12($17)
	lw	$5,dg1+34($5)
	sw	$5,dg1+56($5)

	ulw	$5,dg1+0
	ulw	$5,dg1+12
	ulw	$5,dg1+0($17)
	ulw	$5,dg1+12($17)
	ulw	$5,dg1+34($5)
	usw	$5,dg1+56($5)

	la	$5,dl1+0
	la	$5,dl1+12
	la	$5,dl1+123456
	la	$5,dl1+0($17)
	la	$5,dl1+12($17)
	la	$5,dl1+123456($17)

	lw	$5,dl1+0
	lw	$5,dl1+12
	lw	$5,dl1+0($17)
	lw	$5,dl1+12($17)
	lw	$5,dl1+34($5)
	sw	$5,dl1+56($5)

	ulw	$5,dl1+0
	ulw	$5,dl1+12
	ulw	$5,dl1+0($17)
	ulw	$5,dl1+12($17)
	ulw	$5,dl1+34($5)
	usw	$5,dl1+56($5)

	la	$5,fn
	la	$5,.Lfn
	la	$25,fn
	la	$25,.Lfn
	jal	fn
	jal	.Lfn


	la	$5,dg2+0
	la	$5,dg2+12
	la	$5,dg2+123456
	la	$5,dg2+0($17)
	la	$5,dg2+12($17)
	la	$5,dg2+123456($17)

	lw	$5,dg2+0
	lw	$5,dg2+12
	lw	$5,dg2+0($17)
	lw	$5,dg2+12($17)
	lw	$5,dg2+34($5)
	sw	$5,dg2+56($5)

	ulw	$5,dg2+0
	ulw	$5,dg2+12
	ulw	$5,dg2+0($17)
	ulw	$5,dg2+12($17)
	ulw	$5,dg2+34($5)
	usw	$5,dg2+56($5)

	la	$5,dl2+0
	la	$5,dl2+12
	la	$5,dl2+123456
	la	$5,dl2+0($17)
	la	$5,dl2+12($17)
	la	$5,dl2+123456($17)

	lw	$5,dl2+0
	lw	$5,dl2+12
	lw	$5,dl2+0($17)
	lw	$5,dl2+12($17)
	lw	$5,dl2+34($5)
	sw	$5,dl2+56($5)

	ulw	$5,dl2+0
	ulw	$5,dl2+12
	ulw	$5,dl2+0($17)
	ulw	$5,dl2+12($17)
	ulw	$5,dl2+34($5)
	usw	$5,dl2+56($5)

	la	$5,fn2
	la	$5,.Lfn2
	la	$25,fn2
	la	$25,.Lfn2
	jal	fn2
	jal	.Lfn2

# Check that filling delay slots doesn't break our relocations.

	la	$5,dg1
	b	.Lfn
	lw	$5,dg2
	b	.Lfn2

	la	$5,dl1
	b	.Lfn
	la	$5,dl2+12
	b	.Lfn2
	la	$5,dl1+123456
	b	.Lfn
	lw	$5,dl2
	b	.Lfn2
	lw	$5,dl1+12
	b	.Lfn
	lw	$5,dl2+34($5)
	b	.Lfn2

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8

	.end	fn

	.globl	fn2
	.ent	fn2
	.type	fn2,@function
fn2:
.Lfn2:
	.end	fn2

	.globl  __start
__start:
	
	.data
	.align	2
sp2:
	.space	60
	.globl	dg2
dg2:
dl2:
	.space	60

