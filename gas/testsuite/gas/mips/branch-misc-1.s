# Source file used to test the branches to locals in this file.

	.text
l1:
	.space 20
l2:
	.space 20
l3:
	.space 20

x:
	bal	l1
	bal	l2
	bal	l3
	bal	l4
	bal	l5
	bal	l6

	.space 20
l4:
	.space 20
l5:
	.space 20
l6:

# Force at least 8 (non-delay-slot) zero bytes, to make 'objdump' print ...
	.space	8
