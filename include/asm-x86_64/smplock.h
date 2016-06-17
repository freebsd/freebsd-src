/*
 * <asm/smplock.h>
 */
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <asm/current.h>

extern spinlock_cacheline_t kernel_flag_cacheline;  
#define kernel_flag kernel_flag_cacheline.lock      

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
extern __inline__ void lock_kernel(void)
{
#if 1
	if (!++current->lock_depth)
		spin_lock(&kernel_flag);
#else
	__asm__ __volatile__(
		"incl %1\n\t"
		"jne 9f"
		spin_lock_string
		"\n9:"
		:"=m" (__dummy_lock(&kernel_flag)),
		 "=m" (current->lock_depth));
#endif
}

extern __inline__ void unlock_kernel(void)
{
	if (current->lock_depth < 0)
		out_of_line_bug();
#if 1
	if (--current->lock_depth < 0)
		spin_unlock(&kernel_flag);
#else
	__asm__ __volatile__(
		"decl %1\n\t"
		"jns 9f\n\t"
		spin_unlock_string
		"\n9:"
		:"=m" (__dummy_lock(&kernel_flag)),
		 "=m" (current->lock_depth));
#endif
}
