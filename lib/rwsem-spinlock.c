/* rwsem-spinlock.c: R/W semaphores: contention handling functions for generic spinlock
 *                                   implementation
 *
 * Copyright (c) 2001   David Howells (dhowells@redhat.com).
 * - Derived partially from idea by Andrea Arcangeli <andrea@suse.de>
 * - Derived also from comments by Linus
 *
 * Trylock by Brian Watson (Brian.J.Watson@compaq.com).
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
void rwsemtrace(struct rw_semaphore *sem, const char *str)
{
	if (sem->debug)
		printk("[%d] %s({%d,%d})\n",
		       current->pid,str,sem->activity,list_empty(&sem->wait_list)?0:1);
}
#endif

/*
 * initialise the semaphore
 */
void init_rwsem(struct rw_semaphore *sem)
{
	sem->activity = 0;
	spin_lock_init(&sem->wait_lock);
	INIT_LIST_HEAD(&sem->wait_list);
#if RWSEM_DEBUG
	sem->debug = 0;
#endif
}

/*
 * handle the lock being released whilst there are processes blocked on it that can now run
 * - if we come here, then:
 *   - the 'active count' _reached_ zero
 *   - the 'waiting count' is non-zero
 * - the spinlock must be held by the caller
 * - woken process blocks are discarded from the list after having flags zeroised
 */
static inline struct rw_semaphore *__rwsem_do_wake(struct rw_semaphore *sem)
{
	struct rwsem_waiter *waiter;
	int woken;

	rwsemtrace(sem,"Entering __rwsem_do_wake");

	waiter = list_entry(sem->wait_list.next,struct rwsem_waiter,list);

	/* try to grant a single write lock if there's a writer at the front of the queue
	 * - we leave the 'waiting count' incremented to signify potential contention
	 */
	if (waiter->flags & RWSEM_WAITING_FOR_WRITE) {
		sem->activity = -1;
		list_del(&waiter->list);
		waiter->flags = 0;
		wake_up_process(waiter->task);
		goto out;
	}

	/* grant an infinite number of read locks to the readers at the front of the queue */
	woken = 0;
	do {
		list_del(&waiter->list);
		waiter->flags = 0;
		wake_up_process(waiter->task);
		woken++;
		if (list_empty(&sem->wait_list))
			break;
		waiter = list_entry(sem->wait_list.next,struct rwsem_waiter,list);
	} while (waiter->flags&RWSEM_WAITING_FOR_READ);

	sem->activity += woken;

 out:
	rwsemtrace(sem,"Leaving __rwsem_do_wake");
	return sem;
}

/*
 * wake a single writer
 */
static inline struct rw_semaphore *__rwsem_wake_one_writer(struct rw_semaphore *sem)
{
	struct rwsem_waiter *waiter;

	sem->activity = -1;

	waiter = list_entry(sem->wait_list.next,struct rwsem_waiter,list);
	list_del(&waiter->list);

	waiter->flags = 0;
	wake_up_process(waiter->task);
	return sem;
}

/*
 * get a read lock on the semaphore
 */
void __down_read(struct rw_semaphore *sem)
{
	struct rwsem_waiter waiter;
	struct task_struct *tsk;

	rwsemtrace(sem,"Entering __down_read");

	spin_lock(&sem->wait_lock);

	if (sem->activity>=0 && list_empty(&sem->wait_list)) {
		/* granted */
		sem->activity++;
		spin_unlock(&sem->wait_lock);
		goto out;
	}

	tsk = current;
	set_task_state(tsk,TASK_UNINTERRUPTIBLE);

	/* set up my own style of waitqueue */
	waiter.task = tsk;
	waiter.flags = RWSEM_WAITING_FOR_READ;

	list_add_tail(&waiter.list,&sem->wait_list);

	/* we don't need to touch the semaphore struct anymore */
	spin_unlock(&sem->wait_lock);

	/* wait to be given the lock */
	for (;;) {
		if (!waiter.flags)
			break;
		schedule();
		set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	}

	tsk->state = TASK_RUNNING;

 out:
	rwsemtrace(sem,"Leaving __down_read");
}

/*
 * trylock for reading -- returns 1 if successful, 0 if contention
 */
int __down_read_trylock(struct rw_semaphore *sem)
{
	int ret = 0;
	rwsemtrace(sem,"Entering __down_read_trylock");

	spin_lock(&sem->wait_lock);

	if (sem->activity>=0 && list_empty(&sem->wait_list)) {
		/* granted */
		sem->activity++;
		ret = 1;
	}

	spin_unlock(&sem->wait_lock);

	rwsemtrace(sem,"Leaving __down_read_trylock");
	return ret;
}

/*
 * get a write lock on the semaphore
 * - note that we increment the waiting count anyway to indicate an exclusive lock
 */
void __down_write(struct rw_semaphore *sem)
{
	struct rwsem_waiter waiter;
	struct task_struct *tsk;

	rwsemtrace(sem,"Entering __down_write");

	spin_lock(&sem->wait_lock);

	if (sem->activity==0 && list_empty(&sem->wait_list)) {
		/* granted */
		sem->activity = -1;
		spin_unlock(&sem->wait_lock);
		goto out;
	}

	tsk = current;
	set_task_state(tsk,TASK_UNINTERRUPTIBLE);

	/* set up my own style of waitqueue */
	waiter.task = tsk;
	waiter.flags = RWSEM_WAITING_FOR_WRITE;

	list_add_tail(&waiter.list,&sem->wait_list);

	/* we don't need to touch the semaphore struct anymore */
	spin_unlock(&sem->wait_lock);

	/* wait to be given the lock */
	for (;;) {
		if (!waiter.flags)
			break;
		schedule();
		set_task_state(tsk, TASK_UNINTERRUPTIBLE);
	}

	tsk->state = TASK_RUNNING;

 out:
	rwsemtrace(sem,"Leaving __down_write");
}

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
int __down_write_trylock(struct rw_semaphore *sem)
{
	int ret = 0;
	rwsemtrace(sem,"Entering __down_write_trylock");

	spin_lock(&sem->wait_lock);

	if (sem->activity==0 && list_empty(&sem->wait_list)) {
		/* granted */
		sem->activity = -1;
		ret = 1;
	}

	spin_unlock(&sem->wait_lock);

	rwsemtrace(sem,"Leaving __down_write_trylock");
	return ret;
}

/*
 * release a read lock on the semaphore
 */
void __up_read(struct rw_semaphore *sem)
{
	rwsemtrace(sem,"Entering __up_read");

	spin_lock(&sem->wait_lock);

	if (--sem->activity==0 && !list_empty(&sem->wait_list))
		sem = __rwsem_wake_one_writer(sem);

	spin_unlock(&sem->wait_lock);

	rwsemtrace(sem,"Leaving __up_read");
}

/*
 * release a write lock on the semaphore
 */
void __up_write(struct rw_semaphore *sem)
{
	rwsemtrace(sem,"Entering __up_write");

	spin_lock(&sem->wait_lock);

	sem->activity = 0;
	if (!list_empty(&sem->wait_list))
		sem = __rwsem_do_wake(sem);

	spin_unlock(&sem->wait_lock);

	rwsemtrace(sem,"Leaving __up_write");
}

EXPORT_SYMBOL(init_rwsem);
EXPORT_SYMBOL(__down_read);
EXPORT_SYMBOL(__down_write);
EXPORT_SYMBOL(__up_read);
EXPORT_SYMBOL(__up_write);
#if RWSEM_DEBUG
EXPORT_SYMBOL(rwsemtrace);
#endif
