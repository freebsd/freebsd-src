/* 
 * Copyright (C) 2001 Julian Elischer <julian@freebsd.org>.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/jail.h>
#include <sys/kse.h>
#include <sys/ktr.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_map.h>

/*
 * Thread related storage.
 */
static uma_zone_t thread_zone;
static int allocated_threads;
static int active_threads;
static int cached_threads;

SYSCTL_NODE(_kern, OID_AUTO, threads, CTLFLAG_RW, 0, "thread allocation");

SYSCTL_INT(_kern_threads, OID_AUTO, active, CTLFLAG_RD,
	&active_threads, 0, "Number of active threads in system.");

SYSCTL_INT(_kern_threads, OID_AUTO, cached, CTLFLAG_RD,
	&cached_threads, 0, "Number of threads in thread cache.");

SYSCTL_INT(_kern_threads, OID_AUTO, allocated, CTLFLAG_RD,
	&allocated_threads, 0, "Number of threads in zone.");

static int oiks_debug = 1;	/* 0 disable, 1 printf, 2 enter debugger */
SYSCTL_INT(_kern_threads, OID_AUTO, oiks, CTLFLAG_RW,
	&oiks_debug, 0, "OIKS thread debug");

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

struct threadqueue zombie_threads = TAILQ_HEAD_INITIALIZER(zombie_threads);
struct mtx zombie_thread_lock;
MTX_SYSINIT(zombie_thread_lock, &zombie_thread_lock,
    "zombie_thread_lock", MTX_SPIN);

/*
 * Pepare a thread for use.
 */
static void
thread_ctor(void *mem, int size, void *arg)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	bzero(&td->td_startzero,
	    (unsigned)RANGEOF(struct thread, td_startzero, td_endzero));
	td->td_state = TDS_NEW;
	td->td_flags |= TDF_UNBOUND;
#if 0
	/*
	 * Maybe move these here from process creation, but maybe not.   
	 * Moving them here takes them away from their "natural" place
	 * in the fork process.
	 */
	/* XXX td_contested does not appear to be initialized for threads! */
	LIST_INIT(&td->td_contested);
	callout_init(&td->td_slpcallout, 1);
#endif
	cached_threads--;	/* XXXSMP */
	active_threads++;	/* XXXSMP */
}

/*
 * Reclaim a thread after use.
 */
static void
thread_dtor(void *mem, int size, void *arg)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;

#ifdef INVARIANTS
	/* Verify that this thread is in a safe state to free. */
	switch (td->td_state) {
	case TDS_SLP:
	case TDS_MTX:
	case TDS_RUNQ:
		/*
		 * We must never unlink a thread that is in one of
		 * these states, because it is currently active.
		 */
		panic("bad state for thread unlinking");
		/* NOTREACHED */
	case TDS_UNQUEUED:
	case TDS_NEW:
	case TDS_RUNNING:
	case TDS_SURPLUS:
		break;
	default:
		panic("bad thread state");
		/* NOTREACHED */
	}
#endif

	/* Update counters. */
	active_threads--;	/* XXXSMP */
	cached_threads++;	/* XXXSMP */
}

/*
 * Initialize type-stable parts of a thread (when newly created).
 */
static void
thread_init(void *mem, int size)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	pmap_new_thread(td);
	cpu_thread_setup(td);
	cached_threads++;	/* XXXSMP */
	allocated_threads++;	/* XXXSMP */
}

/*
 * Tear down type-stable parts of a thread (just before being discarded).
 */
static void
thread_fini(void *mem, int size)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	pmap_dispose_thread(td);
	cached_threads--;	/* XXXSMP */
	allocated_threads--;	/* XXXSMP */
}

/*
 * Initialize global thread allocation resources.
 */
void
threadinit(void)
{

	thread_zone = uma_zcreate("THREAD", sizeof (struct thread),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE, 0);
}

/*
 * Stash an embarasingly esxtra thread into the zombie thread queue.
 */
void
thread_stash(struct thread *td)
{
	mtx_lock_spin(&zombie_thread_lock);
	TAILQ_INSERT_HEAD(&zombie_threads, td, td_runq);
	mtx_unlock_spin(&zombie_thread_lock);
}

/* 
 * reap any  zombie threads for this Processor.
 */
void
thread_reap(void)
{
	struct thread *td_reaped;

	/*
	 * don't even bother to lock if none at this instant
	 * We really don't care about the next instant..
	 */
	if (!TAILQ_EMPTY(&zombie_threads)) {
		mtx_lock_spin(&zombie_thread_lock);
		while (!TAILQ_EMPTY(&zombie_threads)) {
			td_reaped = TAILQ_FIRST(&zombie_threads);
			TAILQ_REMOVE(&zombie_threads, td_reaped, td_runq);
			mtx_unlock_spin(&zombie_thread_lock);
			thread_free(td_reaped);
			mtx_lock_spin(&zombie_thread_lock);
		}
		mtx_unlock_spin(&zombie_thread_lock);
	}
}

/*
 * Allocate a thread.
 */
struct thread *
thread_alloc(void)
{
	thread_reap(); /* check if any zombies to get */
	return (uma_zalloc(thread_zone, M_WAITOK));
}

/*
 * Deallocate a thread.
 */
void
thread_free(struct thread *td)
{
	uma_zfree(thread_zone, td);
}

/*
 * Store the thread context in the UTS's mailbox.
 */
int
thread_export_context(struct thread *td)
{
	struct kse *ke;
	uintptr_t td2_mbx;
	void *addr1;
	void *addr2;
	int error;

#ifdef __ia64__
	td2_mbx = 0;		/* pacify gcc (!) */
#endif
	/* Export the register contents. */
	error = cpu_export_context(td);

	ke = td->td_kse;
	addr1 = (caddr_t)ke->ke_mailbox
			+ offsetof(struct kse_mailbox, kmbx_completed_threads);
	addr2 = (caddr_t)td->td_mailbox
			+ offsetof(struct thread_mailbox , next_completed);
	/* Then link it into it's KSE's list of completed threads. */
	if (!error) {
		error = td2_mbx = fuword(addr1);
		if (error == -1)
			error = EFAULT;
		else
			error = 0;
	}
	if (!error)
		error = suword(addr2, td2_mbx);
	if (!error)
		error = suword(addr1, (u_long)td->td_mailbox);
	if (error == -1)
		error = EFAULT;
	return (error);
}


/*
 * Discard the current thread and exit from its context.
 *
 * Because we can't free a thread while we're operating under its context,
 * push the current thread into our KSE's ke_tdspare slot, freeing the
 * thread that might be there currently. Because we know that only this
 * processor will run our KSE, we needn't worry about someone else grabbing
 * our context before we do a cpu_throw.
 */
void
thread_exit(void)
{
	struct thread *td;
	struct kse *ke;
	struct proc *p;
	struct ksegrp	*kg;

	td = curthread;
	kg = td->td_ksegrp;
	p = td->td_proc;
	ke = td->td_kse;

	mtx_assert(&sched_lock, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	CTR1(KTR_PROC, "thread_exit: thread %p", td);
	KASSERT(!mtx_owned(&Giant), ("dying thread owns giant"));

	if (ke->ke_tdspare != NULL) {
		thread_stash(ke->ke_tdspare);
		ke->ke_tdspare = NULL;
	}
	cpu_thread_exit(td);	/* XXXSMP */

	/* Reassign this thread's KSE. */
	if (ke != NULL) {
		ke->ke_thread = NULL;
		td->td_kse = NULL;
		ke->ke_state = KES_UNQUEUED;
		kse_reassign(ke);
	}

	/* Unlink this thread from its proc. and the kseg */
	if (p != NULL) {
		TAILQ_REMOVE(&p->p_threads, td, td_plist);
		p->p_numthreads--;
		if (kg != NULL) {
			TAILQ_REMOVE(&kg->kg_threads, td, td_kglist);
			kg->kg_numthreads--;
		}
		/*
		 * The test below is NOT true if we are the
		 * sole exiting thread. P_STOPPED_SNGL is unset
		 * in exit1() after it is the only survivor.
		 */
		if (P_SHOULDSTOP(p) == P_STOPPED_SNGL) {
			if (p->p_numthreads == p->p_suspcount) {
				TAILQ_REMOVE(&p->p_suspended,
				    p->p_singlethread, td_runq);
				setrunqueue(p->p_singlethread);
				p->p_suspcount--;
			}
		}
	}
	td->td_state	= TDS_SURPLUS;
	td->td_proc	= NULL;
	td->td_ksegrp	= NULL;
	td->td_last_kse	= NULL;
	ke->ke_tdspare = td;
	PROC_UNLOCK(p);
	cpu_throw();
	/* NOTREACHED */
}

/*
 * Link a thread to a process.
 *
 * Note that we do not link to the proc's ucred here.
 * The thread is linked as if running but no KSE assigned.
 */
void
thread_link(struct thread *td, struct ksegrp *kg)
{
	struct proc *p;

	p = kg->kg_proc;
	td->td_state = TDS_NEW;
	td->td_proc	= p;
	td->td_ksegrp	= kg;
	td->td_last_kse	= NULL;

	TAILQ_INSERT_HEAD(&p->p_threads, td, td_plist);
	TAILQ_INSERT_HEAD(&kg->kg_threads, td, td_kglist);
	p->p_numthreads++;
	kg->kg_numthreads++;
	if (oiks_debug && p->p_numthreads > 4) {
		printf("OIKS %d\n", p->p_numthreads);
		if (oiks_debug > 1)
			Debugger("OIKS");
	}
	td->td_critnest = 0;
	td->td_kse	= NULL;
}

/*
 * Set up the upcall pcb in either a given thread or a new one
 * if none given. Use the upcall for the given KSE
 * XXXKSE possibly fix cpu_set_upcall() to not need td->td_kse set.
 */
struct thread *
thread_schedule_upcall(struct thread *td, struct kse *ke)
{
	struct thread *td2;

	mtx_assert(&sched_lock, MA_OWNED);
	if (ke->ke_tdspare != NULL) {
		td2 = ke->ke_tdspare;
		ke->ke_tdspare = NULL;
	} else {
		mtx_unlock_spin(&sched_lock);
		td2 = thread_alloc();
		mtx_lock_spin(&sched_lock);
	}
	CTR3(KTR_PROC, "thread_schedule_upcall: thread %p (pid %d, %s)",
	     td, td->td_proc->p_pid, td->td_proc->p_comm);
	thread_link(td2, ke->ke_ksegrp);
	cpu_set_upcall(td2, ke->ke_pcb);
	td2->td_ucred = crhold(td->td_ucred);
	td2->td_flags = TDF_UNBOUND|TDF_UPCALLING;
	td2->td_priority = td->td_priority;
	setrunqueue(td2);
	return (td2);
}

/*
 * The extra work we go through if we are a threaded process when we 
 * return to userland
 *
 * If we are a KSE process and returning to user mode, check for
 * extra work to do before we return (e.g. for more syscalls
 * to complete first).  If we were in a critical section, we should
 * just return to let it finish. Same if we were in the UTS (in
 * which case we will have no thread mailbox registered).  The only
 * traps we suport will have set the mailbox.  We will clear it here.
 */
int
thread_userret(struct proc *p, struct ksegrp *kg, struct kse *ke,
    struct thread *td, struct trapframe *frame)
{
	int error = 0;

	if (ke->ke_tdspare == NULL) {
		ke->ke_tdspare = thread_alloc();
	}
	if (td->td_flags & TDF_UNBOUND) {
		/*
		 * Are we returning from a thread that had a mailbox?
		 *
		 * XXX Maybe this should be in a separate function.
		 */
		if (((td->td_flags & TDF_UPCALLING) == 0) && td->td_mailbox) {
			/*
			 * [XXXKSE Future enhancement]
			 * We could also go straight back to the syscall
			 * if we never had to do an upcall since then.
			 * If the KSE's copy is == the thread's copy..
			 * AND there are no other completed threads.
			 */
			/*
			 * We will go back as an upcall or go do another thread.
			 * Either way we need to save the context back to
			 * the user thread mailbox.
			 * So the UTS can restart it later.
			 */
			error = thread_export_context(td);
			td->td_mailbox = NULL;
			if (error) {
				/*
				 * Failing to do the KSE
				 * operation just defaults operation
				 * back to synchonous operation.
				 */
				goto cont;
			}

			if (TAILQ_FIRST(&kg->kg_runq)) {
				/*
				 * Uh-oh.. don't return to the user.
				 * Instead, switch to the thread that
				 * needs to run. The question is:
				 * What do we do with the thread we have now?
				 * We have put the completion block
				 * on the kse mailbox. If we had more energy,
				 * we could lazily do so, assuming someone
				 * else might get to userland earlier
				 * and deliver it earlier than we could.
				 * To do that we could save it off the KSEG.
				 * An upcalling KSE would 'reap' all completed
				 * threads.
				 * Being in a hurry, we'll do nothing and
				 * leave it on the current KSE for now.
				 *
				 * As for the other threads to run;
				 * we COULD rush through all the threads
				 * in this KSEG at this priority, or we
				 * could throw the ball back into the court
				 * and just run the highest prio kse available.
				 * What is OUR priority?
				 * the priority of the highest sycall waiting
				 * to be returned?
				 * For now, just let another KSE run (easiest).
				 */
				PROC_LOCK(p);
				mtx_lock_spin(&sched_lock);
				thread_exit(); /* Abandon current thread. */
				/* NOTREACHED */
			} else { /* if (number of returning syscalls = 1) */
				/*
				 * Swap our frame for the upcall frame.
				 *
				 * XXXKSE Assumes we are going to user land
				 * and not nested in the kernel
				 */
				td->td_flags |= TDF_UPCALLING;
			}
		}
		/*
		 * This is NOT just an 'else' clause for the above test...
		 */
		if (td->td_flags & TDF_UPCALLING) {
			CTR3(KTR_PROC, "userret: upcall thread %p (pid %d, %s)",
			    td, p->p_pid, p->p_comm);
			/*
			 * Make sure that it has the correct frame loaded.
			 * While we know that we are on the same KSEGRP
			 * as we were created on, we could very easily
			 * have come in on another KSE. We therefore need
			 * to do the copy of the frame after the last
			 * possible switch() (the one above).
			 */
			bcopy(ke->ke_frame, frame, sizeof(struct trapframe));

			/*
			 * Decide what we are sending to the user
			 * upcall sets one argument. The address of the mbox.
			 */
			cpu_set_args(td, ke);

			/*
			 * There is no more work to do and we are going to ride
			 * this thead/KSE up to userland. Make sure the user's
			 * pointer to the thread mailbox is cleared before we
			 * re-enter the kernel next time for any reason..
			 * We might as well do it here.
			 */
			td->td_flags &= ~TDF_UPCALLING;	/* Hmmmm. */
			error = suword((caddr_t)td->td_kse->ke_mailbox +
			    offsetof(struct kse_mailbox, kmbx_current_thread),
			    0);
		}
		/*
		 * Stop any chance that we may be separated from
		 * the KSE we are currently on. This is "biting the bullet",
		 * we are committing to go to user space as as THIS KSE here.
		 */
cont:
		td->td_flags &= ~TDF_UNBOUND;
	}
	return (error);
}

/*
 * Enforce single-threading.
 *
 * Returns 1 if the caller must abort (another thread is waiting to
 * exit the process or similar). Process is locked!
 * Returns 0 when you are successfully the only thread running.
 * A process has successfully single threaded in the suspend mode when
 * There are no threads in user mode. Threads in the kernel must be
 * allowed to continue until they get to the user boundary. They may even
 * copy out their return values and data before suspending. They may however be
 * accellerated in reaching the user boundary as we will wake up
 * any sleeping threads that are interruptable. (PCATCH).
 */
int
thread_single(int force_exit)
{
	struct thread *td;
	struct thread *td2;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT((td != NULL), ("curthread is NULL"));

	if ((p->p_flag & P_KSES) == 0)
		return (0);

	if (p->p_singlethread) {
		/*
		 * Someone is already single threading!
		 */
		return (1);
	}

	if (force_exit == SNGLE_EXIT)
		p->p_flag |= P_SINGLE_EXIT;
	else
		p->p_flag &= ~P_SINGLE_EXIT;
	p->p_flag |= P_STOPPED_SNGL;
	p->p_singlethread = td;
	while ((p->p_numthreads - p->p_suspcount) != 1) {
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			switch(td2->td_state) {
			case TDS_SUSPENDED:
				if (force_exit == SNGLE_EXIT) {
					TAILQ_REMOVE(&p->p_suspended,
					    td, td_runq);
					setrunqueue(td); /* Should suicide. */
				}
			case TDS_SLP:
				if (td2->td_flags & TDF_CVWAITQ) {
					cv_abort(td2);
				} else {
					abortsleep(td2);
				}
				break;
			/* etc. XXXKSE */
			default:
				;
			}
		}
		/*
		 * XXXKSE-- idea
		 * It's possible that we can just wake up when
		 * there are no runnable KSEs, because that would
		 * indicate that only this thread is runnable and
		 * there are no running KSEs in userland.
		 * --
		 * Wake us up when everyone else has suspended.
		 * (or died)
		 */
		mtx_lock_spin(&sched_lock);
		TAILQ_INSERT_TAIL(&p->p_suspended, td, td_runq);
		td->td_state = TDS_SUSPENDED;
		p->p_suspcount++;
		mtx_unlock(&Giant);
		PROC_UNLOCK(p);
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&Giant);
		PROC_LOCK(p);
	}
	return (0);
}

/*
 * Called in from locations that can safely check to see
 * whether we have to suspend or at least throttle for a
 * single-thread event (e.g. fork).
 *
 * Such locations include userret().
 * If the "return_instead" argument is non zero, the thread must be able to
 * accept 0 (caller may continue), or 1 (caller must abort) as a result.
 *
 * The 'return_instead' argument tells the function if it may do a
 * thread_exit() or suspend, or whether the caller must abort and back
 * out instead.
 *
 * If the thread that set the single_threading request has set the
 * P_SINGLE_EXIT bit in the process flags then this call will never return
 * if 'return_instead' is false, but will exit.
 *
 * P_SINGLE_EXIT | return_instead == 0| return_instead != 0
 *---------------+--------------------+---------------------
 *       0       | returns 0          |   returns 0 or 1
 *               | when ST ends       |   immediatly
 *---------------+--------------------+---------------------
 *       1       | thread exits       |   returns 1
 *               |                    |  immediatly
 * 0 = thread_exit() or suspension ok,
 * other = return error instead of stopping the thread.
 *
 * While a full suspension is under effect, even a single threading
 * thread would be suspended if it made this call (but it shouldn't).
 * This call should only be made from places where
 * thread_exit() would be safe as that may be the outcome unless 
 * return_instead is set.
 */
int
thread_suspend_check(int return_instead)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	while (P_SHOULDSTOP(p)) {
		if (P_SHOULDSTOP(p) == P_STOPPED_SNGL) {
			KASSERT(p->p_singlethread != NULL,
			    ("singlethread not set"));

			/*
			 * The only suspension in action is
			 * a single-threading. Treat it ever
			 * so slightly different if it is
			 * in a special situation.
			 */
			if (p->p_singlethread == td) {
				return (0);	/* Exempt from stopping. */
			}

		} 

		if (return_instead) {
			return (1);
		}

		/*
		 * If the process is waiting for us to exit,
		 * this thread should just suicide.
		 * Assumes that P_SINGLE_EXIT implies P_STOPPED_SNGL.
		 */
		if ((p->p_flag & P_SINGLE_EXIT) && (p->p_singlethread != td)) {
			mtx_lock_spin(&sched_lock);
			while (mtx_owned(&Giant))
				mtx_unlock(&Giant);
			thread_exit();
		}

		/*
		 * When a thread suspends, it just
		 * moves to the processes's suspend queue
		 * and stays there.
		 *
		 * XXXKSE if TDF_BOUND is true
		 * it will not release it's KSE which might
		 * lead to deadlock if there are not enough KSEs
		 * to complete all waiting threads.
		 * Maybe be able to 'lend' it out again.
		 * (lent kse's can not go back to userland?)
		 * and can only be lent in STOPPED state.
		 */
		mtx_assert(&Giant, MA_NOTOWNED);
		mtx_lock_spin(&sched_lock);
		p->p_suspcount++;
		td->td_state = TDS_SUSPENDED;
		TAILQ_INSERT_TAIL(&p->p_suspended, td, td_runq);
		PROC_UNLOCK(p);
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
	}
	return (0);
}

/*
 * Allow all threads blocked by single threading to continue running.
 */
void
thread_unsuspend(struct proc *p)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (!P_SHOULDSTOP(p)) {
		while (( td = TAILQ_FIRST(&p->p_suspended))) {
			TAILQ_REMOVE(&p->p_suspended, td, td_runq);
			p->p_suspcount--;
			setrunqueue(td);
		}
	} else if ((P_SHOULDSTOP(p) == P_STOPPED_SNGL) &&
	    (p->p_numthreads == p->p_suspcount)) {
		/*
		 * Stopping everything also did the job for the single
		 * threading request. Now we've downgraded to single-threaded,
		 * let it continue.
		 */
		TAILQ_REMOVE(&p->p_suspended, p->p_singlethread, td_runq);
		p->p_suspcount--;
		setrunqueue(p->p_singlethread);
	}
}

void
thread_single_end(void)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_flag &= ~P_STOPPED_SNGL;
	p->p_singlethread = NULL;
	thread_unsuspend(p);
}

