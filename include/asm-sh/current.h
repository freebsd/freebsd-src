#ifndef __ASM_SH_CURRENT_H
#define __ASM_SH_CURRENT_H

/*
 * Copyright (C) 1999 Niibe Yutaka
 *
 */

struct task_struct;

static __inline__ struct task_struct * get_current(void)
{
	struct task_struct *current;

	__asm__("stc	r7_bank, %0"
		:"=r" (current));

	return current;
}

#define current get_current()

#endif /* __ASM_SH_CURRENT_H */
