 .text
 .global _start
_start:
	enter	$zero + 0xff00, $zero + 0xf0
	enter	$zero - 0xff00, $zero - 0xf0
	leave
	ret
 .p2align 4,0x90
