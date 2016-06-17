/* rwsem.c: R/W semaphores: contention handling functions
 *
 * Written by David Howells (dhowells@redhat.com).
 * Derived from arch/i386/kernel/semaphore.c
 */
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/module.h>

struct rwsem_waiter {
	struct list_head	list;
	struct task_struct	*task;
	unsigned int		flags;
#define RWSEM_WAITING_FOR_READ	0x00000001
#define RWSEM_WAITING_FOR_WRITE	0x00000002
};

#if RWSEM_DEBUG
#undef rwsemtrace
void rwsemtrace(struct rw_semaphore *sem, const char *str)
{
	printk("sem=%p\n",sem);
	printk("(sem)=%08lx\n",sem->count);
	if (sem->debug)
		printk("[%d] %s({%08lx})\n",current->pid,str,sem->count);
}
#endif

/*
 * handle the lock being released whilst there are processes blocked on it that can now run
 * - if we come here, then:
 *   - the 'active part' of the count (&0x0000ffff) reached zero but has been re-incremented
 *   - the 'waiting part' of the count (&0xffff0000) is negative (and will still be so)
 *   - there must be someone on the queue
 * - the spinlock must be held by the caller
 * - woken process blocks are discarded from the list after having flags zeroised
 */
static inline struct rw_semaphore *__rwsem_do_wake(struct rw_semaphore *sem)
{
	struct rwsem_waiter *waiter;
	struct list_head *next;
	signed long oldcount;
	int woken, loop;

	rwsemtrace(sem,"Entering __rwsem_do_wake");

	/* only wake someone up if we can transition the active part of the count from 0 -> 1 */
 try_again:
	oldcount = rwsem_atomic_update(RWSEM_ACTIVE_BIAS,sem) - RWSEM_ACTIVE_BIAS;
	if (oldcount & RWSEM_ACTIVE_MASK)
		goto undo;

	waiter = list_entry(sem->wait_list.next,struct rwsem_waiter,list);

	/* try to grant a single write lock if there's a writer at the front of the queue
	 * - note we leave the 'active part' of the count incremented by 1 and the waiting part
	 *   incremented by 0x00010000
	 */
	if (!(waiter->flags & RWSEM_WAITING_FOR_WRITE))
		goto readers_only;

	list_del(&waiter->list);
	waiter->flags = 0;
	wake_up_process(waiter->task);
	goto out;

	/* grant an infinite number of read locks to the readers at the front of the queue
	 * - note we increment the 'active part' of the count by the number of readers (less one
	 *   for the activity decrement we've already done) before waking any processes up
	 */
 readers_only:
	woken = 0;
	do {
		woken++;

		if (waiter->list.next==&sem->wait_list)
			break;

		waiter = list_entry(waiter->list.next,struct rwsem_waiter,list);

	} while (waiter->flags & RWSEM_WAITING_FOR_READ);

	loop = woken;
	woken *= RWSEM_ACTIVE_BIAS-RWSEM_WAITING_BIAS;
	woken -= RWSEM_ACTIVE_BIAS;
	rwsem_atomic_add(woken,sem);

	next = sem->wait_list.next;
	for (; loop>0; loop--) {
		waiter = list_entry(next,struct rwsem_waiter,list);
		next = waiter->list.next;
		waiter->flags = 0;
		wake_up_process(waiter->task);
	}

	sem->wait_list.next = next;
	next->prev = &sem->wait_list;

 out:
	rwsemtrace(sem,"Leaving __rwsem_do_wake");
	return sem;

	/* undo the change to count, but check for a transition 1->0 */
 undo:
	if (rwsem_atomic_update(-RWSEM_ACTIVE_BIAS,sem)!=0)
		goto out;
	goto try_again;
}

/*
 * wait for a lock to be granted
 */
static inline struct rw_semaphore *rwsem_down_failed_common(struct rw_semaphore *sem,
								 struct rwsem_waiter *waiter,
								 signed long adjustment)
{
	struct task_struct *tsk = current;
	signed long count;

	set_task_state(tsk,TASK_UNINTERRUPTIBLE);

	/* set up my own style of waitqueue */
	spin_lock(&sem->wait_lock);
	waiter->task = tsk;

	list_add_tail(&waiter->list,&sem->wait_list);

	/* note that we're now waiting on the lock, but no longer actively read-locking */
	count = rwsem_atomic_update(adjustment,sem);

	/* if there are no longer active locks, wake the front queued process(es) up
	 * - it might even be this process, since the waker takes a more active part
	 */
	if (!(count & RWSEM_ACTIVE_MASK))
		sem = __rwsem_do_wake(sem);

	spin_unlock(&sem->wait_lock);

	/* wait to be given the lock */
	for (;;) {
		if (!waiter->flags)
			break;
		schedule();
		set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	}

	tsk->state = TASK_RUNNING;

	return sem;
}

/*
 * wait for the read lock to be granted
 */
struct rw_semaphore *rwsem_down_read_failed(struct rw_semaphore *sem)
{
	struct rwsem_waiter waiter;

	rwsemtrace(sem,"Entering rwsem_down_read_failed");

	waiter.flags = RWSEM_WAITING_FOR_READ;
	rwsem_down_failed_common(sem,&waiter,RWSEM_WAITING_BIAS-RWSEM_ACTIVE_BIAS);

	rwsemtrace(sem,"Leaving rwsem_down_read_failed");
	return sem;
}

/*
 * wait for the write lock to be granted
 */
struct rw_semaphore *rwsem_down_write_failed(struct rw_semaphore *sem)
{
	struct rwsem_waiter waiter;

	rwsemtrace(sem,"Entering rwsem_down_write_failed");

	waiter.flags = RWSEM_WAITING_FOR_WRITE;
	rwsem_down_failed_common(sem,&waiter,-RWSEM_ACTIVE_BIAS);

	rwsemtrace(sem,"Leaving rwsem_down_write_failed");
	return sem;
}

/*
 * handle waking up a waiter on the semaphore
 * - up_read has decremented the active part of the count if we come here
 */
struct rw_semaphore *rwsem_wake(struct rw_semaphore *sem)
{
	rwsemtrace(sem,"Entering rwsem_wake");

	spin_lock(&sem->wait_lock);

	/* do nothing if list empty */
	if (!list_empty(&sem->wait_list))
		sem = __rwsem_do_wake(sem);

	spin_unlock(&sem->wait_lock);

	rwsemtrace(sem,"Leaving rwsem_wake");

	return sem;
}

EXPORT_SYMBOL_NOVERS(rwsem_down_read_failed);
EXPORT_SYMBOL_NOVERS(rwsem_down_write_failed);
EXPORT_SYMBOL_NOVERS(rwsem_wake);
#if RWSEM_DEBUG
EXPORT_SYMBOL(rwsemtrace);
#endif
