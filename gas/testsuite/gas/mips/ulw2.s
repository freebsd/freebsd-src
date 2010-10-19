# Source file used to test the ulw macro (harder).
	
	.text
text_label:

	ulw	$4,0($5)
	ulw	$4,1($5)

	ulw	$5,0($5)	# warns
	ulw	$5,1($5)	# warns

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
