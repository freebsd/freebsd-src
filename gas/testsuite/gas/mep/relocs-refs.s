
	.global main
	.global foo
	.global bar
main:
	nop
	nop
	lb $5, foo($3)
	bsr foo
	repeat $5, foo
	
	nop
	nop
	lb $5, (-foo & 0xffff)($3)
	bsr -foo
	repeat $5, -foo

	nop
	nop
	lb $5, (foo + bar)($3)
	bsr (foo + bar)
	repeat $5, (foo + bar)

	jmp (foo << 3)
	jmp (foo >> 3)
	jmp (foo - bar) & 0x7fffff
	jmp (foo - main) & 0x7fffff
	jmp (.text - foo) & 0x7fffff
	jmp (.data - foo) & 0x7fffff
	jmp (foo - %sizeof(.text))
	jmp (foo * 7)
	jmp (foo / 7)
	jmp (foo % 7)
	jmp (foo ^ bar)
	jmp (foo | bar)
	jmp (foo & bar)
	jmp (foo == bar) << 5
	jmp (foo < bar) << 5
	jmp (foo <= bar) << 5
	jmp (foo > bar) << 5
	jmp (foo >= bar) << 5
        # jmp (foo != bar)	# FIXME this appears to not work atm.
	jmp (foo && bar) << 5
	jmp (foo || bar) << 5

	nop
	nop
	nop
	nop

	jmp %sizeof(.data) >> (((main ^ (bar + 0xf)) - ((foo | .text) << 2)) / 3)

	nop
	nop
	nop
