/* $FreeBSD$ */
/* Do not modify. This file is auto-generated from ecp_nistz256-avx2.pl. */
.text	

.align	64
.LAVX2_AND_MASK:
.LAVX2_POLY:
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x000001ff, 0x000001ff, 0x000001ff, 0x000001ff
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x00040000, 0x00040000, 0x00040000, 0x00040000
.quad	0x1fe00000, 0x1fe00000, 0x1fe00000, 0x1fe00000
.quad	0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff

.LAVX2_POLY_x2:
.quad	0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC
.quad	0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC
.quad	0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC
.quad	0x400007FC, 0x400007FC, 0x400007FC, 0x400007FC
.quad	0x3FFFFFFE, 0x3FFFFFFE, 0x3FFFFFFE, 0x3FFFFFFE
.quad	0x3FFFFFFE, 0x3FFFFFFE, 0x3FFFFFFE, 0x3FFFFFFE
.quad	0x400FFFFE, 0x400FFFFE, 0x400FFFFE, 0x400FFFFE
.quad	0x7F7FFFFE, 0x7F7FFFFE, 0x7F7FFFFE, 0x7F7FFFFE
.quad	0x03FFFFFC, 0x03FFFFFC, 0x03FFFFFC, 0x03FFFFFC

.LAVX2_POLY_x8:
.quad	0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8
.quad	0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8
.quad	0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8, 0xFFFFFFF8
.quad	0x80000FF8, 0x80000FF8, 0x80000FF8, 0x80000FF8
.quad	0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC
.quad	0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC, 0x7FFFFFFC
.quad	0x801FFFFC, 0x801FFFFC, 0x801FFFFC, 0x801FFFFC
.quad	0xFEFFFFFC, 0xFEFFFFFC, 0xFEFFFFFC, 0xFEFFFFFC
.quad	0x07FFFFF8, 0x07FFFFF8, 0x07FFFFF8, 0x07FFFFF8

.LONE:
.quad	0x00000020, 0x00000020, 0x00000020, 0x00000020
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x1fffc000, 0x1fffc000, 0x1fffc000, 0x1fffc000
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1f7fffff, 0x1f7fffff, 0x1f7fffff, 0x1f7fffff
.quad	0x03ffffff, 0x03ffffff, 0x03ffffff, 0x03ffffff
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000




.LTO_MONT_AVX2:
.quad	0x00000400, 0x00000400, 0x00000400, 0x00000400
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x1ff80000, 0x1ff80000, 0x1ff80000, 0x1ff80000
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x0fffffff, 0x0fffffff, 0x0fffffff, 0x0fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x00000003, 0x00000003, 0x00000003, 0x00000003

.LFROM_MONT_AVX2:
.quad	0x00000001, 0x00000001, 0x00000001, 0x00000001
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000
.quad	0x1ffffe00, 0x1ffffe00, 0x1ffffe00, 0x1ffffe00
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1fffffff, 0x1fffffff, 0x1fffffff, 0x1fffffff
.quad	0x1ffbffff, 0x1ffbffff, 0x1ffbffff, 0x1ffbffff
.quad	0x001fffff, 0x001fffff, 0x001fffff, 0x001fffff
.quad	0x00000000, 0x00000000, 0x00000000, 0x00000000

.LIntOne:
.long	1,1,1,1,1,1,1,1
.globl	ecp_nistz256_avx2_transpose_convert
.type	ecp_nistz256_avx2_transpose_convert,@function
.align	64
ecp_nistz256_avx2_transpose_convert:
	vzeroupper

	vmovdqa	0(%rsi),%ymm0
	leaq	112(%rsi),%rax
	vmovdqa	32(%rsi),%ymm4
	leaq	.LAVX2_AND_MASK(%rip),%rdx
	vmovdqa	64(%rsi),%ymm1
	vmovdqa	96(%rsi),%ymm5
	vmovdqa	128-112(%rax),%ymm2
	vmovdqa	160-112(%rax),%ymm6
	vmovdqa	192-112(%rax),%ymm3
	vmovdqa	224-112(%rax),%ymm7


	vpunpcklqdq	%ymm1,%ymm0,%ymm8
	vpunpcklqdq	%ymm3,%ymm2,%ymm9
	vpunpckhqdq	%ymm1,%ymm0,%ymm10
	vpunpckhqdq	%ymm3,%ymm2,%ymm11

	vpunpcklqdq	%ymm5,%ymm4,%ymm12
	vpunpcklqdq	%ymm7,%ymm6,%ymm13
	vpunpckhqdq	%ymm5,%ymm4,%ymm14
	vpunpckhqdq	%ymm7,%ymm6,%ymm15

	vperm2i128	$0x20,%ymm9,%ymm8,%ymm0
	vperm2i128	$0x20,%ymm11,%ymm10,%ymm1
	vperm2i128	$0x31,%ymm9,%ymm8,%ymm2
	vperm2i128	$0x31,%ymm11,%ymm10,%ymm3

	vperm2i128	$0x20,%ymm13,%ymm12,%ymm4
	vperm2i128	$0x20,%ymm15,%ymm14,%ymm5
	vperm2i128	$0x31,%ymm13,%ymm12,%ymm6
	vperm2i128	$0x31,%ymm15,%ymm14,%ymm7
	vmovdqa	(%rdx),%ymm15

	vpand	(%rdx),%ymm0,%ymm8
	vpsrlq	$29,%ymm0,%ymm0
	vpand	%ymm15,%ymm0,%ymm9
	vpsrlq	$29,%ymm0,%ymm0
	vpsllq	$6,%ymm1,%ymm10
	vpxor	%ymm0,%ymm10,%ymm10
	vpand	%ymm15,%ymm10,%ymm10
	vpsrlq	$23,%ymm1,%ymm1
	vpand	%ymm15,%ymm1,%ymm11
	vpsrlq	$29,%ymm1,%ymm1
	vpsllq	$12,%ymm2,%ymm12
	vpxor	%ymm1,%ymm12,%ymm12
	vpand	%ymm15,%ymm12,%ymm12
	vpsrlq	$17,%ymm2,%ymm2
	vpand	%ymm15,%ymm2,%ymm13
	vpsrlq	$29,%ymm2,%ymm2
	vpsllq	$18,%ymm3,%ymm14
	vpxor	%ymm2,%ymm14,%ymm14
	vpand	%ymm15,%ymm14,%ymm14
	vpsrlq	$11,%ymm3,%ymm3
	vmovdqa	%ymm8,0(%rdi)
	leaq	112(%rdi),%rax
	vpand	%ymm15,%ymm3,%ymm8
	vpsrlq	$29,%ymm3,%ymm3

	vmovdqa	%ymm9,32(%rdi)
	vmovdqa	%ymm10,64(%rdi)
	vmovdqa	%ymm11,96(%rdi)
	vmovdqa	%ymm12,128-112(%rax)
	vmovdqa	%ymm13,160-112(%rax)
	vmovdqa	%ymm14,192-112(%rax)
	vmovdqa	%ymm8,224-112(%rax)
	vmovdqa	%ymm3,256-112(%rax)
	leaq	448(%rdi),%rax

	vpand	%ymm15,%ymm4,%ymm8
	vpsrlq	$29,%ymm4,%ymm4
	vpand	%ymm15,%ymm4,%ymm9
	vpsrlq	$29,%ymm4,%ymm4
	vpsllq	$6,%ymm5,%ymm10
	vpxor	%ymm4,%ymm10,%ymm10
	vpand	%ymm15,%ymm10,%ymm10
	vpsrlq	$23,%ymm5,%ymm5
	vpand	%ymm15,%ymm5,%ymm11
	vpsrlq	$29,%ymm5,%ymm5
	vpsllq	$12,%ymm6,%ymm12
	vpxor	%ymm5,%ymm12,%ymm12
	vpand	%ymm15,%ymm12,%ymm12
	vpsrlq	$17,%ymm6,%ymm6
	vpand	%ymm15,%ymm6,%ymm13
	vpsrlq	$29,%ymm6,%ymm6
	vpsllq	$18,%ymm7,%ymm14
	vpxor	%ymm6,%ymm14,%ymm14
	vpand	%ymm15,%ymm14,%ymm14
	vpsrlq	$11,%ymm7,%ymm7
	vmovdqa	%ymm8,288-448(%rax)
	vpand	%ymm15,%ymm7,%ymm8
	vpsrlq	$29,%ymm7,%ymm7

	vmovdqa	%ymm9,320-448(%rax)
	vmovdqa	%ymm10,352-448(%rax)
	vmovdqa	%ymm11,384-448(%rax)
	vmovdqa	%ymm12,416-448(%rax)
	vmovdqa	%ymm13,448-448(%rax)
	vmovdqa	%ymm14,480-448(%rax)
	vmovdqa	%ymm8,512-448(%rax)
	vmovdqa	%ymm7,544-448(%rax)

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_transpose_convert,.-ecp_nistz256_avx2_transpose_convert

.globl	ecp_nistz256_avx2_convert_transpose_back
.type	ecp_nistz256_avx2_convert_transpose_back,@function
.align	32
ecp_nistz256_avx2_convert_transpose_back:
	vzeroupper
	movl	$3,%ecx

.Lconv_loop:
	vmovdqa	0(%rsi),%ymm0
	leaq	160(%rsi),%rax
	vmovdqa	32(%rsi),%ymm1
	vmovdqa	64(%rsi),%ymm2
	vmovdqa	96(%rsi),%ymm3
	vmovdqa	128-160(%rax),%ymm4
	vmovdqa	160-160(%rax),%ymm5
	vmovdqa	192-160(%rax),%ymm6
	vmovdqa	224-160(%rax),%ymm7
	vmovdqa	256-160(%rax),%ymm8

	vpsllq	$29,%ymm1,%ymm1
	vpsllq	$58,%ymm2,%ymm9
	vpaddq	%ymm1,%ymm0,%ymm0
	vpaddq	%ymm9,%ymm0,%ymm0

	vpsrlq	$6,%ymm2,%ymm2
	vpsllq	$23,%ymm3,%ymm3
	vpsllq	$52,%ymm4,%ymm10
	vpaddq	%ymm2,%ymm3,%ymm3
	vpaddq	%ymm3,%ymm10,%ymm1

	vpsrlq	$12,%ymm4,%ymm4
	vpsllq	$17,%ymm5,%ymm5
	vpsllq	$46,%ymm6,%ymm11
	vpaddq	%ymm4,%ymm5,%ymm5
	vpaddq	%ymm5,%ymm11,%ymm2

	vpsrlq	$18,%ymm6,%ymm6
	vpsllq	$11,%ymm7,%ymm7
	vpsllq	$40,%ymm8,%ymm12
	vpaddq	%ymm6,%ymm7,%ymm7
	vpaddq	%ymm7,%ymm12,%ymm3

	vpunpcklqdq	%ymm1,%ymm0,%ymm9
	vpunpcklqdq	%ymm3,%ymm2,%ymm10
	vpunpckhqdq	%ymm1,%ymm0,%ymm11
	vpunpckhqdq	%ymm3,%ymm2,%ymm12

	vperm2i128	$0x20,%ymm10,%ymm9,%ymm0
	vperm2i128	$0x20,%ymm12,%ymm11,%ymm1
	vperm2i128	$0x31,%ymm10,%ymm9,%ymm2
	vperm2i128	$0x31,%ymm12,%ymm11,%ymm3

	vmovdqa	%ymm0,0(%rdi)
	vmovdqa	%ymm1,96(%rdi)
	vmovdqa	%ymm2,192(%rdi)
	vmovdqa	%ymm3,288(%rdi)

	leaq	288(%rsi),%rsi
	leaq	32(%rdi),%rdi

	decl	%ecx
	jnz	.Lconv_loop

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_convert_transpose_back,.-ecp_nistz256_avx2_convert_transpose_back
.type	avx2_normalize,@function
.align	32
avx2_normalize:
	vpsrlq	$29,%ymm0,%ymm11
	vpand	%ymm12,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$29,%ymm1,%ymm11
	vpand	%ymm12,%ymm1,%ymm1
	vpaddq	%ymm11,%ymm2,%ymm2

	vpsrlq	$29,%ymm2,%ymm11
	vpand	%ymm12,%ymm2,%ymm2
	vpaddq	%ymm11,%ymm3,%ymm3

	vpsrlq	$29,%ymm3,%ymm11
	vpand	%ymm12,%ymm3,%ymm3
	vpaddq	%ymm11,%ymm4,%ymm4

	vpsrlq	$29,%ymm4,%ymm11
	vpand	%ymm12,%ymm4,%ymm4
	vpaddq	%ymm11,%ymm5,%ymm5

	vpsrlq	$29,%ymm5,%ymm11
	vpand	%ymm12,%ymm5,%ymm5
	vpaddq	%ymm11,%ymm6,%ymm6

	vpsrlq	$29,%ymm6,%ymm11
	vpand	%ymm12,%ymm6,%ymm6
	vpaddq	%ymm11,%ymm7,%ymm7

	vpsrlq	$29,%ymm7,%ymm11
	vpand	%ymm12,%ymm7,%ymm7
	vpaddq	%ymm11,%ymm8,%ymm8


	.byte	0xf3,0xc3
.size	avx2_normalize,.-avx2_normalize

.type	avx2_normalize_n_store,@function
.align	32
avx2_normalize_n_store:
	vpsrlq	$29,%ymm0,%ymm11
	vpand	%ymm12,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm1,%ymm1

	vpsrlq	$29,%ymm1,%ymm11
	vpand	%ymm12,%ymm1,%ymm1
	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vpaddq	%ymm11,%ymm2,%ymm2

	vpsrlq	$29,%ymm2,%ymm11
	vpand	%ymm12,%ymm2,%ymm2
	vmovdqa	%ymm1,32(%rdi)
	vpaddq	%ymm11,%ymm3,%ymm3

	vpsrlq	$29,%ymm3,%ymm11
	vpand	%ymm12,%ymm3,%ymm3
	vmovdqa	%ymm2,64(%rdi)
	vpaddq	%ymm11,%ymm4,%ymm4

	vpsrlq	$29,%ymm4,%ymm11
	vpand	%ymm12,%ymm4,%ymm4
	vmovdqa	%ymm3,96(%rdi)
	vpaddq	%ymm11,%ymm5,%ymm5

	vpsrlq	$29,%ymm5,%ymm11
	vpand	%ymm12,%ymm5,%ymm5
	vmovdqa	%ymm4,128-160(%rax)
	vpaddq	%ymm11,%ymm6,%ymm6

	vpsrlq	$29,%ymm6,%ymm11
	vpand	%ymm12,%ymm6,%ymm6
	vmovdqa	%ymm5,160-160(%rax)
	vpaddq	%ymm11,%ymm7,%ymm7

	vpsrlq	$29,%ymm7,%ymm11
	vpand	%ymm12,%ymm7,%ymm7
	vmovdqa	%ymm6,192-160(%rax)
	vpaddq	%ymm11,%ymm8,%ymm8

	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)

	.byte	0xf3,0xc3
.size	avx2_normalize_n_store,.-avx2_normalize_n_store



.type	avx2_mul_x4,@function
.align	32
avx2_mul_x4:
	leaq	.LAVX2_POLY(%rip),%rax

	vpxor	%ymm0,%ymm0,%ymm0
	vpxor	%ymm1,%ymm1,%ymm1
	vpxor	%ymm2,%ymm2,%ymm2
	vpxor	%ymm3,%ymm3,%ymm3
	vpxor	%ymm4,%ymm4,%ymm4
	vpxor	%ymm5,%ymm5,%ymm5
	vpxor	%ymm6,%ymm6,%ymm6
	vpxor	%ymm7,%ymm7,%ymm7

	vmovdqa	224(%rax),%ymm14
	vmovdqa	256(%rax),%ymm15

	movl	$9,%ecx
	leaq	-512(%rsi),%rsi
	jmp	.Lavx2_mul_x4_loop

.align	32
.Lavx2_mul_x4_loop:
	vmovdqa	0(%rdx),%ymm9
	leaq	32(%rdx),%rdx

	vpmuludq	0+512(%rsi),%ymm9,%ymm11
	vpmuludq	32+512(%rsi),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm0,%ymm0
	vpmuludq	64+512(%rsi),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm1,%ymm1
	vpand	%ymm12,%ymm0,%ymm10
	vpmuludq	96+512(%rsi),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm2
	vpmuludq	128+512(%rsi),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	160+512(%rsi),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	192+512(%rsi),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	224+512(%rsi),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6


	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	256+512(%rsi),%ymm9,%ymm8
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
.byte	0x67
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm13
.byte	0x67
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm7,%ymm6
	vpaddq	%ymm13,%ymm8,%ymm7

	decl	%ecx
	jnz	.Lavx2_mul_x4_loop

	vpxor	%ymm8,%ymm8,%ymm8

	.byte	0xf3,0xc3
.size	avx2_mul_x4,.-avx2_mul_x4




.type	avx2_mul_by1_x4,@function
.align	32
avx2_mul_by1_x4:
	leaq	.LAVX2_POLY(%rip),%rax

	vpxor	%ymm0,%ymm0,%ymm0
	vpxor	%ymm1,%ymm1,%ymm1
	vpxor	%ymm2,%ymm2,%ymm2
	vpxor	%ymm3,%ymm3,%ymm3
	vpxor	%ymm4,%ymm4,%ymm4
	vpxor	%ymm5,%ymm5,%ymm5
	vpxor	%ymm6,%ymm6,%ymm6
	vpxor	%ymm7,%ymm7,%ymm7
	vpxor	%ymm8,%ymm8,%ymm8

	vmovdqa	96+.LONE(%rip),%ymm14
	vmovdqa	224+.LONE(%rip),%ymm15

	movl	$9,%ecx
	jmp	.Lavx2_mul_by1_x4_loop

.align	32
.Lavx2_mul_by1_x4_loop:
	vmovdqa	0(%rsi),%ymm9
.byte	0x48,0x8d,0xb6,0x20,0,0,0

	vpsllq	$5,%ymm9,%ymm13
	vpmuludq	%ymm14,%ymm9,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm3
.byte	0x67
	vpmuludq	%ymm12,%ymm9,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm11,%ymm4,%ymm4
	vpaddq	%ymm11,%ymm5,%ymm5
	vpaddq	%ymm11,%ymm6,%ymm6
	vpsllq	$23,%ymm9,%ymm11

.byte	0x67,0x67
	vpmuludq	%ymm15,%ymm9,%ymm13
	vpsubq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
.byte	0x67,0x67
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm13
	vmovdqa	%ymm5,%ymm4
	vpmuludq	224(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm6,%ymm5
	vpaddq	%ymm11,%ymm7,%ymm6
	vpmuludq	256(%rax),%ymm10,%ymm7

	decl	%ecx
	jnz	.Lavx2_mul_by1_x4_loop

	.byte	0xf3,0xc3
.size	avx2_mul_by1_x4,.-avx2_mul_by1_x4



.type	avx2_sqr_x4,@function
.align	32
avx2_sqr_x4:
	leaq	.LAVX2_POLY(%rip),%rax

	vmovdqa	224(%rax),%ymm14
	vmovdqa	256(%rax),%ymm15

	vmovdqa	0(%rsi),%ymm9
	vmovdqa	32(%rsi),%ymm1
	vmovdqa	64(%rsi),%ymm2
	vmovdqa	96(%rsi),%ymm3
	vmovdqa	128(%rsi),%ymm4
	vmovdqa	160(%rsi),%ymm5
	vmovdqa	192(%rsi),%ymm6
	vmovdqa	224(%rsi),%ymm7
	vpaddq	%ymm1,%ymm1,%ymm1
	vmovdqa	256(%rsi),%ymm8
	vpaddq	%ymm2,%ymm2,%ymm2
	vmovdqa	%ymm1,0(%rcx)
	vpaddq	%ymm3,%ymm3,%ymm3
	vmovdqa	%ymm2,32(%rcx)
	vpaddq	%ymm4,%ymm4,%ymm4
	vmovdqa	%ymm3,64(%rcx)
	vpaddq	%ymm5,%ymm5,%ymm5
	vmovdqa	%ymm4,96(%rcx)
	vpaddq	%ymm6,%ymm6,%ymm6
	vmovdqa	%ymm5,128(%rcx)
	vpaddq	%ymm7,%ymm7,%ymm7
	vmovdqa	%ymm6,160(%rcx)
	vpaddq	%ymm8,%ymm8,%ymm8
	vmovdqa	%ymm7,192(%rcx)
	vmovdqa	%ymm8,224(%rcx)


	vpmuludq	%ymm9,%ymm9,%ymm0
	vpmuludq	%ymm9,%ymm1,%ymm1
	vpand	%ymm12,%ymm0,%ymm10
	vpmuludq	%ymm9,%ymm2,%ymm2
	vpmuludq	%ymm9,%ymm3,%ymm3
	vpmuludq	%ymm9,%ymm4,%ymm4
	vpmuludq	%ymm9,%ymm5,%ymm5
	vpmuludq	%ymm9,%ymm6,%ymm6
	vpmuludq	%ymm12,%ymm10,%ymm11
	vpmuludq	%ymm9,%ymm7,%ymm7
	vpmuludq	%ymm9,%ymm8,%ymm8
	vmovdqa	32(%rsi),%ymm9

	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm13
	vpand	%ymm12,%ymm0,%ymm10
	vpmuludq	32(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm1,%ymm1
	vpmuludq	64(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm2
	vpmuludq	96(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	128(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	160(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	64(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpmuludq	64(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm2
	vpmuludq	96(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	128(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	160(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	96(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm13
	vpmuludq	96(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm3,%ymm3
	vpmuludq	128(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	160(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	128(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm11
	vpmuludq	128(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm4,%ymm4
	vpmuludq	160(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	160(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96+.LAVX2_POLY(%rip),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm13
	vpmuludq	160(%rcx),%ymm9,%ymm11
	vpaddq	%ymm13,%ymm5,%ymm5
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	192(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm11
	vpmuludq	192(%rcx),%ymm9,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm6

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	224(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm13

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm7
	vpmuludq	224(%rcx),%ymm9,%ymm8
	vmovdqa	256(%rsi),%ymm9
	vpaddq	%ymm11,%ymm0,%ymm13
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpand	%ymm12,%ymm0,%ymm10
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7


	vpmuludq	%ymm9,%ymm9,%ymm8

	vpmuludq	%ymm12,%ymm10,%ymm11
	vpaddq	%ymm11,%ymm0,%ymm13
	vpsrlq	$29,%ymm13,%ymm13
	vpaddq	%ymm11,%ymm1,%ymm0
	vpaddq	%ymm11,%ymm2,%ymm1
	vpmuludq	96(%rax),%ymm10,%ymm11
	vpaddq	%ymm13,%ymm0,%ymm0
	vpaddq	%ymm11,%ymm3,%ymm2
	vmovdqa	%ymm4,%ymm3
	vpsllq	$18,%ymm10,%ymm11
	vmovdqa	%ymm5,%ymm4
	vpmuludq	%ymm14,%ymm10,%ymm13
	vpaddq	%ymm11,%ymm6,%ymm5
	vpmuludq	%ymm15,%ymm10,%ymm11
	vpaddq	%ymm13,%ymm7,%ymm6
	vpaddq	%ymm11,%ymm8,%ymm7

	vpxor	%ymm8,%ymm8,%ymm8

	.byte	0xf3,0xc3
.size	avx2_sqr_x4,.-avx2_sqr_x4



.type	avx2_sub_x4,@function
.align	32
avx2_sub_x4:
	vmovdqa	0(%rsi),%ymm0
	leaq	160(%rsi),%rsi
	leaq	.LAVX2_POLY_x8+128(%rip),%rax
	leaq	128(%rdx),%rdx
	vmovdqa	32-160(%rsi),%ymm1
	vmovdqa	64-160(%rsi),%ymm2
	vmovdqa	96-160(%rsi),%ymm3
	vmovdqa	128-160(%rsi),%ymm4
	vmovdqa	160-160(%rsi),%ymm5
	vmovdqa	192-160(%rsi),%ymm6
	vmovdqa	224-160(%rsi),%ymm7
	vmovdqa	256-160(%rsi),%ymm8

	vpaddq	0-128(%rax),%ymm0,%ymm0
	vpaddq	32-128(%rax),%ymm1,%ymm1
	vpaddq	64-128(%rax),%ymm2,%ymm2
	vpaddq	96-128(%rax),%ymm3,%ymm3
	vpaddq	128-128(%rax),%ymm4,%ymm4
	vpaddq	160-128(%rax),%ymm5,%ymm5
	vpaddq	192-128(%rax),%ymm6,%ymm6
	vpaddq	224-128(%rax),%ymm7,%ymm7
	vpaddq	256-128(%rax),%ymm8,%ymm8

	vpsubq	0-128(%rdx),%ymm0,%ymm0
	vpsubq	32-128(%rdx),%ymm1,%ymm1
	vpsubq	64-128(%rdx),%ymm2,%ymm2
	vpsubq	96-128(%rdx),%ymm3,%ymm3
	vpsubq	128-128(%rdx),%ymm4,%ymm4
	vpsubq	160-128(%rdx),%ymm5,%ymm5
	vpsubq	192-128(%rdx),%ymm6,%ymm6
	vpsubq	224-128(%rdx),%ymm7,%ymm7
	vpsubq	256-128(%rdx),%ymm8,%ymm8

	.byte	0xf3,0xc3
.size	avx2_sub_x4,.-avx2_sub_x4

.type	avx2_select_n_store,@function
.align	32
avx2_select_n_store:
	vmovdqa	2312(%rsp),%ymm10
	vpor	2344(%rsp),%ymm10,%ymm10

	vpandn	%ymm0,%ymm10,%ymm0
	vpandn	%ymm1,%ymm10,%ymm1
	vpandn	%ymm2,%ymm10,%ymm2
	vpandn	%ymm3,%ymm10,%ymm3
	vpandn	%ymm4,%ymm10,%ymm4
	vpandn	%ymm5,%ymm10,%ymm5
	vpandn	%ymm6,%ymm10,%ymm6
	vmovdqa	2344(%rsp),%ymm9
	vpandn	%ymm7,%ymm10,%ymm7
	vpandn	2312(%rsp),%ymm9,%ymm9
	vpandn	%ymm8,%ymm10,%ymm8

	vpand	0(%rsi),%ymm9,%ymm11
	leaq	160(%rsi),%rax
	vpand	32(%rsi),%ymm9,%ymm10
	vpxor	%ymm11,%ymm0,%ymm0
	vpand	64(%rsi),%ymm9,%ymm11
	vpxor	%ymm10,%ymm1,%ymm1
	vpand	96(%rsi),%ymm9,%ymm10
	vpxor	%ymm11,%ymm2,%ymm2
	vpand	128-160(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm3,%ymm3
	vpand	160-160(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm4,%ymm4
	vpand	192-160(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm5,%ymm5
	vpand	224-160(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm6,%ymm6
	vpand	256-160(%rax),%ymm9,%ymm11
	vmovdqa	2344(%rsp),%ymm9
	vpxor	%ymm10,%ymm7,%ymm7

	vpand	0(%rdx),%ymm9,%ymm10
	leaq	160(%rdx),%rax
	vpxor	%ymm11,%ymm8,%ymm8
	vpand	32(%rdx),%ymm9,%ymm11
	vpxor	%ymm10,%ymm0,%ymm0
	vpand	64(%rdx),%ymm9,%ymm10
	vpxor	%ymm11,%ymm1,%ymm1
	vpand	96(%rdx),%ymm9,%ymm11
	vpxor	%ymm10,%ymm2,%ymm2
	vpand	128-160(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm3,%ymm3
	vpand	160-160(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm4,%ymm4
	vpand	192-160(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm5,%ymm5
	vpand	224-160(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm6,%ymm6
	vpand	256-160(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm7,%ymm7
	vpxor	%ymm10,%ymm8,%ymm8
	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vmovdqa	%ymm1,32(%rdi)
	vmovdqa	%ymm2,64(%rdi)
	vmovdqa	%ymm3,96(%rdi)
	vmovdqa	%ymm4,128-160(%rax)
	vmovdqa	%ymm5,160-160(%rax)
	vmovdqa	%ymm6,192-160(%rax)
	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)


	.byte	0xf3,0xc3
.size	avx2_select_n_store,.-avx2_select_n_store


.globl	ecp_nistz256_avx2_point_add_affine_x4
.type	ecp_nistz256_avx2_point_add_affine_x4,@function
.align	32
ecp_nistz256_avx2_point_add_affine_x4:
	movq	%rsp,%rax
	pushq	%rbp
	vzeroupper
	leaq	-8(%rax),%rbp












	subq	$2624,%rsp
	andq	$-64,%rsp

	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rdx,%r10

	vmovdqa	0(%rsi),%ymm0
	vmovdqa	.LAVX2_AND_MASK(%rip),%ymm12
	vpxor	%ymm1,%ymm1,%ymm1
	leaq	256(%rsi),%rax
	vpor	32(%rsi),%ymm0,%ymm0
	vpor	64(%rsi),%ymm0,%ymm0
	vpor	96(%rsi),%ymm0,%ymm0
	vpor	128-256(%rax),%ymm0,%ymm0
	leaq	256(%rax),%rcx
	vpor	160-256(%rax),%ymm0,%ymm0
	vpor	192-256(%rax),%ymm0,%ymm0
	vpor	224-256(%rax),%ymm0,%ymm0
	vpor	256-256(%rax),%ymm0,%ymm0
	vpor	288-256(%rax),%ymm0,%ymm0
	vpor	320-256(%rax),%ymm0,%ymm0
	vpor	352-256(%rax),%ymm0,%ymm0
	vpor	384-512(%rcx),%ymm0,%ymm0
	vpor	416-512(%rcx),%ymm0,%ymm0
	vpor	448-512(%rcx),%ymm0,%ymm0
	vpor	480-512(%rcx),%ymm0,%ymm0
	vpor	512-512(%rcx),%ymm0,%ymm0
	vpor	544-512(%rcx),%ymm0,%ymm0
	vpcmpeqq	%ymm1,%ymm0,%ymm0
	vmovdqa	%ymm0,2304(%rsp)

	vpxor	%ymm1,%ymm1,%ymm1
	vmovdqa	0(%r10),%ymm0
	leaq	256(%r10),%rax
	vpor	32(%r10),%ymm0,%ymm0
	vpor	64(%r10),%ymm0,%ymm0
	vpor	96(%r10),%ymm0,%ymm0
	vpor	128-256(%rax),%ymm0,%ymm0
	leaq	256(%rax),%rcx
	vpor	160-256(%rax),%ymm0,%ymm0
	vpor	192-256(%rax),%ymm0,%ymm0
	vpor	224-256(%rax),%ymm0,%ymm0
	vpor	256-256(%rax),%ymm0,%ymm0
	vpor	288-256(%rax),%ymm0,%ymm0
	vpor	320-256(%rax),%ymm0,%ymm0
	vpor	352-256(%rax),%ymm0,%ymm0
	vpor	384-512(%rcx),%ymm0,%ymm0
	vpor	416-512(%rcx),%ymm0,%ymm0
	vpor	448-512(%rcx),%ymm0,%ymm0
	vpor	480-512(%rcx),%ymm0,%ymm0
	vpor	512-512(%rcx),%ymm0,%ymm0
	vpor	544-512(%rcx),%ymm0,%ymm0
	vpcmpeqq	%ymm1,%ymm0,%ymm0
	vmovdqa	%ymm0,2336(%rsp)


	leaq	576(%r9),%rsi
	leaq	576(%rsp),%rdi
	leaq	2368(%rsp),%rcx
	call	avx2_sqr_x4
	call	avx2_normalize_n_store


	leaq	0(%r10),%rsi
	leaq	576(%rsp),%rdx
	leaq	0(%rsp),%rdi
	call	avx2_mul_x4

	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vmovdqa	%ymm1,32(%rdi)
	vmovdqa	%ymm2,64(%rdi)
	vmovdqa	%ymm3,96(%rdi)
	vmovdqa	%ymm4,128-160(%rax)
	vmovdqa	%ymm5,160-160(%rax)
	vmovdqa	%ymm6,192-160(%rax)
	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)



	leaq	576(%r9),%rsi
	leaq	576(%rsp),%rdx
	leaq	288(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	288(%r10),%rsi
	leaq	288(%rsp),%rdx
	leaq	288(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	0(%rsp),%rsi
	leaq	0(%r9),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	288(%rsp),%rsi
	leaq	288(%r9),%rdx
	leaq	1152(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	576(%r9),%rdx
	leaq	576(%r8),%rdi
	call	avx2_mul_x4
	call	avx2_normalize

	leaq	.LONE(%rip),%rsi
	leaq	576(%r9),%rdx
	call	avx2_select_n_store


	leaq	1152(%rsp),%rsi
	leaq	1728(%rsp),%rdi
	leaq	2368(%rsp),%rcx
	call	avx2_sqr_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1440(%rsp),%rdi
	call	avx2_sqr_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1440(%rsp),%rdx
	leaq	2016(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	0(%r9),%rsi
	leaq	1440(%rsp),%rdx
	leaq	0(%rsp),%rdi
	call	avx2_mul_x4

	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vmovdqa	%ymm1,32(%rdi)
	vmovdqa	%ymm2,64(%rdi)
	vmovdqa	%ymm3,96(%rdi)
	vmovdqa	%ymm4,128-160(%rax)
	vmovdqa	%ymm5,160-160(%rax)
	vmovdqa	%ymm6,192-160(%rax)
	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)







	vpaddq	%ymm0,%ymm0,%ymm0
	leaq	1440(%rsp),%rdi
	vpaddq	%ymm1,%ymm1,%ymm1
	vpaddq	%ymm2,%ymm2,%ymm2
	vpaddq	%ymm3,%ymm3,%ymm3
	vpaddq	%ymm4,%ymm4,%ymm4
	vpaddq	%ymm5,%ymm5,%ymm5
	vpaddq	%ymm6,%ymm6,%ymm6
	vpaddq	%ymm7,%ymm7,%ymm7
	vpaddq	%ymm8,%ymm8,%ymm8
	call	avx2_normalize_n_store

















	leaq	1856(%rsp),%rsi
	leaq	.LAVX2_POLY_x2+128(%rip),%rax
	leaq	2144(%rsp),%rdx
	leaq	1568(%rsp),%rcx
	leaq	0(%r8),%rdi

	vmovdqa	0-128(%rsi),%ymm0
	vmovdqa	32-128(%rsi),%ymm1
	vmovdqa	64-128(%rsi),%ymm2
	vmovdqa	96-128(%rsi),%ymm3
	vmovdqa	128-128(%rsi),%ymm4
	vmovdqa	160-128(%rsi),%ymm5
	vmovdqa	192-128(%rsi),%ymm6
	vmovdqa	224-128(%rsi),%ymm7
	vmovdqa	256-128(%rsi),%ymm8

	vpaddq	0-128(%rax),%ymm0,%ymm0
	vpaddq	32-128(%rax),%ymm1,%ymm1
	vpaddq	64-128(%rax),%ymm2,%ymm2
	vpaddq	96-128(%rax),%ymm3,%ymm3
	vpaddq	128-128(%rax),%ymm4,%ymm4
	vpaddq	160-128(%rax),%ymm5,%ymm5
	vpaddq	192-128(%rax),%ymm6,%ymm6
	vpaddq	224-128(%rax),%ymm7,%ymm7
	vpaddq	256-128(%rax),%ymm8,%ymm8

	vpsubq	0-128(%rdx),%ymm0,%ymm0
	vpsubq	32-128(%rdx),%ymm1,%ymm1
	vpsubq	64-128(%rdx),%ymm2,%ymm2
	vpsubq	96-128(%rdx),%ymm3,%ymm3
	vpsubq	128-128(%rdx),%ymm4,%ymm4
	vpsubq	160-128(%rdx),%ymm5,%ymm5
	vpsubq	192-128(%rdx),%ymm6,%ymm6
	vpsubq	224-128(%rdx),%ymm7,%ymm7
	vpsubq	256-128(%rdx),%ymm8,%ymm8

	vpsubq	0-128(%rcx),%ymm0,%ymm0
	vpsubq	32-128(%rcx),%ymm1,%ymm1
	vpsubq	64-128(%rcx),%ymm2,%ymm2
	vpsubq	96-128(%rcx),%ymm3,%ymm3
	vpsubq	128-128(%rcx),%ymm4,%ymm4
	vpsubq	160-128(%rcx),%ymm5,%ymm5
	vpsubq	192-128(%rcx),%ymm6,%ymm6
	vpsubq	224-128(%rcx),%ymm7,%ymm7
	vpsubq	256-128(%rcx),%ymm8,%ymm8
	call	avx2_normalize

	leaq	0(%r10),%rsi
	leaq	0(%r9),%rdx
	call	avx2_select_n_store


	leaq	0(%rsp),%rsi
	leaq	0(%r8),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1152(%rsp),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	2016(%rsp),%rsi
	leaq	288(%r9),%rdx
	leaq	288(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	288(%rsp),%rdx
	leaq	288(%r8),%rdi
	call	avx2_sub_x4
	call	avx2_normalize

	leaq	288(%r10),%rsi
	leaq	288(%r9),%rdx
	call	avx2_select_n_store







	leaq	288(%r8),%rsi
	leaq	288(%r8),%rdi
	call	avx2_mul_by1_x4
	call	avx2_normalize_n_store

	vzeroupper
	movq	%rbp,%rsp
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_point_add_affine_x4,.-ecp_nistz256_avx2_point_add_affine_x4



.globl	ecp_nistz256_avx2_point_add_affines_x4
.type	ecp_nistz256_avx2_point_add_affines_x4,@function
.align	32
ecp_nistz256_avx2_point_add_affines_x4:
	movq	%rsp,%rax
	pushq	%rbp
	vzeroupper
	leaq	-8(%rax),%rbp











	subq	$2624,%rsp
	andq	$-64,%rsp

	movq	%rdi,%r8
	movq	%rsi,%r9
	movq	%rdx,%r10

	vmovdqa	0(%rsi),%ymm0
	vmovdqa	.LAVX2_AND_MASK(%rip),%ymm12
	vpxor	%ymm1,%ymm1,%ymm1
	leaq	256(%rsi),%rax
	vpor	32(%rsi),%ymm0,%ymm0
	vpor	64(%rsi),%ymm0,%ymm0
	vpor	96(%rsi),%ymm0,%ymm0
	vpor	128-256(%rax),%ymm0,%ymm0
	leaq	256(%rax),%rcx
	vpor	160-256(%rax),%ymm0,%ymm0
	vpor	192-256(%rax),%ymm0,%ymm0
	vpor	224-256(%rax),%ymm0,%ymm0
	vpor	256-256(%rax),%ymm0,%ymm0
	vpor	288-256(%rax),%ymm0,%ymm0
	vpor	320-256(%rax),%ymm0,%ymm0
	vpor	352-256(%rax),%ymm0,%ymm0
	vpor	384-512(%rcx),%ymm0,%ymm0
	vpor	416-512(%rcx),%ymm0,%ymm0
	vpor	448-512(%rcx),%ymm0,%ymm0
	vpor	480-512(%rcx),%ymm0,%ymm0
	vpor	512-512(%rcx),%ymm0,%ymm0
	vpor	544-512(%rcx),%ymm0,%ymm0
	vpcmpeqq	%ymm1,%ymm0,%ymm0
	vmovdqa	%ymm0,2304(%rsp)

	vpxor	%ymm1,%ymm1,%ymm1
	vmovdqa	0(%r10),%ymm0
	leaq	256(%r10),%rax
	vpor	32(%r10),%ymm0,%ymm0
	vpor	64(%r10),%ymm0,%ymm0
	vpor	96(%r10),%ymm0,%ymm0
	vpor	128-256(%rax),%ymm0,%ymm0
	leaq	256(%rax),%rcx
	vpor	160-256(%rax),%ymm0,%ymm0
	vpor	192-256(%rax),%ymm0,%ymm0
	vpor	224-256(%rax),%ymm0,%ymm0
	vpor	256-256(%rax),%ymm0,%ymm0
	vpor	288-256(%rax),%ymm0,%ymm0
	vpor	320-256(%rax),%ymm0,%ymm0
	vpor	352-256(%rax),%ymm0,%ymm0
	vpor	384-512(%rcx),%ymm0,%ymm0
	vpor	416-512(%rcx),%ymm0,%ymm0
	vpor	448-512(%rcx),%ymm0,%ymm0
	vpor	480-512(%rcx),%ymm0,%ymm0
	vpor	512-512(%rcx),%ymm0,%ymm0
	vpor	544-512(%rcx),%ymm0,%ymm0
	vpcmpeqq	%ymm1,%ymm0,%ymm0
	vmovdqa	%ymm0,2336(%rsp)


	leaq	0(%r10),%rsi
	leaq	0(%r9),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	288(%r10),%rsi
	leaq	288(%r9),%rdx
	leaq	1152(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	576(%r8),%rdi
	call	avx2_mul_by1_x4
	call	avx2_normalize

	vmovdqa	2304(%rsp),%ymm9
	vpor	2336(%rsp),%ymm9,%ymm9

	vpandn	%ymm0,%ymm9,%ymm0
	leaq	.LONE+128(%rip),%rax
	vpandn	%ymm1,%ymm9,%ymm1
	vpandn	%ymm2,%ymm9,%ymm2
	vpandn	%ymm3,%ymm9,%ymm3
	vpandn	%ymm4,%ymm9,%ymm4
	vpandn	%ymm5,%ymm9,%ymm5
	vpandn	%ymm6,%ymm9,%ymm6
	vpandn	%ymm7,%ymm9,%ymm7

	vpand	0-128(%rax),%ymm9,%ymm11
	vpandn	%ymm8,%ymm9,%ymm8
	vpand	32-128(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm0,%ymm0
	vpand	64-128(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm1,%ymm1
	vpand	96-128(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm2,%ymm2
	vpand	128-128(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm3,%ymm3
	vpand	160-128(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm4,%ymm4
	vpand	192-128(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm5,%ymm5
	vpand	224-128(%rax),%ymm9,%ymm10
	vpxor	%ymm11,%ymm6,%ymm6
	vpand	256-128(%rax),%ymm9,%ymm11
	vpxor	%ymm10,%ymm7,%ymm7
	vpxor	%ymm11,%ymm8,%ymm8
	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vmovdqa	%ymm1,32(%rdi)
	vmovdqa	%ymm2,64(%rdi)
	vmovdqa	%ymm3,96(%rdi)
	vmovdqa	%ymm4,128-160(%rax)
	vmovdqa	%ymm5,160-160(%rax)
	vmovdqa	%ymm6,192-160(%rax)
	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)



	leaq	1152(%rsp),%rsi
	leaq	1728(%rsp),%rdi
	leaq	2368(%rsp),%rcx
	call	avx2_sqr_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1440(%rsp),%rdi
	call	avx2_sqr_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1440(%rsp),%rdx
	leaq	2016(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	0(%r9),%rsi
	leaq	1440(%rsp),%rdx
	leaq	0(%rsp),%rdi
	call	avx2_mul_x4

	vmovdqa	%ymm0,0(%rdi)
	leaq	160(%rdi),%rax
	vmovdqa	%ymm1,32(%rdi)
	vmovdqa	%ymm2,64(%rdi)
	vmovdqa	%ymm3,96(%rdi)
	vmovdqa	%ymm4,128-160(%rax)
	vmovdqa	%ymm5,160-160(%rax)
	vmovdqa	%ymm6,192-160(%rax)
	vmovdqa	%ymm7,224-160(%rax)
	vmovdqa	%ymm8,256-160(%rax)







	vpaddq	%ymm0,%ymm0,%ymm0
	leaq	1440(%rsp),%rdi
	vpaddq	%ymm1,%ymm1,%ymm1
	vpaddq	%ymm2,%ymm2,%ymm2
	vpaddq	%ymm3,%ymm3,%ymm3
	vpaddq	%ymm4,%ymm4,%ymm4
	vpaddq	%ymm5,%ymm5,%ymm5
	vpaddq	%ymm6,%ymm6,%ymm6
	vpaddq	%ymm7,%ymm7,%ymm7
	vpaddq	%ymm8,%ymm8,%ymm8
	call	avx2_normalize_n_store

















	leaq	1856(%rsp),%rsi
	leaq	.LAVX2_POLY_x2+128(%rip),%rax
	leaq	2144(%rsp),%rdx
	leaq	1568(%rsp),%rcx
	leaq	0(%r8),%rdi

	vmovdqa	0-128(%rsi),%ymm0
	vmovdqa	32-128(%rsi),%ymm1
	vmovdqa	64-128(%rsi),%ymm2
	vmovdqa	96-128(%rsi),%ymm3
	vmovdqa	128-128(%rsi),%ymm4
	vmovdqa	160-128(%rsi),%ymm5
	vmovdqa	192-128(%rsi),%ymm6
	vmovdqa	224-128(%rsi),%ymm7
	vmovdqa	256-128(%rsi),%ymm8

	vpaddq	0-128(%rax),%ymm0,%ymm0
	vpaddq	32-128(%rax),%ymm1,%ymm1
	vpaddq	64-128(%rax),%ymm2,%ymm2
	vpaddq	96-128(%rax),%ymm3,%ymm3
	vpaddq	128-128(%rax),%ymm4,%ymm4
	vpaddq	160-128(%rax),%ymm5,%ymm5
	vpaddq	192-128(%rax),%ymm6,%ymm6
	vpaddq	224-128(%rax),%ymm7,%ymm7
	vpaddq	256-128(%rax),%ymm8,%ymm8

	vpsubq	0-128(%rdx),%ymm0,%ymm0
	vpsubq	32-128(%rdx),%ymm1,%ymm1
	vpsubq	64-128(%rdx),%ymm2,%ymm2
	vpsubq	96-128(%rdx),%ymm3,%ymm3
	vpsubq	128-128(%rdx),%ymm4,%ymm4
	vpsubq	160-128(%rdx),%ymm5,%ymm5
	vpsubq	192-128(%rdx),%ymm6,%ymm6
	vpsubq	224-128(%rdx),%ymm7,%ymm7
	vpsubq	256-128(%rdx),%ymm8,%ymm8

	vpsubq	0-128(%rcx),%ymm0,%ymm0
	vpsubq	32-128(%rcx),%ymm1,%ymm1
	vpsubq	64-128(%rcx),%ymm2,%ymm2
	vpsubq	96-128(%rcx),%ymm3,%ymm3
	vpsubq	128-128(%rcx),%ymm4,%ymm4
	vpsubq	160-128(%rcx),%ymm5,%ymm5
	vpsubq	192-128(%rcx),%ymm6,%ymm6
	vpsubq	224-128(%rcx),%ymm7,%ymm7
	vpsubq	256-128(%rcx),%ymm8,%ymm8
	call	avx2_normalize

	leaq	0(%r10),%rsi
	leaq	0(%r9),%rdx
	call	avx2_select_n_store


	leaq	0(%rsp),%rsi
	leaq	0(%r8),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_sub_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	1152(%rsp),%rdx
	leaq	864(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	2016(%rsp),%rsi
	leaq	288(%r9),%rdx
	leaq	288(%rsp),%rdi
	call	avx2_mul_x4
	call	avx2_normalize_n_store


	leaq	864(%rsp),%rsi
	leaq	288(%rsp),%rdx
	leaq	288(%r8),%rdi
	call	avx2_sub_x4
	call	avx2_normalize

	leaq	288(%r10),%rsi
	leaq	288(%r9),%rdx
	call	avx2_select_n_store







	leaq	288(%r8),%rsi
	leaq	288(%r8),%rdi
	call	avx2_mul_by1_x4
	call	avx2_normalize_n_store

	vzeroupper
	movq	%rbp,%rsp
	popq	%rbp
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_point_add_affines_x4,.-ecp_nistz256_avx2_point_add_affines_x4



.globl	ecp_nistz256_avx2_to_mont
.type	ecp_nistz256_avx2_to_mont,@function
.align	32
ecp_nistz256_avx2_to_mont:
	vzeroupper
	vmovdqa	.LAVX2_AND_MASK(%rip),%ymm12
	leaq	.LTO_MONT_AVX2(%rip),%rdx
	call	avx2_mul_x4
	call	avx2_normalize_n_store

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_to_mont,.-ecp_nistz256_avx2_to_mont



.globl	ecp_nistz256_avx2_from_mont
.type	ecp_nistz256_avx2_from_mont,@function
.align	32
ecp_nistz256_avx2_from_mont:
	vzeroupper
	vmovdqa	.LAVX2_AND_MASK(%rip),%ymm12
	leaq	.LFROM_MONT_AVX2(%rip),%rdx
	call	avx2_mul_x4
	call	avx2_normalize_n_store

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_from_mont,.-ecp_nistz256_avx2_from_mont



.globl	ecp_nistz256_avx2_set1
.type	ecp_nistz256_avx2_set1,@function
.align	32
ecp_nistz256_avx2_set1:
	leaq	.LONE+128(%rip),%rax
	leaq	128(%rdi),%rdi
	vzeroupper
	vmovdqa	0-128(%rax),%ymm0
	vmovdqa	32-128(%rax),%ymm1
	vmovdqa	64-128(%rax),%ymm2
	vmovdqa	96-128(%rax),%ymm3
	vmovdqa	128-128(%rax),%ymm4
	vmovdqa	160-128(%rax),%ymm5
	vmovdqa	%ymm0,0-128(%rdi)
	vmovdqa	192-128(%rax),%ymm0
	vmovdqa	%ymm1,32-128(%rdi)
	vmovdqa	224-128(%rax),%ymm1
	vmovdqa	%ymm2,64-128(%rdi)
	vmovdqa	256-128(%rax),%ymm2
	vmovdqa	%ymm3,96-128(%rdi)
	vmovdqa	%ymm4,128-128(%rdi)
	vmovdqa	%ymm5,160-128(%rdi)
	vmovdqa	%ymm0,192-128(%rdi)
	vmovdqa	%ymm1,224-128(%rdi)
	vmovdqa	%ymm2,256-128(%rdi)

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_set1,.-ecp_nistz256_avx2_set1
.globl	ecp_nistz256_avx2_multi_gather_w7
.type	ecp_nistz256_avx2_multi_gather_w7,@function
.align	32
ecp_nistz256_avx2_multi_gather_w7:
	vzeroupper
	leaq	.LIntOne(%rip),%rax

	vmovd	%edx,%xmm0
	vmovd	%ecx,%xmm1
	vmovd	%r8d,%xmm2
	vmovd	%r9d,%xmm3

	vpxor	%ymm4,%ymm4,%ymm4
	vpxor	%ymm5,%ymm5,%ymm5
	vpxor	%ymm6,%ymm6,%ymm6
	vpxor	%ymm7,%ymm7,%ymm7
	vpxor	%ymm8,%ymm8,%ymm8
	vpxor	%ymm9,%ymm9,%ymm9
	vpxor	%ymm10,%ymm10,%ymm10
	vpxor	%ymm11,%ymm11,%ymm11
	vmovdqa	(%rax),%ymm12

	vpermd	%ymm0,%ymm4,%ymm0
	vpermd	%ymm1,%ymm4,%ymm1
	vpermd	%ymm2,%ymm4,%ymm2
	vpermd	%ymm3,%ymm4,%ymm3

	movl	$64,%ecx
	leaq	112(%rdi),%rdi
	jmp	.Lmulti_select_loop_avx2


.align	32
.Lmulti_select_loop_avx2:
	vpcmpeqd	%ymm0,%ymm12,%ymm15

	vmovdqa	0(%rsi),%ymm13
	vmovdqa	32(%rsi),%ymm14
	vpand	%ymm15,%ymm13,%ymm13
	vpand	%ymm15,%ymm14,%ymm14
	vpxor	%ymm13,%ymm4,%ymm4
	vpxor	%ymm14,%ymm5,%ymm5

	vpcmpeqd	%ymm1,%ymm12,%ymm15

	vmovdqa	4096(%rsi),%ymm13
	vmovdqa	4128(%rsi),%ymm14
	vpand	%ymm15,%ymm13,%ymm13
	vpand	%ymm15,%ymm14,%ymm14
	vpxor	%ymm13,%ymm6,%ymm6
	vpxor	%ymm14,%ymm7,%ymm7

	vpcmpeqd	%ymm2,%ymm12,%ymm15

	vmovdqa	8192(%rsi),%ymm13
	vmovdqa	8224(%rsi),%ymm14
	vpand	%ymm15,%ymm13,%ymm13
	vpand	%ymm15,%ymm14,%ymm14
	vpxor	%ymm13,%ymm8,%ymm8
	vpxor	%ymm14,%ymm9,%ymm9

	vpcmpeqd	%ymm3,%ymm12,%ymm15

	vmovdqa	12288(%rsi),%ymm13
	vmovdqa	12320(%rsi),%ymm14
	vpand	%ymm15,%ymm13,%ymm13
	vpand	%ymm15,%ymm14,%ymm14
	vpxor	%ymm13,%ymm10,%ymm10
	vpxor	%ymm14,%ymm11,%ymm11

	vpaddd	(%rax),%ymm12,%ymm12
	leaq	64(%rsi),%rsi

	decl	%ecx
	jnz	.Lmulti_select_loop_avx2

	vmovdqu	%ymm4,0-112(%rdi)
	vmovdqu	%ymm5,32-112(%rdi)
	vmovdqu	%ymm6,64-112(%rdi)
	vmovdqu	%ymm7,96-112(%rdi)
	vmovdqu	%ymm8,128-112(%rdi)
	vmovdqu	%ymm9,160-112(%rdi)
	vmovdqu	%ymm10,192-112(%rdi)
	vmovdqu	%ymm11,224-112(%rdi)

	vzeroupper
	.byte	0xf3,0xc3
.size	ecp_nistz256_avx2_multi_gather_w7,.-ecp_nistz256_avx2_multi_gather_w7


.globl	ecp_nistz_avx2_eligible
.type	ecp_nistz_avx2_eligible,@function
.align	32
ecp_nistz_avx2_eligible:
	movl	OPENSSL_ia32cap_P+8(%rip),%eax
	shrl	$5,%eax
	andl	$1,%eax
	.byte	0xf3,0xc3
.size	ecp_nistz_avx2_eligible,.-ecp_nistz_avx2_eligible
