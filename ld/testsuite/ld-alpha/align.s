	/* Force .data aligned to 4K, so that .got very likely gets
	   placed at 0x1200131d0.  */
	.data
	.balign	4096

	/* Force .text aligned to 4K, so it very likely gets placed at
	   0x120001000.  */
	.text
	.balign	4096
