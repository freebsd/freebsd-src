# Instructions to load/store global/interrupt description table
# register.

	.text
foo:
	sidt (%rax)
	lidt (%rax)
	sgdt (%rax)
	lgdt (%rax)
	sidtq (%rax)
	lidtq (%rax)
	sgdtq (%rax)
	lgdtq (%rax)
	.p2align	4,0
