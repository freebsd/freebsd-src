# MIPS ELF GOT reloc n64

	.data
	.align	3
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
	dla	$5,dg1+0
	dla	$5,dg1+12
	dla	$5,dg1+123456
	dla	$5,dg1+0($17)
	dla	$5,dg1+12($17)
	dla	$5,dg1+123456($17)
	
	ld	$5,dg1+0
	ld	$5,dg1+12
	ld	$5,dg1+0($17)
	ld	$5,dg1+12($17)
	ld	$5,dg1+34($5)
	sd	$5,dg1+56($5)

	ulw	$5,dg1+0
	ulw	$5,dg1+12
	ulw	$5,dg1+0($17)
	ulw	$5,dg1+12($17)
	ulw	$5,dg1+34($5)
	usw	$5,dg1+56($5)

	dla	$5,dl1+0
	dla	$5,dl1+12
	dla	$5,dl1+123456
	dla	$5,dl1+0($17)
	dla	$5,dl1+12($17)
	dla	$5,dl1+123456($17)
	
	ld	$5,dl1+0
	ld	$5,dl1+12
	ld	$5,dl1+0($17)
	ld	$5,dl1+12($17)
	ld	$5,dl1+34($5)
	sd	$5,dl1+56($5)

	ulw	$5,dl1+0
	ulw	$5,dl1+12
	ulw	$5,dl1+0($17)
	ulw	$5,dl1+12($17)
	ulw	$5,dl1+34($5)
	usw	$5,dl1+56($5)

	dla	$5,fn
	dla	$5,.Lfn
	dla	$25,fn
	dla	$25,.Lfn
	jal	fn
	jal	.Lfn


	dla	$5,dg2+0
	dla	$5,dg2+12
	dla	$5,dg2+123456
	dla	$5,dg2+0($17)
	dla	$5,dg2+12($17)
	dla	$5,dg2+123456($17)
	
	ld	$5,dg2+0
	ld	$5,dg2+12
	ld	$5,dg2+0($17)
	ld	$5,dg2+12($17)
	ld	$5,dg2+34($5)
	sd	$5,dg2+56($5)

	ulw	$5,dg2+0
	ulw	$5,dg2+12
	ulw	$5,dg2+0($17)
	ulw	$5,dg2+12($17)
	ulw	$5,dg2+34($5)
	usw	$5,dg2+56($5)

	dla	$5,dl2+0
	dla	$5,dl2+12
	dla	$5,dl2+123456
	dla	$5,dl2+0($17)
	dla	$5,dl2+12($17)
	dla	$5,dl2+123456($17)
	
	ld	$5,dl2+0
	ld	$5,dl2+12
	ld	$5,dl2+0($17)
	ld	$5,dl2+12($17)
	ld	$5,dl2+34($5)
	sd	$5,dl2+56($5)

	ulw	$5,dl2+0
	ulw	$5,dl2+12
	ulw	$5,dl2+0($17)
	ulw	$5,dl2+12($17)
	ulw	$5,dl2+34($5)
	usw	$5,dl2+56($5)

	dla	$5,fn2
	dla	$5,.Lfn2
	dla	$25,fn2
	dla	$25,.Lfn2
	jal	fn2
	jal	.Lfn2

# Check that filling delay slots doesn't break our relocations.

	dla	$5,dg1
	b	.Lfn
	ld	$5,dg2
	b	.Lfn2

	dla	$5,dl1
	b	.Lfn
	dla	$5,dl2+12
	b	.Lfn2
	dla	$5,dl1+123456
	b	.Lfn

	ld	$5,dl2
	b	.Lfn2
	ld	$5,dl1+12
	b	.Lfn
	ld	$5,dl2+34($5)
	b	.Lfn2

# Force at least 8 (non-deddlay-slot) zero bytes, to make 'objdump' print ...
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
	.align	3
sp2:
	.space	60
	.globl	dg2
dg2:
dl2:
	.space	60
