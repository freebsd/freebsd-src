;;; Test 68HCS12 and 68HC12 mixes (compatible case)
;;;
	.sect .text
	.globl _start
_start:
	bsr	main
	bra	_start
