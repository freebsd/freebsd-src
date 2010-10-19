# Source file used to test the ulh and ulhu macros (harder).

	.text
text_label:

	ulh	$4,0($5)		# warns
	ulh	$4,1($5)		# warns

	ulh	$5,0($5)		# warns
	ulh	$5,1($5)		# warns

	ulhu	$4,0($5)		# warns
	ulhu	$4,1($5)		# warns

	ulhu	$5,0($5)		# warns
	ulhu	$5,1($5)		# warns

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
