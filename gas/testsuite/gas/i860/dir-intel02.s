// Intel assembler directives:
// Test that the .atmp directive is recognized and functions.

	.text

	.atmp r31
	or	0x12345678,r0,r24
	adds	r24,r9,r8

	.atmp r5
	or	0xf0f05a5a,r0,r24
	adds	r24,r9,r8

