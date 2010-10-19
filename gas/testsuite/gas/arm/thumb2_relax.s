	.text
	.thumb
	.syntax unified
thumb2_relax:
	.macro	ls op w=".w"
1:
	\op	r1, [r5]
	\op	r1, [r5, #(far_\op + 4)]
	\op	r1, [r5, #far_\op]
	\op\w	r1, [r5, #far_\op]
	\op	r1, [r5, #-far_\op]
	\op	r1, [r5], #far_\op
	\op	r1, [r5], #far_\op
	\op	r1, [r5, #far_\op]!
	\op	r1, [r5, #-far_\op]!
	\op	r1, [r5, r4]
	\op	r1, [r9, ip]
	\op	r1, 1f
	\op\w	r1, 1f
	\op	r8, 1f
	\op	r1, 2f
	\op	r1, 1b
	.align 2
1:
	nop
2:
	.endm
.equ far_ldrb, 0x1f
.equ far_ldrsb, 0x1f
.equ far_ldrh, 0x3e
.equ far_ldrsh, 0x3e
.equ far_ldr, 0x7c
.equ far_strb, 0x1f
.equ far_strh, 0x3e
.equ far_str, 0x7c
	ls	ldrb
	ls	ldrsb
	ls	ldrh
	ls	ldrsh
	ls	ldr
	ls	strb
	ls	strh
	ls	str
	.purgem ls
1:
	adr	r1, 1f
	adr.w	r1, 1f
	adr	r8, 1f
	adr	r1, 2f
	adr	r1, 1b
.align 2
1:
	nop
2:
	nop
	@ Relaxation with conflicting alignment requirements.
	adr	r1, 1f
	adr	r1, 2f
1:
	nop
2:
	nop
