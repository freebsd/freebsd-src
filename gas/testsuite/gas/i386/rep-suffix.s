# Disassembling with -Msuffix.
	.text
_start:
	rep lodsb
	rep stosb
	rep lodsw
	rep stosw
	rep lodsl
	rep stosl
