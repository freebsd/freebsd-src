/*
 *	from: debug.h, part of Bruce Evans interrupt code
 *	$Id: debug.h,v 1.3 1993/11/13 02:25:20 davidg Exp $
 */

#define	SHOW_A_LOT_NOT

#define	BDBTRAP(name) \
	ss ; \
	cmpb	$0,_bdb_exists ; \
	je	1f ; \
	testb	$SEL_RPL_MASK,4(%esp) ; \
	jne	1f ; \
	ss ; \
bdb_/**/name/**/_ljmp: ; \
	ljmp	$0,$0 ; \
1:

#if 1
#define	COUNT_EVENT(group, event)	incl	(group) + (event) * 4
#else
#define	COUNT_EVENT(group, event)
#endif

#ifdef SHOW_A_LOT

#define	GREEN		0x27	/* 0x27 for true green, 0x07 for mono */
#define	CLI_STI_X	63
#define	CPL_X		46
#define	IMEN_X		64
#define	IPENDING_X	29
#define	RED		0x47	/* 0x47 for true red, 0x70 for mono */

#define	SHOW_BIT(bit) ; \
	movl	%ecx,%eax ; \
	shr	$bit,%eax ; \
	andl	$1,%eax ; \
	movb	bit_colors(%eax),%al ; \
	movb	%al,bit * 2 + 1(%ebx)

#define	SHOW_BITS(var, screen_offset) ; \
	pushl	%ebx ; \
	pushl	%ecx ; \
	movl	_Crtat,%ebx ; \
	addl	$screen_offset * 2,%ebx ; \
	movl	_/**/var,%ecx ; \
	call	show_bits ; \
	popl	%ecx ; \
	popl	%ebx

#define	SHOW_CLI \
	COUNT_EVENT(_intrcnt_show, 0) ; \
	pushl	%eax ; \
	movl	_Crtat,%eax ; \
	movb	$RED,CLI_STI_X * 2 + 1(%eax) ; \
	popl	%eax

#define	SHOW_CPL \
	COUNT_EVENT(_intrcnt_show, 1) ; \
	SHOW_BITS(cpl, CPL_X) ; \

#define	SHOW_IMEN \
	COUNT_EVENT(_intrcnt_show, 2) ; \
	SHOW_BITS(imen, IMEN_X)

#define	SHOW_IPENDING \
	COUNT_EVENT(_intrcnt_show, 3) ; \
	SHOW_BITS(ipending, IPENDING_X)

#define	SHOW_STI \
	COUNT_EVENT(_intrcnt_show, 4) ; \
	pushl	%eax ; \
	movl	_Crtat,%eax ; \
	movb	$GREEN,CLI_STI_X * 2 + 1(%eax) ; \
	popl	%eax

#else /* not SHOW_A_LOT */

#define	SHOW_CLI	COUNT_EVENT(_intrcnt_show, 0)
#define	SHOW_CPL	COUNT_EVENT(_intrcnt_show, 1)
#define	SHOW_IMEN	COUNT_EVENT(_intrcnt_show, 2)
#define	SHOW_IPENDING	COUNT_EVENT(_intrcnt_show, 3)
#define	SHOW_STI	COUNT_EVENT(_intrcnt_show, 4)

#endif /* SHOW_A_LOT */
