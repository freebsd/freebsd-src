# 'Compare & Branch' instructions.
 .data
foodata: .word 42
	 .text
footext:

	.global cmpbeqb
cmpbeqb:
cmpbeqb r1, r2, 0x56
cmpbeqb r3, r4, 0x4348
cmpbeqb $0, r5, 0x36
cmpbeqb $1, r6, 0x345678

	.global cmpbneb
cmpbneb:
cmpbneb r7, r8, 250
cmpbneb r9, r10, 0xf000
cmpbneb $2, r11, 0x2
cmpbneb $3, r12, 0xfffffe

	.global cmpbhib
cmpbhib:
cmpbhib r13, r14, 0400
cmpbhib r15, ra, 258
cmpbhib $4, sp, -0x2
cmpbhib $-4, r1, -260

	.global cmpblsb
cmpblsb:
cmpblsb r2, r3, 0x78
cmpblsb r4, r5, 0x100
cmpblsb $-1, r6, -0370
cmpblsb $7, r7, -0x102

	.global cmpbgtb
cmpbgtb:
cmpbgtb r8, r9, -250
cmpbgtb r10, r11, 07700
cmpbgtb $8, r12, 0xfe
cmpbgtb $16, r13, 0xfffff2

	.global cmpbleb
cmpbleb:
cmpbleb r14, r15, -0xfe
cmpbleb ra, sp, -01000
cmpbleb $0x10, r1, 066
cmpbleb $020, r2, -0xffff02

	.global cmpblob
cmpblob:
cmpblob r3, r4, -070
cmpblob r5, r6, -0xfffffe
cmpblob $32, r7, +0x24
cmpblob $0x20, r8, 16777214

	.global cmpbhsb
cmpbhsb:
cmpbhsb r9, r10, 0xf0
cmpbhsb r11, r12, 0402
cmpbhsb $040, r13, -254
cmpbhsb $20, r14, 0x1000

	.global cmpbltb
cmpbltb:
cmpbltb r15, ra, 0x10
cmpbltb sp, r1, 1122
cmpbltb $12, r2, -020
cmpbltb $0xc, r3, -0x800000

	.global cmpbgeb
cmpbgeb:
cmpbgeb r4, r5, 0x0
cmpbgeb r6, r7, 0x400000
cmpbgeb $48, r8, 0
cmpbgeb $060, r9, -0x100000


	.global cmpbeqw
cmpbeqw:
cmpbeqw r1, r2, 0x56
cmpbeqw r3, r4, 0x4348
cmpbeqw $0, r5, 0x36
cmpbeqw $1, r6, 0x345678

	.global cmpbnew
cmpbnew:
cmpbnew r7, r8, 250
cmpbnew r9, r10, 0xf000
cmpbnew $2, r11, 0x2
cmpbnew $3, r12, 0xfffffe

	.global cmpbhiw
cmpbhiw:
cmpbhiw r13, r14, 0400
cmpbhiw r15, ra, 258
cmpbhiw $4, sp, -0x2
cmpbhiw $-4, r1, -260

	.global cmpblsw
cmpblsw:
cmpblsw r2, r3, 0x78
cmpblsw r4, r5, 0x100
cmpblsw $-1, r6, -0370
cmpblsw $7, r7, -0x102

	.global cmpbgtw
cmpbgtw:
cmpbgtw r8, r9, -250
cmpbgtw r10, r11, 07700
cmpbgtw $8, r12, 0xfe
cmpbgtw $16, r13, 0xfffff2

	.global cmpblew
cmpblew:
cmpblew r14, r15, -0xfe
cmpblew ra, sp, -01000
cmpblew $0x10, r1, 066
cmpblew $020, r2, -0xffff02

	.global cmpblow
cmpblow:
cmpblow r3, r4, -070
cmpblow r5, r6, -0xfffffe
cmpblow $32, r7, +0x24
cmpblow $0x20, r8, 16777214

	.global cmpbhsw
cmpbhsw:
cmpbhsw r9, r10, 0xf0
cmpbhsw r11, r12, 0402
cmpbhsw $040, r13, -254
cmpbhsw $20, r14, 0x1000

	.global cmpbltw
cmpbltw:
cmpbltw r15, ra, 0x10
cmpbltw sp, r1, 1122
cmpbltw $12, r2, -020
cmpbltw $0xc, r3, -0x800000

	.global cmpbgew
cmpbgew:
cmpbgew r4, r5, 0x0
cmpbgew r6, r7, 0x400000
cmpbgew $48, r8, 0
cmpbgew $060, r9, -0x100000


	.global cmpbeqd
cmpbeqd:
cmpbeqd r1, r2, 0x56
cmpbeqd r3, r4, 0x4348
cmpbeqd $0, r5, 0x36
cmpbeqd $1, r6, 0x345678

	.global cmpbned
cmpbned:
cmpbned r7, r8, 250
cmpbned r9, r10, 0xf000
cmpbned $2, r11, 0x2
cmpbned $3, r12, 0xfffffe

	.global cmpbhid
cmpbhid:
cmpbhid r13, r14, 0400
cmpbhid r15, ra, 258
cmpbhid $4, sp, -0x2
cmpbhid $-4, r1, -260

	.global cmpblsd
cmpblsd:
cmpblsd r2, r3, 0x78
cmpblsd r4, r5, 0x100
cmpblsd $-1, r6, -0370
cmpblsd $7, r7, -0x102

	.global cmpbgtd
cmpbgtd:
cmpbgtd r8, r9, -250
cmpbgtd r10, r11, 07700
cmpbgtd $8, r12, 0xfe
cmpbgtd $16, r13, 0xfffff2

	.global cmpbled
cmpbled:
cmpbled r14, r15, -0xfe
cmpbled ra, sp, -01000
cmpbled $0x10, r1, 066
cmpbled $020, r2, -0xffff02

	.global cmpblod
cmpblod:
cmpblod r3, r4, -070
cmpblod r5, r6, -0xfffffe
cmpblod $32, r7, +0x24
cmpblod $0x20, r8, 16777214

	.global cmpbhsd
cmpbhsd:
cmpbhsd r9, r10, 0xf0
cmpbhsd r11, r12, 0402
cmpbhsd $040, r13, -254
cmpbhsd $20, r14, 0x1000

	.global cmpbltd
cmpbltd:
cmpbltd r15, ra, 0x10
cmpbltd sp, r1, 1122
cmpbltd $12, r2, -020
cmpbltd $0xc, r3, -0x800000

	.global cmpbged
cmpbged:
cmpbged r4, r5, 0x0
cmpbged r6, r7, 0x400000
cmpbged $48, r8, 0
cmpbged $060, r9, -0x100000
