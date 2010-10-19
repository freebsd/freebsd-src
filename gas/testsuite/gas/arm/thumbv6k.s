	.text
	.align 0
	.thumb
label:
	yield
	wfe
	wfi
	sev
	# arm-aout wants the segment padded to an 16-byte boundary;
	# do this explicitly so it's consistent for all object formats.
	nop
	nop
	nop
	nop
