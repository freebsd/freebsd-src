/*-
 * $FreeBSD$
 */

#include <machine/asmacros.h>

/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.data

	.text

#define TF_GS		-0x04 /* don't laugh! */
#define TF_FS		0x00
#define TF_ES		0x04
#define TF_DS		0x08
#define TF_EDI		0x0c
#define TF_ESI		0x10
#define TF_EBP		0x14
#define TF_ISP		0x18
#define TF_EBX		0x1c
#define TF_EDX		0x20
#define TF_ECX		0x24
#define TF_EAX		0x28
#define TF_TRAPNO	0x2c
#define TF_ERR		0x30
#define TF_EIP		0x34
#define TF_CS		0x38
#define TF_EFLAGS	0x3c
#define TF_ESP		0x40
#define TF_SS		0x44

/*
 * savethread
 */
GEN_ENTRY(savethread)

	/* Switch to new thread.  First, save context as needed. */
	pushl	%edx
	movl	8(%esp), %edx	/* get context area */
	
	movl	%eax,TF_EAX(%edx)
	movl	%ebx,TF_EBX(%edx)
	movl	%ecx,TF_ECX(%edx)
	popl	%eax			/* get dx off the stack again */
	movl	%eax,TF_EDX(%edx)
	movl	(%esp),%eax		/* get the return address */
	movl	%eax,TF_EIP(%edx)
	movl	%esp,TF_ESP(%edx)
	movl	%esp,TF_ISP(%edx)	/* XXX */
	movl	%ebp,TF_EBP(%edx)
	movl	%esi,TF_ESI(%edx)
	movl	%edi,TF_EDI(%edx)
	movl	%cs,TF_CS(%edx)
	movl	%ds,TF_DS(%edx)
	movl	%es,TF_ES(%edx)
	movl	%fs,TF_FS(%edx)
	movl	%gs,TF_GS(%edx)
	ret


GEN_ENTRY(loadthread)
	mov	4(%esp), %edx	/* get context area */

/*	movl	TF_ISP(%edx), %esp */	/* select which is correct */
	movl	TF_ESP(%edx), %esp	/* get the new stack online */
	movl	TF_EBP(%edx), %ebp
	movl	TF_EIP(%edx),%eax
	push	%eax			/* return adddress */
#if 0
	movl	TF_CS(%edx), %cs
	movl	TF_DS(%edx), %ds
	movl	TF_ES(%edx), %es
	movl	TF_FS(%edx), %fs
	movl	TF_GS(%edx), %gs
#endif
	movl	TF_ESI(%edx), %esi
	movl	TF_EDI(%edx), %edi

	movl	TF_EDX(%edx), %eax
	pushl	%eax		
	movl	TF_ECX(%edx), %ecx
	movl	TF_EBX(%edx), %ebx
	movl	TF_EAX(%edx), %eax
	popl	%edx
	ret
