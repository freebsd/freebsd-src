# tests proper handling of aligns on D30V

	.text
	.align 3
start:
	abs	r21,r42
	.align 3
	abs	r21,r42
	.align 4
	abs	r21,r42
	.align 4
	abs	r21,r42
	
	.data
	.long	0xdeadbeef
	
	.text
	abs	r21,r42

	.data
	.align 4
	.long	0xdeadbeef
	
	.text
	.align 3
	abs	r21,r42
	.end

