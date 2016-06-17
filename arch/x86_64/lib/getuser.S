/*
 * __get_user functions.
 *
 * (C) Copyright 1998 Linus Torvalds
 *
 * These functions have a non-standard call interface
 * to make them more efficient, especially as they
 * return an error value in addition to the "real"
 * return value.
 */

/*
 * __get_user_X
 *
 * Inputs:	%rax contains the address
 *
 * Outputs:	%rax is error code (0 or -EFAULT)
 *		%rdx contains zero-extended value
 * 
 * %rbx is destroyed.
 *
 * These functions should not modify any other registers,
 * as they get called from within inline assembly.
 */

#include <linux/linkage.h>
#include <asm/page.h>
#include <asm/errno.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/calling.h>
	
	.text
	.p2align
.globl __get_user_1
__get_user_1:	
	GET_CURRENT(%rbx)
	cmpq tsk_addr_limit(%rbx),%rax
	jae bad_get_user
1:	movzb (%rax),%edx
	xorq %rax,%rax
	ret

	.p2align	
.globl __get_user_2
__get_user_2:
	GET_CURRENT(%rbx) 
	addq $1,%rax
	jc bad_get_user
	cmpq tsk_addr_limit(%rbx),%rax 
	jae	 bad_get_user
2:	movzwl -1(%rax),%edx
	xorq %rax,%rax
	ret

	.p2align
.globl __get_user_4
__get_user_4:
	GET_CURRENT(%rbx) 
	addq $3,%rax
	jc bad_get_user
	cmpq tsk_addr_limit(%rbx),%rax 
	jae bad_get_user
3:	movl -3(%rax),%edx
	xorq %rax,%rax
	ret

	.p2align
.globl __get_user_8
__get_user_8:
	GET_CURRENT(%rbx) 
	addq $7,%rax
	jc bad_get_user
	cmpq tsk_addr_limit(%rbx),%rax
	jae	bad_get_user
4:	movq -7(%rax),%rdx
	xorq %rax,%rax
	ret

ENTRY(bad_get_user)
bad_get_user:
	xorq %rdx,%rdx
	movq $(-EFAULT),%rax
	ret

.section __ex_table,"a"
	.quad 1b,bad_get_user
	.quad 2b,bad_get_user
	.quad 3b,bad_get_user
	.quad 4b,bad_get_user
.previous
