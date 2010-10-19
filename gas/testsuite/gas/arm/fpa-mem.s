	.text
	.globl F
F:
	ldfs	f0, [r0]
	ldfs	f0, [r0], #-4
	ldfd	f0, [r0]
	ldfd	f0, [r0], #-4
	ldfe	f0, [r0]
	ldfe	f0, [r0], #-4
	ldfp	f0, [r0]
	ldfp	f0, [r0], #-4

	stfs	f0, [r0]
	stfs	f0, [r0], #-4
	stfd	f0, [r0]
	stfd	f0, [r0], #-4
	stfe	f0, [r0]
	stfe	f0, [r0], #-4
	stfp	f0, [r0]
	stfp	f0, [r0], #-4
	lfm	f0, 4, [r0]
	lfmfd	f0, 4, [r0]
	lfmea	f0, 4, [r0]
	sfm	f0, 4, [r0]
	sfmfd	f0, 4, [r0]
	sfmea	f0, 4, [r0]
	
	# Test mnemonic that is ambiguous between infix and suffic
	# condition codes
	stfpls	f0, [r0]
	.syntax unified
	stfpls	f0, [r0]
