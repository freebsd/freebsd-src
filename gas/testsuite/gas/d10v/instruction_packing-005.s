	.globl in_data
	.data
	.align 1
	.type	 in_data,@object
	.size	 in_data,2
in_data:
	.word 8192
	.text
	.align 2
	.globl foo
	.type	 foo,@function
foo:
	st r13,@-sp
	ld r0,@((in_data),r14)
	bl func_a
	ld r13,@sp+
	jmp r13
.Lfe1:
	.size	 foo,.Lfe1-foo
	.align 2
	.globl func_a
	.type	 func_a,@function
func_a:
	mv r2,r0
	ldi r3,0
.L7:
	and3 r1,r2,-32768
	addi r3,1
	slli r2,1
	cmpeqi r1,-32768
	mv r0,r2
	bnoti r0,15
	mvf0t r2,r0
	cmpui r3,8
	brf0t .L7
	mv r0,r2
	jmp r13



