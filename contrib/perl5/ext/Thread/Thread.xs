#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* Magic signature for Thread's mg_private is "Th" */ 
#define Thread_MAGIC_SIGNATURE 0x5468

#ifdef __cplusplus
#ifdef I_UNISTD
#include <unistd.h>
#endif
#endif
#include <fcntl.h>
                        
static int sig_pipe[2];
            
#ifndef THREAD_RET_TYPE
#define THREAD_RET_TYPE void *
#define THREAD_RET_CAST(x) ((THREAD_RET_TYPE) x)
#endif

static void
remove_thread(struct perl_thread *t)
{
#ifdef USE_THREADS
    DEBUG_S(WITH_THR(PerlIO_printf(PerlIO_stderr(),
				   "%p: remove_thread %p\n", thr, t)));
    MUTEX_LOCK(&PL_threads_mutex);
    MUTEX_DESTROY(&t->mutex);
    PL_nthreads--;
    t->prev->next = t->next;
    t->next->prev = t->prev;
    COND_BROADCAST(&PL_nthreads_cond);
    MUTEX_UNLOCK(&PL_threads_mutex);
#endif
}

static THREAD_RET_TYPE
threadstart(void *arg)
{
#ifdef USE_THREADS
#ifdef FAKE_THREADS
    Thread savethread = thr;
    LOGOP myop;
    dSP;
    I32 oldscope = PL_scopestack_ix;
    I32 retval;
    AV *av;
    int i;

    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "new thread %p starting at %s\n",
			  thr, SvPEEK(TOPs)));
    thr = (Thread) arg;
    savemark = TOPMARK;
    thr->prev = thr->prev_run = savethread;
    thr->next = savethread->next;
    thr->next_run = savethread->next_run;
    savethread->next = savethread->next_run = thr;
    thr->wait_queue = 0;
    thr->private = 0;

    /* Now duplicate most of perl_call_sv but with a few twists */
    PL_op = (OP*)&myop;
    Zero(PL_op, 1, LOGOP);
    myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_flags |= OPf_KNOW;
    myop.op_flags |= OPf_WANT_LIST;
    PL_op = pp_entersub(ARGS);
    DEBUG_S(if (!PL_op)
	    PerlIO_printf(PerlIO_stderr(), "thread starts at Nullop\n"));
    /*
     * When this thread is next scheduled, we start in the right
     * place. When the thread runs off the end of the sub, perl.c
     * handles things, using savemark to figure out how much of the
     * stack is the return value for any join.
     */
    thr = savethread;		/* back to the old thread */
    return 0;
#else
    Thread thr = (Thread) arg;
    LOGOP myop;
    djSP;
    I32 oldmark = TOPMARK;
    I32 oldscope = PL_scopestack_ix;
    I32 retval;
    SV *sv;
    AV *av = newAV();
    int i, ret;
    dJMPENV;
    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "new thread %p waiting to start\n",
			  thr));

    /* Don't call *anything* requiring dTHR until after SET_THR() */
    /*
     * Wait until our creator releases us. If we didn't do this, then
     * it would be potentially possible for out thread to carry on and
     * do stuff before our creator fills in our "self" field. For example,
     * if we went and created another thread which tried to JOIN with us,
     * then we'd be in a mess.
     */
    MUTEX_LOCK(&thr->mutex);
    MUTEX_UNLOCK(&thr->mutex);

    /*
     * It's safe to wait until now to set the thread-specific pointer
     * from our pthread_t structure to our struct perl_thread, since
     * we're the only thread who can get at it anyway.
     */
    SET_THR(thr);

    /* Only now can we use SvPEEK (which calls sv_newmortal which does dTHR) */
    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "new thread %p starting at %s\n",
			  thr, SvPEEK(TOPs)));

    sv = POPs;
    PUTBACK;
    ENTER;
    SAVETMPS;
    perl_call_sv(sv, G_ARRAY|G_EVAL);
    SPAGAIN;
    retval = SP - (PL_stack_base + oldmark);
    SP = PL_stack_base + oldmark + 1;
    if (SvCUR(thr->errsv)) {
	STRLEN n_a;
	MUTEX_LOCK(&thr->mutex);
	thr->flags |= THRf_DID_DIE;
	MUTEX_UNLOCK(&thr->mutex);
	av_store(av, 0, &PL_sv_no);
	av_store(av, 1, newSVsv(thr->errsv));
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p died: %s\n",
			      thr, SvPV(thr->errsv, n_a)));
    } else {
	DEBUG_S(STMT_START {
	    for (i = 1; i <= retval; i++) {
		PerlIO_printf(PerlIO_stderr(), "%p return[%d] = %s\n",
				thr, i, SvPEEK(SP[i - 1]));
	    }
	} STMT_END);
	av_store(av, 0, &PL_sv_yes);
	for (i = 1; i <= retval; i++, SP++)
	    sv_setsv(*av_fetch(av, i, TRUE), SvREFCNT_inc(*SP));
    }
    FREETMPS;
    LEAVE;

  finishoff:
#if 0    
    /* removed for debug */
    SvREFCNT_dec(PL_curstack);
#endif
    SvREFCNT_dec(thr->cvcache);
    SvREFCNT_dec(thr->threadsv);
    SvREFCNT_dec(thr->specific);
    SvREFCNT_dec(thr->errsv);
    SvREFCNT_dec(thr->errhv);

    /*Safefree(cxstack);*/
    while (PL_curstackinfo->si_next)
	PL_curstackinfo = PL_curstackinfo->si_next;
    while (PL_curstackinfo) {
	PERL_SI *p = PL_curstackinfo->si_prev;
	SvREFCNT_dec(PL_curstackinfo->si_stack);
	Safefree(PL_curstackinfo->si_cxstack);
	Safefree(PL_curstackinfo);
	PL_curstackinfo = p;
    }    
    Safefree(PL_markstack);
    Safefree(PL_scopestack);
    Safefree(PL_savestack);
    Safefree(PL_retstack);
    Safefree(PL_tmps_stack);
    Safefree(PL_ofs);

    SvREFCNT_dec(PL_rs);
    SvREFCNT_dec(PL_nrs);
    SvREFCNT_dec(PL_statname);
    Safefree(PL_screamfirst);
    Safefree(PL_screamnext);
    Safefree(PL_reg_start_tmp);
    SvREFCNT_dec(PL_lastscream);
    SvREFCNT_dec(PL_defoutgv);

    MUTEX_LOCK(&thr->mutex);
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: threadstart finishing: state is %u\n",
			  thr, ThrSTATE(thr)));
    switch (ThrSTATE(thr)) {
    case THRf_R_JOINABLE:
	ThrSETSTATE(thr, THRf_ZOMBIE);
	MUTEX_UNLOCK(&thr->mutex);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "%p: R_JOINABLE thread finished\n", thr));
	break;
    case THRf_R_JOINED:
	ThrSETSTATE(thr, THRf_DEAD);
	MUTEX_UNLOCK(&thr->mutex);
	remove_thread(thr);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "%p: R_JOINED thread finished\n", thr));
	break;
    case THRf_R_DETACHED:
	ThrSETSTATE(thr, THRf_DEAD);
	MUTEX_UNLOCK(&thr->mutex);
	SvREFCNT_dec(av);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "%p: DETACHED thread finished\n", thr));
	remove_thread(thr);	/* This might trigger main thread to finish */
	break;
    default:
	MUTEX_UNLOCK(&thr->mutex);
	croak("panic: illegal state %u at end of threadstart", ThrSTATE(thr));
	/* NOTREACHED */
    }
    return THREAD_RET_CAST(av);	/* Available for anyone to join with */
					/* us unless we're detached, in which */
					/* case noone sees the value anyway. */
#endif    
#else
    return THREAD_RET_CAST(NULL);
#endif
}

static SV *
newthread (SV *startsv, AV *initargs, char *classname)
{
#ifdef USE_THREADS
    dSP;
    Thread savethread;
    int i;
    SV *sv;
    int err;
#ifndef THREAD_CREATE
    static pthread_attr_t attr;
    static int attr_inited = 0;
    sigset_t fullmask, oldmask;
#endif
    
    savethread = thr;
    thr = new_struct_thread(thr);
    /* temporarily pretend to be the child thread in case the
     * XPUSHs() below want to grow the child's stack.  This is
     * safe, since the other thread is not yet created, and we
     * are the only ones who know about it */
    SET_THR(thr);
    SPAGAIN;
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: newthread (%p), tid is %u, preparing stack\n",
			  savethread, thr, thr->tid));
    /* The following pushes the arg list and startsv onto the *new* stack */
    PUSHMARK(SP);
    /* Could easily speed up the following greatly */
    for (i = 0; i <= AvFILL(initargs); i++)
	XPUSHs(SvREFCNT_inc(*av_fetch(initargs, i, FALSE)));
    XPUSHs(SvREFCNT_inc(startsv));
    PUTBACK;

    /* On your marks... */
    SET_THR(savethread);
    MUTEX_LOCK(&thr->mutex);

#ifdef THREAD_CREATE
    err = THREAD_CREATE(thr, threadstart);
#else    
    /* Get set...  */
    sigfillset(&fullmask);
    if (sigprocmask(SIG_SETMASK, &fullmask, &oldmask) == -1)
	croak("panic: sigprocmask");
    err = 0;
    if (!attr_inited) {
	attr_inited = 1;
#ifdef OLD_PTHREADS_API
	err = pthread_attr_create(&attr);
#else
	err = pthread_attr_init(&attr);
#endif
#ifdef OLD_PTHREADS_API
#ifdef VMS
/* This is available with the old pthreads API, but only with */
/* DecThreads (VMS and Digital Unix) */
	if (err == 0)
	    err = pthread_attr_setdetach_np(&attr, ATTR_JOINABLE);
#endif
#else
	if (err == 0)
	    err = pthread_attr_setdetachstate(&attr, ATTR_JOINABLE);
#endif
    }
    if (err == 0)
#ifdef OLD_PTHREADS_API
	err = pthread_create(&thr->self, attr, threadstart, (void*) thr);
#else
	err = pthread_create(&thr->self, &attr, threadstart, (void*) thr);
#endif
#endif
    if (err) {
	MUTEX_UNLOCK(&thr->mutex);
        DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "%p: create of %p failed %d\n",
			      savethread, thr, err));
	/* Thread creation failed--clean up */
	SvREFCNT_dec(thr->cvcache);
	remove_thread(thr);
	MUTEX_DESTROY(&thr->mutex);
	for (i = 0; i <= AvFILL(initargs); i++)
	    SvREFCNT_dec(*av_fetch(initargs, i, FALSE));
	SvREFCNT_dec(startsv);
	return NULL;
    }

#ifdef THREAD_POST_CREATE
    THREAD_POST_CREATE(thr);
#else
    if (sigprocmask(SIG_SETMASK, &oldmask, 0))
	croak("panic: sigprocmask");
#endif

    sv = newSViv(thr->tid);
    sv_magic(sv, thr->oursv, '~', 0, 0);
    SvMAGIC(sv)->mg_private = Thread_MAGIC_SIGNATURE;
    sv = sv_bless(newRV_noinc(sv), gv_stashpv(classname, TRUE));

    /* Go */
    MUTEX_UNLOCK(&thr->mutex);

    return sv;
#else
    croak("No threads in this perl");
    return &PL_sv_undef;
#endif
}

static Signal_t handle_thread_signal _((int sig));

static Signal_t
handle_thread_signal(int sig)
{
    unsigned char c = (unsigned char) sig;
    /*
     * We're not really allowed to call fprintf in a signal handler
     * so don't be surprised if this isn't robust while debugging
     * with -DL.
     */
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
	    "handle_thread_signal: got signal %d\n", sig););
    write(sig_pipe[1], &c, 1);
}

MODULE = Thread		PACKAGE = Thread
PROTOTYPES: DISABLE

void
new(classname, startsv, ...)
	char *		classname
	SV *		startsv
	AV *		av = av_make(items - 2, &ST(2));
    PPCODE:
	XPUSHs(sv_2mortal(newthread(startsv, av, classname)));

void
join(t)
	Thread	t
	AV *	av = NO_INIT
	int	i = NO_INIT
    PPCODE:
#ifdef USE_THREADS
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: joining %p (state %u)\n",
			      thr, t, ThrSTATE(t)););
    	MUTEX_LOCK(&t->mutex);
	switch (ThrSTATE(t)) {
	case THRf_R_JOINABLE:
	case THRf_R_JOINED:
	    ThrSETSTATE(t, THRf_R_JOINED);
	    MUTEX_UNLOCK(&t->mutex);
	    break;
	case THRf_ZOMBIE:
	    ThrSETSTATE(t, THRf_DEAD);
	    MUTEX_UNLOCK(&t->mutex);
	    remove_thread(t);
	    break;
	default:
	    MUTEX_UNLOCK(&t->mutex);
	    croak("can't join with thread");
	    /* NOTREACHED */
	}
	JOIN(t, &av);

	if (SvTRUE(*av_fetch(av, 0, FALSE))) {
	    /* Could easily speed up the following if necessary */
	    for (i = 1; i <= AvFILL(av); i++)
		XPUSHs(sv_2mortal(*av_fetch(av, i, FALSE)));
	} else {
	    STRLEN n_a;
	    char *mess = SvPV(*av_fetch(av, 1, FALSE), n_a);
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
				  "%p: join propagating die message: %s\n",
				  thr, mess));
	    croak(mess);
	}
#endif

void
detach(t)
	Thread	t
    CODE:
#ifdef USE_THREADS
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: detaching %p (state %u)\n",
			      thr, t, ThrSTATE(t)););
    	MUTEX_LOCK(&t->mutex);
	switch (ThrSTATE(t)) {
	case THRf_R_JOINABLE:
	    ThrSETSTATE(t, THRf_R_DETACHED);
	    /* fall through */
	case THRf_R_DETACHED:
	    DETACH(t);
	    MUTEX_UNLOCK(&t->mutex);
	    break;
	case THRf_ZOMBIE:
	    ThrSETSTATE(t, THRf_DEAD);
	    DETACH(t);
	    MUTEX_UNLOCK(&t->mutex);
	    remove_thread(t);
	    break;
	default:
	    MUTEX_UNLOCK(&t->mutex);
	    croak("can't detach thread");
	    /* NOTREACHED */
	}
#endif

void
equal(t1, t2)
	Thread	t1
	Thread	t2
    PPCODE:
	PUSHs((t1 == t2) ? &PL_sv_yes : &PL_sv_no);

void
flags(t)
	Thread	t
    PPCODE:
#ifdef USE_THREADS
	PUSHs(sv_2mortal(newSViv(t->flags)));
#endif

void
self(classname)
	char *	classname
    PREINIT:
	SV *sv;
    PPCODE:        
#ifdef USE_THREADS
	sv = newSViv(thr->tid);
	sv_magic(sv, thr->oursv, '~', 0, 0);
	SvMAGIC(sv)->mg_private = Thread_MAGIC_SIGNATURE;
	PUSHs(sv_2mortal(sv_bless(newRV_noinc(sv),
				  gv_stashpv(classname, TRUE))));
#endif

U32
tid(t)
	Thread	t
    CODE:
#ifdef USE_THREADS
    	MUTEX_LOCK(&t->mutex);
	RETVAL = t->tid;
    	MUTEX_UNLOCK(&t->mutex);
#else 
	RETVAL = 0;
#endif
    OUTPUT:
	RETVAL

void
DESTROY(t)
	SV *	t
    PPCODE:
	PUSHs(&PL_sv_yes);

void
yield()
    CODE:
{
#ifdef USE_THREADS
	YIELD;
#endif
}

void
cond_wait(sv)
	SV *	sv
	MAGIC *	mg = NO_INIT
CODE:                       
#ifdef USE_THREADS
	if (SvROK(sv))
	    sv = SvRV(sv);

	mg = condpair_magic(sv);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: cond_wait %p\n", thr, sv));
	MUTEX_LOCK(MgMUTEXP(mg));
	if (MgOWNER(mg) != thr) {
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	    croak("cond_wait for lock that we don't own\n");
	}
	MgOWNER(mg) = 0;
	COND_SIGNAL(MgOWNERCONDP(mg));
	COND_WAIT(MgCONDP(mg), MgMUTEXP(mg));
	while (MgOWNER(mg))
	    COND_WAIT(MgOWNERCONDP(mg), MgMUTEXP(mg));
	MgOWNER(mg) = thr;
	MUTEX_UNLOCK(MgMUTEXP(mg));
#endif

void
cond_signal(sv)
	SV *	sv
	MAGIC *	mg = NO_INIT
CODE:
#ifdef USE_THREADS
	if (SvROK(sv))
	    sv = SvRV(sv);

	mg = condpair_magic(sv);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: cond_signal %p\n",thr,sv));
	MUTEX_LOCK(MgMUTEXP(mg));
	if (MgOWNER(mg) != thr) {
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	    croak("cond_signal for lock that we don't own\n");
	}
	COND_SIGNAL(MgCONDP(mg));
	MUTEX_UNLOCK(MgMUTEXP(mg));
#endif

void
cond_broadcast(sv)
	SV *	sv
	MAGIC *	mg = NO_INIT
CODE: 
#ifdef USE_THREADS
	if (SvROK(sv))
	    sv = SvRV(sv);

	mg = condpair_magic(sv);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: cond_broadcast %p\n",
			      thr, sv));
	MUTEX_LOCK(MgMUTEXP(mg));
	if (MgOWNER(mg) != thr) {
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	    croak("cond_broadcast for lock that we don't own\n");
	}
	COND_BROADCAST(MgCONDP(mg));
	MUTEX_UNLOCK(MgMUTEXP(mg));
#endif

void
list(classname)
	char *	classname
    PREINIT:
	Thread	t;
	AV *	av;
	SV **	svp;
	int	n = 0;
    PPCODE:
#ifdef USE_THREADS
	av = newAV();
	/*
	 * Iterate until we have enough dynamic storage for all threads.
	 * We mustn't do any allocation while holding threads_mutex though.
	 */
	MUTEX_LOCK(&PL_threads_mutex);
	do {
	    n = PL_nthreads;
	    MUTEX_UNLOCK(&PL_threads_mutex);
	    if (AvFILL(av) < n - 1) {
		int i = AvFILL(av);
		for (i = AvFILL(av); i < n - 1; i++) {
		    SV *sv = newSViv(0);	/* fill in tid later */
		    sv_magic(sv, 0, '~', 0, 0);	/* fill in other magic later */
		    av_push(av, sv_bless(newRV_noinc(sv),
					 gv_stashpv(classname, TRUE)));
	
		}
	    }
	    MUTEX_LOCK(&PL_threads_mutex);
	} while (n < PL_nthreads);
	n = PL_nthreads;	/* Get the final correct value */

	/*
	 * At this point, there's enough room to fill in av.
	 * Note that we are holding threads_mutex so the list
	 * won't change out from under us but all the remaining
	 * processing is "fast" (no blocking, malloc etc.)
	 */
	t = thr;
	svp = AvARRAY(av);
	do {
	    SV *sv = (SV*)SvRV(*svp);
	    sv_setiv(sv, t->tid);
	    SvMAGIC(sv)->mg_obj = SvREFCNT_inc(t->oursv);
	    SvMAGIC(sv)->mg_flags |= MGf_REFCOUNTED;
	    SvMAGIC(sv)->mg_private = Thread_MAGIC_SIGNATURE;
	    t = t->next;
	    svp++;
	} while (t != thr);
	/*  */
	MUTEX_UNLOCK(&PL_threads_mutex);
	/* Truncate any unneeded slots in av */
	av_fill(av, n - 1);
	/* Finally, push all the new objects onto the stack and drop av */
	EXTEND(SP, n);
	for (svp = AvARRAY(av); n > 0; n--, svp++)
	    PUSHs(*svp);
	(void)sv_2mortal((SV*)av);
#endif


MODULE = Thread		PACKAGE = Thread::Signal

void
kill_sighandler_thread()
    PPCODE:
	write(sig_pipe[1], "\0", 1);
	PUSHs(&PL_sv_yes);

void
init_thread_signals()
    PPCODE:
	PL_sighandlerp = handle_thread_signal;
	if (pipe(sig_pipe) == -1)
	    XSRETURN_UNDEF;
	PUSHs(&PL_sv_yes);

void
await_signal()
    PREINIT:
	unsigned char c;
	SSize_t ret;
    CODE:
	do {
	    ret = read(sig_pipe[0], &c, 1);
	} while (ret == -1 && errno == EINTR);
	if (ret == -1)
	    croak("panic: await_signal");
	ST(0) = sv_newmortal();
	if (ret)
	    sv_setsv(ST(0), c ? psig_ptr[c] : &PL_sv_no);
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "await_signal returning %s\n", SvPEEK(ST(0))););

MODULE = Thread		PACKAGE = Thread::Specific

void
data(classname = "Thread::Specific")
	char *	classname
    PPCODE:
#ifdef USE_THREADS
	if (AvFILL(thr->specific) == -1) {
	    GV *gv = gv_fetchpv("Thread::Specific::FIELDS", TRUE, SVt_PVHV);
	    av_store(thr->specific, 0, newRV((SV*)GvHV(gv)));
	}
	XPUSHs(sv_bless(newRV((SV*)thr->specific),gv_stashpv(classname,TRUE)));
#endif
