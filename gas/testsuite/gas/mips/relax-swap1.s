# Source file used to test branch relaxation with swapping.

	.text
foo:

	move	$2, $4
	b	foo
	move	$2, $4
	b	bar

	lw	$2, ($4)
	b	foo
	lw	$2, ($4)
	b	bar

	sw	$2, ($4)
	b	foo
	sw	$2, ($4)
	b	bar

	move	$2, $4
	beq	$2, $3, foo
	move	$2, $4
	beq	$2, $3, bar
	move	$2, $4
	beq	$4, $5, foo
	move	$2, $4
	beq	$4, $5, bar

	addiu	$2, $4, 1
	beq	$2, $3, foo
	addiu	$2, $4, 1
	beq	$2, $3, bar
	addiu	$2, $4, 1
	beq	$4, $5, foo
	addiu	$2, $4, 1
	beq	$4, $5, bar

	lw	$2, ($4)
	beq	$2, $3, foo
	lw	$2, ($4)
	beq	$2, $3, bar
	lw	$2, ($4)
	beq	$4, $5, foo
	lw	$2, ($4)
	beq	$4, $5, bar

	sw	$2, ($4)
	beq	$2, $3, foo
	sw	$2, ($4)
	beq	$2, $3, bar
	sw	$2, ($4)
	beq	$4, $5, foo
	sw	$2, ($4)
	beq	$4, $5, bar

	mfc1	$2, $0
	move	$6, $7
	beq	$2, $3, foo
	mfc1	$2, $0
	move	$6, $7
	beq	$2, $3, bar
	mfc1	$2, $0
	move	$6, $7
	beq	$4, $5, foo
	mfc1	$2, $0
	move	$6, $7
	beq	$4, $5, bar

	move	$2, $4
	bc1t	foo
	move	$2, $4
	bc1t	bar

	.set	nomove
	move	$2, $4
	b	foo
	move	$2, $4
	b	bar
	.set	move

	move	$2, $4
0:	b	foo
	move	$2, $4
0:	b	bar

	.set	noreorder
	move	$6, $7
	.set	reorder
	move	$2, $4
	b	foo
	.set	noreorder
	move	$6, $7
	.set	reorder
	move	$2, $4
	b	bar

	sw	$2, 0f
0:	b	foo
	sw	$2, 0f
0:	b	bar

	lwc1	$0, ($4)
	b	foo
	lwc1	$0, ($4)
	b	bar

	cfc1	$2, $31
	b	foo
	cfc1	$2, $31
	b	bar

	ctc1	$2, $31
	b	foo
	ctc1	$2, $31
	b	bar

	mtc1	$2, $31
	b	foo
	mtc1	$2, $31
	b	bar

	mfhi	$2
	b	foo
	mfhi	$2
	b	bar

	move	$2, $4
	jr	$2
	move	$2, $4
	jr	$4

	move	$2, $4
	jalr	$2
	move	$2, $4
	jalr	$4

	move	$2, $31
	jalr	$3
	move	$31, $4
	jalr	$5

	move	$31, $4
	jalr	$2, $3
	move	$2, $31
	jalr	$2, $3

        .space  0x20000         # to make a 128kb loop body
bar:
# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
        .space  8
