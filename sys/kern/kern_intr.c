/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>
#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/stdarg.h>
#ifdef DDB
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#endif

/*
 * Describe an interrupt thread.  There is one of these per interrupt event.
 */
struct intr_thread {
	struct intr_event *it_event;
	struct thread *it_thread;	/* Kernel thread. */
	int	it_flags;		/* (j) IT_* flags. */
	int	it_need;		/* Needs service. */
};

/* Interrupt thread flags kept in it_flags */
#define	IT_DEAD		0x000001	/* Thread is waiting to exit. */

struct	intr_entropy {
	struct	thread *td;
	uintptr_t event;
};

struct	intr_event *clk_intr_event;
struct	intr_event *tty_intr_event;
void	*softclock_ih;
void	*vm_ih;

static MALLOC_DEFINE(M_ITHREAD, "ithread", "Interrupt Threads");

static int intr_storm_threshold = 500;
TUNABLE_INT("hw.intr_storm_threshold", &intr_storm_threshold);
SYSCTL_INT(_hw, OID_AUTO, intr_storm_threshold, CTLFLAG_RW,
    &intr_storm_threshold, 0,
    "Number of consecutive interrupts before storm protection is enabled");
static TAILQ_HEAD(, intr_event) event_list =
    TAILQ_HEAD_INITIALIZER(event_list);

static void	intr_event_update(struct intr_event *ie);
static struct intr_thread *ithread_create(const char *name);
#ifdef notyet
static void	ithread_destroy(struct intr_thread *ithread);
#endif
static void	ithread_execute_handlers(struct proc *p, struct intr_event *ie);
static void	ithread_loop(void *);
static void	ithread_update(struct intr_thread *ithd);
static void	start_softintr(void *);

u_char
intr_priority(enum intr_type flags)
{
	u_char pri;

	flags &= (INTR_TYPE_TTY | INTR_TYPE_BIO | INTR_TYPE_NET |
	    INTR_TYPE_CAM | INTR_TYPE_MISC | INTR_TYPE_CLK | INTR_TYPE_AV);
	switch (flags) {
	case INTR_TYPE_TTY:
		pri = PI_TTYLOW;
		break;
	case INTR_TYPE_BIO:
		/*
		 * XXX We need to refine this.  BSD/OS distinguishes
		 * between tape and disk priorities.
		 */
		pri = PI_DISK;
		break;
	case INTR_TYPE_NET:
		pri = PI_NET;
		break;
	case INTR_TYPE_CAM:
		pri = PI_DISK;          /* XXX or PI_CAM? */
		break;
	case INTR_TYPE_AV:		/* Audio/video */
		pri = PI_AV;
		break;
	case INTR_TYPE_CLK:
		pri = PI_REALTIME;
		break;
	case INTR_TYPE_MISC:
		pri = PI_DULL;          /* don't care */
		break;
	default:
		/* We didn't specify an interrupt level. */
		panic("intr_priority: no interrupt type in flags");
	}

	return pri;
}

/*
 * Update an ithread based on the associated intr_event.
 */
static void
ithread_update(struct intr_thread *ithd)
{
	struct intr_event *ie;
	struct thread *td;
	u_char pri;

	ie = ithd->it_event;
	td = ithd->it_thread;

	/* Determine the overall priority of this event. */
	if (TAILQ_EMPTY(&ie->ie_handlers))
		pri = PRI_MAX_ITHD;
	else
		pri = TAILQ_FIRST(&ie->ie_handlers)->ih_pri;

	/* Update name and priority. */
	strlcpy(td->td_proc->p_comm, ie->ie_fullname,
	    sizeof(td->td_proc->p_comm));
	mtx_lock_spin(&sched_lock);
	sched_prio(td, pri);
	mtx_unlock_spin(&sched_lock);
}

/*
 * Regenerate the full name of an interrupt event and update its priority.
 */
static void
intr_event_update(struct intr_event *ie)
{
	struct intr_handler *ih;
	char *last;
	int missed, space;

	/* Start off with no entropy and just the name of the event. */
	mtx_assert(&ie->ie_lock, MA_OWNED);
	strlcpy(ie->ie_fullname, ie->ie_name, sizeof(ie->ie_fullname));
	ie->ie_flags &= ~IE_ENTROPY;
	missed = 0;
	space = 1;

	/* Run through all the handlers updating values. */
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (strlen(ie->ie_fullname) + strlen(ih->ih_name) + 1 <
		    sizeof(ie->ie_fullname)) {
			strcat(ie->ie_fullname, " ");
			strcat(ie->ie_fullname, ih->ih_name);
			space = 0;
		} else
			missed++;
		if (ih->ih_flags & IH_ENTROPY)
			ie->ie_flags |= IE_ENTROPY;
	}

	/*
	 * If the handler names were too long, add +'s to indicate missing
	 * names. If we run out of room and still have +'s to add, change
	 * the last character from a + to a *.
	 */
	last = &ie->ie_fullname[sizeof(ie->ie_fullname) - 2];
	while (missed-- > 0) {
		if (strlen(ie->ie_fullname) + 1 == sizeof(ie->ie_fullname)) {
			if (*last == '+') {
				*last = '*';
				break;
			} else
				*last = '+';
		} else if (space) {
			strcat(ie->ie_fullname, " +");
			space = 0;
		} else
			strcat(ie->ie_fullname, "+");
	}

	/*
	 * If this event has an ithread, update it's priority and
	 * name.
	 */
	if (ie->ie_thread != NULL)
		ithread_update(ie->ie_thread);
	CTR2(KTR_INTR, "%s: updated %s", __func__, ie->ie_fullname);
}

int
intr_event_create(struct intr_event **event, void *source, int flags,
    void (*enable)(void *), const char *fmt, ...)
{
	struct intr_event *ie;
	va_list ap;

	/* The only valid flag during creation is IE_SOFT. */
	if ((flags & ~IE_SOFT) != 0)
		return (EINVAL);
	ie = malloc(sizeof(struct intr_event), M_ITHREAD, M_WAITOK | M_ZERO);
	ie->ie_source = source;
	ie->ie_enable = enable;
	ie->ie_flags = flags;
	TAILQ_INIT(&ie->ie_handlers);
	mtx_init(&ie->ie_lock, "intr event", NULL, MTX_DEF);

	va_start(ap, fmt);
	vsnprintf(ie->ie_name, sizeof(ie->ie_name), fmt, ap);
	va_end(ap);
	strlcpy(ie->ie_fullname, ie->ie_name, sizeof(ie->ie_fullname));
	mtx_pool_lock(mtxpool_sleep, &event_list);
	TAILQ_INSERT_TAIL(&event_list, ie, ie_list);
	mtx_pool_unlock(mtxpool_sleep, &event_list);
	if (event != NULL)
		*event = ie;
	CTR2(KTR_INTR, "%s: created %s", __func__, ie->ie_name);
	return (0);
}

int
intr_event_destroy(struct intr_event *ie)
{

	mtx_lock(&ie->ie_lock);
	if (!TAILQ_EMPTY(&ie->ie_handlers)) {
		mtx_unlock(&ie->ie_lock);
		return (EBUSY);
	}
	mtx_pool_lock(mtxpool_sleep, &event_list);
	TAILQ_REMOVE(&event_list, ie, ie_list);
	mtx_pool_unlock(mtxpool_sleep, &event_list);
	mtx_unlock(&ie->ie_lock);
	mtx_destroy(&ie->ie_lock);
	free(ie, M_ITHREAD);
	return (0);
}

static struct intr_thread *
ithread_create(const char *name)
{
	struct intr_thread *ithd;
	struct thread *td;
	struct proc *p;
	int error;

	ithd = malloc(sizeof(struct intr_thread), M_ITHREAD, M_WAITOK | M_ZERO);

	error = kthread_create(ithread_loop, ithd, &p, RFSTOPPED | RFHIGHPID,
	    0, "%s", name);
	if (error)
		panic("kthread_create() failed with %d", error);
	td = FIRST_THREAD_IN_PROC(p);	/* XXXKSE */
	mtx_lock_spin(&sched_lock);
	td->td_ksegrp->kg_pri_class = PRI_ITHD;
	TD_SET_IWAIT(td);
	mtx_unlock_spin(&sched_lock);
	td->td_pflags |= TDP_ITHREAD;
	ithd->it_thread = td;
	CTR2(KTR_INTR, "%s: created %s", __func__, name);
	return (ithd);
}

#ifdef notyet
static void
ithread_destroy(struct intr_thread *ithread)
{
	struct thread *td;

	td = ithread->it_thread;
	mtx_lock_spin(&sched_lock);
	ithread->it_flags |= IT_DEAD;
	if (TD_AWAITING_INTR(td)) {
		TD_CLR_IWAIT(td);
		setrunqueue(td, SRQ_INTR);
	}
	mtx_unlock_spin(&sched_lock);
	CTR2(KTR_INTR, "%s: killing %s", __func__, ithread->it_name);
}
#endif

int
intr_event_add_handler(struct intr_event *ie, const char *name,
    driver_intr_t handler, void *arg, u_char pri, enum intr_type flags,
    void **cookiep)
{
	struct intr_handler *ih, *temp_ih;
	struct intr_thread *it;

	if (ie == NULL || name == NULL || handler == NULL)
		return (EINVAL);

	/* Allocate and populate an interrupt handler structure. */
	ih = malloc(sizeof(struct intr_handler), M_ITHREAD, M_WAITOK | M_ZERO);
	ih->ih_handler = handler;
	ih->ih_argument = arg;
	ih->ih_name = name;
	ih->ih_event = ie;
	ih->ih_pri = pri;
	if (flags & INTR_FAST)
		ih->ih_flags = IH_FAST;
	else if (flags & INTR_EXCL)
		ih->ih_flags = IH_EXCLUSIVE;
	if (flags & INTR_MPSAFE)
		ih->ih_flags |= IH_MPSAFE;
	if (flags & INTR_ENTROPY)
		ih->ih_flags |= IH_ENTROPY;

	/* We can only have one exclusive handler in a event. */
	mtx_lock(&ie->ie_lock);
	if (!TAILQ_EMPTY(&ie->ie_handlers)) {
		if ((flags & INTR_EXCL) ||
		    (TAILQ_FIRST(&ie->ie_handlers)->ih_flags & IH_EXCLUSIVE)) {
			mtx_unlock(&ie->ie_lock);
			free(ih, M_ITHREAD);
			return (EINVAL);
		}
	}

	/* Add the new handler to the event in priority order. */
	TAILQ_FOREACH(temp_ih, &ie->ie_handlers, ih_next) {
		if (temp_ih->ih_pri > ih->ih_pri)
			break;
	}
	if (temp_ih == NULL)
		TAILQ_INSERT_TAIL(&ie->ie_handlers, ih, ih_next);
	else
		TAILQ_INSERT_BEFORE(temp_ih, ih, ih_next);
	intr_event_update(ie);

	/* Create a thread if we need one. */
	while (ie->ie_thread == NULL && !(flags & INTR_FAST)) {
		if (ie->ie_flags & IE_ADDING_THREAD)
			msleep(ie, &ie->ie_lock, curthread->td_priority,
			    "ithread", 0);
		else {
			ie->ie_flags |= IE_ADDING_THREAD;
			mtx_unlock(&ie->ie_lock);
			it = ithread_create("intr: newborn");
			mtx_lock(&ie->ie_lock);
			ie->ie_flags &= ~IE_ADDING_THREAD;
			ie->ie_thread = it;
			it->it_event = ie;
			ithread_update(it);
			wakeup(ie);
		}
	}
	CTR3(KTR_INTR, "%s: added %s to %s", __func__, ih->ih_name,
	    ie->ie_name);
	mtx_unlock(&ie->ie_lock);

	if (cookiep != NULL)
		*cookiep = ih;
	return (0);
}

int
intr_event_remove_handler(void *cookie)
{
	struct intr_handler *handler = (struct intr_handler *)cookie;
	struct intr_event *ie;
#ifdef INVARIANTS
	struct intr_handler *ih;
#endif
#ifdef notyet
	int dead;
#endif

	if (handler == NULL)
		return (EINVAL);
	ie = handler->ih_event;
	KASSERT(ie != NULL,
	    ("interrupt handler \"%s\" has a NULL interrupt event",
		handler->ih_name));
	mtx_lock(&ie->ie_lock);
	CTR3(KTR_INTR, "%s: removing %s from %s", __func__, handler->ih_name,
	    ie->ie_name);
#ifdef INVARIANTS
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next)
		if (ih == handler)
			goto ok;
	mtx_unlock(&ie->ie_lock);
	panic("interrupt handler \"%s\" not found in interrupt event \"%s\"",
	    ih->ih_name, ie->ie_name);
ok:
#endif
	/*
	 * If there is no ithread, then just remove the handler and return.
	 * XXX: Note that an INTR_FAST handler might be running on another
	 * CPU!
	 */
	if (ie->ie_thread == NULL) {
		TAILQ_REMOVE(&ie->ie_handlers, handler, ih_next);
		mtx_unlock(&ie->ie_lock);
		free(handler, M_ITHREAD);
		return (0);
	}

	/*
	 * If the interrupt thread is already running, then just mark this
	 * handler as being dead and let the ithread do the actual removal.
	 *
	 * During a cold boot while cold is set, msleep() does not sleep,
	 * so we have to remove the handler here rather than letting the
	 * thread do it.
	 */
	mtx_lock_spin(&sched_lock);
	if (!TD_AWAITING_INTR(ie->ie_thread->it_thread) && !cold) {
		handler->ih_flags |= IH_DEAD;

		/*
		 * Ensure that the thread will process the handler list
		 * again and remove this handler if it has already passed
		 * it on the list.
		 */
		ie->ie_thread->it_need = 1;
	} else
		TAILQ_REMOVE(&ie->ie_handlers, handler, ih_next);
	mtx_unlock_spin(&sched_lock);
	while (handler->ih_flags & IH_DEAD)
		msleep(handler, &ie->ie_lock, curthread->td_priority, "iev_rmh",
		    0);
	intr_event_update(ie);
#ifdef notyet
	/*
	 * XXX: This could be bad in the case of ppbus(8).  Also, I think
	 * this could lead to races of stale data when servicing an
	 * interrupt.
	 */
	dead = 1;
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (!(ih->ih_flags & IH_FAST)) {
			dead = 0;
			break;
		}
	}
	if (dead) {
		ithread_destroy(ie->ie_thread);
		ie->ie_thread = NULL;
	}
#endif
	mtx_unlock(&ie->ie_lock);
	free(handler, M_ITHREAD);
	return (0);
}

int
intr_event_schedule_thread(struct intr_event *ie)
{
	struct intr_entropy entropy;
	struct intr_thread *it;
	struct thread *td;
	struct thread *ctd;
	struct proc *p;

	/*
	 * If no ithread or no handlers, then we have a stray interrupt.
	 */
	if (ie == NULL || TAILQ_EMPTY(&ie->ie_handlers) ||
	    ie->ie_thread == NULL)
		return (EINVAL);

	ctd = curthread;
	it = ie->ie_thread;
	td = it->it_thread;
	p = td->td_proc;

	/*
	 * If any of the handlers for this ithread claim to be good
	 * sources of entropy, then gather some.
	 */
	if (harvest.interrupt && ie->ie_flags & IE_ENTROPY) {
		CTR3(KTR_INTR, "%s: pid %d (%s) gathering entropy", __func__,
		    p->p_pid, p->p_comm);
		entropy.event = (uintptr_t)ie;
		entropy.td = ctd;
		random_harvest(&entropy, sizeof(entropy), 2, 0,
		    RANDOM_INTERRUPT);
	}

	KASSERT(p != NULL, ("ithread %s has no process", ie->ie_name));

	/*
	 * Set it_need to tell the thread to keep running if it is already
	 * running.  Then, grab sched_lock and see if we actually need to
	 * put this thread on the runqueue.
	 */
	it->it_need = 1;
	mtx_lock_spin(&sched_lock);
	if (TD_AWAITING_INTR(td)) {
		CTR3(KTR_INTR, "%s: schedule pid %d (%s)", __func__, p->p_pid,
		    p->p_comm);
		TD_CLR_IWAIT(td);
		setrunqueue(td, SRQ_INTR);
	} else {
		CTR5(KTR_INTR, "%s: pid %d (%s): it_need %d, state %d",
		    __func__, p->p_pid, p->p_comm, it->it_need, td->td_state);
	}
	mtx_unlock_spin(&sched_lock);

	return (0);
}

/*
 * Add a software interrupt handler to a specified event.  If a given event
 * is not specified, then a new event is created.
 */
int
swi_add(struct intr_event **eventp, const char *name, driver_intr_t handler,
	    void *arg, int pri, enum intr_type flags, void **cookiep)
{
	struct intr_event *ie;
	int error;

	if (flags & (INTR_FAST | INTR_ENTROPY))
		return (EINVAL);

	ie = (eventp != NULL) ? *eventp : NULL;

	if (ie != NULL) {
		if (!(ie->ie_flags & IE_SOFT))
			return (EINVAL);
	} else {
		error = intr_event_create(&ie, NULL, IE_SOFT, NULL,
		    "swi%d:", pri);
		if (error)
			return (error);
		if (eventp != NULL)
			*eventp = ie;
	}
	return (intr_event_add_handler(ie, name, handler, arg,
		    (pri * RQ_PPQ) + PI_SOFT, flags, cookiep));
		    /* XXKSE.. think of a better way to get separate queues */
}

/*
 * Schedule a software interrupt thread.
 */
void
swi_sched(void *cookie, int flags)
{
	struct intr_handler *ih = (struct intr_handler *)cookie;
	struct intr_event *ie = ih->ih_event;
	int error;

	PCPU_LAZY_INC(cnt.v_intr);

	CTR3(KTR_INTR, "swi_sched: %s %s need=%d", ie->ie_name, ih->ih_name,
	    ih->ih_need);

	/*
	 * Set ih_need for this handler so that if the ithread is already
	 * running it will execute this handler on the next pass.  Otherwise,
	 * it will execute it the next time it runs.
	 */
	atomic_store_rel_int(&ih->ih_need, 1);
	if (!(flags & SWI_DELAY)) {
		error = intr_event_schedule_thread(ie);
		KASSERT(error == 0, ("stray software interrupt"));
	}
}

/*
 * Remove a software interrupt handler.  Currently this code does not
 * remove the associated interrupt event if it becomes empty.  Calling code
 * may do so manually via intr_event_destroy(), but that's not really
 * an optimal interface.
 */
int
swi_remove(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

static void
ithread_execute_handlers(struct proc *p, struct intr_event *ie)
{
	struct intr_handler *ih, *ihn;

	/* Interrupt handlers should not sleep. */
	if (!(ie->ie_flags & IE_SOFT))
		THREAD_NO_SLEEPING();
	TAILQ_FOREACH_SAFE(ih, &ie->ie_handlers, ih_next, ihn) {

		/*
		 * If this handler is marked for death, remove it from
		 * the list of handlers and wake up the sleeper.
		 */
		if (ih->ih_flags & IH_DEAD) {
			mtx_lock(&ie->ie_lock);
			TAILQ_REMOVE(&ie->ie_handlers, ih, ih_next);
			ih->ih_flags &= ~IH_DEAD;
			wakeup(ih);
			mtx_unlock(&ie->ie_lock);
			continue;
		}

		/*
		 * For software interrupt threads, we only execute
		 * handlers that have their need flag set.  Hardware
		 * interrupt threads always invoke all of their handlers.
		 */
		if (ie->ie_flags & IE_SOFT) {
			if (!ih->ih_need)
				continue;
			else
				atomic_store_rel_int(&ih->ih_need, 0);
		}

		/* Fast handlers are handled in primary interrupt context. */
		if (ih->ih_flags & IH_FAST)
			continue;

		/* Execute this handler. */
		CTR6(KTR_INTR, "%s: pid %d exec %p(%p) for %s flg=%x",
		    __func__, p->p_pid, (void *)ih->ih_handler, ih->ih_argument,
		    ih->ih_name, ih->ih_flags);

		if (!(ih->ih_flags & IH_MPSAFE))
			mtx_lock(&Giant);
		ih->ih_handler(ih->ih_argument);
		if (!(ih->ih_flags & IH_MPSAFE))
			mtx_unlock(&Giant);
	}
	if (!(ie->ie_flags & IE_SOFT))
		THREAD_SLEEPING_OK();

	/*
	 * Interrupt storm handling:
	 *
	 * If this interrupt source is currently storming, then throttle
	 * it to only fire the handler once  per clock tick.
	 *
	 * If this interrupt source is not currently storming, but the
	 * number of back to back interrupts exceeds the storm threshold,
	 * then enter storming mode.
	 */
	if (intr_storm_threshold != 0 && ie->ie_count >= intr_storm_threshold) {
		if (ie->ie_warned == 0) {
			printf(
	"Interrupt storm detected on \"%s\"; throttling interrupt source\n",
			    ie->ie_name);
			ie->ie_warned = 1;
		}
		tsleep(&ie->ie_count, curthread->td_priority, "istorm", 1);
	} else
		ie->ie_count++;

	/*
	 * Now that all the handlers have had a chance to run, reenable
	 * the interrupt source.
	 */
	if (ie->ie_enable != NULL)
		ie->ie_enable(ie->ie_source);
}

/*
 * This is the main code for interrupt threads.
 */
static void
ithread_loop(void *arg)
{
	struct intr_thread *ithd;
	struct intr_event *ie;
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	ithd = (struct intr_thread *)arg;
	KASSERT(ithd->it_thread == td,
	    ("%s: ithread and proc linkage out of sync", __func__));
	ie = ithd->it_event;
	ie->ie_count = 0;

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		/*
		 * If we are an orphaned thread, then just die.
		 */
		if (ithd->it_flags & IT_DEAD) {
			CTR3(KTR_INTR, "%s: pid %d (%s) exiting", __func__,
			    p->p_pid, p->p_comm);
			free(ithd, M_ITHREAD);
			kthread_exit(0);
		}

		/*
		 * Service interrupts.  If another interrupt arrives while
		 * we are running, it will set it_need to note that we
		 * should make another pass.
		 */
		while (ithd->it_need) {
			/*
			 * This might need a full read and write barrier
			 * to make sure that this write posts before any
			 * of the memory or device accesses in the
			 * handlers.
			 */
			atomic_store_rel_int(&ithd->it_need, 0);
			ithread_execute_handlers(p, ie);
		}
		WITNESS_WARN(WARN_PANIC, NULL, "suspending ithread");
		mtx_assert(&Giant, MA_NOTOWNED);

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		mtx_lock_spin(&sched_lock);
		if (!ithd->it_need && !(ithd->it_flags & IT_DEAD)) {
			TD_SET_IWAIT(td);
			ie->ie_count = 0;
			mi_switch(SW_VOL, NULL);
		}
		mtx_unlock_spin(&sched_lock);
	}
}

#ifdef DDB
/*
 * Dump details about an interrupt handler
 */
static void
db_dump_intrhand(struct intr_handler *ih)
{
	int comma;

	db_printf("\t%-10s ", ih->ih_name);
	switch (ih->ih_pri) {
	case PI_REALTIME:
		db_printf("CLK ");
		break;
	case PI_AV:
		db_printf("AV  ");
		break;
	case PI_TTYHIGH:
	case PI_TTYLOW:
		db_printf("TTY ");
		break;
	case PI_TAPE:
		db_printf("TAPE");
		break;
	case PI_NET:
		db_printf("NET ");
		break;
	case PI_DISK:
	case PI_DISKLOW:
		db_printf("DISK");
		break;
	case PI_DULL:
		db_printf("DULL");
		break;
	default:
		if (ih->ih_pri >= PI_SOFT)
			db_printf("SWI ");
		else
			db_printf("%4u", ih->ih_pri);
		break;
	}
	db_printf(" ");
	db_printsym((uintptr_t)ih->ih_handler, DB_STGY_PROC);
	db_printf("(%p)", ih->ih_argument);
	if (ih->ih_need ||
	    (ih->ih_flags & (IH_FAST | IH_EXCLUSIVE | IH_ENTROPY | IH_DEAD |
	    IH_MPSAFE)) != 0) {
		db_printf(" {");
		comma = 0;
		if (ih->ih_flags & IH_FAST) {
			db_printf("FAST");
			comma = 1;
		}
		if (ih->ih_flags & IH_EXCLUSIVE) {
			if (comma)
				db_printf(", ");
			db_printf("EXCL");
			comma = 1;
		}
		if (ih->ih_flags & IH_ENTROPY) {
			if (comma)
				db_printf(", ");
			db_printf("ENTROPY");
			comma = 1;
		}
		if (ih->ih_flags & IH_DEAD) {
			if (comma)
				db_printf(", ");
			db_printf("DEAD");
			comma = 1;
		}
		if (ih->ih_flags & IH_MPSAFE) {
			if (comma)
				db_printf(", ");
			db_printf("MPSAFE");
			comma = 1;
		}
		if (ih->ih_need) {
			if (comma)
				db_printf(", ");
			db_printf("NEED");
		}
		db_printf("}");
	}
	db_printf("\n");
}

/*
 * Dump details about a event.
 */
void
db_dump_intr_event(struct intr_event *ie, int handlers)
{
	struct intr_handler *ih;
	struct intr_thread *it;
	int comma;

	db_printf("%s ", ie->ie_fullname);
	it = ie->ie_thread;
	if (it != NULL)
		db_printf("(pid %d)", it->it_thread->td_proc->p_pid);
	else
		db_printf("(no thread)");
	if ((ie->ie_flags & (IE_SOFT | IE_ENTROPY | IE_ADDING_THREAD)) != 0 ||
	    (it != NULL && it->it_need)) {
		db_printf(" {");
		comma = 0;
		if (ie->ie_flags & IE_SOFT) {
			db_printf("SOFT");
			comma = 1;
		}
		if (ie->ie_flags & IE_ENTROPY) {
			if (comma)
				db_printf(", ");
			db_printf("ENTROPY");
			comma = 1;
		}
		if (ie->ie_flags & IE_ADDING_THREAD) {
			if (comma)
				db_printf(", ");
			db_printf("ADDING_THREAD");
			comma = 1;
		}
		if (it != NULL && it->it_need) {
			if (comma)
				db_printf(", ");
			db_printf("NEED");
		}
		db_printf("}");
	}
	db_printf("\n");

	if (handlers)
		TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next)
		    db_dump_intrhand(ih);
}

/*
 * Dump data about interrupt handlers
 */
DB_SHOW_COMMAND(intr, db_show_intr)
{
	struct intr_event *ie;
	int quit, all, verbose;

	quit = 0;
	verbose = index(modif, 'v') != NULL;
	all = index(modif, 'a') != NULL;
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	TAILQ_FOREACH(ie, &event_list, ie_list) {
		if (!all && TAILQ_EMPTY(&ie->ie_handlers))
			continue;
		db_dump_intr_event(ie, verbose);
	}
}
#endif /* DDB */

/*
 * Start standard software interrupt threads
 */
static void
start_softintr(void *dummy)
{
	struct proc *p;

	if (swi_add(&clk_intr_event, "clock", softclock, NULL, SWI_CLOCK,
		INTR_MPSAFE, &softclock_ih) ||
	    swi_add(NULL, "vm", swi_vm, NULL, SWI_VM, INTR_MPSAFE, &vm_ih))
		panic("died while creating standard software ithreads");

	p = clk_intr_event->ie_thread->it_thread->td_proc;
	PROC_LOCK(p);
	p->p_flag |= P_NOLOAD;
	PROC_UNLOCK(p);
}
SYSINIT(start_softintr, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_softintr, NULL)

/*
 * Sysctls used by systat and others: hw.intrnames and hw.intrcnt.
 * The data for this machine dependent, and the declarations are in machine
 * dependent code.  The layout of intrnames and intrcnt however is machine
 * independent.
 *
 * We do not know the length of intrcnt and intrnames at compile time, so
 * calculate things at run time.
 */
static int
sysctl_intrnames(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_handle_opaque(oidp, intrnames, eintrnames - intrnames,
	   req));
}

SYSCTL_PROC(_hw, OID_AUTO, intrnames, CTLTYPE_OPAQUE | CTLFLAG_RD,
    NULL, 0, sysctl_intrnames, "", "Interrupt Names");

static int
sysctl_intrcnt(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_handle_opaque(oidp, intrcnt,
	    (char *)eintrcnt - (char *)intrcnt, req));
}

SYSCTL_PROC(_hw, OID_AUTO, intrcnt, CTLTYPE_OPAQUE | CTLFLAG_RD,
    NULL, 0, sysctl_intrcnt, "", "Interrupt Counts");

#ifdef DDB
/*
 * DDB command to dump the interrupt statistics.
 */
DB_SHOW_COMMAND(intrcnt, db_show_intrcnt)
{
	u_long *i;
	char *cp;
	int quit;

	cp = intrnames;
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	for (i = intrcnt, quit = 0; i != eintrcnt && !quit; i++) {
		if (*cp == '\0')
			break;
		if (*i != 0)
			db_printf("%s\t%lu\n", cp, *i);
		cp += strlen(cp) + 1;
	}
}
#endif
