	.text
	ld $2, 0x0123456789abcdef($3)
	ld $2, 0xabcdef0123456789($3)
	ld $2, 0xffffffff80000000($3)
	ld $2, 0xffffffff00000000($3)
	ld $2, 0xffffffff7fffffff($3)
	ld $2, 0xabcdef01($3)
	ld $2, 0x01234567($3)

	sd $2, 0x0123456789abcdef($3)
	sd $2, 0xabcdef0123456789($3)
	sd $2, 0xffffffff80000000($3)
	sd $2, 0xffffffff00000000($3)
	sd $2, 0xffffffff7fffffff($3)
	sd $2, 0xabcdef01($3)
	sd $2, 0x01234567($3)

	lw $2, 0x0123456789abcdef($3)
	lw $2, 0xabcdef0123456789($3)
	lw $2, 0xffffffff80000000($3)
	lw $2, 0xffffffff00000000($3)
	lw $2, 0xffffffff7fffffff($3)
	lw $2, 0xabcdef01($3)
	lw $2, 0x01234567($3)

	sw $2, 0x0123456789abcdef($3)
	sw $2, 0xabcdef0123456789($3)
	sw $2, 0xffffffff80000000($3)
	sw $2, 0xffffffff00000000($3)
	sw $2, 0xffffffff7fffffff($3)
	sw $2, 0xabcdef01($3)
	sw $2, 0x01234567($3)

	dla $2, 0x0123456789abcdef
	dla $2, 0xabcdef0123456789
	dla $2, 0xffffffff80000000
	dla $2, 0xffffffff00000000
	dla $2, 0xabcdef01
	dla $2, 0x7fffffff
	dla $2, 0x01234567

	.space 8
