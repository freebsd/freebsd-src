#ifndef _SPARC64_CURRENT_H
#define _SPARC64_CURRENT_H

/* Sparc rules... */
register struct task_struct *current asm("g6");

#endif /* !(_SPARC64_CURRENT_H) */
