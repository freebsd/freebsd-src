# Source file used to test the uld macro (harder).
	
	.text
text_label:

	uld	$4,0($5)
	uld	$4,1($5)

	uld	$5,0($5)	# warns
	uld	$5,1($5)	# warns

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space  8
