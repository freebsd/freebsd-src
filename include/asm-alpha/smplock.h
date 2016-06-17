/*
 * <asm/smplock.h>
 *
 * Default SMP lock implementation
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

extern spinlock_t kernel_flag;

#define kernel_locked()		spin_is_locked(&kernel_flag)

/*
 * Release global kernel lock and global interrupt lock
 */
static __inline__ void release_kernel_lock(struct task_struct *task, int cpu)
{
	if (task->lock_depth >= 0)
		spin_unlock(&kernel_flag);
	release_irqlock(cpu);
	__sti();
}

/*
 * Re-acquire the kernel lock
 */
static __inline__ void reacquire_kernel_lock(struct task_struct *task)
{
	if (task->lock_depth >= 0)
		spin_lock(&kernel_flag);
}

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
