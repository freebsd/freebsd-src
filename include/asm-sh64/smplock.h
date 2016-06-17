#ifndef __ASM_SH64_SMPLOCK_H
#define __ASM_SH64_SMPLOCK_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/smplock.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#include <linux/config.h>

#ifndef CONFIG_SMP

#define lock_kernel()				do { } while(0)
#define unlock_kernel()				do { } while(0)
#define release_kernel_lock(task, cpu, depth)	((depth) = 1)
#define reacquire_kernel_lock(task, cpu, depth)	do { } while(0)

#else

#error "We do not support SMP on ST50 yet"
/*
 * Default SMP lock implementation
 */

#include <linux/interrupt.h>
#include <asm/spinlock.h>

extern spinlock_t kernel_flag;

/*
 * Getting the big kernel lock.
 *
 * This cannot happen asynchronously,
 * so we only need to worry about other
 * CPU's.
 */
extern __inline__ void lock_kernel(void)
{
	if (!++current->lock_depth)
		spin_lock(&kernel_flag);
}

extern __inline__ void unlock_kernel(void)
{
	if (--current->lock_depth < 0)
		spin_unlock(&kernel_flag);
}

/*
 * Release global kernel lock and global interrupt lock
 */
#define release_kernel_lock(task, cpu) \
do { \
	if (task->lock_depth >= 0) \
		spin_unlock(&kernel_flag); \
	release_irqlock(cpu); \
	__sti(); \
} while (0)

/*
 * Re-acquire the kernel lock
 */
#define reacquire_kernel_lock(task) \
do { \
	if (task->lock_depth >= 0) \
		spin_lock(&kernel_flag); \
} while (0)

#endif /* CONFIG_SMP */

#endif /* __ASM_SH64_SMPLOCK_H */
