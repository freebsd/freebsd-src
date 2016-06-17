#ifndef _ASM_IA64_DIV64_H
#define _ASM_IA64_DIV64_H

/*
 * Copyright (C) 1999 Hewlett-Packard Co
 * Copyright (C) 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * vsprintf uses this to divide a 64-bit integer N by a small integer BASE.
 * This is incredibly hard on IA-64...
 */

#define do_div(n,base)						\
({								\
	int _res;						\
	_res = ((unsigned long) (n)) % (unsigned) (base);	\
	(n) = ((unsigned long) (n)) / (unsigned) (base);	\
	_res;							\
})

#endif /* _ASM_IA64_DIV64_H */
