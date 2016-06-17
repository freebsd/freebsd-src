/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Default SMP lock implementation
 */
#ifndef __ASM_SMPLOCK_H
#define __ASM_SMPLOCK_H

#include <linux/interrupt.h>
#include <linux/spinlock.h>

extern spinlock_t kernel_flag;

#define kernel_locked()		spin_is_locked(&kernel_flag)

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


/*
 * Getting the big kernel lock.
 *
 * This cannot happen asynchronously,
 * so we only need to worry about other
 * CPU's.
 */
static __inline__ void lock_kernel(void)
{
	if (!++current->lock_depth)
		spin_lock(&kernel_flag);
}

static __inline__ void unlock_kernel(void)
{
	if (--current->lock_depth < 0)
		spin_unlock(&kernel_flag);
}

#endif /* __ASM_SMPLOCK_H */
