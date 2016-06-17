/*
 *  linux/include/asm-arm/proc-armo/processor.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   27-06-1996	RMK	Created
 *   10-10-1996	RMK	Brought up to date with SA110
 *   26-09-1996	RMK	Added 'EXTRA_THREAD_STRUCT*'
 *   28-09-1996	RMK	Moved start_thread into the processor dependencies
 *   11-01-1998	RMK	Added new uaccess_t
 *   09-09-1998	PJB	Delete redundant `wp_works_ok'
 *   30-05-1999	PJB	Save sl across context switches
 */
#ifndef __ASM_PROC_PROCESSOR_H
#define __ASM_PROC_PROCESSOR_H

#include <linux/string.h>

#define KERNEL_STACK_SIZE 4096

struct context_save_struct {
	unsigned long r4;
	unsigned long r5;
	unsigned long r6;
	unsigned long r7;
	unsigned long r8;
	unsigned long r9;
	unsigned long sl;
	unsigned long fp;
	unsigned long pc;
};

#define INIT_CSS (struct context_save_struct){ 0, 0, 0, 0, 0, 0, 0, 0, SVC26_MODE }

typedef struct {
	void (*put_byte)(void);			/* Special calling convention */
	void (*get_byte)(void);			/* Special calling convention */
	void (*put_half)(void);			/* Special calling convention */
	void (*get_half)(void);			/* Special calling convention */
	void (*put_word)(void);			/* Special calling convention */
	void (*get_word)(void);			/* Special calling convention */
	unsigned long (*copy_from_user)(void *to, const void *from, unsigned long sz);
	unsigned long (*copy_to_user)(void *to, const void *from, unsigned long sz);
	unsigned long (*clear_user)(void *addr, unsigned long sz);
	unsigned long (*strncpy_from_user)(char *to, const char *from, unsigned long sz);
	unsigned long (*strnlen_user)(const char *s, long n);
} uaccess_t;

extern uaccess_t uaccess_user, uaccess_kernel;

#define EXTRA_THREAD_STRUCT							\
	uaccess_t	*uaccess;		/* User access functions*/

#define EXTRA_THREAD_STRUCT_INIT		\
	.uaccess	= &uaccess_kernel,

#define start_thread(regs,pc,sp)					\
({									\
	unsigned long *stack = (unsigned long *)sp;			\
	set_fs(USER_DS);						\
	memzero(regs->uregs, sizeof (regs->uregs));			\
	regs->ARM_pc = pc;		/* pc */			\
	regs->ARM_sp = sp;		/* sp */			\
	regs->ARM_r2 = stack[2];	/* r2 (envp) */			\
	regs->ARM_r1 = stack[1];	/* r1 (argv) */			\
	regs->ARM_r0 = stack[0];	/* r0 (argc) */			\
})

#define KSTK_EIP(tsk)	(((unsigned long *)(4096+(unsigned long)(tsk)))[1020])
#define KSTK_ESP(tsk)	(((unsigned long *)(4096+(unsigned long)(tsk)))[1018])

/* Allocation and freeing of basic task resources. */
/*
 * NOTE! The task struct and the stack go together
 */
extern unsigned long get_page_8k(int priority);
extern void free_page_8k(unsigned long page);

#define ll_alloc_task_struct()	((struct task_struct *)get_page_8k(GFP_KERNEL))
#define ll_free_task_struct(p)  free_page_8k((unsigned long)(p))

#endif
