/*
 * __put_user functions.
 *
 * (C) Copyright 1998 Linus Torvalds
 *
 * These functions have a non-standard call interface
 * to make them more efficient.
 */

/*
 * __put_user_X
 *
 * Inputs:	%rax contains the address
 *		%rdx contains the value
 *
 * Outputs:	%rax is error code (0 or -EFAULT)
 *		%rbx is corrupted (will contain "current_task").
 *
 * These functions should not modify any other registers,
 * as they get called from within inline assembly.
 */

/* FIXME: putuser.S should be really merged with getuser.S, and preprocessor should be used to keep code duplication lower */

#include <linux/linkage.h>
#include <asm/page.h>
#include <asm/errno.h>
#include <asm/current.h>
#include <asm/offset.h>

.text
.p2align
.globl __put_user_1
__put_user_1:
	GET_CURRENT(%rbx)
	cmpq tsk_addr_limit(%rbx),%rax
	jae bad_put_user
1:	movb %dl,(%rax)
	xorq %rax,%rax
	ret

.p2align
.globl __put_user_2
__put_user_2:
	GET_CURRENT(%rbx) 
	addq $1,%rax
	jc		bad_put_user
	cmpq	tsk_addr_limit(%rbx),%rax
	jae 	bad_put_user
2:	movw %dx,-1(%rax)
	xorq %rax,%rax
	ret

.p2align
.globl __put_user_4
__put_user_4:
	GET_CURRENT(%rbx) 
	addq $3,%rax
	jc		bad_put_user
	cmpq	tsk_addr_limit(%rbx),%rax
	jae 	bad_put_user
3:	movl %edx,-3(%rax)
	xorq %rax,%rax
	ret

.p2align
.globl __put_user_8
__put_user_8:
	GET_CURRENT(%rbx) 
	addq $7,%rax
	jc	bad_put_user
	cmpq	tsk_addr_limit(%rbx),%rax
	jae 	bad_put_user
4:	movq %rdx,-7(%rax)
	xorq %rax,%rax
	ret

ENTRY(bad_put_user)	
bad_put_user:
	movq $(-EFAULT),%rax
	ret

.section __ex_table,"a"
	.quad 1b,bad_put_user
	.quad 2b,bad_put_user
	.quad 3b,bad_put_user
	.quad 4b,bad_put_user	
.previous
