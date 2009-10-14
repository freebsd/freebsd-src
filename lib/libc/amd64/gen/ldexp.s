	.file	"ldexp.c"
#APP
	.ident	"$FreeBSD$"
#NO_APP
	.text
	.p2align 4,,15
.globl ldexp
	.type	ldexp, @function
ldexp:
.LFB2:
	cvtsi2sd	%edi, %xmm1
	movsd	%xmm0, -16(%rsp)
	movsd	%xmm1, -8(%rsp)
	fldl	-8(%rsp)
	fldl	-16(%rsp)
#APP
	fscale 
#NO_APP
	fstp	%st(1)
	fstpl	-16(%rsp)
	movsd	-16(%rsp), %xmm0
	ret
.LFE2:
	.size	ldexp, .-ldexp
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0x0
	.byte	0x1
	.string	"zR"
	.uleb128 0x1
	.sleb128 -8
	.byte	0x10
	.uleb128 0x1
	.byte	0x3
	.byte	0xc
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1
.LASFDE1:
	.long	.LASFDE1-.Lframe1
	.long	.LFB2
	.long	.LFE2-.LFB2
	.uleb128 0x0
	.align 8
.LEFDE1:
	.ident	"GCC: (GNU) 4.2.1 20070719  [FreeBSD]"
