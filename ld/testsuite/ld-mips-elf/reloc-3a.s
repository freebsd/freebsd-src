	.globl	_start
	.globl	gd
	.ent	tstarta
tstarta:
_start:
	la	$4,lda-tstarta-0x8010
			# (.text2+8-0x8010) - tstarta [+ (tstarta+0x4)]
			#     relocation:  .text2 - 0x8004
			#     final value: .text2 - tstarta - 0x8008

	la	$4,lda-tstarta-0x8000
			# (.text2+8-0x8000) - tstarta [+ (tstarta+0xc)]
			#     relocation:  .text2 - 0x7fec
			#     final value: .text2 - tstarta - 0x7ff8

	la	$4,lda-tstarta
			# (.text2+8) - tstarta [+ (tstarta+0x14)]
			#     relocation:  .text2 + 0x1c
			#     final value: .text2 - tstarta + 0x8

	la	$4,lda-tstarta+0x7ff0
			# (.text2+8+0x7ff0) - tstarta [+ (tstarta+0x1c)]
			#     relocation:  .text2 + 0x8014
			#     final value: .text2 - tstarta + 0x7ff8

	la	$4,lda-tstarta+0x8010	# (.text2+8)-(tstarta+0x8010)+0x24
			# (.text2+8+0x8010) - tstarta [+ (tstarta+0x24)]
			#     relocation:  .text2 + 0x803c
			#     final value: .text2 - tstarta + 0x8018

	.end	tstarta

	.org	0xfff0
	.section .text2
	.word	1
gd:	.word	2
lda:	.word	3
	.word	4
