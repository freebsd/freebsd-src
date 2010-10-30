	.globl _entry
	.section .foo,"awx",%progbits
_entry:
	.byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.section .note,"",%note
	.byte 0
