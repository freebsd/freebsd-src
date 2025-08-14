/* Do not modify. This file is auto-generated from ecp_nistp384-ppc64.pl. */
.machine	"any"
.abiversion	2
.text

.globl	p384_felem_mul
.type	p384_felem_mul,@function
.type	p384_felem_mul,@function
.align	4
p384_felem_mul:
.localentry	p384_felem_mul,0


	stdu	1, -176(1)
	mflr	0
	std	14, 56(1)
	std	15, 64(1)
	std	16, 72(1)
	std	17, 80(1)
	std	18, 88(1)
	std	19, 96(1)
	std	20, 104(1)
	std	21, 112(1)
	std	22, 120(1)

	bl	_p384_felem_mul_core

	mtlr	0
	ld	14, 56(1)
	ld	15, 64(1)
	ld	16, 72(1)
	ld	17, 80(1)
	ld	18, 88(1)
	ld	19, 96(1)
	ld	20, 104(1)
	ld	21, 112(1)
	ld	22, 120(1)
	addi	1, 1, 176
	blr	
.size	p384_felem_mul,.-p384_felem_mul

.globl	p384_felem_square
.type	p384_felem_square,@function
.type	p384_felem_square,@function
.align	4
p384_felem_square:
.localentry	p384_felem_square,0


	stdu	1, -176(1)
	mflr	0
	std	14, 56(1)
	std	15, 64(1)
	std	16, 72(1)
	std	17, 80(1)

	bl	_p384_felem_square_core

	mtlr	0
	ld	14, 56(1)
	ld	15, 64(1)
	ld	16, 72(1)
	ld	17, 80(1)
	addi	1, 1, 176
	blr	
.size	p384_felem_square,.-p384_felem_square





.type	_p384_felem_mul_core,@function
.align	4
_p384_felem_mul_core:
.localentry	_p384_felem_mul_core,0


	ld	6,0(4)
	ld	14,0(5)
	ld	7,8(4)
	ld	15,8(5)
	ld	8,16(4)
	ld	16,16(5)
	ld	9,24(4)
	ld	17,24(5)
	ld	10,32(4)
	ld	18,32(5)
	ld	11,40(4)
	ld	19,40(5)
	ld	12,48(4)
	ld	20,48(5)


	mulld	21, 14, 6
	mulhdu	22, 14, 6
	std	21, 0(3)
	std	22, 8(3)

	vxor	0, 0, 0


	mtvsrdd	32+13, 14, 6
	mtvsrdd	32+14, 7, 15
	.long	0x102D7023


	mtvsrdd	32+15, 15, 6
	mtvsrdd	32+16, 7, 16
	mtvsrdd	32+17, 0, 8
	mtvsrdd	32+18, 0, 14
	.long	0x126F8023
	.long	0x105194E3


	mtvsrdd	32+13, 16, 6
	mtvsrdd	32+14, 7, 17
	mtvsrdd	32+15, 14, 8
	mtvsrdd	32+16, 9, 15
	.long	0x126D7023
	.long	0x106F84E3


	mtvsrdd	32+13, 17, 6
	mtvsrdd	32+14, 7, 18
	mtvsrdd	32+15, 15, 8
	mtvsrdd	32+16, 9, 16
	mtvsrdd	32+17, 0, 10
	mtvsrdd	32+18, 0, 14
	.long	0x126D7023
	.long	0x108F84E3
	.long	0x10919123


	mtvsrdd	32+13, 18, 6
	mtvsrdd	32+14, 7, 19
	mtvsrdd	32+15, 16, 8
	mtvsrdd	32+16, 9, 17
	mtvsrdd	32+17, 14, 10
	mtvsrdd	32+18, 11, 15
	.long	0x126D7023
	.long	0x10AF84E3
	.long	0x10B19163

	stxv	32+1, 16(3)
	stxv	32+2, 32(3)
	stxv	32+3, 48(3)
	stxv	32+4, 64(3)
	stxv	32+5, 80(3)


	mtvsrdd	32+13, 19, 6
	mtvsrdd	32+14, 7, 20
	mtvsrdd	32+15, 17, 8
	mtvsrdd	32+16, 9, 18
	mtvsrdd	32+17, 15, 10
	mtvsrdd	32+18, 11, 16
	.long	0x126D7023
	.long	0x10CF84E3
	mtvsrdd	32+13, 0, 12
	mtvsrdd	32+14, 0, 14
	.long	0x127191A3
	.long	0x10CD74E3


	mtvsrdd	32+13, 19, 7
	mtvsrdd	32+14, 8, 20
	mtvsrdd	32+15, 17, 9
	mtvsrdd	32+16, 10, 18
	mtvsrdd	32+17, 15, 11
	mtvsrdd	32+18, 12, 16
	.long	0x126D7023
	.long	0x10EF84E3
	.long	0x10F191E3


	mtvsrdd	32+13, 19, 8
	mtvsrdd	32+14, 9, 20
	mtvsrdd	32+15, 17, 10
	mtvsrdd	32+16, 11, 18
	mtvsrdd	32+17, 0, 12
	mtvsrdd	32+18, 0, 16
	.long	0x126D7023
	.long	0x110F84E3
	.long	0x11119223


	mtvsrdd	32+13, 19, 9
	mtvsrdd	32+14, 10, 20
	mtvsrdd	32+15, 17, 11
	mtvsrdd	32+16, 12, 18
	.long	0x126D7023
	.long	0x112F84E3


	mtvsrdd	32+13, 19, 10
	mtvsrdd	32+14, 11, 20
	mtvsrdd	32+15, 0, 12
	mtvsrdd	32+16, 0, 18
	.long	0x126D7023
	.long	0x114F84E3


	mtvsrdd	32+17, 19, 11
	mtvsrdd	32+18, 12, 20
	.long	0x11719023

	stxv	32+6, 96(3)
	stxv	32+7, 112(3)
	stxv	32+8, 128(3)
	stxv	32+9, 144(3)
	stxv	32+10, 160(3)
	stxv	32+11, 176(3)


	mulld	21, 20, 12
	mulhdu	22, 20, 12

	std	21, 192(3)
	std	22, 200(3)

	blr	
.size	_p384_felem_mul_core,.-_p384_felem_mul_core





.type	_p384_felem_square_core,@function
.align	4
_p384_felem_square_core:
.localentry	_p384_felem_square_core,0


	ld	6, 0(4)
	ld	7, 8(4)
	ld	8, 16(4)
	ld	9, 24(4)
	ld	10, 32(4)
	ld	11, 40(4)
	ld	12, 48(4)

	vxor	0, 0, 0


	mulld	14, 6, 6
	mulhdu	15, 6, 6
	std	14, 0(3)
	std	15, 8(3)


	add	14, 6, 6
	mtvsrdd	32+13, 0, 14
	mtvsrdd	32+14, 0, 7
	.long	0x102D7023


	mtvsrdd	32+15, 7, 14
	mtvsrdd	32+16, 7, 8
	.long	0x104F8023


	add	15, 7, 7
	mtvsrdd	32+13, 8, 14
	mtvsrdd	32+14, 15, 9
	.long	0x106D7023


	mtvsrdd	32+13, 9, 14
	mtvsrdd	32+14, 15, 10
	mtvsrdd	32+15, 0, 8
	.long	0x108D7023
	.long	0x108F7923


	mtvsrdd	32+13, 10, 14
	mtvsrdd	32+14, 15, 11
	add	16, 8, 8
	mtvsrdd	32+15, 0, 16
	mtvsrdd	32+16, 0, 9
	.long	0x10AD7023
	.long	0x10AF8163

	stxv	32+1, 16(3)
	stxv	32+2, 32(3)
	stxv	32+3, 48(3)
	stxv	32+4, 64(3)


	mtvsrdd	32+13, 11, 14
	mtvsrdd	32+14, 15, 12
	mtvsrdd	32+15, 9, 16
	mtvsrdd	32+16, 9, 10
	stxv	32+5, 80(3)
	.long	0x126D7023
	.long	0x10CF84E3


	add	17, 9, 9
	mtvsrdd	32+13, 11, 15
	mtvsrdd	32+14, 16, 12
	mtvsrdd	32+15, 0, 17
	mtvsrdd	32+16, 0, 10
	.long	0x126D7023
	.long	0x10EF84E3


	mtvsrdd	32+13, 11, 16
	mtvsrdd	32+14, 17, 12
	mtvsrdd	32+15, 0, 10
	.long	0x126D7023
	.long	0x110F7CE3


	add	14, 10, 10
	mtvsrdd	32+13, 11, 17
	mtvsrdd	32+14, 14, 12
	.long	0x112D7023


	mtvsrdd	32+13, 11, 14
	mtvsrdd	32+14, 11, 12
	.long	0x114D7023

	stxv	32+6, 96(3)
	stxv	32+7, 112(3)







	mulld	6, 12, 11
	mulhdu	7, 12, 11
	addc	8, 6, 6
	adde	9, 7, 7

	stxv	32+8, 128(3)
	stxv	32+9, 144(3)
	stxv	32+10, 160(3)



	mulld	14, 12, 12
	mulhdu	15, 12, 12

	std	8, 176(3)
	std	9, 184(3)
	std	14, 192(3)
	std	15, 200(3)

	blr	
.size	_p384_felem_square_core,.-_p384_felem_square_core




.macro	F128_X_8 _off1 _off2
	ld	9,\_off1(3)
	ld	8,\_off2(3)
	srdi	10,9,61
	rldimi	10,8,3,0
	sldi	9,9,3
	std	9,\_off1(3)
	std	10,\_off2(3)
.endm	

.globl	p384_felem128_mul_by_8
.type	p384_felem128_mul_by_8,@function
.type	p384_felem128_mul_by_8,@function
.align	4
p384_felem128_mul_by_8:
.localentry	p384_felem128_mul_by_8,0


	F128_X_8	0, 8

	F128_X_8	16, 24

	F128_X_8	32, 40

	F128_X_8	48, 56

	F128_X_8	64, 72

	F128_X_8	80, 88

	F128_X_8	96, 104

	F128_X_8	112, 120

	F128_X_8	128, 136

	F128_X_8	144, 152

	F128_X_8	160, 168

	F128_X_8	176, 184

	F128_X_8	192, 200

	blr	
.size	p384_felem128_mul_by_8,.-p384_felem128_mul_by_8




.macro	F128_X_2 _off1 _off2
	ld	9,\_off1(3)
	ld	8,\_off2(3)
	srdi	10,9,63
	rldimi	10,8,1,0
	sldi	9,9,1
	std	9,\_off1(3)
	std	10,\_off2(3)
.endm	

.globl	p384_felem128_mul_by_2
.type	p384_felem128_mul_by_2,@function
.type	p384_felem128_mul_by_2,@function
.align	4
p384_felem128_mul_by_2:
.localentry	p384_felem128_mul_by_2,0


	F128_X_2	0, 8

	F128_X_2	16, 24

	F128_X_2	32, 40

	F128_X_2	48, 56

	F128_X_2	64, 72

	F128_X_2	80, 88

	F128_X_2	96, 104

	F128_X_2	112, 120

	F128_X_2	128, 136

	F128_X_2	144, 152

	F128_X_2	160, 168

	F128_X_2	176, 184

	F128_X_2	192, 200

	blr	
.size	p384_felem128_mul_by_2,.-p384_felem128_mul_by_2

.globl	p384_felem_diff128
.type	p384_felem_diff128,@function
.type	p384_felem_diff128,@function
.align	4
p384_felem_diff128:
.localentry	p384_felem_diff128,0


	addis	5, 2, .LConst_two127@toc@ha
	addi	5, 5, .LConst_two127@toc@l

	ld	10, 0(3)
	ld	8, 8(3)
	li	9, 0
	addc	10, 10, 9
	li	7, -1
	rldicr	7, 7, 0, 0
	adde	8, 8, 7
	ld	11, 0(4)
	ld	12, 8(4)
	subfc	11, 11, 10
	subfe	12, 12, 8
	std	11, 0(3)
	std	12, 8(3)


	ld	8, 16(3)
	ld	7, 24(3)
	ld	10, 24(5)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 16(4)
	ld	12, 24(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 16(3)
	std	12, 24(3)

	ld	8, 32(3)
	ld	7, 40(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 32(4)
	ld	12, 40(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 32(3)
	std	12, 40(3)

	ld	8, 48(3)
	ld	7, 56(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 48(4)
	ld	12, 56(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 48(3)
	std	12, 56(3)

	ld	8, 64(3)
	ld	7, 72(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 64(4)
	ld	12, 72(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 64(3)
	std	12, 72(3)

	ld	8, 80(3)
	ld	7, 88(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 80(4)
	ld	12, 88(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 80(3)
	std	12, 88(3)

	ld	8, 96(3)
	ld	7, 104(3)
	ld	6, 40(5)
	addc	8, 8, 9
	adde	7, 7, 6
	ld	11, 96(4)
	ld	12, 104(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 96(3)
	std	12, 104(3)

	ld	8, 112(3)
	ld	7, 120(3)
	ld	6, 56(5)
	addc	8, 8, 9
	adde	7, 7, 6
	ld	11, 112(4)
	ld	12, 120(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 112(3)
	std	12, 120(3)

	ld	8, 128(3)
	ld	7, 136(3)
	ld	6, 72(5)
	addc	8, 8, 9
	adde	7, 7, 6
	ld	11, 128(4)
	ld	12, 136(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 128(3)
	std	12, 136(3)

	ld	8, 144(3)
	ld	7, 152(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 144(4)
	ld	12, 152(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 144(3)
	std	12, 152(3)

	ld	8, 160(3)
	ld	7, 168(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 160(4)
	ld	12, 168(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 160(3)
	std	12, 168(3)

	ld	8, 176(3)
	ld	7, 184(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 176(4)
	ld	12, 184(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 176(3)
	std	12, 184(3)

	ld	8, 192(3)
	ld	7, 200(3)
	addc	8, 8, 9
	adde	7, 7, 10
	ld	11, 192(4)
	ld	12, 200(4)
	subfc	11, 11, 8
	subfe	12, 12, 7
	std	11, 192(3)
	std	12, 200(3)

	blr	
.size	p384_felem_diff128,.-p384_felem_diff128

.data	
.align	4
.LConst_two127:

.long	0x00000000, 0x00000000, 0x00000000, 0x80000000

.long	0x00000000, 0x00000000, 0xffffff80, 0x7fffffff

.long	0x00000000, 0x00000000, 0xffff7f80, 0x80007fff

.long	0x00000000, 0x00000000, 0xffffff80, 0x7f7fffff

.long	0x00000000, 0x00000000, 0x7fffff80, 0x7fffffff

.abiversion	2
.text

.globl	p384_felem_diff_128_64
.type	p384_felem_diff_128_64,@function
.type	p384_felem_diff_128_64,@function
.align	4
p384_felem_diff_128_64:
.localentry	p384_felem_diff_128_64,0

	addis	5, 2, .LConst_128_two64@toc@ha
	addi	5, 5, .LConst_128_two64@toc@l

	ld	9, 0(3)
	ld	10, 8(3)
	ld	8, 48(5)
	li	7, 0
	addc	9, 9, 8
	li	6, 1
	adde	10, 10, 6
	ld	11, 0(4)
	subfc	8, 11, 9
	subfe	12, 7, 10
	std	8, 0(3)
	std	12, 8(3)

	ld	9, 16(3)
	ld	10, 24(3)
	ld	8, 0(5)
	addc	9, 9, 8
	addze	10, 10
	ld	11, 8(4)
	subfc	11, 11, 9
	subfe	12, 7, 10
	std	11, 16(3)
	std	12, 24(3)

	ld	9, 32(3)
	ld	10, 40(3)
	ld	8, 16(5)
	addc	9, 9, 8
	addze	10, 10
	ld	11, 16(4)
	subfc	11, 11, 9
	subfe	12, 7, 10
	std	11, 32(3)
	std	12, 40(3)

	ld	10, 48(3)
	ld	8, 56(3)

	li	9, -256
	addc	10, 10, 9
	addze	8, 8
	ld	11, 24(4)
	subfc	11, 11, 10
	subfe	12, 7, 8
	std	11, 48(3)
	std	12, 56(3)

	ld	10, 64(3)
	ld	8, 72(3)
	addc	10, 10, 9
	addze	8, 8
	ld	11, 32(4)
	subfc	11, 11, 10
	subfe	12, 7, 8
	std	11, 64(3)
	std	12, 72(3)

	ld	10, 80(3)
	ld	8, 88(3)
	addc	10, 10, 9
	addze	8, 8
	ld	11, 40(4)
	subfc	11, 11, 10
	subfe	12, 7, 8
	std	11, 80(3)
	std	12, 88(3)

	ld	10, 96(3)
	ld	8, 104(3)
	addc	10, 10, 9
	addze	9, 8
	ld	11, 48(4)
	subfc	11, 11, 10
	subfe	12, 7, 9
	std	11, 96(3)
	std	12, 104(3)

	blr	
.size	p384_felem_diff_128_64,.-p384_felem_diff_128_64

.data	
.align	4
.LConst_128_two64:

.long	0xffffff00, 0xfeffffff, 0x00000000, 0x00000000

.long	0xffffff00, 0xfffffffe, 0x00000000, 0x00000000

.long	0xffffff00, 0xffffffff, 0x00000000, 0x00000000

.long	0xffff0000, 0x0000ffff, 0x00000001, 0x00000000

.LConst_two60:

.long	0xfffffff0, 0x0fefffff, 0x0, 0x0

.long	0xfffff000, 0x10000fff, 0x0, 0x0

.long	0xeffffff0, 0x0fffffff, 0x0, 0x0

.long	0xfffffff0, 0x0fffffff, 0x0, 0x0

.abiversion	2
.text



.globl	p384_felem_diff64
.type	p384_felem_diff64,@function
.type	p384_felem_diff64,@function
.align	4
p384_felem_diff64:
.localentry	p384_felem_diff64,0

	addis	5, 2, .LConst_two60@toc@ha
	addi	5, 5, .LConst_two60@toc@l

	ld	9, 0(3)
	ld	8, 16(5)
	li	7, 0
	add	9, 9, 8
	ld	11, 0(4)
	subf	8, 11, 9
	std	8, 0(3)

	ld	9, 8(3)
	ld	8, 0(5)
	add	9, 9, 8
	ld	11, 8(4)
	subf	11, 11, 9
	std	11, 8(3)

	ld	9, 16(3)
	ld	8, 32(5)
	add	9, 9, 8
	ld	11, 16(4)
	subf	11, 11, 9
	std	11, 16(3)

	ld	10, 24(3)
	ld	9, 48(5)
	add	10, 10, 9
	ld	12, 24(4)
	subf	12, 12, 10
	std	12, 24(3)

	ld	10, 32(3)
	add	10, 10, 9
	ld	11, 32(4)
	subf	11, 11, 10
	std	11, 32(3)

	ld	10, 40(3)
	add	10, 10, 9
	ld	12, 40(4)
	subf	12, 12, 10
	std	12, 40(3)

	ld	10, 48(3)
	add	10, 10, 9
	ld	11, 48(4)
	subf	11, 11, 10
	std	11, 48(3)

	blr	
.size	p384_felem_diff64,.-p384_felem_diff64

.abiversion	2
.text



.macro	SHR o_h o_l in_h in_l nbits
	srdi	\o_l, \in_l, \nbits
	rldimi	\o_l, \in_h, 64-\nbits, 0
	srdi	\o_h, \in_h, \nbits
.endm	




.global	p384_felem_reduce
.type	p384_felem_reduce,@function
.align	4
p384_felem_reduce:
.localentry	p384_felem_reduce,0


	stdu	1, -208(1)
	mflr	0
	std	14, 56(1)
	std	15, 64(1)
	std	16, 72(1)
	std	17, 80(1)
	std	18, 88(1)
	std	19, 96(1)
	std	20, 104(1)
	std	21, 112(1)
	std	22, 120(1)
	std	23, 128(1)
	std	24, 136(1)
	std	25, 144(1)
	std	26, 152(1)
	std	27, 160(1)
	std	28, 168(1)
	std	29, 176(1)
	std	30, 184(1)
	std	31, 192(1)

	bl	_p384_felem_reduce_core

	mtlr	0
	ld	14, 56(1)
	ld	15, 64(1)
	ld	16, 72(1)
	ld	17, 80(1)
	ld	18, 88(1)
	ld	19, 96(1)
	ld	20, 104(1)
	ld	21, 112(1)
	ld	22, 120(1)
	ld	23, 128(1)
	ld	24, 136(1)
	ld	25, 144(1)
	ld	26, 152(1)
	ld	27, 160(1)
	ld	28, 168(1)
	ld	29, 176(1)
	ld	30, 184(1)
	ld	31, 192(1)
	addi	1, 1, 208
	blr	
.size	p384_felem_reduce,.-p384_felem_reduce





.type	_p384_felem_reduce_core,@function
.align	4
_p384_felem_reduce_core:
.localentry	_p384_felem_reduce_core,0

	addis	12, 2, .LConst@toc@ha
	addi	12, 12, .LConst@toc@l


	ld	11, 8(12)


	ld	26, 96(4)
	ld	27, 96+8(4)
	add	27, 27, 11


	ld	24, 80(4)
	ld	25, 80+8(4)
	add	25, 25, 11


	ld	22, 64(4)
	ld	23, 64+8(4)
	add	23, 23, 11


	ld	20, 48(4)
	ld	21, 48+8(4)
	add	21, 21, 11

	ld	11, 48+8(12)


	ld	18, 32(4)
	ld	19, 32+8(4)
	add	19, 19, 11

	ld	11, 16+8(12)


	ld	16, 16(4)
	ld	17, 16+8(4)
	add	17, 17, 11

	ld	11, 32+8(12)


	ld	14, 0(4)
	ld	15, 0+8(4)
	add	15, 15, 11


	li	7, -1




	ld	5, 192(4)
	ld	6, 192+8(4)
	SHR	9, 10, 6, 5, 32
	ld	30, 128(4)
	ld	31, 136(4)
	addc	30, 30, 10
	adde	31, 31, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	ld	28, 112(4)
	ld	29, 120(4)
	addc	28, 28, 11
	addze	29, 29


	SHR	9, 10, 6, 5, 8
	addc	28, 28, 10
	adde	29, 29, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	26, 26, 11
	addze	27, 27


	SHR	9, 10, 6, 5, 16
	subfc	26, 10, 26
	subfe	27, 9, 27


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	24, 11, 24
	subfe	25, 9, 25


	SHR	9, 10, 6, 5, 48
	addc	26, 26, 10
	adde	27, 27, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	24, 24, 11
	addze	25, 25




	ld	5, 176(4)
	ld	6, 176+8(4)
	SHR	9, 10, 6, 5, 32
	addc	28, 28, 10
	adde	29, 29, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	addc	26, 26, 11
	addze	27, 27


	SHR	9, 10, 6, 5, 8
	addc	26, 26, 10
	adde	27, 27, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	24, 24, 11
	addze	25, 25


	SHR	9, 10, 6, 5, 16
	subfc	24, 10, 24
	subfe	25, 9, 25


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	22, 11, 22
	subfe	23, 9, 23


	SHR	9, 10, 6, 5, 48
	addc	24, 24, 10
	adde	25, 25, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	22, 22, 11
	addze	23, 23




	ld	5, 160(4)
	ld	6, 160+8(4)
	SHR	9, 10, 6, 5, 32
	addc	26, 26, 10
	adde	27, 27, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	addc	24, 24, 11
	addze	25, 25


	SHR	9, 10, 6, 5, 8
	addc	24, 24, 10
	adde	25, 25, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	22, 22, 11
	addze	23, 23


	SHR	9, 10, 6, 5, 16
	subfc	22, 10, 22
	subfe	23, 9, 23


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	20, 11, 20
	subfe	21, 9, 21


	SHR	9, 10, 6, 5, 48
	addc	22, 22, 10
	adde	23, 23, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	20, 20, 11
	addze	21, 21




	ld	5, 144(4)
	ld	6, 144+8(4)
	SHR	9, 10, 6, 5, 32
	addc	24, 24, 10
	adde	25, 25, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	addc	22, 22, 11
	addze	23, 23


	SHR	9, 10, 6, 5, 8
	addc	22, 22, 10
	adde	23, 23, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	20, 20, 11
	addze	21, 21


	SHR	9, 10, 6, 5, 16
	subfc	20, 10, 20
	subfe	21, 9, 21


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	18, 11, 18
	subfe	19, 9, 19


	SHR	9, 10, 6, 5, 48
	addc	20, 20, 10
	adde	21, 21, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	18, 18, 11
	addze	19, 19




	mr	5, 30
	mr	6, 31
	SHR	9, 10, 6, 5, 32
	addc	22, 22, 10
	adde	23, 23, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	addc	20, 20, 11
	addze	21, 21


	SHR	9, 10, 6, 5, 8
	addc	20, 20, 10
	adde	21, 21, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	18, 18, 11
	addze	19, 19


	SHR	9, 10, 6, 5, 16
	subfc	18, 10, 18
	subfe	19, 9, 19


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	16, 11, 16
	subfe	17, 9, 17


	SHR	9, 10, 6, 5, 48
	addc	18, 18, 10
	adde	19, 19, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	16, 16, 11
	addze	17, 17




	mr	5, 28
	mr	6, 29
	SHR	9, 10, 6, 5, 32
	addc	20, 20, 10
	adde	21, 21, 9


	srdi	11, 7, 32
	and	11, 11, 5
	sldi	11, 11, 24
	addc	18, 18, 11
	addze	19, 19


	SHR	9, 10, 6, 5, 8
	addc	18, 18, 10
	adde	19, 19, 9


	andi.	11, 5, 0xff
	sldi	11, 11, 48
	addc	16, 16, 11
	addze	17, 17


	SHR	9, 10, 6, 5, 16
	subfc	16, 10, 16
	subfe	17, 9, 17


	srdi	11, 7, 48
	and	11, 11, 5
	sldi	11, 11, 40
	li	9, 0
	subfc	14, 11, 14
	subfe	15, 9, 15


	SHR	9, 10, 6, 5, 48
	addc	16, 16, 10
	adde	17, 17, 9


	srdi	11, 7, 16
	and	11, 11, 5
	sldi	11, 11, 8
	addc	14, 14, 11
	addze	15, 15






	SHR	9, 10, 23, 22, 56
	addc	24, 24, 10
	adde	25, 25, 9
	srdi	11, 7, 8
	and	22, 22, 11
	li	23, 0



	SHR	9, 10, 25, 24, 56
	addc	26, 26, 10
	adde	27, 27, 9
	and	24, 24, 11
	li	25, 0




	SHR	31, 30, 27, 26, 48
	srdi	11, 7, 16
	and	26, 26, 11
	li	27, 0



	SHR	9, 10, 31, 30, 40
	addc	20, 20, 10
	adde	21, 21, 9


	srdi	11, 7, 24
	and	10, 30, 11
	sldi	10, 10, 16
	addc	18, 18, 10
	addze	19, 19


	SHR	9, 10, 31, 30, 16
	addc	18, 18, 10
	adde	19, 19, 9


	srdi	11, 7, 48
	and	10, 30, 11
	sldi	10, 10, 40
	addc	16, 16, 10
	addze	17, 17


	SHR	9, 10, 31, 30, 24
	subfc	16, 10, 16
	subfe	17, 9, 17


	srdi	11, 7, 40
	and	10, 30, 11
	sldi	10, 10, 32
	li	9, 0
	subfc	14, 10, 14
	subfe	15, 9, 15


	addc	14, 14, 30
	adde	15, 15, 31




	SHR	9, 10, 15, 14, 56
	addc	16, 16, 10
	adde	17, 17, 9


	srdi	11, 7, 8
	and	14, 14, 11
	li	15, 0


	SHR	9, 10, 17, 16, 56
	addc	18, 18, 10
	adde	19, 19, 9


	and	16, 16, 11
	li	17, 0


	SHR	9, 10, 19, 18, 56
	addc	20, 20, 10
	adde	21, 21, 9


	and	18, 18, 11
	li	19, 0


	SHR	9, 10, 21, 20, 56
	addc	22, 22, 10
	adde	23, 23, 9


	and	20, 20, 11
	li	21, 0


	SHR	9, 10, 23, 22, 56
	addc	24, 24, 10
	adde	25, 25, 9


	and	22, 22, 11


	SHR	9, 10, 25, 24, 56
	addc	26, 26, 10
	adde	27, 27, 9


	and	24, 24, 11

	std	14, 0(3)
	std	16, 8(3)
	std	18, 16(3)
	std	20, 24(3)
	std	22, 32(3)
	std	24, 40(3)
	std	26, 48(3)
	blr	
.size	_p384_felem_reduce_core,.-_p384_felem_reduce_core

.data	
.align	4
.LConst:

.long	0x0, 0x0, 0xfffffff0, 0xfffffff

.long	0x0, 0x0, 0xfffffff0, 0xfefffff

.long	0x0, 0x0, 0xfffff000, 0x10000fff

.long	0x0, 0x0, 0xeffffff0, 0xfffffff

.abiversion	2
.text




.global	p384_felem_square_reduce
.type	p384_felem_square_reduce,@function
.align	4
p384_felem_square_reduce:
.localentry	p384_felem_square_reduce,0

	stdu	1, -512(1)
	mflr	0
	std	14, 56(1)
	std	15, 64(1)
	std	16, 72(1)
	std	17, 80(1)
	std	18, 88(1)
	std	19, 96(1)
	std	20, 104(1)
	std	21, 112(1)
	std	22, 120(1)
	std	23, 128(1)
	std	24, 136(1)
	std	25, 144(1)
	std	26, 152(1)
	std	27, 160(1)
	std	28, 168(1)
	std	29, 176(1)
	std	30, 184(1)
	std	31, 192(1)

	std	3, 496(1)
	addi	3, 1, 208
	bl	_p384_felem_square_core

	mr	4, 3
	ld	3, 496(1)
	bl	_p384_felem_reduce_core

	ld	14, 56(1)
	ld	15, 64(1)
	ld	16, 72(1)
	ld	17, 80(1)
	ld	18, 88(1)
	ld	19, 96(1)
	ld	20, 104(1)
	ld	21, 112(1)
	ld	22, 120(1)
	ld	23, 128(1)
	ld	24, 136(1)
	ld	25, 144(1)
	ld	26, 152(1)
	ld	27, 160(1)
	ld	28, 168(1)
	ld	29, 176(1)
	ld	30, 184(1)
	ld	31, 192(1)
	addi	1, 1, 512
	mtlr	0
	blr	
.size	p384_felem_square_reduce,.-p384_felem_square_reduce




.global	p384_felem_mul_reduce
.type	p384_felem_mul_reduce,@function
.align	5
p384_felem_mul_reduce:
.localentry	p384_felem_mul_reduce,0

	stdu	1, -512(1)
	mflr	0
	std	14, 56(1)
	std	15, 64(1)
	std	16, 72(1)
	std	17, 80(1)
	std	18, 88(1)
	std	19, 96(1)
	std	20, 104(1)
	std	21, 112(1)
	std	22, 120(1)
	std	23, 128(1)
	std	24, 136(1)
	std	25, 144(1)
	std	26, 152(1)
	std	27, 160(1)
	std	28, 168(1)
	std	29, 176(1)
	std	30, 184(1)
	std	31, 192(1)

	std	3, 496(1)
	addi	3, 1, 208
	bl	_p384_felem_mul_core

	mr	4, 3
	ld	3, 496(1)
	bl	_p384_felem_reduce_core

	ld	14, 56(1)
	ld	15, 64(1)
	ld	16, 72(1)
	ld	17, 80(1)
	ld	18, 88(1)
	ld	19, 96(1)
	ld	20, 104(1)
	ld	21, 112(1)
	ld	22, 120(1)
	ld	23, 128(1)
	ld	24, 136(1)
	ld	25, 144(1)
	ld	26, 152(1)
	ld	27, 160(1)
	ld	28, 168(1)
	ld	29, 176(1)
	ld	30, 184(1)
	ld	31, 192(1)
	addi	1, 1, 512
	mtlr	0
	blr	
.size	p384_felem_mul_reduce,.-p384_felem_mul_reduce
