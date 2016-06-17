/*
 * linux/kernel/itimer.c
 *
 * Copyright (C) 1992 Darren Senn
 */

/* These are all the functions necessary to implement itimers */

#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>

/*
 * change timeval to jiffies, trying to avoid the 
 * most obvious overflows..
 *
 * The tv_*sec values are signed, but nothing seems to 
 * indicate whether we really should use them as signed values
 * when doing itimers. POSIX doesn't mention this (but if
 * alarm() uses itimers without checking, we have to use unsigned
 * arithmetic).
 */
static unsigned long tvtojiffies(struct timeval *value)
{
	unsigned long sec = (unsigned) value->tv_sec;
	unsigned long usec = (unsigned) value->tv_usec;

	if (sec > (ULONG_MAX / HZ))
		return ULONG_MAX;
	usec += 1000000 / HZ - 1;
	usec /= 1000000 / HZ;
	return HZ*sec+usec;
}

static void jiffiestotv(unsigned long jiffies, struct timeval *value)
{
	value->tv_usec = (jiffies % HZ) * (1000000 / HZ);
	value->tv_sec = jiffies / HZ;
}

int do_getitimer(int which, struct itimerval *value)
{
	register unsigned long val, interval;

	switch (which) {
	case ITIMER_REAL:
		interval = current->it_real_incr;
		val = 0;
		/* 
		 * FIXME! This needs to be atomic, in case the kernel timer happens!
		 */
		if (timer_pending(&current->real_timer)) {
			val = current->real_timer.expires - jiffies;

			/* look out for negative/zero itimer.. */
			if ((long) val <= 0)
				val = 1;
		}
		break;
	case ITIMER_VIRTUAL:
		val = current->it_virt_value;
		interval = current->it_virt_incr;
		break;
	case ITIMER_PROF:
		val = current->it_prof_value;
		interval = current->it_prof_incr;
		break;
	default:
		return(-EINVAL);
	}
	jiffiestotv(val, &value->it_value);
	jiffiestotv(interval, &value->it_interval);
	return 0;
}

/* SMP: Only we modify our itimer values. */
asmlinkage long sys_getitimer(int which, struct itimerval *value)
{
	int error = -EFAULT;
	struct itimerval get_buffer;

	if (value) {
		error = do_getitimer(which, &get_buffer);
		if (!error &&
		    copy_to_user(value, &get_buffer, sizeof(get_buffer)))
			error = -EFAULT;
	}
	return error;
}

void it_real_fn(unsigned long __data)
{
	struct task_struct * p = (struct task_struct *) __data;
	unsigned long interval;

	send_sig(SIGALRM, p, 1);
	interval = p->it_real_incr;
	if (interval) {
		if (interval > (unsigned long) LONG_MAX)
			interval = LONG_MAX;
		p->real_timer.expires = jiffies + interval;
		add_timer(&p->real_timer);
	}
}

int do_setitimer(int which, struct itimerval *value, struct itimerval *ovalue)
{
	register unsigned long i, j;
	int k;

	i = tvtojiffies(&value->it_interval);
	j = tvtojiffies(&value->it_value);
	if (ovalue && (k = do_getitimer(which, ovalue)) < 0)
		return k;
	switch (which) {
		case ITIMER_REAL:
			del_timer_sync(&current->real_timer);
			current->it_real_value = j;
			current->it_real_incr = i;
			if (!j)
				break;
			if (j > (unsigned long) LONG_MAX)
				j = LONG_MAX;
			i = j + jiffies;
			current->real_timer.expires = i;
			add_timer(&current->real_timer);
			break;
		case ITIMER_VIRTUAL:
			if (j)
				j++;
			current->it_virt_value = j;
			current->it_virt_incr = i;
			break;
		case ITIMER_PROF:
			if (j)
				j++;
			current->it_prof_value = j;
			current->it_prof_incr = i;
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

/* SMP: Again, only we play with our itimers, and signals are SMP safe
 *      now so that is not an issue at all anymore.
 */
asmlinkage long sys_setitimer(int which, struct itimerval *value,
			      struct itimerval *ovalue)
{
	struct itimerval set_buffer, get_buffer;
	int error;

	if (value) {
		if(copy_from_user(&set_buffer, value, sizeof(set_buffer)))
			return -EFAULT;
	} else
		memset((char *) &set_buffer, 0, sizeof(set_buffer));

	error = do_setitimer(which, &set_buffer, ovalue ? &get_buffer : 0);
	if (error || !ovalue)
		return error;

	if (copy_to_user(ovalue, &get_buffer, sizeof(get_buffer)))
		return -EFAULT; 
	return 0;
}
