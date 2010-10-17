# Source file used to test branch likely relaxation with swapping.

	.text
foo:

	move	$2, $4
	beql	$2, $3, foo
	move	$2, $4
	beql	$2, $3, bar
	move	$2, $4
	beql	$4, $5, foo
	move	$2, $4
	beql	$4, $5, bar

	addiu	$2, $4, 1
	beql	$2, $3, foo
	addiu	$2, $4, 1
	beql	$2, $3, bar
	addiu	$2, $4, 1
	beql	$4, $5, foo
	addiu	$2, $4, 1
	beql	$4, $5, bar

	lw	$2, ($4)
	beql	$2, $3, foo
	lw	$2, ($4)
	beql	$2, $3, bar
	lw	$2, ($4)
	beql	$4, $5, foo
	lw	$2, ($4)
	beql	$4, $5, bar

	sw	$2, ($4)
	beql	$2, $3, foo
	sw	$2, ($4)
	beql	$2, $3, bar
	sw	$2, ($4)
	beql	$4, $5, foo
	sw	$2, ($4)
	beql	$4, $5, bar

	teq	$2, $4
	beq	$4, $5, foo
	teq	$2, $4
	beq	$4, $5, bar

        .space  0x20000         # to make a 128kb loop body
bar:
# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
        .space  8
