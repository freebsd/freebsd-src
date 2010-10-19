        .global entry
@ Same as vfp-bad.s, but for Thumb-2
	.syntax unified
	.thumb
        .text
entry:
	fstd	d0, [r0], #8
	fstd	d0, [r0, #-8]!
	fsts	s0, [r0], #8
	fsts	s0, [r0, #-8]!
	fldd	d0, [r0], #8
	fldd	d0, [r0, #-8]!
	flds	s0, [r0], #8
	flds	s0, [r0, #-8]!
