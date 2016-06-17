/*
 * This file contains the procedures for the handling of select and poll
 *
 * Created for Linux based loosely upon Mathius Lattner's minix
 * patches by Peter MacDonald. Heavily edited by Linus.
 *
 *  4 February 1994
 *     COFF/ELF binary emulation. If the process has the STICKY_TIMEOUTS
 *     flag set in its personality we do *not* modify the given timeout
 *     parameter to reflect time remaining.
 *
 *  24 January 2000
 *     Changed sys_poll()/do_poll() to use PAGE_SIZE chunk-based allocation 
 *     of fds to overcome nfds < 16390 descriptors limit (Tigran Aivazian).
 */

#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/poll.h>
#include <linux/personality.h> /* for STICKY_TIMEOUTS */
#include <linux/file.h>

#include <asm/uaccess.h>

#define ROUND_UP(x,y) (((x)+(y)-1)/(y))
#define DEFAULT_POLLMASK (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM)

struct poll_table_entry {
	struct file * filp;
	wait_queue_t wait;
	wait_queue_head_t * wait_address;
};

struct poll_table_page {
	struct poll_table_page * next;
	struct poll_table_entry * entry;
	struct poll_table_entry entries[0];
};

#define POLL_TABLE_FULL(table) \
	((unsigned long)((table)->entry+1) > PAGE_SIZE + (unsigned long)(table))

/*
 * Ok, Peter made a complicated, but straightforward multiple_wait() function.
 * I have rewritten this, taking some shortcuts: This code may not be easy to
 * follow, but it should be free of race-conditions, and it's practical. If you
 * understand what I'm doing here, then you understand how the linux
 * sleep/wakeup mechanism works.
 *
 * Two very simple procedures, poll_wait() and poll_freewait() make all the
 * work.  poll_wait() is an inline-function defined in <linux/poll.h>,
 * as all select/poll functions have to call it to add an entry to the
 * poll table.
 */

void poll_freewait(poll_table* pt)
{
	struct poll_table_page * p = pt->table;
	while (p) {
		struct poll_table_entry * entry;
		struct poll_table_page *old;

		entry = p->entry;
		do {
			entry--;
			remove_wait_queue(entry->wait_address,&entry->wait);
			fput(entry->filp);
		} while (entry > p->entries);
		old = p;
		p = p->next;
		free_page((unsigned long) old);
	}
}

void __pollwait(struct file * filp, wait_queue_head_t * wait_address, poll_table *p)
{
	struct poll_table_page *table = p->table;

	if (!table || POLL_TABLE_FULL(table)) {
		struct poll_table_page *new_table;

		new_table = (struct poll_table_page *) __get_free_page(GFP_KERNEL);
		if (!new_table) {
			p->error = -ENOMEM;
			__set_current_state(TASK_RUNNING);
			return;
		}
		new_table->entry = new_table->entries;
		new_table->next = table;
		p->table = new_table;
		table = new_table;
	}

	/* Add a new entry */
	{
		struct poll_table_entry * entry = table->entry;
		table->entry = entry+1;
	 	get_file(filp);
	 	entry->filp = filp;
		entry->wait_address = wait_address;
		init_waitqueue_entry(&entry->wait, current);
		add_wait_queue(wait_address,&entry->wait);
	}
}

#define __IN(fds, n)		(fds->in + n)
#define __OUT(fds, n)		(fds->out + n)
#define __EX(fds, n)		(fds->ex + n)
#define __RES_IN(fds, n)	(fds->res_in + n)
#define __RES_OUT(fds, n)	(fds->res_out + n)
#define __RES_EX(fds, n)	(fds->res_ex + n)

#define BITS(fds, n)		(*__IN(fds, n)|*__OUT(fds, n)|*__EX(fds, n))

static int max_select_fd(unsigned long n, fd_set_bits *fds)
{
	unsigned long *open_fds;
	unsigned long set;
	int max;

	/* handle last in-complete long-word first */
	set = ~(~0UL << (n & (__NFDBITS-1)));
	n /= __NFDBITS;
	open_fds = current->files->open_fds->fds_bits+n;
	max = 0;
	if (set) {
		set &= BITS(fds, n);
		if (set) {
			if (!(set & ~*open_fds))
				goto get_max;
			return -EBADF;
		}
	}
	while (n) {
		open_fds--;
		n--;
		set = BITS(fds, n);
		if (!set)
			continue;
		if (set & ~*open_fds)
			return -EBADF;
		if (max)
			continue;
get_max:
		do {
			max++;
			set >>= 1;
		} while (set);
		max += n * __NFDBITS;
	}

	return max;
}

#define BIT(i)		(1UL << ((i)&(__NFDBITS-1)))
#define MEM(i,m)	((m)+(unsigned)(i)/__NFDBITS)
#define ISSET(i,m)	(((i)&*(m)) != 0)
#define SET(i,m)	(*(m) |= (i))

#define POLLIN_SET (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#define POLLOUT_SET (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR)
#define POLLEX_SET (POLLPRI)

int do_select(int n, fd_set_bits *fds, long *timeout)
{
	poll_table table, *wait;
	int retval, i, off;
	long __timeout = *timeout;

 	read_lock(&current->files->file_lock);
	retval = max_select_fd(n, fds);
	read_unlock(&current->files->file_lock);

	if (retval < 0)
		return retval;
	n = retval;

	poll_initwait(&table);
	wait = &table;
	if (!__timeout)
		wait = NULL;
	retval = 0;
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		for (i = 0 ; i < n; i++) {
			unsigned long bit = BIT(i);
			unsigned long mask;
			struct file *file;

			off = i / __NFDBITS;
			if (!(bit & BITS(fds, off)))
				continue;
			file = fget(i);
			mask = POLLNVAL;
			if (file) {
				mask = DEFAULT_POLLMASK;
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, wait);
				fput(file);
			}
			if ((mask & POLLIN_SET) && ISSET(bit, __IN(fds,off))) {
				SET(bit, __RES_IN(fds,off));
				retval++;
				wait = NULL;
			}
			if ((mask & POLLOUT_SET) && ISSET(bit, __OUT(fds,off))) {
				SET(bit, __RES_OUT(fds,off));
				retval++;
				wait = NULL;
			}
			if ((mask & POLLEX_SET) && ISSET(bit, __EX(fds,off))) {
				SET(bit, __RES_EX(fds,off));
				retval++;
				wait = NULL;
			}
		}
		wait = NULL;
		if (retval || !__timeout || signal_pending(current))
			break;
		if(table.error) {
			retval = table.error;
			break;
		}
		__timeout = schedule_timeout(__timeout);
	}
	current->state = TASK_RUNNING;

	poll_freewait(&table);

	/*
	 * Up-to-date the caller timeout.
	 */
	*timeout = __timeout;
	return retval;
}

static void *select_bits_alloc(int size)
{
	return kmalloc(6 * size, GFP_KERNEL);
}

static void select_bits_free(void *bits, int size)
{
	kfree(bits);
}

/*
 * We can actually return ERESTARTSYS instead of EINTR, but I'd
 * like to be certain this leads to no problems. So I return
 * EINTR just for safety.
 *
 * Update: ERESTARTSYS breaks at least the xview clock binary, so
 * I'm trying ERESTARTNOHAND which restart only when you want to.
 */
#define MAX_SELECT_SECONDS \
	((unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)

asmlinkage long
sys_select(int n, fd_set *inp, fd_set *outp, fd_set *exp, struct timeval *tvp)
{
	fd_set_bits fds;
	char *bits;
	long timeout;
	int ret, size, max_fdset;

	timeout = MAX_SCHEDULE_TIMEOUT;
	if (tvp) {
		time_t sec, usec;

		if ((ret = verify_area(VERIFY_READ, tvp, sizeof(*tvp)))
		    || (ret = __get_user(sec, &tvp->tv_sec))
		    || (ret = __get_user(usec, &tvp->tv_usec)))
			goto out_nofds;

		ret = -EINVAL;
		if (sec < 0 || usec < 0)
			goto out_nofds;

		if ((unsigned long) sec < MAX_SELECT_SECONDS) {
			timeout = ROUND_UP(usec, 1000000/HZ);
			timeout += sec * (unsigned long) HZ;
		}
	}

	ret = -EINVAL;
	if (n < 0)
		goto out_nofds;

	/* max_fdset can increase, so grab it once to avoid race */
	max_fdset = current->files->max_fdset;
	if (n > max_fdset)
		n = max_fdset;

	/*
	 * We need 6 bitmaps (in/out/ex for both incoming and outgoing),
	 * since we used fdset we need to allocate memory in units of
	 * long-words. 
	 */
	ret = -ENOMEM;
	size = FDS_BYTES(n);
	bits = select_bits_alloc(size);
	if (!bits)
		goto out_nofds;
	fds.in      = (unsigned long *)  bits;
	fds.out     = (unsigned long *) (bits +   size);
	fds.ex      = (unsigned long *) (bits + 2*size);
	fds.res_in  = (unsigned long *) (bits + 3*size);
	fds.res_out = (unsigned long *) (bits + 4*size);
	fds.res_ex  = (unsigned long *) (bits + 5*size);

	if ((ret = get_fd_set(n, inp, fds.in)) ||
	    (ret = get_fd_set(n, outp, fds.out)) ||
	    (ret = get_fd_set(n, exp, fds.ex)))
		goto out;
	zero_fd_set(n, fds.res_in);
	zero_fd_set(n, fds.res_out);
	zero_fd_set(n, fds.res_ex);

	ret = do_select(n, &fds, &timeout);

	if (tvp && !(current->personality & STICKY_TIMEOUTS)) {
		time_t sec = 0, usec = 0;
		if (timeout) {
			sec = timeout / HZ;
			usec = timeout % HZ;
			usec *= (1000000/HZ);
		}
		put_user(sec, &tvp->tv_sec);
		put_user(usec, &tvp->tv_usec);
	}

	if (ret < 0)
		goto out;
	if (!ret) {
		ret = -ERESTARTNOHAND;
		if (signal_pending(current))
			goto out;
		ret = 0;
	}

	set_fd_set(n, inp, fds.res_in);
	set_fd_set(n, outp, fds.res_out);
	set_fd_set(n, exp, fds.res_ex);

out:
	select_bits_free(bits, size);
out_nofds:
	return ret;
}

#define POLLFD_PER_PAGE  ((PAGE_SIZE) / sizeof(struct pollfd))

static void do_pollfd(unsigned int num, struct pollfd * fdpage,
	poll_table ** pwait, int *count)
{
	int i;

	for (i = 0; i < num; i++) {
		int fd;
		unsigned int mask;
		struct pollfd *fdp;

		mask = 0;
		fdp = fdpage+i;
		fd = fdp->fd;
		if (fd >= 0) {
			struct file * file = fget(fd);
			mask = POLLNVAL;
			if (file != NULL) {
				mask = DEFAULT_POLLMASK;
				if (file->f_op && file->f_op->poll)
					mask = file->f_op->poll(file, *pwait);
				mask &= fdp->events | POLLERR | POLLHUP;
				fput(file);
			}
			if (mask) {
				*pwait = NULL;
				(*count)++;
			}
		}
		fdp->revents = mask;
	}
}

static int do_poll(unsigned int nfds, unsigned int nchunks, unsigned int nleft, 
	struct pollfd *fds[], poll_table *wait, long timeout)
{
	int count;
	poll_table* pt = wait;

	for (;;) {
		unsigned int i;

		set_current_state(TASK_INTERRUPTIBLE);
		count = 0;
		for (i=0; i < nchunks; i++)
			do_pollfd(POLLFD_PER_PAGE, fds[i], &pt, &count);
		if (nleft)
			do_pollfd(nleft, fds[nchunks], &pt, &count);
		pt = NULL;
		if (count || !timeout || signal_pending(current))
			break;
		count = wait->error;
		if (count)
			break;
		timeout = schedule_timeout(timeout);
	}
	current->state = TASK_RUNNING;
	return count;
}

asmlinkage long sys_poll(struct pollfd * ufds, unsigned int nfds, long timeout)
{
	int i, j, fdcount, err;
	struct pollfd **fds;
	poll_table table, *wait;
	int nchunks, nleft;

	/* Do a sanity check on nfds ... */
	if (nfds > current->files->max_fdset && nfds > OPEN_MAX)
		return -EINVAL;

	if (timeout) {
		/* Careful about overflow in the intermediate values */
		if ((unsigned long) timeout < MAX_SCHEDULE_TIMEOUT / HZ)
			timeout = (unsigned long)(timeout*HZ+999)/1000+1;
		else /* Negative or overflow */
			timeout = MAX_SCHEDULE_TIMEOUT;
	}

	poll_initwait(&table);
	wait = &table;
	if (!timeout)
		wait = NULL;

	err = -ENOMEM;
	fds = NULL;
	if (nfds != 0) {
		fds = (struct pollfd **)kmalloc(
			(1 + (nfds - 1) / POLLFD_PER_PAGE) * sizeof(struct pollfd *),
			GFP_KERNEL);
		if (fds == NULL)
			goto out;
	}

	nchunks = 0;
	nleft = nfds;
	while (nleft > POLLFD_PER_PAGE) { /* allocate complete PAGE_SIZE chunks */
		fds[nchunks] = (struct pollfd *)__get_free_page(GFP_KERNEL);
		if (fds[nchunks] == NULL)
			goto out_fds;
		nchunks++;
		nleft -= POLLFD_PER_PAGE;
	}
	if (nleft) { /* allocate last PAGE_SIZE chunk, only nleft elements used */
		fds[nchunks] = (struct pollfd *)__get_free_page(GFP_KERNEL);
		if (fds[nchunks] == NULL)
			goto out_fds;
	}

	err = -EFAULT;
	for (i=0; i < nchunks; i++)
		if (copy_from_user(fds[i], ufds + i*POLLFD_PER_PAGE, PAGE_SIZE))
			goto out_fds1;
	if (nleft) {
		if (copy_from_user(fds[nchunks], ufds + nchunks*POLLFD_PER_PAGE, 
				nleft * sizeof(struct pollfd)))
			goto out_fds1;
	}

	fdcount = do_poll(nfds, nchunks, nleft, fds, wait, timeout);

	/* OK, now copy the revents fields back to user space. */
	for(i=0; i < nchunks; i++)
		for (j=0; j < POLLFD_PER_PAGE; j++, ufds++)
			__put_user((fds[i] + j)->revents, &ufds->revents);
	if (nleft)
		for (j=0; j < nleft; j++, ufds++)
			__put_user((fds[nchunks] + j)->revents, &ufds->revents);

	err = fdcount;
	if (!fdcount && signal_pending(current))
		err = -EINTR;

out_fds1:
	if (nleft)
		free_page((unsigned long)(fds[nchunks]));
out_fds:
	for (i=0; i < nchunks; i++)
		free_page((unsigned long)(fds[i]));
	if (nfds != 0)
		kfree(fds);
out:
	poll_freewait(&table);
	return err;
}
