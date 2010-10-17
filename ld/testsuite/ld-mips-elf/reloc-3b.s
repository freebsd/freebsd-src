	.globl	_start
	.globl	gd
	.ent	tstartb
tstartb:
	la	$4,gd-tstartb-0x8010
			# (gd-0x8010) - tstartb [+ (tstartb+0x4)]
			#     relocation:  gd - 0x800c
			#     final value: gd - tstartb - 0x8010

	la	$4,gd-tstartb-0x8000
			# (gd-0x8000) - tstartb [+ (tstartb+0xc)]
			#     relocation:  gd - 0x7ff4
			#     final value: gd - tstartb - 0x8000

	la	$4,gd-tstartb
			# (gd) - tstartb [+ (tstartb+0x14)]
			#     relocation:  gd + 0x14
			#     final value: gd - tstartb

	la	$4,gd-tstartb+0x7ff0
			# (gd+0x7ff0) - tstartb [+ (tstartb+0x1c)]
			#     relocation:  gd + 0x800c
			#     final value: gd - tstartb + 0x7ff0

	la	$4,gd-tstartb+0x8010
			# (gd+0x8010) - tstartb [+ (tstartb+0x24)]
			#     relocation:  gd + 0x8034
			#     final value: gd - tstartb + 0x8010

	la	$4,ldb-tstartb-0x8010
			# (.text2+0x10-0x8010) - tstartb [+ (tstartb+0x2c)]
			#     relocation:  .text2 - 0x7fd4
			#     final value: .text2 - tstartb - 0x8000

	la	$4,ldb-tstartb-0x8000
			# (.text2+0x10-0x8000) - tstartb [+ (tstartb+0x34)]
			#     relocation:  .text2 - 0x7fbc
			#     final value: .text2 - tstartb - 0x7ff0

	la	$4,ldb-tstartb
			# (.text2+0x10) - tstartb [+ (tstartb+0x3c)]
			#     relocation:  .text2 + 0x4c
			#     final value: .text2 - tstartb + 0x10

	la	$4,ldb-tstartb+0x7ff0
			# (.text2+0x10+0x7ff0) - tstartb [+ (tstartb+0x44)]
			#     relocation:  .text2 + 0x8044
			#     final value: .text2 - tstartb + 0x8000

	la	$4,ldb-tstartb+0x8010
			# (.text2+0x10+0x8010) - tstartb [+ (tstartb+0x4c)]
			#     relocation:  .text2 + 0x806c
			#     final value: .text2 - tstartb + 0x8020

	.end	tstartb
	.space	16
	.section .text2
ldb:	.word	5
