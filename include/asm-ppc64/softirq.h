#ifndef __ASM_SOFTIRQ_H
#define __ASM_SOFTIRQ_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/hardirq.h>

#define local_bh_disable()	do { local_bh_count(smp_processor_id())++; barrier(); } while (0)
#define __local_bh_enable()	do { barrier(); local_bh_count(smp_processor_id())--; } while (0)

#define local_bh_enable()  \
do {                                                    \
	barrier();					\
        if (!--local_bh_count(smp_processor_id())       \
            && softirq_pending(smp_processor_id())) {   \
                do_softirq();                           \
        }                                               \
} while (0)

#define in_softirq() (local_bh_count(smp_processor_id()) != 0)

#endif	/* __ASM_SOFTIRQ_H */
