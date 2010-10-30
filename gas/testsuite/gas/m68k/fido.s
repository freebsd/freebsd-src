# Test parsing of the operands of the fido-specific instructions.
	.text
	.globl	foo
foo:	
	sleep
	trapx #0
	trapx #1
	trapx #2
	trapx #3
	trapx #4
	trapx #5
	trapx #6
	trapx #7
	trapx #8
	trapx #9
	trapx #10
	trapx #11
	trapx #12
	trapx #13
	trapx #14
	trapx #15
	movec #0xffe,%d0
	movec #0xffe,%a0
	movec #0xfff,%d1
	movec #0xfff,%a1
	movec %d2,#0xffe
	movec %a2,#0xffe
	movec %d3,#0xfff
	movec %a3,#0xfff
	movec %cac,%d4
	movec %cac,%a4
	movec %mbb,%d5
	movec %mbb,%a5
	movec %d6,%cac
	movec %a6,%cac
	movec %d7,%mbb
	movec %a7,%mbb
