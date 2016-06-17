#ifndef _ASMARM_CURRENT_H
#define _ASMARM_CURRENT_H

static inline struct task_struct *get_current(void) __attribute__ (( __const__ ));

static inline struct task_struct *get_current(void)
{
	register unsigned long sp asm ("sp");
	return (struct task_struct *)(sp & ~0x1fff);
}

#define current (get_current())

#endif /* _ASMARM_CURRENT_H */
