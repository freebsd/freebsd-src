	.code16
	.text
	.globl _start
_start:
	.org 0x420
	int $0x42
	lret $2
	.org 0xf065
	jmp _start+((0x42) << 4)
