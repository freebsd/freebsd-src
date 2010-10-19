	.data
	.globl	big_external_data_label
big_external_data_label:
	.fill	1000

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4

	.globl	small_external_data_label
small_external_data_label:
	.fill	1

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
