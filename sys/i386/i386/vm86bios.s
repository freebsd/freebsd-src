/*-
 * Copyright (c) 1998 Jonathan Lemon
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: vm86bios.s,v 1.1 1998/03/23 19:52:39 jlemon Exp $
 */

#include "opt_vm86.h"

#include <machine/asmacros.h>		/* miscellaneous asm macros */
#include <machine/trap.h>

#include "assym.s"

	.data
	ALIGN_DATA

	.globl	_in_vm86call, _vm86pcb

_in_vm86call:		.long	0
_vm86pcb:		.long	0

	.text

/*
 * vm86_bioscall(struct trapframe_vm86 *vm86)
 */
ENTRY(vm86_bioscall)
	movl	_vm86pcb,%edx		/* data area, see vm86.c for layout */
	movl	4(%esp),%eax
	movl	%eax,PCB_EIP(%edx)	/* save argument pointer */
	pushl	%ebx
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%fs
	pushl	%gs

#ifdef SMP	
	pushl	%edx
	ALIGN_LOCK			/* Get global lock */
	popl	%edx
#endif
	movl	_curproc,%ecx
	pushl	%ecx			/* save _curproc value */
	testl	%ecx,%ecx
	je	1f			/* no process to save */

#if NNPX > 0
	cmpl	%ecx,_npxproc		/* do we need to save fp? */
	jne	1f
	movl	P_ADDR(%ecx),%ecx
	addl	$PCB_SAVEFPU,%ecx
	pushl	%edx
	call	_npxsave
	popl	%edx			/* recover our pcb */
#endif

1:
	movl	PCB_EBP(%edx),%ebx	/* target frame location */
	movl	%ebx,%edi		/* destination */
	movl    PCB_EIP(%edx),%esi	/* source (set on entry) */
	movl	$21,%ecx		/* sizeof(struct vm86frame)/4 */
	cld
	rep
	movsl				/* copy frame to new stack */

	movl	TF_TRAPNO(%ebx),%ebx
	cmpl	$256,%ebx
	jb	1f			/* no page frame to map */

	andl	$~PAGE_MASK,%ebx
#if 0
	orl	$PG_V|PG_RW|PG_U,%ebx	/* XXX assembler error?? */
#endif
	orl	$0x7,%ebx
	movl	PCB_EBX(%edx),%eax	/* va of vm86 page table */
	movl	%ebx,4(%eax)		/* set vm86 PTE entry 1 */
1:
	movl	_curpcb,%eax
	pushl	%eax			/* save curpcb */
	movl	%edx,_curpcb		/* set curpcb to vm86pcb */
	movl	$0,_curproc		/* erase curproc */

	movl	_my_tr,%esi
	leal	_gdt(,%esi,8),%ebx	/* entry in GDT */
	movl	0(%ebx),%eax
	movl	%eax,PCB_FS(%edx)	/* save first word */
	movl	4(%ebx),%eax
	andl    $~0x200, %eax		/* flip 386BSY -> 386TSS */
	movl	%eax,PCB_GS(%edx)	/* save second word */

	movl	PCB_EXT(%edx),%edi	/* vm86 tssd entry */
	movl	0(%edi),%eax
	movl	%eax,0(%ebx)
	movl	4(%edi),%eax
	movl	%eax,4(%ebx)
	shll	$3,%esi			/* GSEL(entry, SEL_KPL) */
	ltr	%si

	movl	%cr3,%eax
	pushl	%eax			/* save address space */
#ifdef SMP
	movl	_my_idlePTD,%ecx
#else
	movl	_IdlePTD,%ecx
#endif
	movl	%ecx,%ebx
	addl	$KERNBASE,%ebx		/* va of Idle PTD */
	movl	0(%ebx),%eax
	pushl	%eax			/* old ptde != 0 when booting */
	pushl	%ebx			/* keep for reuse */

	movl	%esp,PCB_ESP(%edx)	/* save current stack location */

	movl	PCB_ESI(%edx),%eax	/* mapping for vm86 page table */
	movl	%eax,0(%ebx)		/* ... install as PTD entry 0 */

	movl	%ecx,%cr3		/* new page tables */
	movl	PCB_EBP(%edx),%esp	/* switch to new stack */
	
	call	_vm86_prepcall		/* finish setup */

	movl	$1,_in_vm86call		/* set flag for trap() */

	/*
	 * Return via _doreti
	 */
#ifdef SMP
	ECPL_LOCK
#ifdef CPL_AND_CML
#error Not ready for CPL_AND_CML
#endif
	pushl	_cpl			/* cpl to restore */
	ECPL_UNLOCK
#else
	pushl	_cpl			/* cpl to restore */
#endif
	subl	$4,%esp			/* dummy unit */
	MPLOCKED incb _intr_nesting_level
	MEXITCOUNT
	jmp	_doreti


/*
 * vm86_biosret(struct trapframe_vm86 *vm86)
 */
ENTRY(vm86_biosret)
	movl	_vm86pcb,%edx		/* data area */

	movl	4(%esp),%esi		/* source */
	movl	PCB_EIP(%edx),%edi	/* destination */
	movl	$21,%ecx		/* size */
	cld
	rep
	movsl				/* copy frame to original frame */

	movl	PCB_ESP(%edx),%esp	/* back to old stack */
	popl	%ebx			/* saved va of Idle PTD */
	popl	%eax
	movl	%eax,0(%ebx)		/* restore old pte */
	popl	%eax
	movl	%eax,%cr3		/* install old page table */

	movl	$0,_in_vm86call		/* reset trapflag */
	movl	PCB_EBX(%edx),%ebx	/* va of vm86 page table */
	movl	$0,4(%ebx)		/* ...clear entry 1 */

	movl	_my_tr,%esi
	leal	_gdt(,%esi,8),%ebx	/* entry in GDT */
	movl	PCB_FS(%edx),%eax
	movl	%eax,0(%ebx)		/* restore first word */
	movl	PCB_GS(%edx),%eax
	movl	%eax,4(%ebx)		/* restore second word */
	shll	$3,%esi			/* GSEL(entry, SEL_KPL) */
	ltr	%si
	
	popl	_curpcb			/* restore curpcb/curproc */
	popl	_curproc
	movl	PCB_EIP(%edx),%edx	/* original stack frame */
	movl	TF_TRAPNO(%edx),%eax	/* return (trapno) */

	popl	%gs
	popl	%fs
	popl	%edi
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret				/* back to our normal program */
