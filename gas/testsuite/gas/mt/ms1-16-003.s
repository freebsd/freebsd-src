	.text
	.global iflush
iflush:
	iflush

	.global mul
mul:
	mul R0, R0, R0
	
	.global muli
muli:
	muli R0, R0, #0

	.global dbnz
dbnz_:
	dbnz	r0, dbnz

	.global fbcbincs
fbcbincs:
	fbcbincs #0, #0, #0, #0, #0, #0, #0, #0, #0, #0

	.global mfbcbincs
mfbcbincs:
	mfbcbincs r0, #0, #0, #0, #0, #0, #0, #0, #0

	
	.global fbcbincrs
fbcbincrs:
	fbcbincrs r0, #0, #0, #0, #0, #0, #0, #0, #0, #0

	.global mfbcbincrs
mfbcbincrs:
	mfbcbincrs r0, r0, #0, #0, #0, #0, #0, #0, #0
	
	
	.global wfbinc
wfbinc:
# Documentation error.
#	wfbinc #0, r0, #0, #0, #0, #0, #0, #0, #0, #0
	wfbinc #0, #0, #0, #0, #0, #0, #0, #0, #0, #0

	.global mwfbinc
mwfbinc:
# Documentation error.
#	mwfbinc r0, #0, #0, r0, #0, #0, #0, #0, #0
	mwfbinc r0, #0, #0, #0, #0, #0, #0, #0, #0

	.global wfbincr
wfbincr:
	wfbincr r0, #0, #0, #0, #0, #0, #0, #0, #0, #0

	.global mwfbincr
mwfbincr:
	mwfbincr r0, r0, #0, #0, #0, #0, #0, #0, #0
