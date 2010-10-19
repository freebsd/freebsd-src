	.global _start
_start:
	mov r5,#0xf
	mov r6,#0xf
	mov r7,#0xf
	mov r8,#0xf
	mov r9,#0xf
	mov r10,#0xf
	mov r11,#0xf
	mov r12,#0xf
.12:
	jmpr cc_Z,.13
	jmpr cc_NZ,.12
	jmpr cc_C,.12
	jmpr cc_C,0x45
	jmpr cc_NC,.end
	jmpr cc_UC,.end
	jmpr cc_EQ,.end
	jmpr cc_NE,.end
.13:
	jmpr cc_ULE,.end
	jmpr cc_SGE,.end
	jmpr cc_SLE,.end
.end:
	jmpr cc_NET,.12
	callr .end
	callr .end
