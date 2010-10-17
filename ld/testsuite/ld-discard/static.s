	.section	.data.exit,"aw"
data:
	.section	.text.exit,"ax"
text:
	.text
	.globl _start
_start:
	.long	data
	.section	.debug_info
	.long	0
	.long	text
