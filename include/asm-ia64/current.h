#ifndef _ASM_IA64_CURRENT_H
#define _ASM_IA64_CURRENT_H

/*
 * Modified 1998-2000
 *	David Mosberger-Tang <davidm@hpl.hp.com>, Hewlett-Packard Co
 */

/* In kernel mode, thread pointer (r13) is used to point to the
   current task structure.  */
register struct task_struct *current asm ("r13");

#endif /* _ASM_IA64_CURRENT_H */
