#ifndef _PARISC_CURRENT_H
#define _PARISC_CURRENT_H

#include <asm/processor.h>

struct task_struct;

static inline struct task_struct * get_current(void)
{
	register unsigned long cr;

	__asm__ __volatile__("mfctl %%cr30,%0" : "=r" (cr) );
	return (struct task_struct *)cr;
}
 
#define current get_current()

#endif /* !(_PARISC_CURRENT_H) */
