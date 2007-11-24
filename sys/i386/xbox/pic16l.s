/*-
 * Copyright (c) 2005 Rink Springer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <machine/asmacros.h>

.text

/*
 * send a command to the PIC16L
 *
 * void pic16l_setbyte (int addr, int reg, int data)
 *
 */
ENTRY(pic16l_setbyte)
	push	%ebp
	mov	%esp,%ebp

	push	%ebx

	movw	$0xc000,%dx

1:	xor	%eax,%eax
	inw	%dx,%ax
	shr	$0x0b,%eax
	and	$0x01,%eax
	test	%eax,%eax
	jne	1b
	
	mov	$50,%ecx
2:	movw	$0xc004,%dx
	movl	0x8(%ebp),%eax
	outb	%al,%dx
	movw	$0xc008,%dx
	movl	0xc(%ebp),%eax
	outb	%al,%dx
	movw	$0xc006,%dx
	movl	0x10(%ebp),%eax
	outw	%ax,%dx
	
	movw	$0xc000,%dx
	inw 	%dx,%ax
	outw	%ax,%dx
	
	movw	$0xc002,%dx
	movb	$0x1a,%al
	outb	%al,%dx
	
	movw	$0xc000,%dx
3:
	inb 	%dx,%al
	movb	%al,%bl
	orb 	$0x36,%al
	jz  	3b

	orb 	$0x10,%bl
	jnz  	5f
	
4:
	push	%ecx
	xor	%ecx,%ecx
l:	loop	l
	pop	%ecx

	dec	%ecx
	jz	5f
	jmp	2b
5:

	pop	%ebx

	leave
	ret

/*
 * instructs the pic16l to reboot the xbox
 *
 * void pic16l_reboot();
 *
 */
ENTRY(pic16l_reboot)
	pushl	$0x01
	pushl	$0x02
	pushl	$0x20
	call	pic16l_setbyte
	addl	$12,%esp
	ret

/*
 * instructs the pic16l to power-off the xbox
 *
 * void pic16l_poweroff();
 *
 */
ENTRY(pic16l_poweroff)
	pushl	$0x80
	pushl	$0x02
	pushl	$0x20
	call	pic16l_setbyte
	addl	$12,%esp
	ret

pic16l_ledhlp:
	movw	$0xc000,%dx
1:	xor	%eax,%eax
	inw	%dx,%ax
	shr	$0x0b,%eax
	and	$0x01,%eax
	test	%eax,%eax
	jne	1b
	
	mov	$400,%ecx
	
2:
	movw	$0xc004,%dx
	movb	$0x20,%al
	outb	%al,%dx

	movw	$0xc008,%dx
	movb	%bh,%al
	outb	%al,%dx

	movw	$0xc006,%dx
	movb	%bl,%al
	outb	%al,%dx

	movw	$0xc000,%dx
	inw 	%dx,%ax
	outw	%ax,%dx
	
	movw	$0xc002,%dx
	movb	$0x1a,%al
	outb	%al,%dx
	
	movw	$0xc000,%dx
3:
	inb 	%dx,%al
	movb	%al,%bl
	orb 	$0x36,%al
	jz  	3b
	
	orb 	$0x10,%bl
	jz  	4f
	
	ret
	
4:
	push	%ecx
	xor	%ecx,%ecx
l2:	loop	l2
	pop	%ecx
	dec	%ecx
	jz	5f
	jmp	2b
5:
	ret

/*
 * changes the front led
 *
 * void pic16l_setled (int val);
 */
ENTRY(pic16l_setled)
	push	%ebp
	mov	%esp,%ebp

	push	%ebx

	movl	0x8(%ebp),%ebx
	orl	$0x800,%ebx
	call	pic16l_ledhlp
	movl	$0x701,%ebx
	call	pic16l_ledhlp

	pop	%ebx

	leave
	ret
