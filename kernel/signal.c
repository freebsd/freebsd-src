/*
 *  linux/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  1997-11-02  Modified for POSIX.1b signals by Richard Henderson
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/sched.h>

#include <asm/uaccess.h>

/*
 * SLAB caches for signal bits.
 */

#define DEBUG_SIG 0

#if DEBUG_SIG
#define SIG_SLAB_DEBUG	(SLAB_DEBUG_FREE | SLAB_RED_ZONE /* | SLAB_POISON */)
#else
#define SIG_SLAB_DEBUG	0
#endif

static kmem_cache_t *sigqueue_cachep;

atomic_t nr_queued_signals;
int max_queued_signals = 1024;

void __init signals_init(void)
{
	sigqueue_cachep =
		kmem_cache_create("sigqueue",
				  sizeof(struct sigqueue),
				  __alignof__(struct sigqueue),
				  SIG_SLAB_DEBUG, NULL, NULL);
	if (!sigqueue_cachep)
		panic("signals_init(): cannot create sigqueue SLAB cache");
}


/* Given the mask, find the first available signal that should be serviced. */

static int
next_signal(struct task_struct *tsk, sigset_t *mask)
{
	unsigned long i, *s, *m, x;
	int sig = 0;
	
	s = tsk->pending.signal.sig;
	m = mask->sig;
	switch (_NSIG_WORDS) {
	default:
		for (i = 0; i < _NSIG_WORDS; ++i, ++s, ++m)
			if ((x = *s &~ *m) != 0) {
				sig = ffz(~x) + i*_NSIG_BPW + 1;
				break;
			}
		break;

	case 2: if ((x = s[0] &~ m[0]) != 0)
			sig = 1;
		else if ((x = s[1] &~ m[1]) != 0)
			sig = _NSIG_BPW + 1;
		else
			break;
		sig += ffz(~x);
		break;

	case 1: if ((x = *s &~ *m) != 0)
			sig = ffz(~x) + 1;
		break;
	}
	
	return sig;
}

static void flush_sigqueue(struct sigpending *queue)
{
	struct sigqueue *q, *n;

	sigemptyset(&queue->signal);
	q = queue->head;
	queue->head = NULL;
	queue->tail = &queue->head;

	while (q) {
		n = q->next;
		kmem_cache_free(sigqueue_cachep, q);
		atomic_dec(&nr_queued_signals);
		q = n;
	}
}

/*
 * Flush all pending signals for a task.
 */

void
flush_signals(struct task_struct *t)
{
	t->sigpending = 0;
	flush_sigqueue(&t->pending);
}

void exit_sighand(struct task_struct *tsk)
{
	struct signal_struct * sig = tsk->sig;

	spin_lock_irq(&tsk->sigmask_lock);
	if (sig) {
		tsk->sig = NULL;
		if (atomic_dec_and_test(&sig->count))
			kmem_cache_free(sigact_cachep, sig);
	}
	tsk->sigpending = 0;
	flush_sigqueue(&tsk->pending);
	spin_unlock_irq(&tsk->sigmask_lock);
}

/*
 * Flush all handlers for a task.
 */

void
flush_signal_handlers(struct task_struct *t)
{
	int i;
	struct k_sigaction *ka = &t->sig->action[0];
	for (i = _NSIG ; i != 0 ; i--) {
		if (ka->sa.sa_handler != SIG_IGN)
			ka->sa.sa_handler = SIG_DFL;
		ka->sa.sa_flags = 0;
		sigemptyset(&ka->sa.sa_mask);
		ka++;
	}
}

/*
 * sig_exit - cause the current task to exit due to a signal.
 */

void
sig_exit(int sig, int exit_code, struct siginfo *info)
{
	struct task_struct *t;

	sigaddset(&current->pending.signal, sig);
	recalc_sigpending(current);
	current->flags |= PF_SIGNALED;

	/* Propagate the signal to all the tasks in
	 *  our thread group
	 */
	if (info && (unsigned long)info != 1
	    && info->si_code != SI_TKILL) {
		read_lock(&tasklist_lock);
		for_each_thread(t) {
			force_sig_info(sig, info, t);
		}
		read_unlock(&tasklist_lock);
	}

	do_exit(exit_code);
	/* NOTREACHED */
}

/* Notify the system that a driver wants to block all signals for this
 * process, and wants to be notified if any signals at all were to be
 * sent/acted upon.  If the notifier routine returns non-zero, then the
 * signal will be acted upon after all.  If the notifier routine returns 0,
 * then then signal will be blocked.  Only one block per process is
 * allowed.  priv is a pointer to private data that the notifier routine
 * can use to determine if the signal should be blocked or not.  */

void
block_all_signals(int (*notifier)(void *priv), void *priv, sigset_t *mask)
{
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	current->notifier_mask = mask;
	current->notifier_data = priv;
	current->notifier = notifier;
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
}

/* Notify the system that blocking has ended. */

void
unblock_all_signals(void)
{
	unsigned long flags;

	spin_lock_irqsave(&current->sigmask_lock, flags);
	current->notifier = NULL;
	current->notifier_data = NULL;
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
}

static int collect_signal(int sig, struct sigpending *list, siginfo_t *info)
{
	if (sigismember(&list->signal, sig)) {
		/* Collect the siginfo appropriate to this signal.  */
		struct sigqueue *q, **pp;
		pp = &list->head;
		while ((q = *pp) != NULL) {
			if (q->info.si_signo == sig)
				goto found_it;
			pp = &q->next;
		}

		/* Ok, it wasn't in the queue.  We must have
		   been out of queue space.  So zero out the
		   info.  */
		sigdelset(&list->signal, sig);
		info->si_signo = sig;
		info->si_errno = 0;
		info->si_code = 0;
		info->si_pid = 0;
		info->si_uid = 0;
		return 1;

found_it:
		if ((*pp = q->next) == NULL)
			list->tail = pp;

		/* Copy the sigqueue information and free the queue entry */
		copy_siginfo(info, &q->info);
		kmem_cache_free(sigqueue_cachep,q);
		atomic_dec(&nr_queued_signals);

		/* Non-RT signals can exist multiple times.. */
		if (sig >= SIGRTMIN) {
			while ((q = *pp) != NULL) {
				if (q->info.si_signo == sig)
					goto found_another;
				pp = &q->next;
			}
		}

		sigdelset(&list->signal, sig);
found_another:
		return 1;
	}
	return 0;
}

/*
 * Dequeue a signal and return the element to the caller, which is 
 * expected to free it.
 *
 * All callers must be holding current->sigmask_lock.
 */

int
dequeue_signal(sigset_t *mask, siginfo_t *info)
{
	int sig = 0;

#if DEBUG_SIG
printk("SIG dequeue (%s:%d): %d ", current->comm, current->pid,
	signal_pending(current));
#endif

	sig = next_signal(current, mask);
	if (sig) {
		if (current->notifier) {
			if (sigismember(current->notifier_mask, sig)) {
				if (!(current->notifier)(current->notifier_data)) {
					current->sigpending = 0;
					return 0;
				}
			}
		}

		if (!collect_signal(sig, &current->pending, info))
			sig = 0;
				
		/* XXX: Once POSIX.1b timers are in, if si_code == SI_TIMER,
		   we need to xchg out the timer overrun values.  */
	}
	recalc_sigpending(current);

#if DEBUG_SIG
printk(" %d -> %d\n", signal_pending(current), sig);
#endif

	return sig;
}

static int rm_from_queue(int sig, struct sigpending *s)
{
	struct sigqueue *q, **pp;

	if (!sigismember(&s->signal, sig))
		return 0;

	sigdelset(&s->signal, sig);

	pp = &s->head;

	while ((q = *pp) != NULL) {
		if (q->info.si_signo == sig) {
			if ((*pp = q->next) == NULL)
				s->tail = pp;
			kmem_cache_free(sigqueue_cachep,q);
			atomic_dec(&nr_queued_signals);
			continue;
		}
		pp = &q->next;
	}
	return 1;
}

/*
 * Remove signal sig from t->pending.
 * Returns 1 if sig was found.
 *
 * All callers must be holding t->sigmask_lock.
 */
static int rm_sig_from_queue(int sig, struct task_struct *t)
{
	return rm_from_queue(sig, &t->pending);
}

/*
 * Bad permissions for sending the signal
 */
int bad_signal(int sig, struct siginfo *info, struct task_struct *t)
{
	return (!info || ((unsigned long)info != 1 && SI_FROMUSER(info)))
	    && ((sig != SIGCONT) || (current->session != t->session))
	    && (current->euid ^ t->suid) && (current->euid ^ t->uid)
	    && (current->uid ^ t->suid) && (current->uid ^ t->uid)
	    && !capable(CAP_KILL);
}

/*
 * Signal type:
 *    < 0 : global action (kill - spread to all non-blocked threads)
 *    = 0 : ignored
 *    > 0 : wake up.
 */
static int signal_type(int sig, struct signal_struct *signals)
{
	unsigned long handler;

	if (!signals)
		return 0;
	
	handler = (unsigned long) signals->action[sig-1].sa.sa_handler;
	if (handler > 1)
		return 1;

	/* "Ignore" handler.. Illogical, but that has an implicit handler for SIGCHLD */
	if (handler == 1)
		return sig == SIGCHLD;

	/* Default handler. Normally lethal, but.. */
	switch (sig) {

	/* Ignored */
	case SIGCONT: case SIGWINCH:
	case SIGCHLD: case SIGURG:
		return 0;

	/* Implicit behaviour */
	case SIGTSTP: case SIGTTIN: case SIGTTOU:
		return 1;

	/* Implicit actions (kill or do special stuff) */
	default:
		return -1;
	}
}
		

/*
 * Determine whether a signal should be posted or not.
 *
 * Signals with SIG_IGN can be ignored, except for the
 * special case of a SIGCHLD. 
 *
 * Some signals with SIG_DFL default to a non-action.
 */
static int ignored_signal(int sig, struct task_struct *t)
{
	/* Don't ignore traced or blocked signals */
	if ((t->ptrace & PT_PTRACED) || sigismember(&t->blocked, sig))
		return 0;

	return signal_type(sig, t->sig) == 0;
}

/*
 * Handle TASK_STOPPED cases etc implicit behaviour
 * of certain magical signals.
 *
 * SIGKILL gets spread out to every thread. 
 */
static void handle_stop_signal(int sig, struct task_struct *t)
{
	switch (sig) {
	case SIGKILL: case SIGCONT:
		/* Wake up the process if stopped.  */
		if (t->state == TASK_STOPPED)
			wake_up_process(t);
		t->exit_code = 0;
		rm_sig_from_queue(SIGSTOP, t);
		rm_sig_from_queue(SIGTSTP, t);
		rm_sig_from_queue(SIGTTOU, t);
		rm_sig_from_queue(SIGTTIN, t);
		break;

	case SIGSTOP: case SIGTSTP:
	case SIGTTIN: case SIGTTOU:
		/* If we're stopping again, cancel SIGCONT */
		rm_sig_from_queue(SIGCONT, t);
		break;
	}
}

static int send_signal(int sig, struct siginfo *info, struct sigpending *signals)
{
	struct sigqueue * q = NULL;

	/* Real-time signals must be queued if sent by sigqueue, or
	   some other real-time mechanism.  It is implementation
	   defined whether kill() does so.  We attempt to do so, on
	   the principle of least surprise, but since kill is not
	   allowed to fail with EAGAIN when low on memory we just
	   make sure at least one signal gets delivered and don't
	   pass on the info struct.  */

	if (atomic_read(&nr_queued_signals) < max_queued_signals) {
		q = kmem_cache_alloc(sigqueue_cachep, GFP_ATOMIC);
	}

	if (q) {
		atomic_inc(&nr_queued_signals);
		q->next = NULL;
		*signals->tail = q;
		signals->tail = &q->next;
		switch ((unsigned long) info) {
			case 0:
				q->info.si_signo = sig;
				q->info.si_errno = 0;
				q->info.si_code = SI_USER;
				q->info.si_pid = current->pid;
				q->info.si_uid = current->uid;
				break;
			case 1:
				q->info.si_signo = sig;
				q->info.si_errno = 0;
				q->info.si_code = SI_KERNEL;
				q->info.si_pid = 0;
				q->info.si_uid = 0;
				break;
			default:
				copy_siginfo(&q->info, info);
				break;
		}
	} else if (sig >= SIGRTMIN && info && (unsigned long)info != 1
		   && info->si_code != SI_USER) {
		/*
		 * Queue overflow, abort.  We may abort if the signal was rt
		 * and sent by user using something other than kill().
		 */
		return -EAGAIN;
	}

	sigaddset(&signals->signal, sig);
	return 0;
}

/*
 * Tell a process that it has a new active signal..
 *
 * NOTE! we rely on the previous spin_lock to
 * lock interrupts for us! We can only be called with
 * "sigmask_lock" held, and the local interrupt must
 * have been disabled when that got acquired!
 *
 * No need to set need_resched since signal event passing
 * goes through ->blocked
 */
static inline void signal_wake_up(struct task_struct *t)
{
	t->sigpending = 1;

#ifdef CONFIG_SMP
	/*
	 * If the task is running on a different CPU 
	 * force a reschedule on the other CPU to make
	 * it notice the new signal quickly.
	 *
	 * The code below is a tad loose and might occasionally
	 * kick the wrong CPU if we catch the process in the
	 * process of changing - but no harm is done by that
	 * other than doing an extra (lightweight) IPI interrupt.
	 */
	spin_lock(&runqueue_lock);
	if (task_has_cpu(t) && t->processor != smp_processor_id())
		smp_send_reschedule(t->processor);
	spin_unlock(&runqueue_lock);
#endif /* CONFIG_SMP */

	if (t->state & TASK_INTERRUPTIBLE) {
		wake_up_process(t);
		return;
	}
}

static int deliver_signal(int sig, struct siginfo *info, struct task_struct *t)
{
	int retval = send_signal(sig, info, &t->pending);

	if (!retval && !sigismember(&t->blocked, sig))
		signal_wake_up(t);

	return retval;
}

int
send_sig_info(int sig, struct siginfo *info, struct task_struct *t)
{
	unsigned long flags;
	int ret;


#if DEBUG_SIG
printk("SIG queue (%s:%d): %d ", t->comm, t->pid, sig);
#endif

	ret = -EINVAL;
	if (sig < 0 || sig > _NSIG)
		goto out_nolock;
	/* The somewhat baroque permissions check... */
	ret = -EPERM;
	if (bad_signal(sig, info, t))
		goto out_nolock;

	/* The null signal is a permissions and process existence probe.
	   No signal is actually delivered.  Same goes for zombies. */
	ret = 0;
	if (!sig || !t->sig)
		goto out_nolock;

	spin_lock_irqsave(&t->sigmask_lock, flags);
	handle_stop_signal(sig, t);

	/* Optimize away the signal, if it's a signal that can be
	   handled immediately (ie non-blocked and untraced) and
	   that is ignored (either explicitly or by default).  */

	if (ignored_signal(sig, t))
		goto out;

	/* Support queueing exactly one non-rt signal, so that we
	   can get more detailed information about the cause of
	   the signal. */
	if (sig < SIGRTMIN && sigismember(&t->pending.signal, sig))
		goto out;

	ret = deliver_signal(sig, info, t);
out:
	spin_unlock_irqrestore(&t->sigmask_lock, flags);
out_nolock:
#if DEBUG_SIG
printk(" %d -> %d\n", signal_pending(t), ret);
#endif

	return ret;
}

/*
 * Force a signal that the process can't ignore: if necessary
 * we unblock the signal and change any SIG_IGN to SIG_DFL.
 */

int
force_sig_info(int sig, struct siginfo *info, struct task_struct *t)
{
	unsigned long int flags;

	spin_lock_irqsave(&t->sigmask_lock, flags);
	if (t->sig == NULL) {
		spin_unlock_irqrestore(&t->sigmask_lock, flags);
		return -ESRCH;
	}

	if (t->sig->action[sig-1].sa.sa_handler == SIG_IGN)
		t->sig->action[sig-1].sa.sa_handler = SIG_DFL;
	sigdelset(&t->blocked, sig);
	recalc_sigpending(t);
	spin_unlock_irqrestore(&t->sigmask_lock, flags);

	return send_sig_info(sig, info, t);
}

/*
 * kill_pg_info() sends a signal to a process group: this is what the tty
 * control characters do (^C, ^Z etc)
 */

int
kill_pg_info(int sig, struct siginfo *info, pid_t pgrp)
{
	int retval = -EINVAL;
	if (pgrp > 0) {
		struct task_struct *p;

		retval = -ESRCH;
		read_lock(&tasklist_lock);
		for_each_task(p) {
			if (p->pgrp == pgrp && thread_group_leader(p)) {
				int err = send_sig_info(sig, info, p);
				if (retval)
					retval = err;
			}
		}
		read_unlock(&tasklist_lock);
	}
	return retval;
}

/*
 * kill_sl_info() sends a signal to the session leader: this is used
 * to send SIGHUP to the controlling process of a terminal when
 * the connection is lost.
 */

int
kill_sl_info(int sig, struct siginfo *info, pid_t sess)
{
	int retval = -EINVAL;
	if (sess > 0) {
		struct task_struct *p;

		retval = -ESRCH;
		read_lock(&tasklist_lock);
		for_each_task(p) {
			if (p->leader && p->session == sess) {
				int err = send_sig_info(sig, info, p);
				if (retval)
					retval = err;
			}
		}
		read_unlock(&tasklist_lock);
	}
	return retval;
}

inline int
kill_proc_info(int sig, struct siginfo *info, pid_t pid)
{
	int error;
	struct task_struct *p;

	read_lock(&tasklist_lock);
	p = find_task_by_pid(pid);
	error = -ESRCH;
	if (p) {
		if (!thread_group_leader(p)) {
                       struct task_struct *tg;
                       tg = find_task_by_pid(p->tgid);
                       if (tg)
                               p = tg;
                }
		error = send_sig_info(sig, info, p);
	}
	read_unlock(&tasklist_lock);
	return error;
}


/*
 * kill_something_info() interprets pid in interesting ways just like kill(2).
 *
 * POSIX specifies that kill(-1,sig) is unspecified, but what we have
 * is probably wrong.  Should make it like BSD or SYSV.
 */

static int kill_something_info(int sig, struct siginfo *info, int pid)
{
	if (!pid) {
		return kill_pg_info(sig, info, current->pgrp);
	} else if (pid == -1) {
		int retval = 0, count = 0;
		struct task_struct * p;

		read_lock(&tasklist_lock);
		for_each_task(p) {
			if (p->pid > 1 && p != current && thread_group_leader(p)) {
				int err = send_sig_info(sig, info, p);
				++count;
				if (err != -EPERM)
					retval = err;
			}
		}
		read_unlock(&tasklist_lock);
		return count ? retval : -ESRCH;
	} else if (pid < 0) {
		return kill_pg_info(sig, info, -pid);
	} else {
		return kill_proc_info(sig, info, pid);
	}
}

/*
 * These are for backward compatibility with the rest of the kernel source.
 */

int
send_sig(int sig, struct task_struct *p, int priv)
{
	return send_sig_info(sig, (void*)(long)(priv != 0), p);
}

void
force_sig(int sig, struct task_struct *p)
{
	force_sig_info(sig, (void*)1L, p);
}

int
kill_pg(pid_t pgrp, int sig, int priv)
{
	return kill_pg_info(sig, (void *)(long)(priv != 0), pgrp);
}

int
kill_sl(pid_t sess, int sig, int priv)
{
	return kill_sl_info(sig, (void *)(long)(priv != 0), sess);
}

int
kill_proc(pid_t pid, int sig, int priv)
{
	return kill_proc_info(sig, (void *)(long)(priv != 0), pid);
}

/*
 * Joy. Or not. Pthread wants us to wake up every thread
 * in our parent group.
 */
static void wake_up_parent(struct task_struct *parent)
{
	struct task_struct *tsk = parent;

	do {
		wake_up_interruptible(&tsk->wait_chldexit);
		tsk = next_thread(tsk);
	} while (tsk != parent);
}

/*
 * Let a parent know about a status change of a child.
 */

void do_notify_parent(struct task_struct *tsk, int sig)
{
	struct siginfo info;
	int why, status;

	info.si_signo = sig;
	info.si_errno = 0;
	info.si_pid = tsk->pid;
	info.si_uid = tsk->uid;

	/* FIXME: find out whether or not this is supposed to be c*time. */
	info.si_utime = tsk->times.tms_utime;
	info.si_stime = tsk->times.tms_stime;

	status = tsk->exit_code & 0x7f;
	why = SI_KERNEL;	/* shouldn't happen */
	switch (tsk->state) {
	case TASK_STOPPED:
		/* FIXME -- can we deduce CLD_TRAPPED or CLD_CONTINUED? */
		if (tsk->ptrace & PT_PTRACED)
			why = CLD_TRAPPED;
		else
			why = CLD_STOPPED;
		break;

	default:
		if (tsk->exit_code & 0x80)
			why = CLD_DUMPED;
		else if (tsk->exit_code & 0x7f)
			why = CLD_KILLED;
		else {
			why = CLD_EXITED;
			status = tsk->exit_code >> 8;
		}
		break;
	}
	info.si_code = why;
	info.si_status = status;

	send_sig_info(sig, &info, tsk->p_pptr);
	wake_up_parent(tsk->p_pptr);
}


/*
 * We need the tasklist lock because it's the only
 * thing that protects out "parent" pointer.
 *
 * exit.c calls "do_notify_parent()" directly, because
 * it already has the tasklist lock.
 */
void
notify_parent(struct task_struct *tsk, int sig)
{
	read_lock(&tasklist_lock);
	do_notify_parent(tsk, sig);
	read_unlock(&tasklist_lock);
}

EXPORT_SYMBOL(dequeue_signal);
EXPORT_SYMBOL(flush_signals);
EXPORT_SYMBOL(force_sig);
EXPORT_SYMBOL(force_sig_info);
EXPORT_SYMBOL(kill_pg);
EXPORT_SYMBOL(kill_pg_info);
EXPORT_SYMBOL(kill_proc);
EXPORT_SYMBOL(kill_proc_info);
EXPORT_SYMBOL(kill_sl);
EXPORT_SYMBOL(kill_sl_info);
EXPORT_SYMBOL(notify_parent);
EXPORT_SYMBOL(recalc_sigpending);
EXPORT_SYMBOL(send_sig);
EXPORT_SYMBOL(send_sig_info);
EXPORT_SYMBOL(block_all_signals);
EXPORT_SYMBOL(unblock_all_signals);


/*
 * System call entry points.
 */

/*
 * We don't need to get the kernel lock - this is all local to this
 * particular thread.. (and that's good, because this is _heavily_
 * used by various programs)
 */

asmlinkage long
sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset, size_t sigsetsize)
{
	int error = -EINVAL;
	sigset_t old_set, new_set;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		goto out;

	if (set) {
		error = -EFAULT;
		if (copy_from_user(&new_set, set, sizeof(*set)))
			goto out;
		sigdelsetmask(&new_set, sigmask(SIGKILL)|sigmask(SIGSTOP));

		spin_lock_irq(&current->sigmask_lock);
		old_set = current->blocked;

		error = 0;
		switch (how) {
		default:
			error = -EINVAL;
			break;
		case SIG_BLOCK:
			sigorsets(&current->blocked, &old_set, &new_set);
			break;
		case SIG_UNBLOCK:
			signandsets(&current->blocked, &old_set, &new_set);
			break;
		case SIG_SETMASK:
			current->blocked = new_set;
			break;
		}

		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
		if (error)
			goto out;
		if (oset)
			goto set_old;
	} else if (oset) {
		spin_lock_irq(&current->sigmask_lock);
		old_set = current->blocked;
		spin_unlock_irq(&current->sigmask_lock);

	set_old:
		error = -EFAULT;
		if (copy_to_user(oset, &old_set, sizeof(*oset)))
			goto out;
	}
	error = 0;
out:
	return error;
}

long do_sigpending(void *set, unsigned long sigsetsize)
{
	long error = -EINVAL;
	sigset_t pending;

	if (sigsetsize > sizeof(sigset_t))
		goto out;

	spin_lock_irq(&current->sigmask_lock);
	sigandsets(&pending, &current->blocked, &current->pending.signal);
	spin_unlock_irq(&current->sigmask_lock);

	error = -EFAULT;
	if (!copy_to_user(set, &pending, sigsetsize))
		error = 0;
out:
	return error;
}	

asmlinkage long
sys_rt_sigpending(sigset_t *set, size_t sigsetsize)
{
	return do_sigpending(set, sigsetsize);
}

asmlinkage long
sys_rt_sigtimedwait(const sigset_t *uthese, siginfo_t *uinfo,
		    const struct timespec *uts, size_t sigsetsize)
{
	int ret, sig;
	sigset_t these;
	struct timespec ts;
	siginfo_t info;
	long timeout = 0;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&these, uthese, sizeof(these)))
		return -EFAULT;
		
	/*
	 * Invert the set of allowed signals to get those we
	 * want to block.
	 */
	sigdelsetmask(&these, sigmask(SIGKILL)|sigmask(SIGSTOP));
	signotset(&these);

	if (uts) {
		if (copy_from_user(&ts, uts, sizeof(ts)))
			return -EFAULT;
		if (ts.tv_nsec >= 1000000000L || ts.tv_nsec < 0
		    || ts.tv_sec < 0)
			return -EINVAL;
	}

	spin_lock_irq(&current->sigmask_lock);
	sig = dequeue_signal(&these, &info);
	if (!sig) {
		timeout = MAX_SCHEDULE_TIMEOUT;
		if (uts)
			timeout = (timespec_to_jiffies(&ts)
				   + (ts.tv_sec || ts.tv_nsec));

		if (timeout) {
			/* None ready -- temporarily unblock those we're
			 * interested while we are sleeping in so that we'll
			 * be awakened when they arrive.  */
			sigset_t oldblocked = current->blocked;
			sigandsets(&current->blocked, &current->blocked, &these);
			recalc_sigpending(current);
			spin_unlock_irq(&current->sigmask_lock);

			current->state = TASK_INTERRUPTIBLE;
			timeout = schedule_timeout(timeout);

			spin_lock_irq(&current->sigmask_lock);
			sig = dequeue_signal(&these, &info);
			current->blocked = oldblocked;
			recalc_sigpending(current);
		}
	}
	spin_unlock_irq(&current->sigmask_lock);

	if (sig) {
		ret = sig;
		if (uinfo) {
			if (copy_siginfo_to_user(uinfo, &info))
				ret = -EFAULT;
		}
	} else {
		ret = -EAGAIN;
		if (timeout)
			ret = -EINTR;
	}

	return ret;
}

asmlinkage long
sys_kill(int pid, int sig)
{
	struct siginfo info;

	info.si_signo = sig;
	info.si_errno = 0;
	info.si_code = SI_USER;
	info.si_pid = current->pid;
	info.si_uid = current->uid;

	return kill_something_info(sig, &info, pid);
}

/*
 *  Kill only one task, even if it's a CLONE_THREAD task.
 */
asmlinkage long
sys_tkill(int pid, int sig)
{
       struct siginfo info;
       int error;
       struct task_struct *p;

       /* This is only valid for single tasks */
       if (pid <= 0)
           return -EINVAL;

       info.si_signo = sig;
       info.si_errno = 0;
       info.si_code = SI_TKILL;
       info.si_pid = current->pid;
       info.si_uid = current->uid;

       read_lock(&tasklist_lock);
       p = find_task_by_pid(pid);
       error = -ESRCH;
       if (p) {
               error = send_sig_info(sig, &info, p);
       }
       read_unlock(&tasklist_lock);
       return error;
}

asmlinkage long
sys_rt_sigqueueinfo(int pid, int sig, siginfo_t *uinfo)
{
	siginfo_t info;

	if (copy_from_user(&info, uinfo, sizeof(siginfo_t)))
		return -EFAULT;

	/* Not even root can pretend to send signals from the kernel.
	   Nor can they impersonate a kill(), which adds source info.  */
	if (info.si_code >= 0)
		return -EPERM;
	info.si_signo = sig;

	/* POSIX.1b doesn't mention process groups.  */
	return kill_proc_info(sig, &info, pid);
}

int
do_sigaction(int sig, const struct k_sigaction *act, struct k_sigaction *oact)
{
	struct k_sigaction *k;

	if (sig < 1 || sig > _NSIG ||
	    (act && (sig == SIGKILL || sig == SIGSTOP)))
		return -EINVAL;

	k = &current->sig->action[sig-1];

	spin_lock(&current->sig->siglock);

	if (oact)
		*oact = *k;

	if (act) {
		*k = *act;
		sigdelsetmask(&k->sa.sa_mask, sigmask(SIGKILL) | sigmask(SIGSTOP));

		/*
		 * POSIX 3.3.1.3:
		 *  "Setting a signal action to SIG_IGN for a signal that is
		 *   pending shall cause the pending signal to be discarded,
		 *   whether or not it is blocked."
		 *
		 *  "Setting a signal action to SIG_DFL for a signal that is
		 *   pending and whose default action is to ignore the signal
		 *   (for example, SIGCHLD), shall cause the pending signal to
		 *   be discarded, whether or not it is blocked"
		 *
		 * Note the silly behaviour of SIGCHLD: SIG_IGN means that the
		 * signal isn't actually ignored, but does automatic child
		 * reaping, while SIG_DFL is explicitly said by POSIX to force
		 * the signal to be ignored.
		 */

		if (k->sa.sa_handler == SIG_IGN
		    || (k->sa.sa_handler == SIG_DFL
			&& (sig == SIGCONT ||
			    sig == SIGCHLD ||
			    sig == SIGURG ||
			    sig == SIGWINCH))) {
			spin_lock_irq(&current->sigmask_lock);
			if (rm_sig_from_queue(sig, current))
				recalc_sigpending(current);
			spin_unlock_irq(&current->sigmask_lock);
		}
	}

	spin_unlock(&current->sig->siglock);
	return 0;
}

int 
do_sigaltstack (const stack_t *uss, stack_t *uoss, unsigned long sp)
{
	stack_t oss;
	int error;

	if (uoss) {
		oss.ss_sp = (void *) current->sas_ss_sp;
		oss.ss_size = current->sas_ss_size;
		oss.ss_flags = sas_ss_flags(sp);
	}

	if (uss) {
		void *ss_sp;
		size_t ss_size;
		int ss_flags;

		error = -EFAULT;
		if (verify_area(VERIFY_READ, uss, sizeof(*uss))
		    || __get_user(ss_sp, &uss->ss_sp)
		    || __get_user(ss_flags, &uss->ss_flags)
		    || __get_user(ss_size, &uss->ss_size))
			goto out;

		error = -EPERM;
		if (on_sig_stack (sp))
			goto out;

		error = -EINVAL;
		/*
		 *
		 * Note - this code used to test ss_flags incorrectly
		 *  	  old code may have been written using ss_flags==0
		 *	  to mean ss_flags==SS_ONSTACK (as this was the only
		 *	  way that worked) - this fix preserves that older
		 *	  mechanism
		 */
		if (ss_flags != SS_DISABLE && ss_flags != SS_ONSTACK && ss_flags != 0)
			goto out;

		if (ss_flags == SS_DISABLE) {
			ss_size = 0;
			ss_sp = NULL;
		} else {
			error = -ENOMEM;
			if (ss_size < MINSIGSTKSZ)
				goto out;
		}

		current->sas_ss_sp = (unsigned long) ss_sp;
		current->sas_ss_size = ss_size;
	}

	if (uoss) {
		error = -EFAULT;
		if (copy_to_user(uoss, &oss, sizeof(oss)))
			goto out;
	}

	error = 0;
out:
	return error;
}

asmlinkage long
sys_sigpending(old_sigset_t *set)
{
	return do_sigpending(set, sizeof(*set));
}

#if !defined(__alpha__)
/* Alpha has its own versions with special arguments.  */

asmlinkage long
sys_sigprocmask(int how, old_sigset_t *set, old_sigset_t *oset)
{
	int error;
	old_sigset_t old_set, new_set;

	if (set) {
		error = -EFAULT;
		if (copy_from_user(&new_set, set, sizeof(*set)))
			goto out;
		new_set &= ~(sigmask(SIGKILL)|sigmask(SIGSTOP));

		spin_lock_irq(&current->sigmask_lock);
		old_set = current->blocked.sig[0];

		error = 0;
		switch (how) {
		default:
			error = -EINVAL;
			break;
		case SIG_BLOCK:
			sigaddsetmask(&current->blocked, new_set);
			break;
		case SIG_UNBLOCK:
			sigdelsetmask(&current->blocked, new_set);
			break;
		case SIG_SETMASK:
			current->blocked.sig[0] = new_set;
			break;
		}

		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
		if (error)
			goto out;
		if (oset)
			goto set_old;
	} else if (oset) {
		old_set = current->blocked.sig[0];
	set_old:
		error = -EFAULT;
		if (copy_to_user(oset, &old_set, sizeof(*oset)))
			goto out;
	}
	error = 0;
out:
	return error;
}

#ifndef __sparc__
asmlinkage long
sys_rt_sigaction(int sig, const struct sigaction *act, struct sigaction *oact,
		 size_t sigsetsize)
{
	struct k_sigaction new_sa, old_sa;
	int ret = -EINVAL;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		goto out;

	if (act) {
		if (copy_from_user(&new_sa.sa, act, sizeof(new_sa.sa)))
			return -EFAULT;
	}

	ret = do_sigaction(sig, act ? &new_sa : NULL, oact ? &old_sa : NULL);

	if (!ret && oact) {
		if (copy_to_user(oact, &old_sa.sa, sizeof(old_sa.sa)))
			return -EFAULT;
	}
out:
	return ret;
}
#endif /* __sparc__ */
#endif

#if !defined(__alpha__) && !defined(__ia64__)
/*
 * For backwards compatibility.  Functionality superseded by sigprocmask.
 */
asmlinkage long
sys_sgetmask(void)
{
	/* SMP safe */
	return current->blocked.sig[0];
}

asmlinkage long
sys_ssetmask(int newmask)
{
	int old;

	spin_lock_irq(&current->sigmask_lock);
	old = current->blocked.sig[0];

	siginitset(&current->blocked, newmask & ~(sigmask(SIGKILL)|
						  sigmask(SIGSTOP)));
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	return old;
}
#endif /* !defined(__alpha__) */

#if !defined(__alpha__) && !defined(__ia64__) && !defined(__mips__)
/*
 * For backwards compatibility.  Functionality superseded by sigaction.
 */
asmlinkage unsigned long
sys_signal(int sig, __sighandler_t handler)
{
	struct k_sigaction new_sa, old_sa;
	int ret;

	new_sa.sa.sa_handler = handler;
	new_sa.sa.sa_flags = SA_ONESHOT | SA_NOMASK;

	ret = do_sigaction(sig, &new_sa, &old_sa);

	return ret ? ret : (unsigned long)old_sa.sa.sa_handler;
}
#endif /* !alpha && !__ia64__ && !defined(__mips__) */
