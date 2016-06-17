#ifndef _CRIS_CURRENT_H
#define _CRIS_CURRENT_H

struct task_struct;

extern inline struct task_struct * get_current(void)
{
        struct task_struct *current;
        __asm__("and.d $sp,%0; ":"=r" (current) : "0" (~8191UL));
        return current;
 }
 
#define current get_current()

#endif /* !(_CRIS_CURRENT_H) */
