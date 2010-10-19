	.global _start
_start:
	mov r8,#0xf
	jmps #seg:.12,#sof:.12
	mov r9,#0xf
.12:
	mov r5,#0xf
	mov r7,#0xf
	calls #seg:.13,#sof:.13
.13:
	mov r6,#0xf
	mov r8,#0xf
	