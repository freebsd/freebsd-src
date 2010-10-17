	.section	.gcc_except_table,"aw",@progbits
	.section	.gnu.linkonce.t.blah,"ax",@progbits
.L0:
	jmp .L1
.L1:
	.section	.gcc_except_table,"aw",@progbits
	.uleb128 .L1-.L0

	.text
.L2:
	nop
	nop
	jmp	.L3
	jmp	.L4
	.asciz	"ABCDEFGHI"
	.fill	0x18 - (. - .L2)
.L3:
.L4:
