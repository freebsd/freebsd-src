# Source file used to test relaxation.

	.text
foo:
	b	bar
	bal	bar
	beq	$2, $3, bar
	bne	$4, $5, bar
	blez	$2, bar
	bgtz	$3, bar
	bltz	$4, bar
	bgez	$5, bar
	bc1f	bar
	bc1t	bar

	bltzal	$2, bar
	bgezal	$3, bar

	beql	$2, $3, bar
	bnel	$4, $5, bar
	blezl	$2, bar
	bgtzl	$3, bar
	bltzl	$4, bar
	bgezl	$5, bar
	bc1fl	bar
	bc1tl	bar

	bltzall	$2, bar
	bgezall	$3, bar
	
        .space  0x20000         # to make a 128kb loop body
bar:
	b	foo
	bal	foo
	beq	$2, $3, foo
	bne	$4, $5, foo
	blez	$2, foo
	bgtz	$3, foo
	bltz	$4, foo
	bgez	$5, foo
	bc1f	foo
	bc1t	foo

	bltzal	$2, foo
	bgezal	$3, foo

	beql	$2, $3, foo
	bnel	$4, $5, foo
	blezl	$2, foo
	bgtzl	$3, foo
	bltzl	$4, foo
	bgezl	$5, foo
	bc1fl	foo
	bc1tl	foo

	bltzall	$2, foo
	bgezall	$3, foo
