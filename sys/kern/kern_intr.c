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

struct	int_entropy {
	struct	proc *proc;
	uintptr_t vector;
};

struct	ithd *clk_ithd;
struct	ithd *tty_ithd;
void	*softclock_ih;
void	*vm_ih;

static MALLOC_DEFINE(M_ITHREAD, "ithread", "Interrupt Threads");

static int intr_storm_threshold = 500;
TUNABLE_INT("hw.intr_storm_threshold", &intr_storm_threshold);
SYSCTL_INT(_hw, OID_AUTO, intr_storm_threshold, CTLFLAG_RW,
    &intr_storm_threshold, 0,
    "Number of consecutive interrupts before storm protection is enabled");

static void	ithread_loop(void *);
static void	ithread_update(struct ithd *);
static void	start_softintr(void *);

u_char
ithread_priority(enum intr_type flags)
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
		panic("ithread_priority: no interrupt type in flags");
	}

	return pri;
}

/*
 * Regenerate the name (p_comm) and priority for a threaded interrupt thread.
 */
static void
ithread_update(struct ithd *ithd)
{
	struct intrhand *ih;
	struct thread *td;
	struct proc *p;
	int missed;

	mtx_assert(&ithd->it_lock, MA_OWNED);
	td = ithd->it_td;
	if (td == NULL)
		return;
	p = td->td_proc;

	strlcpy(p->p_comm, ithd->it_name, sizeof(p->p_comm));
	ithd->it_flags &= ~IT_ENTROPY;

	ih = TAILQ_FIRST(&ithd->it_handlers);
	if (ih == NULL) {
		mtx_lock_spin(&sched_lock);
		td->td_priority = PRI_MAX_ITHD;
		td->td_base_pri = PRI_MAX_ITHD;
		mtx_unlock_spin(&sched_lock);
		return;
	}
	mtx_lock_spin(&sched_lock);
	td->td_priority = ih->ih_pri;
	td->td_base_pri = ih->ih_pri;
	mtx_unlock_spin(&sched_lock);
	missed = 0;
	TAILQ_FOREACH(ih, &ithd->it_handlers, ih_next) {
		if (strlen(p->p_comm) + strlen(ih->ih_name) + 1 <
		    sizeof(p->p_comm)) {
			strcat(p->p_comm, " ");
			strcat(p->p_comm, ih->ih_name);
		} else
			missed++;
		if (ih->ih_flags & IH_ENTROPY)
			ithd->it_flags |= IT_ENTROPY;
	}
	while (missed-- > 0) {
		if (strlen(p->p_comm) + 1 == sizeof(p->p_comm)) {
			if (p->p_comm[sizeof(p->p_comm) - 2] == '+')
				p->p_comm[sizeof(p->p_comm) - 2] = '*';
			else
				p->p_comm[sizeof(p->p_comm) - 2] = '+';
		} else
			strcat(p->p_comm, "+");
	}
	CTR2(KTR_INTR, "%s: updated %s", __func__, p->p_comm);
}

int
ithread_create(struct ithd **ithread, uintptr_t vector, int flags,
    void (*disable)(uintptr_t), void (*enable)(uintptr_t), const char *fmt, ...)
{
	struct ithd *ithd;
	struct thread *td;
	struct proc *p;
	int error;
	va_list ap;

	/* The only valid flag during creation is IT_SOFT. */
	if ((flags & ~IT_SOFT) != 0)
		return (EINVAL);

	ithd = malloc(sizeof(struct ithd), M_ITHREAD, M_WAITOK | M_ZERO);
	ithd->it_vector = vector;
	ithd->it_disable = disable;
	ithd->it_enable = enable;
	ithd->it_flags = flags;
	TAILQ_INIT(&ithd->it_handlers);
	mtx_init(&ithd->it_lock, "ithread", NULL, MTX_DEF);

	va_start(ap, fmt);
	vsnprintf(ithd->it_name, sizeof(ithd->it_name), fmt, ap);
	va_end(ap);

	error = kthread_create(ithread_loop, ithd, &p, RFSTOPPED | RFHIGHPID,
	    0, "%s", ithd->it_name);
	if (error) {
		mtx_destroy(&ithd->it_lock);
		free(ithd, M_ITHREAD);
		return (error);
	}
	td = FIRST_THREAD_IN_PROC(p);	/* XXXKSE */
	mtx_lock_spin(&sched_lock);
	td->td_ksegrp->kg_pri_class = PRI_ITHD;
	td->td_priority = PRI_MAX_ITHD;
	TD_SET_IWAIT(td);
	mtx_unlock_spin(&sched_lock);
	ithd->it_td = td;
	td->td_ithd = ithd;
	if (ithread != NULL)
		*ithread = ithd;
	CTR2(KTR_INTR, "%s: created %s", __func__, ithd->it_name);
	return (0);
}

int
ithread_destroy(struct ithd *ithread)
{

	struct thread *td;
	if (ithread == NULL)
		return (EINVAL);

	td = ithread->it_td;
	mtx_lock(&ithread->it_lock);
	if (!TAILQ_EMPTY(&ithread->it_handlers)) {
		mtx_unlock(&ithread->it_lock);
		return (EINVAL);
	}
	ithread->it_flags |= IT_DEAD;
	mtx_lock_spin(&sched_lock);
	if (TD_AWAITING_INTR(td)) {
		TD_CLR_IWAIT(td);
		setrunqueue(td, SRQ_INTR);
	}
	mtx_unlock_spin(&sched_lock);
	mtx_unlock(&ithread->it_lock);
	CTR2(KTR_INTR, "%s: killing %s", __func__, ithread->it_name);
	return (0);
}

int
ithread_add_handler(struct ithd* ithread, const char *name,
    driver_intr_t handler, void *arg, u_char pri, enum intr_type flags,
    void **cookiep)
{
	struct intrhand *ih, *temp_ih;

	if (ithread == NULL || name == NULL || handler == NULL)
		return (EINVAL);

	ih = malloc(sizeof(struct intrhand), M_ITHREAD, M_WAITOK | M_ZERO);
	ih->ih_handler = handler;
	ih->ih_argument = arg;
	ih->ih_name = name;
	ih->ih_ithread = ithread;
	ih->ih_pri = pri;
	if (flags & INTR_FAST)
		ih->ih_flags = IH_FAST;
	else if (flags & INTR_EXCL)
		ih->ih_flags = IH_EXCLUSIVE;
	if (flags & INTR_MPSAFE)
		ih->ih_flags |= IH_MPSAFE;
	if (flags & INTR_ENTROPY)
		ih->ih_flags |= IH_ENTROPY;

	mtx_lock(&ithread->it_lock);
	if ((flags & INTR_EXCL) != 0 && !TAILQ_EMPTY(&ithread->it_handlers))
		goto fail;
	if (!TAILQ_EMPTY(&ithread->it_handlers)) {
		temp_ih = TAILQ_FIRST(&ithread->it_handlers);
		if (temp_ih->ih_flags & IH_EXCLUSIVE)
			goto fail;
		if ((ih->ih_flags & IH_FAST) && !(temp_ih->ih_flags & IH_FAST))
			goto fail;
		if (!(ih->ih_flags & IH_FAST) && (temp_ih->ih_flags & IH_FAST))
			goto fail;
	}

	TAILQ_FOREACH(temp_ih, &ithread->it_handlers, ih_next)
	    if (temp_ih->ih_pri > ih->ih_pri)
		    break;
	if (temp_ih == NULL)
		TAILQ_INSERT_TAIL(&ithread->it_handlers, ih, ih_next);
	else
		TAILQ_INSERT_BEFORE(temp_ih, ih, ih_next);
	ithread_update(ithread);
	mtx_unlock(&ithread->it_lock);

	if (cookiep != NULL)
		*cookiep = ih;
	CTR3(KTR_INTR, "%s: added %s to %s", __func__, ih->ih_name,
	    ithread->it_name);
	return (0);

fail:
	mtx_unlock(&ithread->it_lock);
	free(ih, M_ITHREAD);
	return (EINVAL);
}

int
ithread_remove_handler(void *cookie)
{
	struct intrhand *handler = (struct intrhand *)cookie;
	struct ithd *ithread;
#ifdef INVARIANTS
	struct intrhand *ih;
#endif

	if (handler == NULL)
		return (EINVAL);
	ithread = handler->ih_ithread;
	KASSERT(ithread != NULL,
	    ("interrupt handler \"%s\" has a NULL interrupt thread",
		handler->ih_name));
	CTR3(KTR_INTR, "%s: removing %s from %s", __func__, handler->ih_name,
	    ithread->it_name);
	mtx_lock(&ithread->it_lock);
#ifdef INVARIANTS
	TAILQ_FOREACH(ih, &ithread->it_handlers, ih_next)
		if (ih == handler)
			goto ok;
	mtx_unlock(&ithread->it_lock);
	panic("interrupt handler \"%s\" not found in interrupt thread \"%s\"",
	    ih->ih_name, ithread->it_name);
ok:
#endif
	/*
	 * If the interrupt thread is already running, then just mark this
	 * handler as being dead and let the ithread do the actual removal.
	 *
	 * During a cold boot while cold is set, msleep() does not sleep,
	 * so we have to remove the handler here rather than letting the
	 * thread do it.
	 */
	mtx_lock_spin(&sched_lock);
	if (!TD_AWAITING_INTR(ithread->it_td) && !cold) {
		handler->ih_flags |= IH_DEAD;

		/*
		 * Ensure that the thread will process the handler list
		 * again and remove this handler if it has already passed
		 * it on the list.
		 */
		ithread->it_need = 1;
	} else 
		TAILQ_REMOVE(&ithread->it_handlers, handler, ih_next);
	mtx_unlock_spin(&sched_lock);
	if ((handler->ih_flags & IH_DEAD) != 0)
		msleep(handler, &ithread->it_lock, PUSER, "itrmh", 0);
	ithread_update(ithread);
	mtx_unlock(&ithread->it_lock);
	free(handler, M_ITHREAD);
	return (0);
}

int
ithread_schedule(struct ithd *ithread)
{
	struct int_entropy entropy;
	struct thread *td;
	struct thread *ctd;
	struct proc *p;

	/*
	 * If no ithread or no handlers, then we have a stray interrupt.
	 */
	if ((ithread == NULL) || TAILQ_EMPTY(&ithread->it_handlers))
		return (EINVAL);

	ctd = curthread;
	td = ithread->it_td;
	p = td->td_proc;
	/*
	 * If any of the handlers for this ithread claim to be good
	 * sources of entropy, then gather some.
	 */
	if (harvest.interrupt && ithread->it_flags & IT_ENTROPY) {
		CTR3(KTR_INTR, "%s: pid %d (%s) gathering entropy", __func__,
		    p->p_pid, p->p_comm);
		entropy.vector = ithread->it_vector;
		entropy.proc = ctd->td_proc;
		random_harvest(&entropy, sizeof(entropy), 2, 0,
		    RANDOM_INTERRUPT);
	}

	KASSERT(p != NULL, ("ithread %s has no process", ithread->it_name));
	CTR4(KTR_INTR, "%s: pid %d: (%s) need = %d",
	    __func__, p->p_pid, p->p_comm, ithread->it_need);

	/*
	 * Set it_need to tell the thread to keep running if it is already
	 * running.  Then, grab sched_lock and see if we actually need to
	 * put this thread on the runqueue.
	 */
	ithread->it_need = 1;
	mtx_lock_spin(&sched_lock);
	if (TD_AWAITING_INTR(td)) {
		CTR2(KTR_INTR, "%s: setrunqueue %d", __func__, p->p_pid);
		TD_CLR_IWAIT(td);
		setrunqueue(td, SRQ_INTR);
	} else {
		CTR4(KTR_INTR, "%s: pid %d: it_need %d, state %d",
		    __func__, p->p_pid, ithread->it_need, td->td_state);
	}
	mtx_unlock_spin(&sched_lock);

	return (0);
}

int
swi_add(struct ithd **ithdp, const char *name, driver_intr_t handler, 
	    void *arg, int pri, enum intr_type flags, void **cookiep)
{
	struct ithd *ithd;
	int error;

	if (flags & (INTR_FAST | INTR_ENTROPY))
		return (EINVAL);

	ithd = (ithdp != NULL) ? *ithdp : NULL;

	if (ithd != NULL) {
		if ((ithd->it_flags & IT_SOFT) == 0)
			return(EINVAL);
	} else {
		error = ithread_create(&ithd, pri, IT_SOFT, NULL, NULL,
		    "swi%d:", pri);
		if (error)
			return (error);

		if (ithdp != NULL)
			*ithdp = ithd;
	}
	return (ithread_add_handler(ithd, name, handler, arg,
		    (pri * RQ_PPQ) + PI_SOFT, flags, cookiep));
		    /* XXKSE.. think of a better way to get separate queues */
}


/*
 * Schedule a heavyweight software interrupt process. 
 */
void
swi_sched(void *cookie, int flags)
{
	struct intrhand *ih = (struct intrhand *)cookie;
	struct ithd *it = ih->ih_ithread;
	int error;

	atomic_add_int(&cnt.v_intr, 1); /* one more global interrupt */
		
	CTR3(KTR_INTR, "swi_sched pid %d(%s) need=%d",
		it->it_td->td_proc->p_pid, it->it_td->td_proc->p_comm, it->it_need);

	/*
	 * Set ih_need for this handler so that if the ithread is already
	 * running it will execute this handler on the next pass.  Otherwise,
	 * it will execute it the next time it runs.
	 */
	atomic_store_rel_int(&ih->ih_need, 1);
	if (!(flags & SWI_DELAY)) {
		error = ithread_schedule(it);
		KASSERT(error == 0, ("stray software interrupt"));
	}
}

/*
 * This is the main code for interrupt threads.
 */
static void
ithread_loop(void *arg)
{
	struct ithd *ithd;		/* our thread context */
	struct intrhand *ih;		/* and our interrupt handler chain */
	struct thread *td;
	struct proc *p;
	int count, warming, warned;
	
	td = curthread;
	p = td->td_proc;
	ithd = (struct ithd *)arg;	/* point to myself */
	KASSERT(ithd->it_td == td && td->td_ithd == ithd,
	    ("%s: ithread and proc linkage out of sync", __func__));
	warming = 10 * intr_storm_threshold;
	warned = 0;

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		/*
		 * If we are an orphaned thread, then just die.
		 */
		if (ithd->it_flags & IT_DEAD) {
			CTR3(KTR_INTR, "%s: pid %d: (%s) exiting", __func__,
			    p->p_pid, p->p_comm);
			td->td_ithd = NULL;
			mtx_destroy(&ithd->it_lock);
			free(ithd, M_ITHREAD);
			kthread_exit(0);
		}

		CTR4(KTR_INTR, "%s: pid %d: (%s) need=%d", __func__,
		     p->p_pid, p->p_comm, ithd->it_need);
		count = 0;
		while (ithd->it_need) {
			/*
			 * Service interrupts.  If another interrupt
			 * arrives while we are running, they will set
			 * it_need to denote that we should make
			 * another pass.
			 */
			atomic_store_rel_int(&ithd->it_need, 0);
restart:
			TAILQ_FOREACH(ih, &ithd->it_handlers, ih_next) {
				if (ithd->it_flags & IT_SOFT && !ih->ih_need)
					continue;
				atomic_store_rel_int(&ih->ih_need, 0);
				CTR6(KTR_INTR,
				    "%s: pid %d ih=%p: %p(%p) flg=%x", __func__,
				    p->p_pid, (void *)ih,
				    (void *)ih->ih_handler, ih->ih_argument,
				    ih->ih_flags);

				if ((ih->ih_flags & IH_DEAD) != 0) {
					mtx_lock(&ithd->it_lock);
					TAILQ_REMOVE(&ithd->it_handlers, ih,
					    ih_next);
					wakeup(ih);
					mtx_unlock(&ithd->it_lock);
					goto restart;
				}
				if ((ih->ih_flags & IH_MPSAFE) == 0)
					mtx_lock(&Giant);
				ih->ih_handler(ih->ih_argument);
				if ((ih->ih_flags & IH_MPSAFE) == 0)
					mtx_unlock(&Giant);
			}
			if (ithd->it_enable != NULL) {
				ithd->it_enable(ithd->it_vector);

				/*
				 * Storm detection needs a delay here
				 * to see slightly delayed interrupts
				 * on some machines, but we don't
				 * want to always delay, so only delay
				 * while warming up.
				 *
				 * XXXRW: Calling DELAY() in the interrupt
				 * path surely needs to be revisited.
				 */
				if (warming != 0) {
					DELAY(1);
					--warming;
				}
			}

			/*
			 * If we detect an interrupt storm, sleep until
			 * the next hardclock tick.  We sleep at the
			 * end of the loop instead of at the beginning
			 * to ensure that we see slightly delayed
			 * interrupts.
			 */
			if (count >= intr_storm_threshold) {
				if (!warned) {
					printf(
	"Interrupt storm detected on \"%s\"; throttling interrupt source\n",
					    p->p_comm);
					warned = 1;
				}
				tsleep(&count, td->td_priority, "istorm", 1);

				/*
				 * Fudge the count to re-throttle if the
				 * interrupt is still active.  Our storm
				 * detection is too primitive to detect
				 * whether the storm has gone away
				 * reliably, even if we were to waste a
				 * lot of time spinning for the next
				 * intr_storm_threshold interrupts, so
				 * we assume that the storm hasn't gone
				 * away unless the interrupt repeats
				 * less often the hardclock interrupt.
				 */
				count = INT_MAX - 1;
			}
			count++;
		}
		WITNESS_WARN(WARN_PANIC, NULL, "suspending ithread");
		mtx_assert(&Giant, MA_NOTOWNED);

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		mtx_lock_spin(&sched_lock);
		if (!ithd->it_need) {
			TD_SET_IWAIT(td);
			CTR2(KTR_INTR, "%s: pid %d: done", __func__, p->p_pid);
			mi_switch(SW_VOL, NULL);
			CTR2(KTR_INTR, "%s: pid %d: resumed", __func__, p->p_pid);
		}
		mtx_unlock_spin(&sched_lock);
	}
}

#ifdef DDB
/*
 * Dump details about an interrupt handler
 */
static void
db_dump_intrhand(struct intrhand *ih)
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
 * Dump details about an ithread
 */
void
db_dump_ithread(struct ithd *ithd, int handlers)
{
	struct proc *p;
	struct intrhand *ih;
	int comma;

	if (ithd->it_td != NULL) {
		p = ithd->it_td->td_proc;
		db_printf("%s (pid %d)", p->p_comm, p->p_pid);
	} else
		db_printf("%s: (no thread)", ithd->it_name);
	if ((ithd->it_flags & (IT_SOFT | IT_ENTROPY | IT_DEAD)) != 0 ||
	    ithd->it_need) {
		db_printf(" {");
		comma = 0;
		if (ithd->it_flags & IT_SOFT) {
			db_printf("SOFT");
			comma = 1;
		}
		if (ithd->it_flags & IT_ENTROPY) {
			if (comma)
				db_printf(", ");
			db_printf("ENTROPY");
			comma = 1;
		}
		if (ithd->it_flags & IT_DEAD) {
			if (comma)
				db_printf(", ");
			db_printf("DEAD");
			comma = 1;
		}
		if (ithd->it_need) {
			if (comma)
				db_printf(", ");
			db_printf("NEED");
		}
		db_printf("}");
	}
	db_printf("\n");

	if (handlers)
		TAILQ_FOREACH(ih, &ithd->it_handlers, ih_next)
		    db_dump_intrhand(ih);
}
#endif /* DDB */

/*
 * Start standard software interrupt threads
 */
static void
start_softintr(void *dummy)
{
	struct proc *p;

	if (swi_add(&clk_ithd, "clock", softclock, NULL, SWI_CLOCK,
		INTR_MPSAFE, &softclock_ih) ||
	    swi_add(NULL, "vm", swi_vm, NULL, SWI_VM, INTR_MPSAFE, &vm_ih))
		panic("died while creating standard software ithreads");

	p = clk_ithd->it_td->td_proc;
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
