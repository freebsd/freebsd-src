/*	$Id$	*/

/*
 *  PC console handling
 * originally from: FreeBSD:sys/i386/boot/netboot/start2.S via NetBSD
 */

#include <machine/asm.h>

#define	data32	.byte 0x66

	.text

/**************************************************************************
PUTC - Print a character
**************************************************************************/
ENTRY(vidputc)
	push	%ebp
	mov	%esp,%ebp
	push	%ecx
	push	%ebx
	push	%esi
	push	%edi

	movb	8(%ebp),%cl

	call	CNAME(prot_to_real)	# enter real mode

	movb	%cl,%al
	data32
	mov	$1,%ebx
	movb	$0x0e,%ah
	int	$0x10

	data32
	call	CNAME(real_to_prot) # back to protected mode

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ecx
	pop	%ebp
	ret

/**************************************************************************
GETC - Get a character
**************************************************************************/
ENTRY(kbdgetc)
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi

	call	CNAME(prot_to_real)	# enter real mode

	movb	$0x0,%ah
	int	$0x16
	movb	%al,%bl

	data32
	call	CNAME(real_to_prot) # back to protected mode

	xor	%eax,%eax
	movb	%bl,%al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

/**************************************************************************
ISKEY - Check for keyboard interrupt
**************************************************************************/
ENTRY(kbdiskey)
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi

	call	CNAME(prot_to_real)	# enter real mode

	xor	%ebx,%ebx
	movb	$0x1,%ah
	int	$0x16
	data32
	jz	1f
	movb	%al,%bl
1:

	data32
	call	CNAME(real_to_prot) # back to protected mode

	xor	%eax,%eax
	movb	%bl,%al

	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
