#ifndef _ASM_IA64_SOFTIRQ_H
#define _ASM_IA64_SOFTIRQ_H

/*
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#include <asm/hardirq.h>

#define __local_bh_enable()	do { barrier(); really_local_bh_count()--; } while (0)

#define local_bh_disable()	do { really_local_bh_count()++; barrier(); } while (0)
#define local_bh_enable()								\
do {											\
	__local_bh_enable();								\
	if (__builtin_expect(local_softirq_pending(), 0) && really_local_bh_count() == 0)	\
		do_softirq();								\
} while (0)


#define in_softirq()		(really_local_bh_count() != 0)

#endif /* _ASM_IA64_SOFTIRQ_H */
