#define MSNFS	/* HACK HACK */
/*
 *  linux/fs/locks.c
 *
 *  Provide support for fcntl()'s F_GETLK, F_SETLK, and F_SETLKW calls.
 *  Doug Evans (dje@spiff.uucp), August 07, 1992
 *
 *  Deadlock detection added.
 *  FIXME: one thing isn't handled yet:
 *	- mandatory locks (requires lots of changes elsewhere)
 *  Kelly Carmichael (kelly@[142.24.8.65]), September 17, 1994.
 *
 *  Miscellaneous edits, and a total rewrite of posix_lock_file() code.
 *  Kai Petzke (wpp@marie.physik.tu-berlin.de), 1994
 *  
 *  Converted file_lock_table to a linked list from an array, which eliminates
 *  the limits on how many active file locks are open.
 *  Chad Page (pageone@netcom.com), November 27, 1994
 * 
 *  Removed dependency on file descriptors. dup()'ed file descriptors now
 *  get the same locks as the original file descriptors, and a close() on
 *  any file descriptor removes ALL the locks on the file for the current
 *  process. Since locks still depend on the process id, locks are inherited
 *  after an exec() but not after a fork(). This agrees with POSIX, and both
 *  BSD and SVR4 practice.
 *  Andy Walker (andy@lysaker.kvaerner.no), February 14, 1995
 *
 *  Scrapped free list which is redundant now that we allocate locks
 *  dynamically with kmalloc()/kfree().
 *  Andy Walker (andy@lysaker.kvaerner.no), February 21, 1995
 *
 *  Implemented two lock personalities - FL_FLOCK and FL_POSIX.
 *
 *  FL_POSIX locks are created with calls to fcntl() and lockf() through the
 *  fcntl() system call. They have the semantics described above.
 *
 *  FL_FLOCK locks are created with calls to flock(), through the flock()
 *  system call, which is new. Old C libraries implement flock() via fcntl()
 *  and will continue to use the old, broken implementation.
 *
 *  FL_FLOCK locks follow the 4.4 BSD flock() semantics. They are associated
 *  with a file pointer (filp). As a result they can be shared by a parent
 *  process and its children after a fork(). They are removed when the last
 *  file descriptor referring to the file pointer is closed (unless explicitly
 *  unlocked). 
 *
 *  FL_FLOCK locks never deadlock, an existing lock is always removed before
 *  upgrading from shared to exclusive (or vice versa). When this happens
 *  any processes blocked by the current lock are woken up and allowed to
 *  run before the new lock is applied.
 *  Andy Walker (andy@lysaker.kvaerner.no), June 09, 1995
 *
 *  Removed some race conditions in flock_lock_file(), marked other possible
 *  races. Just grep for FIXME to see them. 
 *  Dmitry Gorodchanin (pgmdsg@ibi.com), February 09, 1996.
 *
 *  Addressed Dmitry's concerns. Deadlock checking no longer recursive.
 *  Lock allocation changed to GFP_ATOMIC as we can't afford to sleep
 *  once we've checked for blocking and deadlocking.
 *  Andy Walker (andy@lysaker.kvaerner.no), April 03, 1996.
 *
 *  Initial implementation of mandatory locks. SunOS turned out to be
 *  a rotten model, so I implemented the "obvious" semantics.
 *  See 'linux/Documentation/mandatory.txt' for details.
 *  Andy Walker (andy@lysaker.kvaerner.no), April 06, 1996.
 *
 *  Don't allow mandatory locks on mmap()'ed files. Added simple functions to
 *  check if a file has mandatory locks, used by mmap(), open() and creat() to
 *  see if system call should be rejected. Ref. HP-UX/SunOS/Solaris Reference
 *  Manual, Section 2.
 *  Andy Walker (andy@lysaker.kvaerner.no), April 09, 1996.
 *
 *  Tidied up block list handling. Added '/proc/locks' interface.
 *  Andy Walker (andy@lysaker.kvaerner.no), April 24, 1996.
 *
 *  Fixed deadlock condition for pathological code that mixes calls to
 *  flock() and fcntl().
 *  Andy Walker (andy@lysaker.kvaerner.no), April 29, 1996.
 *
 *  Allow only one type of locking scheme (FL_POSIX or FL_FLOCK) to be in use
 *  for a given file at a time. Changed the CONFIG_LOCK_MANDATORY scheme to
 *  guarantee sensible behaviour in the case where file system modules might
 *  be compiled with different options than the kernel itself.
 *  Andy Walker (andy@lysaker.kvaerner.no), May 15, 1996.
 *
 *  Added a couple of missing wake_up() calls. Thanks to Thomas Meckel
 *  (Thomas.Meckel@mni.fh-giessen.de) for spotting this.
 *  Andy Walker (andy@lysaker.kvaerner.no), May 15, 1996.
 *
 *  Changed FL_POSIX locks to use the block list in the same way as FL_FLOCK
 *  locks. Changed process synchronisation to avoid dereferencing locks that
 *  have already been freed.
 *  Andy Walker (andy@lysaker.kvaerner.no), Sep 21, 1996.
 *
 *  Made the block list a circular list to minimise searching in the list.
 *  Andy Walker (andy@lysaker.kvaerner.no), Sep 25, 1996.
 *
 *  Made mandatory locking a mount option. Default is not to allow mandatory
 *  locking.
 *  Andy Walker (andy@lysaker.kvaerner.no), Oct 04, 1996.
 *
 *  Some adaptations for NFS support.
 *  Olaf Kirch (okir@monad.swb.de), Dec 1996,
 *
 *  Fixed /proc/locks interface so that we can't overrun the buffer we are handed.
 *  Andy Walker (andy@lysaker.kvaerner.no), May 12, 1997.
 *
 *  Use slab allocator instead of kmalloc/kfree.
 *  Use generic list implementation from <linux/list.h>.
 *  Sped up posix_locks_deadlock by only considering blocked locks.
 *  Matthew Wilcox <willy@thepuffingroup.com>, March, 2000.
 *
 *  Leases and LOCK_MAND
 *  Matthew Wilcox <willy@linuxcare.com>, June, 2000.
 *  Stephen Rothwell <sfr@canb.auug.org.au>, June, 2000.
 */

#include <linux/slab.h>
#include <linux/file.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>

int leases_enable = 1;
int lease_break_time = 45;

LIST_HEAD(file_lock_list);
static LIST_HEAD(blocked_list);

static kmem_cache_t *filelock_cache;

/* Allocate an empty lock structure. */
static struct file_lock *locks_alloc_lock(void)
{
	return kmem_cache_alloc(filelock_cache, SLAB_KERNEL);
}

/* Free a lock which is not in use. */
static inline void locks_free_lock(struct file_lock *fl)
{
	if (fl == NULL) {
		BUG();
		return;
	}
	if (waitqueue_active(&fl->fl_wait))
		panic("Attempting to free lock with active wait queue");

	if (!list_empty(&fl->fl_block))
		panic("Attempting to free lock with active block list");

	if (!list_empty(&fl->fl_link))
		panic("Attempting to free lock on active lock list");

	kmem_cache_free(filelock_cache, fl);
}

void locks_init_lock(struct file_lock *fl)
{
	INIT_LIST_HEAD(&fl->fl_link);
	INIT_LIST_HEAD(&fl->fl_block);
	init_waitqueue_head(&fl->fl_wait);
	fl->fl_next = NULL;
	fl->fl_fasync = NULL;
	fl->fl_owner = 0;
	fl->fl_pid = 0;
	fl->fl_file = NULL;
	fl->fl_flags = 0;
	fl->fl_type = 0;
	fl->fl_start = fl->fl_end = 0;
	fl->fl_notify = NULL;
	fl->fl_insert = NULL;
	fl->fl_remove = NULL;
}

/*
 * Initialises the fields of the file lock which are invariant for
 * free file_locks.
 */
static void init_once(void *foo, kmem_cache_t *cache, unsigned long flags)
{
	struct file_lock *lock = (struct file_lock *) foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) !=
					SLAB_CTOR_CONSTRUCTOR)
		return;

	locks_init_lock(lock);
}

/*
 * Initialize a new lock from an existing file_lock structure.
 */
void locks_copy_lock(struct file_lock *new, struct file_lock *fl)
{
	new->fl_owner = fl->fl_owner;
	new->fl_pid = fl->fl_pid;
	new->fl_file = fl->fl_file;
	new->fl_flags = fl->fl_flags;
	new->fl_type = fl->fl_type;
	new->fl_start = fl->fl_start;
	new->fl_end = fl->fl_end;
	new->fl_notify = fl->fl_notify;
	new->fl_insert = fl->fl_insert;
	new->fl_remove = fl->fl_remove;
	new->fl_u = fl->fl_u;
}

/* Fill in a file_lock structure with an appropriate FLOCK lock. */
static struct file_lock *flock_make_lock(struct file *filp, unsigned int type)
{
	struct file_lock *fl = locks_alloc_lock();
	if (fl == NULL)
		return NULL;

	fl->fl_owner = NULL;
	fl->fl_file = filp;
	fl->fl_pid = current->pid;
	fl->fl_flags = FL_FLOCK;
	fl->fl_type = type;
	fl->fl_start = 0;
	fl->fl_end = OFFSET_MAX;
	fl->fl_notify = NULL;
	fl->fl_insert = NULL;
	fl->fl_remove = NULL;
	
	return fl;
}

static int assign_type(struct file_lock *fl, int type)
{
	switch (type) {
	case F_RDLCK:
	case F_WRLCK:
	case F_UNLCK:
		fl->fl_type = type;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/* Verify a "struct flock" and copy it to a "struct file_lock" as a POSIX
 * style lock.
 */
static int flock_to_posix_lock(struct file *filp, struct file_lock *fl,
			       struct flock *l)
{
	off_t start, end;

	switch (l->l_whence) {
	case 0: /*SEEK_SET*/
		start = 0;
		break;
	case 1: /*SEEK_CUR*/
		start = filp->f_pos;
		break;
	case 2: /*SEEK_END*/
		start = filp->f_dentry->d_inode->i_size;
		break;
	default:
		return -EINVAL;
	}

	/* POSIX-1996 leaves the case l->l_len < 0 undefined;
	   POSIX-2001 defines it. */
	start += l->l_start;
	if (l->l_len < 0) {
		end = start - 1;
		start += l->l_len;
	} else {
		end = start + l->l_len - 1;
	}

	if (start < 0)
		return -EINVAL;
	if (l->l_len > 0 && end < 0)
		return -EOVERFLOW;
	fl->fl_start = start;	/* we record the absolute position */
	fl->fl_end = end;
	if (l->l_len == 0)
		fl->fl_end = OFFSET_MAX;
	
	fl->fl_owner = current->files;
	fl->fl_pid = current->pid;
	fl->fl_file = filp;
	fl->fl_flags = FL_POSIX;
	fl->fl_notify = NULL;
	fl->fl_insert = NULL;
	fl->fl_remove = NULL;

	return assign_type(fl, l->l_type);
}

#if BITS_PER_LONG == 32
static int flock64_to_posix_lock(struct file *filp, struct file_lock *fl,
				 struct flock64 *l)
{
	loff_t start;

	switch (l->l_whence) {
	case 0: /*SEEK_SET*/
		start = 0;
		break;
	case 1: /*SEEK_CUR*/
		start = filp->f_pos;
		break;
	case 2: /*SEEK_END*/
		start = filp->f_dentry->d_inode->i_size;
		break;
	default:
		return -EINVAL;
	}

	if (((start += l->l_start) < 0) || (l->l_len < 0))
		return -EINVAL;
	fl->fl_end = start + l->l_len - 1;
	if (l->l_len > 0 && fl->fl_end < 0)
		return -EOVERFLOW;
	fl->fl_start = start;	/* we record the absolute position */
	if (l->l_len == 0)
		fl->fl_end = OFFSET_MAX;
	
	fl->fl_owner = current->files;
	fl->fl_pid = current->pid;
	fl->fl_file = filp;
	fl->fl_flags = FL_POSIX;
	fl->fl_notify = NULL;
	fl->fl_insert = NULL;
	fl->fl_remove = NULL;

	switch (l->l_type) {
	case F_RDLCK:
	case F_WRLCK:
	case F_UNLCK:
		fl->fl_type = l->l_type;
		break;
	default:
		return -EINVAL;
	}

	return (0);
}
#endif

/* Allocate a file_lock initialised to this type of lease */
static int lease_alloc(struct file *filp, int type, struct file_lock **flp)
{
	struct file_lock *fl = locks_alloc_lock();
	if (fl == NULL)
		return -ENOMEM;

	fl->fl_owner = current->files;
	fl->fl_pid = current->pid;

	fl->fl_file = filp;
	fl->fl_flags = FL_LEASE;
	if (assign_type(fl, type) != 0) {
		locks_free_lock(fl);
		return -EINVAL;
	}
	fl->fl_start = 0;
	fl->fl_end = OFFSET_MAX;
	fl->fl_notify = NULL;
	fl->fl_insert = NULL;
	fl->fl_remove = NULL;

	*flp = fl;
	return 0;
}

/* Check if two locks overlap each other.
 */
static inline int locks_overlap(struct file_lock *fl1, struct file_lock *fl2)
{
	return ((fl1->fl_end >= fl2->fl_start) &&
		(fl2->fl_end >= fl1->fl_start));
}

/*
 * Check whether two locks have the same owner
 * N.B. Do we need the test on PID as well as owner?
 * (Clone tasks should be considered as one "owner".)
 */
static inline int
locks_same_owner(struct file_lock *fl1, struct file_lock *fl2)
{
	return (fl1->fl_owner == fl2->fl_owner) &&
	       (fl1->fl_pid   == fl2->fl_pid);
}

/* Remove waiter from blocker's block list.
 * When blocker ends up pointing to itself then the list is empty.
 */
static void locks_delete_block(struct file_lock *waiter)
{
	list_del(&waiter->fl_block);
	INIT_LIST_HEAD(&waiter->fl_block);
	list_del(&waiter->fl_link);
	INIT_LIST_HEAD(&waiter->fl_link);
	waiter->fl_next = NULL;
}

/* Insert waiter into blocker's block list.
 * We use a circular list so that processes can be easily woken up in
 * the order they blocked. The documentation doesn't require this but
 * it seems like the reasonable thing to do.
 */
static void locks_insert_block(struct file_lock *blocker, 
			       struct file_lock *waiter)
{
	if (!list_empty(&waiter->fl_block)) {
		printk(KERN_ERR "locks_insert_block: removing duplicated lock "
			"(pid=%d %Ld-%Ld type=%d)\n", waiter->fl_pid,
			waiter->fl_start, waiter->fl_end, waiter->fl_type);
		locks_delete_block(waiter);
	}
	list_add_tail(&waiter->fl_block, &blocker->fl_block);
	waiter->fl_next = blocker;
	list_add(&waiter->fl_link, &blocked_list);
}

static inline
void locks_notify_blocked(struct file_lock *waiter)
{
	if (waiter->fl_notify)
		waiter->fl_notify(waiter);
	else
		wake_up(&waiter->fl_wait);
}

/* Wake up processes blocked waiting for blocker.
 * If told to wait then schedule the processes until the block list
 * is empty, otherwise empty the block list ourselves.
 */
static void locks_wake_up_blocks(struct file_lock *blocker, unsigned int wait)
{
	while (!list_empty(&blocker->fl_block)) {
		struct file_lock *waiter = list_entry(blocker->fl_block.next, struct file_lock, fl_block);

		if (wait) {
			locks_notify_blocked(waiter);
			/* Let the blocked process remove waiter from the
			 * block list when it gets scheduled.
			 */
			yield();
		} else {
			/* Remove waiter from the block list, because by the
			 * time it wakes up blocker won't exist any more.
			 */
			locks_delete_block(waiter);
			locks_notify_blocked(waiter);
		}
	}
}

/* Insert file lock fl into an inode's lock list at the position indicated
 * by pos. At the same time add the lock to the global file lock list.
 */
static void locks_insert_lock(struct file_lock **pos, struct file_lock *fl)
{
	list_add(&fl->fl_link, &file_lock_list);

	/* insert into file's list */
	fl->fl_next = *pos;
	*pos = fl;

	if (fl->fl_insert)
		fl->fl_insert(fl);
}

/*
 * Remove lock from the lock lists
 */
static inline void _unhash_lock(struct file_lock **thisfl_p)
{
	struct file_lock *fl = *thisfl_p;

	*thisfl_p = fl->fl_next;
	fl->fl_next = NULL;

	list_del_init(&fl->fl_link);
}

/*
 * Wake up processes that are blocked waiting for this lock,
 * notify the FS that the lock has been cleared and
 * finally free the lock.
 */
static inline void _delete_lock(struct file_lock *fl, unsigned int wait)
{
	fasync_helper(0, fl->fl_file, 0, &fl->fl_fasync);
	if (fl->fl_fasync != NULL){
		printk(KERN_ERR "locks_delete_lock: fasync == %p\n", fl->fl_fasync);
		fl->fl_fasync = NULL;
	}

	if (fl->fl_remove)
		fl->fl_remove(fl);

	locks_wake_up_blocks(fl, wait);
	locks_free_lock(fl);
}

/*
 * Delete a lock and then free it.
 */
static void locks_delete_lock(struct file_lock **thisfl_p, unsigned int wait)
{
	struct file_lock *fl = *thisfl_p;

	_unhash_lock(thisfl_p);
	_delete_lock(fl, wait);
}

/*
 * Call back client filesystem in order to get it to unregister a lock,
 * then delete lock. Essentially useful only in locks_remove_*().
 * Note: this must be called with the semaphore already held!
 */
static inline void locks_unlock_delete(struct file_lock **thisfl_p)
{
	struct file_lock *fl = *thisfl_p;
	int (*lock)(struct file *, int, struct file_lock *);

	_unhash_lock(thisfl_p);
	if (fl->fl_file->f_op &&
	    (lock = fl->fl_file->f_op->lock) != NULL) {
		fl->fl_type = F_UNLCK;
		lock(fl->fl_file, F_SETLK, fl);
	}
	_delete_lock(fl, 0);
}

/* Determine if lock sys_fl blocks lock caller_fl. Common functionality
 * checks for shared/exclusive status of overlapping locks.
 */
static int locks_conflict(struct file_lock *caller_fl, struct file_lock *sys_fl)
{
	switch (caller_fl->fl_type) {
	case F_RDLCK:
		return (sys_fl->fl_type == F_WRLCK);

	case F_WRLCK:
		return (1);

	default:
		printk(KERN_ERR "locks_conflict(): impossible lock type - %d\n",
		       caller_fl->fl_type);
		break;
	}
	return (0);	/* This should never happen */
}

/* Determine if lock sys_fl blocks lock caller_fl. POSIX specific
 * checking before calling the locks_conflict().
 */
static int posix_locks_conflict(struct file_lock *caller_fl, struct file_lock *sys_fl)
{
	/* POSIX locks owned by the same process do not conflict with
	 * each other.
	 */
	if (!(sys_fl->fl_flags & FL_POSIX) ||
	    locks_same_owner(caller_fl, sys_fl))
		return (0);

	/* Check whether they overlap */
	if (!locks_overlap(caller_fl, sys_fl))
		return 0;

	return (locks_conflict(caller_fl, sys_fl));
}

/* Determine if lock sys_fl blocks lock caller_fl. FLOCK specific
 * checking before calling the locks_conflict().
 */
static int flock_locks_conflict(struct file_lock *caller_fl, struct file_lock *sys_fl)
{
	/* FLOCK locks referring to the same filp do not conflict with
	 * each other.
	 */
	if (!(sys_fl->fl_flags & FL_FLOCK) ||
	    (caller_fl->fl_file == sys_fl->fl_file))
		return (0);
#ifdef MSNFS
	if ((caller_fl->fl_type & LOCK_MAND) || (sys_fl->fl_type & LOCK_MAND))
		return 0;
#endif

	return (locks_conflict(caller_fl, sys_fl));
}

static int interruptible_sleep_on_locked(wait_queue_head_t *fl_wait, int timeout)
{
	int result = 0;
	DECLARE_WAITQUEUE(wait, current);

	current->state = TASK_INTERRUPTIBLE;
	add_wait_queue(fl_wait, &wait);
	if (timeout == 0)
		schedule();
	else
		result = schedule_timeout(timeout);
	if (signal_pending(current))
		result = -ERESTARTSYS;
	remove_wait_queue(fl_wait, &wait);
	current->state = TASK_RUNNING;
	return result;
}

static int locks_block_on(struct file_lock *blocker, struct file_lock *waiter)
{
	int result;
	locks_insert_block(blocker, waiter);
	result = interruptible_sleep_on_locked(&waiter->fl_wait, 0);
	locks_delete_block(waiter);
	return result;
}

static int locks_block_on_timeout(struct file_lock *blocker, struct file_lock *waiter, int time)
{
	int result;
	locks_insert_block(blocker, waiter);
	result = interruptible_sleep_on_locked(&waiter->fl_wait, time);
	locks_delete_block(waiter);
	return result;
}

struct file_lock *
posix_test_lock(struct file *filp, struct file_lock *fl)
{
	struct file_lock *cfl;

	lock_kernel();
	for (cfl = filp->f_dentry->d_inode->i_flock; cfl; cfl = cfl->fl_next) {
		if (!(cfl->fl_flags & FL_POSIX))
			continue;
		if (posix_locks_conflict(cfl, fl))
			break;
	}
	unlock_kernel();

	return (cfl);
}

/* This function tests for deadlock condition before putting a process to
 * sleep. The detection scheme is no longer recursive. Recursive was neat,
 * but dangerous - we risked stack corruption if the lock data was bad, or
 * if the recursion was too deep for any other reason.
 *
 * We rely on the fact that a task can only be on one lock's wait queue
 * at a time. When we find blocked_task on a wait queue we can re-search
 * with blocked_task equal to that queue's owner, until either blocked_task
 * isn't found, or blocked_task is found on a queue owned by my_task.
 *
 * Note: the above assumption may not be true when handling lock requests
 * from a broken NFS client. But broken NFS clients have a lot more to
 * worry about than proper deadlock detection anyway... --okir
 */
int posix_locks_deadlock(struct file_lock *caller_fl,
				struct file_lock *block_fl)
{
	struct list_head *tmp;
	fl_owner_t caller_owner, blocked_owner;
	unsigned int	 caller_pid, blocked_pid;

	caller_owner = caller_fl->fl_owner;
	caller_pid = caller_fl->fl_pid;
	blocked_owner = block_fl->fl_owner;
	blocked_pid = block_fl->fl_pid;

next_task:
	if (caller_owner == blocked_owner && caller_pid == blocked_pid)
		return 1;
	list_for_each(tmp, &blocked_list) {
		struct file_lock *fl = list_entry(tmp, struct file_lock, fl_link);
		if ((fl->fl_owner == blocked_owner)
		    && (fl->fl_pid == blocked_pid)) {
			fl = fl->fl_next;
			blocked_owner = fl->fl_owner;
			blocked_pid = fl->fl_pid;
			goto next_task;
		}
	}
	return 0;
}

int locks_mandatory_locked(struct inode *inode)
{
	fl_owner_t owner = current->files;
	struct file_lock *fl;

	/*
	 * Search the lock list for this inode for any POSIX locks.
	 */
	lock_kernel();
	for (fl = inode->i_flock; fl != NULL; fl = fl->fl_next) {
		if (!(fl->fl_flags & FL_POSIX))
			continue;
		if (fl->fl_owner != owner)
			break;
	}
	unlock_kernel();
	return fl ? -EAGAIN : 0;
}

int locks_mandatory_area(int read_write, struct inode *inode,
			 struct file *filp, loff_t offset,
			 size_t count)
{
	struct file_lock *fl;
	struct file_lock *new_fl = locks_alloc_lock();
	int error;

	if (new_fl == NULL)
		return -ENOMEM;

	new_fl->fl_owner = current->files;
	new_fl->fl_pid = current->pid;
	new_fl->fl_file = filp;
	new_fl->fl_flags = FL_POSIX | FL_ACCESS;
	new_fl->fl_type = (read_write == FLOCK_VERIFY_WRITE) ? F_WRLCK : F_RDLCK;
	new_fl->fl_start = offset;
	new_fl->fl_end = offset + count - 1;

	error = 0;
	lock_kernel();

repeat:
	/* Search the lock list for this inode for locks that conflict with
	 * the proposed read/write.
	 */
	for (fl = inode->i_flock; fl != NULL; fl = fl->fl_next) {
		if (!(fl->fl_flags & FL_POSIX))
			continue;
		if (fl->fl_start > new_fl->fl_end)
			break;
		if (posix_locks_conflict(new_fl, fl)) {
			error = -EAGAIN;
			if (filp && (filp->f_flags & O_NONBLOCK))
				break;
			error = -EDEADLK;
			if (posix_locks_deadlock(new_fl, fl))
				break;
	
			error = locks_block_on(fl, new_fl);
			if (error != 0)
				break;
	
			/*
			 * If we've been sleeping someone might have
			 * changed the permissions behind our back.
			 */
			if ((inode->i_mode & (S_ISGID | S_IXGRP)) != S_ISGID)
				break;
			goto repeat;
		}
	}
	locks_free_lock(new_fl);
	unlock_kernel();
	return error;
}

/* Try to create a FLOCK lock on filp. We always insert new FLOCK locks
 * at the head of the list, but that's secret knowledge known only to
 * flock_lock_file and posix_lock_file.
 */
static int flock_lock_file(struct file *filp, unsigned int lock_type,
			   unsigned int wait)
{
	struct file_lock *fl;
	struct file_lock *new_fl = NULL;
	struct file_lock **before;
	struct inode * inode = filp->f_dentry->d_inode;
	int error, change;
	int unlock = (lock_type == F_UNLCK);

	/*
	 * If we need a new lock, get it in advance to avoid races.
	 */
	if (!unlock) {
		error = -ENOLCK;
		new_fl = flock_make_lock(filp, lock_type);
		if (!new_fl)
			return error;
	}

	error = 0;
search:
	change = 0;
	before = &inode->i_flock;
	while (((fl = *before) != NULL) && (fl->fl_flags & FL_FLOCK)) {
		if (filp == fl->fl_file) {
			if (lock_type == fl->fl_type)
				goto out;
			change = 1;
			break;
		}
		before = &fl->fl_next;
	}
	/* change means that we are changing the type of an existing lock,
	 * or else unlocking it.
	 */
	if (change) {
		/* N.B. What if the wait argument is false? */
		locks_delete_lock(before, !unlock);
		/*
		 * If we waited, another lock may have been added ...
		 */
		if (!unlock)
			goto search;
	}
	if (unlock)
		goto out;

repeat:
	for (fl = inode->i_flock; (fl != NULL) && (fl->fl_flags & FL_FLOCK);
	     fl = fl->fl_next) {
		if (!flock_locks_conflict(new_fl, fl))
			continue;
		error = -EAGAIN;
		if (!wait)
			goto out;
		error = locks_block_on(fl, new_fl);
		if (error != 0)
			goto out;
		goto repeat;
	}
	locks_insert_lock(&inode->i_flock, new_fl);
	new_fl = NULL;
	error = 0;

out:
	if (new_fl)
		locks_free_lock(new_fl);
	return error;
}

/**
 *	posix_lock_file:
 *	@filp: The file to apply the lock to
 *	@caller: The lock to be applied
 *	@wait: 1 to retry automatically, 0 to return -EAGAIN
 *
 * Add a POSIX style lock to a file.
 * We merge adjacent locks whenever possible. POSIX locks are sorted by owner
 * task, then by starting address
 *
 * Kai Petzke writes:
 * To make freeing a lock much faster, we keep a pointer to the lock before the
 * actual one. But the real gain of the new coding was, that lock_it() and
 * unlock_it() became one function.
 *
 * To all purists: Yes, I use a few goto's. Just pass on to the next function.
 */

int posix_lock_file(struct file *filp, struct file_lock *caller,
			   unsigned int wait)
{
	struct file_lock *fl;
	struct file_lock *new_fl, *new_fl2;
	struct file_lock *left = NULL;
	struct file_lock *right = NULL;
	struct file_lock **before;
	struct inode * inode = filp->f_dentry->d_inode;
	int error, added = 0;

	/*
	 * We may need two file_lock structures for this operation,
	 * so we get them in advance to avoid races.
	 */
	new_fl = locks_alloc_lock();
	new_fl2 = locks_alloc_lock();
	error = -ENOLCK; /* "no luck" */
	if (!(new_fl && new_fl2))
		goto out_nolock;

	lock_kernel();
	if (caller->fl_type != F_UNLCK) {
  repeat:
		for (fl = inode->i_flock; fl != NULL; fl = fl->fl_next) {
			if (!(fl->fl_flags & FL_POSIX))
				continue;
			if (!posix_locks_conflict(caller, fl))
				continue;
			error = -EAGAIN;
			if (!wait)
				goto out;
			error = -EDEADLK;
			if (posix_locks_deadlock(caller, fl))
				goto out;

			error = locks_block_on(fl, caller);
			if (error != 0)
				goto out;
			goto repeat;
  		}
  	}

	/*
	 * We've allocated the new locks in advance, so there are no
	 * errors possible (and no blocking operations) from here on.
	 * 
	 * Find the first old lock with the same owner as the new lock.
	 */
	
	before = &inode->i_flock;

	/* First skip locks owned by other processes.
	 */
	while ((fl = *before) && (!(fl->fl_flags & FL_POSIX) ||
				  !locks_same_owner(caller, fl))) {
		before = &fl->fl_next;
	}

	/* Process locks with this owner.
	 */
	while ((fl = *before) && locks_same_owner(caller, fl)) {
		/* Detect adjacent or overlapping regions (if same lock type)
		 */
		if (caller->fl_type == fl->fl_type) {
			if (fl->fl_end < caller->fl_start - 1)
				goto next_lock;
			/* If the next lock in the list has entirely bigger
			 * addresses than the new one, insert the lock here.
			 */
			if (fl->fl_start > caller->fl_end + 1)
				break;

			/* If we come here, the new and old lock are of the
			 * same type and adjacent or overlapping. Make one
			 * lock yielding from the lower start address of both
			 * locks to the higher end address.
			 */
			if (fl->fl_start > caller->fl_start)
				fl->fl_start = caller->fl_start;
			else
				caller->fl_start = fl->fl_start;
			if (fl->fl_end < caller->fl_end)
				fl->fl_end = caller->fl_end;
			else
				caller->fl_end = fl->fl_end;
			if (added) {
				locks_delete_lock(before, 0);
				continue;
			}
			caller = fl;
			added = 1;
		}
		else {
			/* Processing for different lock types is a bit
			 * more complex.
			 */
			if (fl->fl_end < caller->fl_start)
				goto next_lock;
			if (fl->fl_start > caller->fl_end)
				break;
			if (caller->fl_type == F_UNLCK)
				added = 1;
			if (fl->fl_start < caller->fl_start)
				left = fl;
			/* If the next lock in the list has a higher end
			 * address than the new one, insert the new one here.
			 */
			if (fl->fl_end > caller->fl_end) {
				right = fl;
				break;
			}
			if (fl->fl_start >= caller->fl_start) {
				/* The new lock completely replaces an old
				 * one (This may happen several times).
				 */
				if (added) {
					locks_delete_lock(before, 0);
					continue;
				}
				/* Replace the old lock with the new one.
				 * Wake up anybody waiting for the old one,
				 * as the change in lock type might satisfy
				 * their needs.
				 */
				locks_wake_up_blocks(fl, 0);	/* This cannot schedule()! */
				fl->fl_start = caller->fl_start;
				fl->fl_end = caller->fl_end;
				fl->fl_type = caller->fl_type;
				fl->fl_u = caller->fl_u;
				caller = fl;
				added = 1;
			}
		}
		/* Go on to next lock.
		 */
	next_lock:
		before = &fl->fl_next;
	}

	error = 0;
	if (!added) {
		if (caller->fl_type == F_UNLCK)
			goto out;
		locks_copy_lock(new_fl, caller);
		locks_insert_lock(before, new_fl);
		new_fl = NULL;
	}
	if (right) {
		if (left == right) {
			/* The new lock breaks the old one in two pieces,
			 * so we have to use the second new lock.
			 */
			left = new_fl2;
			new_fl2 = NULL;
			locks_copy_lock(left, right);
			locks_insert_lock(before, left);
		}
		right->fl_start = caller->fl_end + 1;
		locks_wake_up_blocks(right, 0);
	}
	if (left) {
		left->fl_end = caller->fl_start - 1;
		locks_wake_up_blocks(left, 0);
	}
out:
	unlock_kernel();
out_nolock:
	/*
	 * Free any unused locks.
	 */
	if (new_fl)
		locks_free_lock(new_fl);
	if (new_fl2)
		locks_free_lock(new_fl2);
	return error;
}

static inline int flock_translate_cmd(int cmd) {
#ifdef MSNFS
	if (cmd & LOCK_MAND)
		return cmd & (LOCK_MAND | LOCK_RW);
#endif
	switch (cmd &~ LOCK_NB) {
	case LOCK_SH:
		return F_RDLCK;
	case LOCK_EX:
		return F_WRLCK;
	case LOCK_UN:
		return F_UNLCK;
	}
	return -EINVAL;
}

/* We already had a lease on this file; just change its type */
static int lease_modify(struct file_lock **before, int arg)
{
	struct file_lock *fl = *before;
	int error = assign_type(fl, arg);

	if (error)
		return error;
	locks_wake_up_blocks(fl, 0);
	if (arg == F_UNLCK) {
		struct file *filp = fl->fl_file;

		filp->f_owner.pid = 0;
		filp->f_owner.uid = 0;
		filp->f_owner.euid = 0;
		filp->f_owner.signum = 0;
		locks_delete_lock(before, 0);
	}
	return 0;
}

static void time_out_leases(struct inode *inode)
{
	struct file_lock **before;
	struct file_lock *fl;

	before = &inode->i_flock;
	while ((fl = *before) && (fl->fl_flags & FL_LEASE)
			&& (fl->fl_type & F_INPROGRESS)) {
		if ((fl->fl_break_time == 0)
				|| time_before(jiffies, fl->fl_break_time)) {
			before = &fl->fl_next;
			continue;
		}
		printk(KERN_INFO "lease broken - owner pid = %d\n", fl->fl_pid);
		lease_modify(before, fl->fl_type & ~F_INPROGRESS);
		if (fl == *before)	/* lease_modify may have freed fl */
			before = &fl->fl_next;
	}
}

/**
 *	__get_lease	-	revoke all outstanding leases on file
 *	@inode: the inode of the file to return
 *	@mode: the open mode (read or write)
 *
 *	get_lease (inlined for speed) has checked there already
 *	is a lease on this file.  Leases are broken on a call to open()
 *	or truncate().  This function can sleep unless you
 *	specified %O_NONBLOCK to your open().
 */
int __get_lease(struct inode *inode, unsigned int mode)
{
	int error = 0, future;
	struct file_lock *new_fl, *flock;
	struct file_lock *fl;
	int alloc_err;
	unsigned long break_time;
	int i_have_this_lease = 0;

	alloc_err = lease_alloc(NULL, mode & FMODE_WRITE ? F_WRLCK : F_RDLCK,
			&new_fl);

	lock_kernel();

	time_out_leases(inode);

	flock = inode->i_flock;
	if ((flock == NULL) || (flock->fl_flags & FL_LEASE) == 0)
		goto out;

	for (fl = flock; fl && (fl->fl_flags & FL_LEASE); fl = fl->fl_next)
		if (fl->fl_owner == current->files)
			i_have_this_lease = 1;

	if (mode & FMODE_WRITE) {
		/* If we want write access, we have to revoke any lease. */
		future = F_UNLCK | F_INPROGRESS;
	} else if (flock->fl_type & F_INPROGRESS) {
		/* If the lease is already being broken, we just leave it */
		future = flock->fl_type;
	} else if (flock->fl_type & F_WRLCK) {
		/* Downgrade the exclusive lease to a read-only lease. */
		future = F_RDLCK | F_INPROGRESS;
	} else {
		/* the existing lease was read-only, so we can read too. */
		goto out;
	}

	if (alloc_err && !i_have_this_lease && ((mode & O_NONBLOCK) == 0)) {
		error = alloc_err;
		goto out;
	}

	break_time = 0;
	if (lease_break_time > 0) {
		break_time = jiffies + lease_break_time * HZ;
		if (break_time == 0)
			break_time++;	/* so that 0 means no break time */
	}

	for (fl = flock; fl && (fl->fl_flags & FL_LEASE); fl = fl->fl_next) {
		if (fl->fl_type != future) {
			fl->fl_type = future;
			fl->fl_break_time = break_time;
			kill_fasync(&fl->fl_fasync, SIGIO, POLL_MSG);
		}
	}

	if (i_have_this_lease || (mode & O_NONBLOCK)) {
		error = -EWOULDBLOCK;
		goto out;
	}

restart:
	break_time = flock->fl_break_time;
	if (break_time != 0) {
		break_time -= jiffies;
		if (break_time == 0)
			break_time++;
	}
	error = locks_block_on_timeout(flock, new_fl, break_time);
	if (error >= 0) {
		if (error == 0)
			time_out_leases(inode);
		/* Wait for the next lease that has not been broken yet */
		for (flock = inode->i_flock;
				flock && (flock->fl_flags & FL_LEASE);
				flock = flock->fl_next) {
			if (flock->fl_type & F_INPROGRESS)
				goto restart;
		}
		error = 0;
	}

out:
	unlock_kernel();
	if (!alloc_err)
		locks_free_lock(new_fl);
	return error;
}

/**
 *	lease_get_mtime
 *	@inode: the inode
 *
 * This is to force NFS clients to flush their caches for files with
 * exclusive leases.  The justification is that if someone has an
 * exclusive lease, then they could be modifiying it.
 */
time_t lease_get_mtime(struct inode *inode)
{
	struct file_lock *flock = inode->i_flock;
	if (flock && (flock->fl_flags & FL_LEASE) && (flock->fl_type & F_WRLCK))
		return CURRENT_TIME;
	return inode->i_mtime;
}

/**
 *	fcntl_getlease - Enquire what lease is currently active
 *	@filp: the file
 *
 *	The value returned by this function will be one of
 *	(if no lease break is pending):
 *
 *	%F_RDLCK to indicate a shared lease is held.
 *
 *	%F_WRLCK to indicate an exclusive lease is held.
 *
 *	%F_UNLCK to indicate no lease is held.
 *
 *	(if a lease break is pending):
 *
 *	%F_RDLCK to indicate an exclusive lease needs to be
 *		changed to a shared lease (or removed).
 *
 *	%F_UNLCK to indicate the lease needs to be removed.
 *
 *	XXX: sfr & willy disagree over whether F_INPROGRESS
 *	should be returned to userspace.
 */
int fcntl_getlease(struct file *filp)
{
	struct file_lock *fl;
	int type = F_UNLCK;

	lock_kernel();
	time_out_leases(filp->f_dentry->d_inode);
	for (fl = filp->f_dentry->d_inode->i_flock;
			fl && (fl->fl_flags & FL_LEASE);
			fl = fl->fl_next) {
		if (fl->fl_file == filp) {
			type = fl->fl_type & ~F_INPROGRESS;
			break;
		}
	}
	unlock_kernel();
	return type;
}

/**
 *	fcntl_setlease	-	sets a lease on an open file
 *	@fd: open file descriptor
 *	@filp: file pointer
 *	@arg: type of lease to obtain
 *
 *	Call this fcntl to establish a lease on the file.
 *	Note that you also need to call %F_SETSIG to
 *	receive a signal when the lease is broken.
 */
int fcntl_setlease(unsigned int fd, struct file *filp, long arg)
{
	struct file_lock *fl, **before, **my_before = NULL;
	struct dentry *dentry;
	struct inode *inode;
	int error, rdlease_count = 0, wrlease_count = 0;

	dentry = filp->f_dentry;
	inode = dentry->d_inode;

	if ((current->fsuid != inode->i_uid) && !capable(CAP_LEASE))
		return -EACCES;
	if (!S_ISREG(inode->i_mode))
		return -EINVAL;

	lock_kernel();

	time_out_leases(inode);

	/*
	 * FIXME: What about F_RDLCK and files open for writing?
	 */
	error = -EAGAIN;
	if ((arg == F_WRLCK)
	    && ((atomic_read(&dentry->d_count) > 1)
		|| (atomic_read(&inode->i_count) > 1)))
		goto out_unlock;

	/*
	 * At this point, we know that if there is an exclusive
	 * lease on this file, then we hold it on this filp
	 * (otherwise our open of this file would have blocked).
	 * And if we are trying to acquire an exclusive lease,
	 * then the file is not open by anyone (including us)
	 * except for this filp.
	 */
	for (before = &inode->i_flock;
			((fl = *before) != NULL) && (fl->fl_flags & FL_LEASE);
			before = &fl->fl_next) {
		if (fl->fl_file == filp)
			my_before = before;
		else if (fl->fl_type == (F_INPROGRESS | F_UNLCK))
			/*
			 * Someone is in the process of opening this
			 * file for writing so we may not take an
			 * exclusive lease on it.
			 */
			wrlease_count++;
		else
			rdlease_count++;
	}

	if ((arg == F_RDLCK && (wrlease_count > 0)) ||
	    (arg == F_WRLCK && ((rdlease_count + wrlease_count) > 0)))
		goto out_unlock;

	if (my_before != NULL) {
		error = lease_modify(my_before, arg);
		goto out_unlock;
	}

	error = 0;
	if (arg == F_UNLCK)
		goto out_unlock;

	error = -EINVAL;
	if (!leases_enable)
		goto out_unlock;

	error = lease_alloc(filp, arg, &fl);
	if (error)
		goto out_unlock;

	error = fasync_helper(fd, filp, 1, &fl->fl_fasync);
	if (error < 0) {
		locks_free_lock(fl);
		goto out_unlock;
	}
	fl->fl_next = *before;
	*before = fl;
	list_add(&fl->fl_link, &file_lock_list);
	filp->f_owner.pid = current->pid;
	filp->f_owner.uid = current->uid;
	filp->f_owner.euid = current->euid;
out_unlock:
	unlock_kernel();
	return error;
}

/**
 *	sys_flock: - flock() system call.
 *	@fd: the file descriptor to lock.
 *	@cmd: the type of lock to apply.
 *
 *	Apply a %FL_FLOCK style lock to an open file descriptor.
 *	The @cmd can be one of
 *
 *	%LOCK_SH -- a shared lock.
 *
 *	%LOCK_EX -- an exclusive lock.
 *
 *	%LOCK_UN -- remove an existing lock.
 *
 *	%LOCK_MAND -- a `mandatory' flock.  This exists to emulate Windows Share Modes.
 *
 *	%LOCK_MAND can be combined with %LOCK_READ or %LOCK_WRITE to allow other
 *	processes read and write access respectively.
 */
asmlinkage long sys_flock(unsigned int fd, unsigned int cmd)
{
	struct file *filp;
	int error, type;

	error = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	error = flock_translate_cmd(cmd);
	if (error < 0)
		goto out_putf;
	type = error;

	error = -EBADF;
	if ((type != F_UNLCK)
#ifdef MSNFS
		&& !(type & LOCK_MAND)
#endif
		&& !(filp->f_mode & 3))
		goto out_putf;

	lock_kernel();
	error = flock_lock_file(filp, type,
				(cmd & (LOCK_UN | LOCK_NB)) ? 0 : 1);
	unlock_kernel();

out_putf:
	fput(filp);
out:
	return error;
}

/* Report the first existing lock that would conflict with l.
 * This implements the F_GETLK command of fcntl().
 */
int fcntl_getlk(unsigned int fd, struct flock *l)
{
	struct file *filp;
	struct file_lock *fl, file_lock;
	struct flock flock;
	int error;

	error = -EFAULT;
	if (copy_from_user(&flock, l, sizeof(flock)))
		goto out;
	error = -EINVAL;
	if ((flock.l_type != F_RDLCK) && (flock.l_type != F_WRLCK))
		goto out;

	error = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	error = flock_to_posix_lock(filp, &file_lock, &flock);
	if (error)
		goto out_putf;

	if (filp->f_op && filp->f_op->lock) {
		error = filp->f_op->lock(filp, F_GETLK, &file_lock);
		if (error < 0)
			goto out_putf;
		else if (error == LOCK_USE_CLNT)
		  /* Bypass for NFS with no locking - 2.0.36 compat */
		  fl = posix_test_lock(filp, &file_lock);
		else
		  fl = (file_lock.fl_type == F_UNLCK ? NULL : &file_lock);
	} else {
		fl = posix_test_lock(filp, &file_lock);
	}
 
	flock.l_type = F_UNLCK;
	if (fl != NULL) {
		flock.l_pid = fl->fl_pid;
#if BITS_PER_LONG == 32
		/*
		 * Make sure we can represent the posix lock via
		 * legacy 32bit flock.
		 */
		error = -EOVERFLOW;
		if (fl->fl_start > OFFT_OFFSET_MAX)
			goto out_putf;
		if ((fl->fl_end != OFFSET_MAX)
		    && (fl->fl_end > OFFT_OFFSET_MAX))
			goto out_putf;
#endif
		flock.l_start = fl->fl_start;
		flock.l_len = fl->fl_end == OFFSET_MAX ? 0 :
			fl->fl_end - fl->fl_start + 1;
		flock.l_whence = 0;
		flock.l_type = fl->fl_type;
	}
	error = -EFAULT;
	if (!copy_to_user(l, &flock, sizeof(flock)))
		error = 0;
  
out_putf:
	fput(filp);
out:
	return error;
}

/* Apply the lock described by l to an open file descriptor.
 * This implements both the F_SETLK and F_SETLKW commands of fcntl().
 */
int fcntl_setlk(unsigned int fd, unsigned int cmd, struct flock *l)
{
	struct file *filp;
	struct file_lock *file_lock = locks_alloc_lock();
	struct flock flock;
	struct inode *inode;
	int error;

	if (file_lock == NULL)
		return -ENOLCK;

	/*
	 * This might block, so we do it before checking the inode.
	 */
	error = -EFAULT;
	if (copy_from_user(&flock, l, sizeof(flock)))
		goto out;

	/* Get arguments and validate them ...
	 */

	error = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	error = -EINVAL;
	inode = filp->f_dentry->d_inode;

	/* Don't allow mandatory locks on files that may be memory mapped
	 * and shared.
	 */
	if (IS_MANDLOCK(inode) &&
	    (inode->i_mode & (S_ISGID | S_IXGRP)) == S_ISGID) {
		struct address_space *mapping = inode->i_mapping;

		if (mapping->i_mmap_shared != NULL) {
			error = -EAGAIN;
			goto out_putf;
		}
	}

	error = flock_to_posix_lock(filp, file_lock, &flock);
	if (error)
		goto out_putf;
	
	error = -EBADF;
	switch (flock.l_type) {
	case F_RDLCK:
		if (!(filp->f_mode & FMODE_READ))
			goto out_putf;
		break;
	case F_WRLCK:
		if (!(filp->f_mode & FMODE_WRITE))
			goto out_putf;
		break;
	case F_UNLCK:
		break;
	case F_SHLCK:
	case F_EXLCK:
#ifdef __sparc__
/* warn a bit for now, but don't overdo it */
{
	static int count = 0;
	if (!count) {
		count=1;
		printk(KERN_WARNING
		       "fcntl_setlk() called by process %d (%s) with broken flock() emulation\n",
		       current->pid, current->comm);
	}
}
		if (!(filp->f_mode & 3))
			goto out_putf;
		break;
#endif
	default:
		error = -EINVAL;
		goto out_putf;
	}

	if (filp->f_op && filp->f_op->lock != NULL) {
		error = filp->f_op->lock(filp, cmd, file_lock);
		if (error < 0)
			goto out_putf;
	}
	error = posix_lock_file(filp, file_lock, cmd == F_SETLKW);

out_putf:
	fput(filp);
out:
	locks_free_lock(file_lock);
	return error;
}

#if BITS_PER_LONG == 32
/* Report the first existing lock that would conflict with l.
 * This implements the F_GETLK command of fcntl().
 */
int fcntl_getlk64(unsigned int fd, struct flock64 *l)
{
	struct file *filp;
	struct file_lock *fl, file_lock;
	struct flock64 flock;
	int error;

	error = -EFAULT;
	if (copy_from_user(&flock, l, sizeof(flock)))
		goto out;
	error = -EINVAL;
	if ((flock.l_type != F_RDLCK) && (flock.l_type != F_WRLCK))
		goto out;

	error = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	error = flock64_to_posix_lock(filp, &file_lock, &flock);
	if (error)
		goto out_putf;

	if (filp->f_op && filp->f_op->lock) {
		error = filp->f_op->lock(filp, F_GETLK, &file_lock);
		if (error < 0)
			goto out_putf;
		else if (error == LOCK_USE_CLNT)
		  /* Bypass for NFS with no locking - 2.0.36 compat */
		  fl = posix_test_lock(filp, &file_lock);
		else
		  fl = (file_lock.fl_type == F_UNLCK ? NULL : &file_lock);
	} else {
		fl = posix_test_lock(filp, &file_lock);
	}
 
	flock.l_type = F_UNLCK;
	if (fl != NULL) {
		flock.l_pid = fl->fl_pid;
		flock.l_start = fl->fl_start;
		flock.l_len = fl->fl_end == OFFSET_MAX ? 0 :
			fl->fl_end - fl->fl_start + 1;
		flock.l_whence = 0;
		flock.l_type = fl->fl_type;
	}
	error = -EFAULT;
	if (!copy_to_user(l, &flock, sizeof(flock)))
		error = 0;
  
out_putf:
	fput(filp);
out:
	return error;
}

/* Apply the lock described by l to an open file descriptor.
 * This implements both the F_SETLK and F_SETLKW commands of fcntl().
 */
int fcntl_setlk64(unsigned int fd, unsigned int cmd, struct flock64 *l)
{
	struct file *filp;
	struct file_lock *file_lock = locks_alloc_lock();
	struct flock64 flock;
	struct inode *inode;
	int error;

	if (file_lock == NULL)
		return -ENOLCK;

	/*
	 * This might block, so we do it before checking the inode.
	 */
	error = -EFAULT;
	if (copy_from_user(&flock, l, sizeof(flock)))
		goto out;

	/* Get arguments and validate them ...
	 */

	error = -EBADF;
	filp = fget(fd);
	if (!filp)
		goto out;

	error = -EINVAL;
	inode = filp->f_dentry->d_inode;

	/* Don't allow mandatory locks on files that may be memory mapped
	 * and shared.
	 */
	if (IS_MANDLOCK(inode) &&
	    (inode->i_mode & (S_ISGID | S_IXGRP)) == S_ISGID) {
		struct address_space *mapping = inode->i_mapping;

		if (mapping->i_mmap_shared != NULL) {
			error = -EAGAIN;
			goto out_putf;
		}
	}

	error = flock64_to_posix_lock(filp, file_lock, &flock);
	if (error)
		goto out_putf;
	
	error = -EBADF;
	switch (flock.l_type) {
	case F_RDLCK:
		if (!(filp->f_mode & FMODE_READ))
			goto out_putf;
		break;
	case F_WRLCK:
		if (!(filp->f_mode & FMODE_WRITE))
			goto out_putf;
		break;
	case F_UNLCK:
		break;
	case F_SHLCK:
	case F_EXLCK:
	default:
		error = -EINVAL;
		goto out_putf;
	}

	if (filp->f_op && filp->f_op->lock != NULL) {
		error = filp->f_op->lock(filp, cmd, file_lock);
		if (error < 0)
			goto out_putf;
	}
	error = posix_lock_file(filp, file_lock, cmd == F_SETLKW64);

out_putf:
	fput(filp);
out:
	locks_free_lock(file_lock);
	return error;
}
#endif /* BITS_PER_LONG == 32 */

/*
 * This function is called when the file is being removed
 * from the task's fd array.
 */
void locks_remove_posix(struct file *filp, fl_owner_t owner)
{
	struct inode * inode = filp->f_dentry->d_inode;
	struct file_lock *fl;
	struct file_lock **before;

	/*
	 * For POSIX locks we free all locks on this file for the given task.
	 */
	if (!inode->i_flock) {
		/*
		 * Notice that something might be grabbing a lock right now.
		 * Consider it as a race won by us - event is async, so even if
		 * we miss the lock added we can trivially consider it as added
		 * after we went through this call.
		 */
		return;
	}
	lock_kernel();
	before = &inode->i_flock;
	while ((fl = *before) != NULL) {
		if ((fl->fl_flags & FL_POSIX) && fl->fl_owner == owner) {
			struct file *filp = fl->fl_file;
			/* Note: locks_unlock_delete() can sleep, and
			 * so we may race with the call to sys_close()
			 * by the thread that actually owns this filp.
			 */
			get_file(filp);
			locks_unlock_delete(before);
			fput(filp);
			before = &inode->i_flock;
			continue;
		}
		before = &fl->fl_next;
	}
	unlock_kernel();
}

/*
 * This function is called on the last close of an open file.
 */
void locks_remove_flock(struct file *filp)
{
	struct inode * inode = filp->f_dentry->d_inode; 
	struct file_lock *fl;
	struct file_lock **before;

	if (!inode->i_flock)
		return;

	lock_kernel();
	before = &inode->i_flock;

	while ((fl = *before) != NULL) {
		if (fl->fl_file == filp) {
			if (fl->fl_flags & FL_FLOCK) {
				locks_delete_lock(before, 0);
				continue;
			}
			if (fl->fl_flags & FL_LEASE) {
				lease_modify(before, F_UNLCK);
				continue;
			}
 		}
		before = &fl->fl_next;
	}
	unlock_kernel();
}

/**
 *	posix_block_lock - blocks waiting for a file lock
 *	@blocker: the lock which is blocking
 *	@waiter: the lock which conflicts and has to wait
 *
 * lockd needs to block waiting for locks.
 */
void
posix_block_lock(struct file_lock *blocker, struct file_lock *waiter)
{
	locks_insert_block(blocker, waiter);
}

/**
 *	posix_unblock_lock - stop waiting for a file lock
 *	@waiter: the lock which was waiting
 *
 *	lockd needs to block waiting for locks.
 */
void
posix_unblock_lock(struct file_lock *waiter)
{
	if (!list_empty(&waiter->fl_block))
		locks_delete_block(waiter);
}

static void lock_get_status(char* out, struct file_lock *fl, int id, char *pfx)
{
	struct inode *inode = NULL;

	if (fl->fl_file != NULL)
		inode = fl->fl_file->f_dentry->d_inode;

	out += sprintf(out, "%d:%s ", id, pfx);
	if (fl->fl_flags & FL_POSIX) {
		out += sprintf(out, "%6s %s ",
			     (fl->fl_flags & FL_ACCESS) ? "ACCESS" : "POSIX ",
			     (inode == NULL) ? "*NOINODE*" :
			     (IS_MANDLOCK(inode) &&
			      (inode->i_mode & (S_IXGRP | S_ISGID)) == S_ISGID) ?
			     "MANDATORY" : "ADVISORY ");
	} else if (fl->fl_flags & FL_FLOCK) {
#ifdef MSNFS
		if (fl->fl_type & LOCK_MAND) {
			out += sprintf(out, "FLOCK  MSNFS     ");
		} else
#endif
			out += sprintf(out, "FLOCK  ADVISORY  ");
	} else if (fl->fl_flags & FL_LEASE) {
		out += sprintf(out, "LEASE  ");
		if (fl->fl_type & F_INPROGRESS)
			out += sprintf(out, "BREAKING  ");
		else if (fl->fl_file)
			out += sprintf(out, "ACTIVE    ");
		else
			out += sprintf(out, "BREAKER   ");
	} else {
		out += sprintf(out, "UNKNOWN UNKNOWN  ");
	}
#ifdef MSNFS
	if (fl->fl_type & LOCK_MAND) {
		out += sprintf(out, "%s ",
			       (fl->fl_type & LOCK_READ)
			       ? (fl->fl_type & LOCK_WRITE) ? "RW   " : "READ "
			       : (fl->fl_type & LOCK_WRITE) ? "WRITE" : "NONE ");
	} else
#endif
		out += sprintf(out, "%s ",
			       (fl->fl_type & F_INPROGRESS)
			       ? (fl->fl_type & F_UNLCK) ? "UNLCK" : "READ "
			       : (fl->fl_type & F_WRLCK) ? "WRITE" : "READ ");
	out += sprintf(out, "%d %s:%ld ",
		     fl->fl_pid,
		     inode ? kdevname(inode->i_dev) : "<none>",
		     inode ? inode->i_ino : 0);
	out += sprintf(out, "%Ld ", fl->fl_start);
	if (fl->fl_end == OFFSET_MAX)
		out += sprintf(out, "EOF ");
	else
		out += sprintf(out, "%Ld ", fl->fl_end);
	sprintf(out, "%08lx %08lx %08lx %08lx %08lx\n",
		(long)fl, (long)fl->fl_link.prev, (long)fl->fl_link.next,
		(long)fl->fl_next, (long)fl->fl_block.next);
}

static void move_lock_status(char **p, off_t* pos, off_t offset)
{
	int len;
	len = strlen(*p);
	if(*pos >= offset) {
		/* the complete line is valid */
		*p += len;
		*pos += len;
		return;
	}
	if(*pos+len > offset) {
		/* use the second part of the line */
		int i = offset-*pos;
		memmove(*p,*p+i,len-i);
		*p += len-i;
		*pos += len;
		return;
	}
	/* discard the complete line */
	*pos += len;
}

/**
 *	get_locks_status	-	reports lock usage in /proc/locks
 *	@buffer: address in userspace to write into
 *	@start: ?
 *	@offset: how far we are through the buffer
 *	@length: how much to read
 */

int get_locks_status(char *buffer, char **start, off_t offset, int length)
{
	struct list_head *tmp;
	char *q = buffer;
	off_t pos = 0;
	int i = 0;

	lock_kernel();
	list_for_each(tmp, &file_lock_list) {
		struct list_head *btmp;
		struct file_lock *fl = list_entry(tmp, struct file_lock, fl_link);
		lock_get_status(q, fl, ++i, "");
		move_lock_status(&q, &pos, offset);

		if(pos >= offset+length)
			goto done;

		list_for_each(btmp, &fl->fl_block) {
			struct file_lock *bfl = list_entry(btmp,
					struct file_lock, fl_block);
			lock_get_status(q, bfl, i, " ->");
			move_lock_status(&q, &pos, offset);

			if(pos >= offset+length)
				goto done;
		}
	}
done:
	unlock_kernel();
	*start = buffer;
	if(q-buffer < length)
		return (q-buffer);
	return length;
}

void steal_locks(fl_owner_t from)
{
	struct list_head *tmp;

	if (from == current->files)
		return;

	lock_kernel();
	list_for_each(tmp, &file_lock_list) {
		struct file_lock *fl = list_entry(tmp, struct file_lock,
						  fl_link);
		if (fl->fl_owner == from)
			fl->fl_owner = current->files;
	}
	unlock_kernel();
}

#ifdef MSNFS
/**
 *	lock_may_read - checks that the region is free of locks
 *	@inode: the inode that is being read
 *	@start: the first byte to read
 *	@len: the number of bytes to read
 *
 *	Emulates Windows locking requirements.  Whole-file
 *	mandatory locks (share modes) can prohibit a read and
 *	byte-range POSIX locks can prohibit a read if they overlap.
 *
 *	N.B. this function is only ever called
 *	from knfsd and ownership of locks is never checked.
 */
int lock_may_read(struct inode *inode, loff_t start, unsigned long len)
{
	struct file_lock *fl;
	int result = 1;
	lock_kernel();
	for (fl = inode->i_flock; fl != NULL; fl = fl->fl_next) {
		if (fl->fl_flags == FL_POSIX) {
			if (fl->fl_type == F_RDLCK)
				continue;
			if ((fl->fl_end < start) || (fl->fl_start > (start + len)))
				continue;
		} else if (fl->fl_flags == FL_FLOCK) {
			if (!(fl->fl_type & LOCK_MAND))
				continue;
			if (fl->fl_type & LOCK_READ)
				continue;
		} else
			continue;
		result = 0;
		break;
	}
	unlock_kernel();
	return result;
}

/**
 *	lock_may_write - checks that the region is free of locks
 *	@inode: the inode that is being written
 *	@start: the first byte to write
 *	@len: the number of bytes to write
 *
 *	Emulates Windows locking requirements.  Whole-file
 *	mandatory locks (share modes) can prohibit a write and
 *	byte-range POSIX locks can prohibit a write if they overlap.
 *
 *	N.B. this function is only ever called
 *	from knfsd and ownership of locks is never checked.
 */
int lock_may_write(struct inode *inode, loff_t start, unsigned long len)
{
	struct file_lock *fl;
	int result = 1;
	lock_kernel();
	for (fl = inode->i_flock; fl != NULL; fl = fl->fl_next) {
		if (fl->fl_flags == FL_POSIX) {
			if ((fl->fl_end < start) || (fl->fl_start > (start + len)))
				continue;
		} else if (fl->fl_flags == FL_FLOCK) {
			if (!(fl->fl_type & LOCK_MAND))
				continue;
			if (fl->fl_type & LOCK_WRITE)
				continue;
		} else
			continue;
		result = 0;
		break;
	}
	unlock_kernel();
	return result;
}
#endif

static int __init filelock_init(void)
{
	filelock_cache = kmem_cache_create("file_lock_cache",
			sizeof(struct file_lock), 0, 0, init_once, NULL);
	if (!filelock_cache)
		panic("cannot create file lock slab cache");
	return 0;
}

module_init(filelock_init)
