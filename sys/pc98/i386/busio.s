/* $FreeBSD$ */
/*	$NecBSD: busio.s,v 1.16.4.1 1999/08/16 09:06:08 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 * [Ported for FreeBSD]
 *  Copyright (c) 2001
 *	TAKAHASHI Yoshihiro. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 */

#include <machine/asmacros.h>

#include "assym.s"

/***********************************************************
 * Bus IO access methods (Direct Access)
 ***********************************************************/
#define	BUS_ACCESS_ADDR(BSHREG,ADDRREG) \
	addl BUS_SPACE_HANDLE_BASE/**/(%/**/BSHREG/**/),%/**/ADDRREG

/* 
 *	read_N 
 *	IN:  edx port
 *	OUT: eax data
 */
ENTRY(SBUS_DA_io_space_read_1)
	BUS_ACCESS_ADDR(ebx,edx)
	inb	%dx,%al
	ret

ENTRY(SBUS_DA_io_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	inw	%dx,%ax
	ret

ENTRY(SBUS_DA_io_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	inl	%dx,%eax
	ret

/*
 *	write_N
 *	IN:eax DATA 
 *	   edx PORT
 */
ENTRY(SBUS_DA_io_space_write_1)
	BUS_ACCESS_ADDR(ebx,edx)
	outb	%al,%dx
	ret

ENTRY(SBUS_DA_io_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	outw	%ax,%dx
	ret

ENTRY(SBUS_DA_io_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	outl	%eax,%dx
	ret

/*
 *	read_multi_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    edi BUFP
 */
ENTRY(SBUS_DA_io_space_read_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insb
	ret

ENTRY(SBUS_DA_io_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insw
	ret

ENTRY(SBUS_DA_io_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insl
	ret

/*
 *	write_multi_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    esi BUFP
 */
ENTRY(SBUS_DA_io_space_write_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsb
	ret

ENTRY(SBUS_DA_io_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsw
	ret

ENTRY(SBUS_DA_io_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsl
	ret

/*
 *	read_region_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    edi BUFP
 */
ENTRY(SBUS_DA_io_space_read_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	inb	%dx,%al
	stosb
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_io_space_read_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	inw	%dx,%ax
	stosw
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_io_space_read_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	inl	%dx,%eax
	stosl
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

/*
 *	write_region_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    esi BUFP
 */
ENTRY(SBUS_DA_io_space_write_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsb
	outb	%al,%dx
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret	

ENTRY(SBUS_DA_io_space_write_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsw
	outw	%ax,%dx
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret	

ENTRY(SBUS_DA_io_space_write_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsl
	outl	%eax,%dx
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret	

/*
 *	set_multi_N
 *	IN: eax DATA
 *	    ecx COUNT
 *	    edx PORT
 */
ENTRY(SBUS_DA_io_space_set_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outb	%al,%dx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_io_space_set_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outw	%ax,%dx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_io_space_set_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outl	%eax,%dx
	decl	%ecx
	jnz	1b
2:
	ret

/*
 *	set_region_N
 *	IN: eax DATA
 *	    ecx COUNT
 *	    edx PORT
 */
ENTRY(SBUS_DA_io_space_set_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outb	%al,%dx
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_io_space_set_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outw	%ax,%dx
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_io_space_set_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outl	%eax,%dx
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	ret

/*
 *	copy_region_N
 *	IN: ecx COUNT
 *	    esi SPORT
 *	    edi	DPORT
 */
ENTRY(SBUS_DA_io_space_copy_region_1)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	movl	%esi,%edx
	inb	%dx,%al
	incl	%esi

	movl	%edi,%edx
	outb	%al,%dx
	incl	%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

ENTRY(SBUS_DA_io_space_copy_region_2)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	movl	%esi,%edx
	inw	%dx,%ax
	addl	$2,%esi

	movl	%edi,%edx
	outw	%ax,%dx
	addl	$2,%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

ENTRY(SBUS_DA_io_space_copy_region_4)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	movl	%esi,%edx
	inl	%dx,%eax
	addl	$4,%esi

	movl	%edi,%edx
	outl	%eax,%dx
	addl	$4,%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

/***********************************************************
 * Bus Memory access methods (Direct Access)
 ***********************************************************/
/*
 *	read_N
 */
ENTRY(SBUS_DA_mem_space_read_1)
	BUS_ACCESS_ADDR(ebx,edx)
	movb	(%edx),%al
	ret

ENTRY(SBUS_DA_mem_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	movw	(%edx),%ax
	ret

ENTRY(SBUS_DA_mem_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	movl	(%edx),%eax
	ret

/*
 *	write_N
 */
ENTRY(SBUS_DA_mem_space_write_1)
	BUS_ACCESS_ADDR(ebx,edx)
	movb	%al,(%edx)
	ret

ENTRY(SBUS_DA_mem_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	movw	%ax,(%edx)
	ret

ENTRY(SBUS_DA_mem_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	movl	%eax,(%edx)
	ret

/*
 *	read_multi_N
 */
ENTRY(SBUS_DA_mem_space_read_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movb	(%edx),%al
	stosb
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_mem_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movw	(%edx),%ax	
	stosw
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_mem_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movl	(%edx),%eax	
	stosl
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

/*
 *	write_multi_N
 */
ENTRY(SBUS_DA_mem_space_write_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsb
	movb	%al,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_mem_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsw
	movw	%ax,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_DA_mem_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsl
	movl	%eax,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

/*
 *	read_region_N
 */
ENTRY(SBUS_DA_mem_space_read_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%esi
	movl	%edx,%esi
	rep
	movsb
	popl	%esi
	ret

ENTRY(SBUS_DA_mem_space_read_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%esi
	movl	%edx,%esi
	rep
	movsw
	popl	%esi
	ret

ENTRY(SBUS_DA_mem_space_read_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%esi
	movl	%edx,%esi
	rep
	movsl
	popl	%esi
	ret

/*
 *	write_region_N
 */
ENTRY(SBUS_DA_mem_space_write_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	movsb
	popl	%edi
	ret

ENTRY(SBUS_DA_mem_space_write_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	movsw
	popl	%edi
	ret

ENTRY(SBUS_DA_mem_space_write_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	movsl
	popl	%edi
	ret

/*
 *	set_multi_N
 */
ENTRY(SBUS_DA_mem_space_set_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movb	%al,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_mem_space_set_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movw	%ax,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_DA_mem_space_set_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movl	%eax,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

/*
 *	set_region_N
 */
ENTRY(SBUS_DA_mem_space_set_region_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	stosb
	popl	%edi
	ret

ENTRY(SBUS_DA_mem_space_set_region_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	stosw
	popl	%edi
	ret

ENTRY(SBUS_DA_mem_space_set_region_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%edi
	movl	%edx,%edi
	rep
	stosl
	popl	%edi
	ret

/*
 *	copy_region_N
 */
ENTRY(SBUS_DA_mem_space_copy_region_1)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	cld
	rep
	movsb
	ret

ENTRY(SBUS_DA_mem_space_copy_region_2)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	cld
	rep
	movsw
	ret

ENTRY(SBUS_DA_mem_space_copy_region_4)
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	cld
	rep
	movsl
	ret

#undef	BUS_ACCESS_ADDR

/***********************************************************
 * Bus IO access methods (Relocate Access)
 ***********************************************************/
#define	BUS_ACCESS_ADDR(BSHREG,ADDRREG)	\
	movl BUS_SPACE_HANDLE_IAT/**/(%/**/BSHREG/**/, %/**/ADDRREG/**/, 4), \
	     %/**/ADDRREG
#define	BUS_ACCESS_ADDR2(BSHREG,ADDRREG,DSTREG)	\
	movl BUS_SPACE_HANDLE_IAT/**/(%/**/BSHREG/**/, %/**/ADDRREG/**/, 4), \
	     %/**/DSTREG
/* 
 *	read_N 
 *	IN:  edx port
 *	OUT: eax data
 */
ENTRY(SBUS_RA_io_space_read_1)
	BUS_ACCESS_ADDR(ebx,edx)
	inb	%dx,%al
	ret

ENTRY(SBUS_RA_io_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	inw	%dx,%ax
	ret

ENTRY(SBUS_RA_io_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	inl	%dx,%eax
	ret

/*
 *	write_N
 *	IN:eax DATA 
 *	   edx PORT
 */
ENTRY(SBUS_RA_io_space_write_1)
	BUS_ACCESS_ADDR(ebx,edx)
	outb	%al,%dx
	ret

ENTRY(SBUS_RA_io_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	outw	%ax,%dx
	ret

ENTRY(SBUS_RA_io_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	outl	%eax,%dx
	ret

/*
 *	read_multi_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    edi BUFP
 */
ENTRY(SBUS_RA_io_space_read_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insb
	ret

ENTRY(SBUS_RA_io_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insw
	ret

ENTRY(SBUS_RA_io_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	insl
	ret

/*
 *	write_multi_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    esi BUFP
 */
ENTRY(SBUS_RA_io_space_write_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsb
	ret

ENTRY(SBUS_RA_io_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsw
	ret

ENTRY(SBUS_RA_io_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	rep
	outsl
	ret

/*
 *	read_region_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    edi BUFP
 */
ENTRY(SBUS_RA_io_space_read_region_1)
	cld
	pushl	%eax
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%esi
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inb	%dx,%al
	stosb
	incl	%esi
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	popl	%eax
	ret

ENTRY(SBUS_RA_io_space_read_region_2)
	cld
	pushl	%eax
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%esi
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inw	%dx,%ax
	stosw
	addl	$2,%esi
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	popl	%eax
	ret

ENTRY(SBUS_RA_io_space_read_region_4)
	cld
	pushl	%eax
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%esi
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inl	%dx,%eax
	stosl
	addl	$4,%esi
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	popl	%eax
	ret

/*
 *	write_region_N
 *	IN: ecx COUNT
 *	    edx PORT
 *	    esi BUFP
 */
ENTRY(SBUS_RA_io_space_write_region_1)
	cld
	pushl	%eax
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	lodsb
	outb	%al,%dx
	incl	%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	popl	%eax
	ret	

ENTRY(SBUS_RA_io_space_write_region_2)
	cld
	pushl	%eax
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	lodsw
	outw	%ax,%dx
	addl	$2,%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	popl	%eax
	ret	

ENTRY(SBUS_RA_io_space_write_region_4)
	cld
	pushl	%eax
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	lodsl
	outl	%eax,%dx
	addl	$4,%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	popl	%eax
	ret	

/*
 *	set_multi_N
 *	IN: eax DATA
 *	    ecx COUNT
 *	    edx PORT
 */
ENTRY(SBUS_RA_io_space_set_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outb	%al,%dx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_RA_io_space_set_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outw	%ax,%dx
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_RA_io_space_set_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	outl	%eax,%dx
	decl	%ecx
	jnz	1b
2:
	ret

/*
 *	set_region_N
 *	IN: eax DATA
 *	    ecx COUNT
 *	    edx PORT
 */
ENTRY(SBUS_RA_io_space_set_region_1)
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outb	%al,%dx
	incl	%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_io_space_set_region_2)
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outw	%ax,%dx
	addl	$2,%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_io_space_set_region_4)
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
	movl	%edx,%edi
1:
	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outl	%eax,%dx
	addl	$4,%edi
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

/*
 *	copy_region_N
 *	IN: ecx COUNT
 *	    esi SPORT
 *	    edi	DPORT
 */
ENTRY(SBUS_RA_io_space_copy_region_1)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inb	%dx,%al
	incl	%esi

	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outb	%al,%dx
	incl	%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

ENTRY(SBUS_RA_io_space_copy_region_2)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inw	%dx,%ax
	addl	$2,%esi

	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outw	%ax,%dx
	addl	$2,%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

ENTRY(SBUS_RA_io_space_copy_region_4)
	pushl	%eax
	pushl	%edx
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,esi,edx)
	inl	%dx,%eax
	addl	$4,%esi

	BUS_ACCESS_ADDR2(ebx,edi,edx)
	outl	%eax,%dx
	addl	$4,%edi

	decl	%ecx
	jnz	1b
2:
	popl	%edx
	popl	%eax
	ret

/***********************************************************
 * Bus Memory access methods
 ***********************************************************/
/*
 *	read_N
 */
ENTRY(SBUS_RA_mem_space_read_1)
	BUS_ACCESS_ADDR(ebx,edx)
	movb	(%edx),%al
	ret

ENTRY(SBUS_RA_mem_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	movw	(%edx),%ax
	ret

ENTRY(SBUS_RA_mem_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	movl	(%edx),%eax
	ret

/*
 *	write_N
 */
ENTRY(SBUS_RA_mem_space_write_1)
	BUS_ACCESS_ADDR(ebx,edx)
	movb	%al,(%edx)
	ret

ENTRY(SBUS_RA_mem_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	movw	%ax,(%edx)
	ret

ENTRY(SBUS_RA_mem_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	movl	%eax,(%edx)
	ret

/*
 *	read_multi_N
 */
ENTRY(SBUS_RA_mem_space_read_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movb	(%edx),%al
	stosb
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_RA_mem_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movw	(%edx),%ax	
	stosw
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_RA_mem_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	movl	(%edx),%eax	
	stosl
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

/*
 *	write_multi_N
 */
ENTRY(SBUS_RA_mem_space_write_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsb
	movb	%al,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_RA_mem_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsw
	movw	%ax,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

ENTRY(SBUS_RA_mem_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	cld
	pushl	%eax
	orl	%ecx,%ecx
	jz	2f
1:
	lodsl
	movl	%eax,(%edx)
	decl	%ecx
	jnz	1b
2:
	popl	%eax
	ret

/*
 *	read_region_N
 */
ENTRY(SBUS_RA_mem_space_read_region_1)
	cld
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,esi)
	movsb
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	ret

ENTRY(SBUS_RA_mem_space_read_region_2)
	cld
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,esi)
	movsw
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	ret

ENTRY(SBUS_RA_mem_space_read_region_4)
	cld
	pushl	%esi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,esi)
	movsl
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%esi
	ret

/*
 *	write_region_N
 */
ENTRY(SBUS_RA_mem_space_write_region_1)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	movsb
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_mem_space_write_region_2)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	movsw
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_mem_space_write_region_4)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	movsl
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

/*
 *	set_multi_N
 */
ENTRY(SBUS_RA_mem_space_set_multi_1)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movb	%al,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_RA_mem_space_set_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movw	%ax,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_RA_mem_space_set_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	orl	%ecx,%ecx
	jz	2f
1:
	movl	%eax,(%edx)
	decl	%ecx
	jnz	1b
2:
	ret

/*
 *	set_region_N
 */
ENTRY(SBUS_RA_mem_space_set_region_1)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	stosb
	incl	%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_mem_space_set_region_2)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	stosw
	addl	$2,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

ENTRY(SBUS_RA_mem_space_set_region_4)
	cld
	pushl	%edi
	orl	%ecx,%ecx
	jz	2f
1:
	BUS_ACCESS_ADDR2(ebx,edx,edi)
	stosl
	addl	$4,%edx
	decl	%ecx
	jnz	1b
2:
	popl	%edi
	ret

/*
 *	copy_region_N
 */
ENTRY(SBUS_RA_mem_space_copy_region_1)
	cld
	orl	%ecx,%ecx
	jz	2f
1:
	pushl	%esi
	pushl	%edi
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	movsb
	popl	%edi
	popl	%esi
	incl	%esi
	incl	%edi
	decl	%ecx
	jnz	1b
2:
	ret
	
ENTRY(SBUS_RA_mem_space_copy_region_2)
	cld
	orl	%ecx,%ecx
	jz	2f
1:
	pushl	%esi
	pushl	%edi
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	movsw
	popl	%edi
	popl	%esi
	addl	$2,%esi
	addl	$2,%edi
	decl	%ecx
	jnz	1b
2:
	ret

ENTRY(SBUS_RA_mem_space_copy_region_4)
	cld
	orl	%ecx,%ecx
	jz	2f
1:
	pushl	%esi
	pushl	%edi
	BUS_ACCESS_ADDR(eax,esi)
	BUS_ACCESS_ADDR(ebx,edi)
	movsl
	popl	%edi
	popl	%esi
	addl	$4,%esi
	addl	$4,%edi
	decl	%ecx
	jnz	1b
2:
	ret

#undef	BUS_ACCESS_ADDR
#undef	BUS_ACCESS_ADDR2


#include "opt_mecia.h"
#ifdef DEV_MECIA

/***********************************************************
 * NEPC pcmcia 16 bits bus access
 ***********************************************************/
#define	NEPC_SWITCH_BUS16	\
	pushl	%ebp		;\
	pushl	%eax		;\
	pushl	%edx		;\
	movl	$0x2a8e,%edx	;\
	inb	%dx,%al		;\
	movl	%eax,%ebp	;\
	andl	$~0x20,%eax	;\
	outb	%al,%dx		;\
	popl	%edx		;\
	popl	%eax

#define	NEPC_BUS_RESTORE	\
	pushl	%eax		;\
	movl	%ebp,%eax	;\
	xchgl	%edx,%ebp	;\
	movl	$0x2a8e,%edx	;\
	outb	%al,%dx		;\
	xchgl	%ebp,%edx	;\
	popl	%eax		;\
	popl	%ebp
	
/***********************************************************
 * NEPC pcmcia 16 bits bus acces (Direct Access)
 ***********************************************************/
#define	BUS_ACCESS_ADDR(BSHREG,ADDRREG) \
	addl BUS_SPACE_HANDLE_BASE/**/(%/**/BSHREG/**/),%/**/ADDRREG

ENTRY(NEPC_DA_io_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	inw	%dx,%ax
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_DA_io_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	outw	%ax,%dx
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	insw
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	outsw
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_read_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_read_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_write_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_write_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_set_multi_2)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_set_multi_2
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_DA_io_space_set_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_set_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_copy_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_copy_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	inl	%dx,%eax
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	outl	%eax,%dx
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	insl
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	outsl
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_read_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_read_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_write_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_write_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_set_multi_4)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_set_multi_4
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_DA_io_space_set_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_set_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_DA_io_space_copy_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_DA_io_space_copy_region_4
	NEPC_BUS_RESTORE
	ret

#undef	BUS_ACCESS_ADDR

/***********************************************************
 * NEPC pcmcia 16 bits bus acces (Relocate Access)
 ***********************************************************/
#define	BUS_ACCESS_ADDR(BSHREG,ADDRREG)	\
	movl BUS_SPACE_HANDLE_IAT/**/(%/**/BSHREG/**/, %/**/ADDRREG/**/, 4), \
	     %/**/ADDRREG

ENTRY(NEPC_RA_io_space_read_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	inw	%dx,%ax
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_RA_io_space_write_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	outw	%ax,%dx
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_read_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	insw
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_write_multi_2)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	outsw
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_read_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_read_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_write_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_write_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_set_multi_2)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_set_multi_2
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_RA_io_space_set_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_set_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_copy_region_2)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_copy_region_2
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_read_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	inl	%dx,%eax
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_write_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	outl	%eax,%dx
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_read_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	insl
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_write_multi_4)
	BUS_ACCESS_ADDR(ebx,edx)
	NEPC_SWITCH_BUS16
	cld
	rep
	outsl
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_read_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_read_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_write_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_write_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_set_multi_4)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_set_multi_4
	NEPC_BUS_RESTORE
	ret


ENTRY(NEPC_RA_io_space_set_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_set_region_4
	NEPC_BUS_RESTORE
	ret

ENTRY(NEPC_RA_io_space_copy_region_4)
	NEPC_SWITCH_BUS16
	call SBUS_RA_io_space_copy_region_4
	NEPC_BUS_RESTORE
	ret

#endif /* DEV_MECIA */
