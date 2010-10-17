# bte, btne
	.text

	btne	0,%r31,some_label
	btne	1,%r29,some_label
	btne	2,%r27,some_label
	btne	3,%r25,some_label
	btne	10,%r23,some_label
	btne	11,%r21,some_label
	btne	12,%r19,some_label
	btne	29,%r17,some_label
	btne	30,%r16,some_label
	btne	31,%r8,some_label
	btne	15,%r0,some_fake_extern

	bte	0,%r31,some_label
	bte	1,%r29,some_label
	bte	2,%r27,some_label
	bte	3,%r25,some_label
	bte	10,%r23,some_label
	bte	11,%r21,some_label
	bte	12,%r19,some_label
	bte	29,%r17,some_label
	bte	30,%r16,some_label
	bte	31,%r8,some_label
	bte	15,%r0,some_fake_extern

	btne	%r0,%r31,some_label
	btne	%r1,%r29,some_label
	btne	%r2,%r27,some_label
	btne	%r3,%r25,some_label
	btne	%r10,%r23,some_label
	btne	%r11,%r21,some_label
	btne	%r12,%r19,some_label
	btne	%r29,%r17,some_label
	btne	%r30,%r16,some_label
	btne	%r31,%r8,some_label
	btne	%r15,%r0,some_fake_extern

	bte	%r0,%r31,some_label
	bte	%r1,%r29,some_label
	bte	%r2,%r27,some_label
	bte	%r3,%r25,some_label
	bte	%r10,%r23,some_label
	bte	%r11,%r21,some_label
	bte	%r12,%r19,some_label
	bte	%r29,%r17,some_label
	bte	%r30,%r16,some_label
	bte	%r31,%r8,some_label
	bte	%r15,%r0,some_fake_extern

	nop
	nop
some_label:
	nop	
