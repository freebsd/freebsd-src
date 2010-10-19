	.set	mips3
	.text
	ld $2, 0xfffffffeffffffff($4)
	ld $2, 0xfffffffe00000000($4)
	ld $2, 0xabcdef0123456789($4)
	ld $2, 0x0123456789abcdef($4)
	ld $2, 0x00000001ffffffff($4)
	ld $2, 0x0000000100000000($4)

	ld $2, 0xfffffffeffffffff
	ld $2, 0xfffffffe00000000
	ld $2, 0xabcdef0123456789
	ld $2, 0x0123456789abcdef
	ld $2, 0x00000001ffffffff
	ld $2, 0x0000000100000000

	sd $2, 0xfffffffeffffffff($4)
	sd $2, 0xfffffffe00000000($4)
	sd $2, 0xabcdef0123456789($4)
	sd $2, 0x0123456789abcdef($4)
	sd $2, 0x00000001ffffffff($4)
	sd $2, 0x0000000100000000($4)

	sd $2, 0xfffffffeffffffff
	sd $2, 0xfffffffe00000000
	sd $2, 0xabcdef0123456789
	sd $2, 0x0123456789abcdef
	sd $2, 0x00000001ffffffff
	sd $2, 0x0000000100000000

	lw $2, 0xfffffffeffffffff($4)
	lw $2, 0xfffffffe00000000($4)
	lw $2, 0xabcdef0123456789($4)
	lw $2, 0x0123456789abcdef($4)
	lw $2, 0x00000001ffffffff($4)
	lw $2, 0x0000000100000000($4)

	lw $2, 0xfffffffeffffffff
	lw $2, 0xfffffffe00000000
	lw $2, 0xabcdef0123456789
	lw $2, 0x0123456789abcdef
	lw $2, 0x00000001ffffffff
	lw $2, 0x0000000100000000

	sw $2, 0xfffffffeffffffff($4)
	sw $2, 0xfffffffe00000000($4)
	sw $2, 0xabcdef0123456789($4)
	sw $2, 0x0123456789abcdef($4)
	sw $2, 0x00000001ffffffff($4)
	sw $2, 0x0000000100000000($4)

	sw $2, 0xfffffffeffffffff
	sw $2, 0xfffffffe00000000
	sw $2, 0xabcdef0123456789
	sw $2, 0x0123456789abcdef
	sw $2, 0x00000001ffffffff
	sw $2, 0x0000000100000000

	la $2, 0xfffffffeffffffff($4)
	la $2, 0xfffffffe00000000($4)
	la $2, 0xabcdef0123456789($4)
	la $2, 0x0123456789abcdef($4)
	la $2, 0x00000001ffffffff($4)
	la $2, 0x0000000100000000($4)

	la $2, 0xfffffffeffffffff
	la $2, 0xfffffffe00000000
	la $2, 0xabcdef0123456789
	la $2, 0x0123456789abcdef
	la $2, 0x00000001ffffffff
	la $2, 0x0000000100000000

	.space 8
