	.globl data
	.section	.data.exit,"aw"
data:
	.globl text
	.section	.text.exit,"ax"
text:
	.text
	.globl _start
_start:
	.long	data
	.section	.debug_info
	.long	0
	.long	text
