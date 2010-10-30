	.text
	.arch armv6t2
	.syntax unified
	.global	_start
	.type	_start, %function
_start:
base1:
arm1:
	movw r0, #:lower16:arm2
	movt r1, #:upper16:arm2
	movw r2, #:lower16:(arm2 - arm1)
	movt r3, #:upper16:(arm2 - arm1)
	movw r4, #:lower16:thumb2
	movt r5, #:upper16:thumb2
	movw r6, #:lower16:(thumb2 - arm1)
	movt r7, #:upper16:(thumb2 - arm1)
	.thumb
	.type thumb1, %function
	.thumb_func
thumb1:
	movw r7, #:lower16:arm2
	movt r6, #:upper16:arm2
	movw r5, #:lower16:(arm2 - arm1)
	movt r4, #:upper16:(arm2 - arm1)
	movw r3, #:lower16:thumb2
	movt r2, #:upper16:thumb2
	movw r1, #:lower16:(thumb2 - arm1)
	movt r0, #:upper16:(thumb2 - arm1)

	.section .far, "ax", %progbits
	.arm
arm2:
	movw r0, #:lower16:(arm1 - arm2)
	movt r0, #:upper16:(arm1 - arm2)
	movw r0, #:lower16:(thumb1 - arm2)
	movt r0, #:upper16:(thumb1 - arm2)
	.thumb
	.type thumb2, %function
	.thumb_func
thumb2:
	movw r0, #:lower16:(arm1 - arm2)
	movt r0, #:upper16:(arm1 - arm2)
	movw r0, #:lower16:(thumb1 - arm2)
	movt r0, #:upper16:(thumb1 - arm2)
