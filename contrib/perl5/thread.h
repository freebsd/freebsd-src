#if defined(USE_THREADS) || defined(USE_ITHREADS)

#ifdef WIN32
#  include <win32thread.h>
#else
#  ifdef OLD_PTHREADS_API /* Here be dragons. */
#    define DETACH(t) \
    STMT_START {						\
	if (pthread_detach(&(t)->self)) {			\
	    MUTEX_UNLOCK(&(t)->mutex);				\
	    Perl_croak_nocontext("panic: DETACH");		\
	}							\
    } STMT_END

#    define PERL_GET_CONTEXT	Perl_get_context()
#    define PERL_SET_CONTEXT(t)	Perl_set_context((void*)t)

#    define PTHREAD_GETSPECIFIC_INT
#    ifdef DJGPP
#      define pthread_addr_t any_t
#      define NEED_PTHREAD_INIT
#      define PTHREAD_CREATE_JOINABLE (1)
#    endif
#    ifdef __OPEN_VM
#      define pthread_addr_t void *
#    endif
#    ifdef VMS
#      define pthread_attr_init(a) pthread_attr_create(a)
#      define PTHREAD_ATTR_SETDETACHSTATE(a,s) pthread_setdetach_np(a,s)
#      define PTHREAD_CREATE(t,a,s,d) pthread_create(t,a,s,d)
#      define pthread_key_create(k,d) pthread_keycreate(k,(pthread_destructor_t)(d))
#      define pthread_mutexattr_init(a) pthread_mutexattr_create(a)
#      define pthread_mutexattr_settype(a,t) pthread_mutexattr_setkind_np(a,t)
#    endif
#    if defined(__hpux) && defined(__ux_version) && __ux_version <= 1020
#      define pthread_attr_init(a) pthread_attr_create(a)
       /* XXX pthread_setdetach_np() missing in DCE threads on HP-UX 10.20 */
#      define PTHREAD_ATTR_SETDETACHSTATE(a,s)	(0)
#      define PTHREAD_CREATE(t,a,s,d) pthread_create(t,a,s,d)
#      define pthread_key_create(k,d) pthread_keycreate(k,(pthread_destructor_t)(d))
#      define pthread_mutexattr_init(a) pthread_mutexattr_create(a)
#      define pthread_mutexattr_settype(a,t) pthread_mutexattr_setkind_np(a,t)
#    endif
#    if defined(DJGPP) || defined(__OPEN_VM)
#      define PTHREAD_ATTR_SETDETACHSTATE(a,s) pthread_attr_setdetachstate(a,&(s))
#      define YIELD pthread_yield(NULL)
#    endif
#  endif
#  if !defined(__hpux) || !defined(__ux_version) || __ux_version > 1020
#    define pthread_mutexattr_default NULL
#    define pthread_condattr_default  NULL
#  endif
#endif

#ifndef PTHREAD_CREATE
/* You are not supposed to pass NULL as the 2nd arg of PTHREAD_CREATE(). */
#  define PTHREAD_CREATE(t,a,s,d) pthread_create(t,&(a),s,d)
#endif

#ifndef PTHREAD_ATTR_SETDETACHSTATE
#  define PTHREAD_ATTR_SETDETACHSTATE(a,s) pthread_attr_setdetachstate(a,s)
#endif

#ifndef PTHREAD_CREATE_JOINABLE
#  ifdef OLD_PTHREAD_CREATE_JOINABLE
#    define PTHREAD_CREATE_JOINABLE OLD_PTHREAD_CREATE_JOINABLE
#  else
#    define PTHREAD_CREATE_JOINABLE 0 /* Panic?  No, guess. */
#  endif
#endif

#ifdef I_MACH_CTHREADS

/* cthreads interface */

/* #include <mach/cthreads.h> is in perl.h #ifdef I_MACH_CTHREADS */

#define MUTEX_INIT(m) \
    STMT_START {						\
	*m = mutex_alloc();					\
	if (*m) {						\
	    mutex_init(*m);					\
	} else {						\
	    Perl_croak_nocontext("panic: MUTEX_INIT");		\
	}							\
    } STMT_END

#define MUTEX_LOCK(m)			mutex_lock(*m)
#define MUTEX_UNLOCK(m)			mutex_unlock(*m)
#define MUTEX_DESTROY(m) \
    STMT_START {						\
	mutex_free(*m);						\
	*m = 0;							\
    } STMT_END

#define COND_INIT(c) \
    STMT_START {						\
	*c = condition_alloc();					\
	if (*c) {						\
	    condition_init(*c);					\
	}							\
	else {							\
	    Perl_croak_nocontext("panic: COND_INIT");		\
	}							\
    } STMT_END

#define COND_SIGNAL(c)		condition_signal(*c)
#define COND_BROADCAST(c)	condition_broadcast(*c)
#define COND_WAIT(c, m)		condition_wait(*c, *m)
#define COND_DESTROY(c) \
    STMT_START {						\
	condition_free(*c);					\
	*c = 0;							\
    } STMT_END

#define THREAD_CREATE(thr, f)	(thr->self = cthread_fork(f, thr), 0)
#define THREAD_POST_CREATE(thr)

#define THREAD_RET_TYPE		any_t
#define THREAD_RET_CAST(x)	((any_t) x)

#define DETACH(t)		cthread_detach(t->self)
#define JOIN(t, avp)		(*(avp) = (AV *)cthread_join(t->self))

#define PERL_SET_CONTEXT(t)	cthread_set_data(cthread_self(), t)
#define PERL_GET_CONTEXT	cthread_data(cthread_self())

#define INIT_THREADS		cthread_init()
#define YIELD			cthread_yield()
#define ALLOC_THREAD_KEY	NOOP
#define FREE_THREAD_KEY		NOOP
#define SET_THREAD_SELF(thr)	(thr->self = cthread_self())

#endif /* I_MACH_CTHREADS */

#ifndef YIELD
#  ifdef SCHED_YIELD
#    define YIELD SCHED_YIELD
#  else
#    ifdef HAS_SCHED_YIELD
#      define YIELD sched_yield()
#    else
#      ifdef HAS_PTHREAD_YIELD
    /* pthread_yield(NULL) platforms are expected
     * to have #defined YIELD for themselves. */
#        define YIELD pthread_yield()
#      endif
#    endif
#  endif
#endif

#ifdef __hpux
#  define MUTEX_INIT_NEEDS_MUTEX_ZEROED
#endif

#ifndef MUTEX_INIT

#  ifdef MUTEX_INIT_NEEDS_MUTEX_ZEROED
    /* Temporary workaround, true bug is deeper. --jhi 1999-02-25 */
#    define MUTEX_INIT(m) \
    STMT_START {						\
	Zero((m), 1, perl_mutex);                               \
 	if (pthread_mutex_init((m), pthread_mutexattr_default))	\
	    Perl_croak_nocontext("panic: MUTEX_INIT");		\
    } STMT_END
#  else
#    define MUTEX_INIT(m) \
    STMT_START {						\
	if (pthread_mutex_init((m), pthread_mutexattr_default))	\
	    Perl_croak_nocontext("panic: MUTEX_INIT");		\
    } STMT_END
#  endif

#  define MUTEX_LOCK(m) \
    STMT_START {						\
	if (pthread_mutex_lock((m)))				\
	    Perl_croak_nocontext("panic: MUTEX_LOCK");		\
    } STMT_END

#  define MUTEX_UNLOCK(m) \
    STMT_START {						\
	if (pthread_mutex_unlock((m)))				\
	    Perl_croak_nocontext("panic: MUTEX_UNLOCK");	\
    } STMT_END

#  define MUTEX_DESTROY(m) \
    STMT_START {						\
	if (pthread_mutex_destroy((m)))				\
	    Perl_croak_nocontext("panic: MUTEX_DESTROY");	\
    } STMT_END
#endif /* MUTEX_INIT */

#ifndef COND_INIT
#  define COND_INIT(c) \
    STMT_START {						\
	if (pthread_cond_init((c), pthread_condattr_default))	\
	    Perl_croak_nocontext("panic: COND_INIT");		\
    } STMT_END

#  define COND_SIGNAL(c) \
    STMT_START {						\
	if (pthread_cond_signal((c)))				\
	    Perl_croak_nocontext("panic: COND_SIGNAL");		\
    } STMT_END

#  define COND_BROADCAST(c) \
    STMT_START {						\
	if (pthread_cond_broadcast((c)))			\
	    Perl_croak_nocontext("panic: COND_BROADCAST");	\
    } STMT_END

#  define COND_WAIT(c, m) \
    STMT_START {						\
	if (pthread_cond_wait((c), (m)))			\
	    Perl_croak_nocontext("panic: COND_WAIT");		\
    } STMT_END

#  define COND_DESTROY(c) \
    STMT_START {						\
	if (pthread_cond_destroy((c)))				\
	    Perl_croak_nocontext("panic: COND_DESTROY");	\
    } STMT_END
#endif /* COND_INIT */

/* DETACH(t) must only be called while holding t->mutex */
#ifndef DETACH
#  define DETACH(t) \
    STMT_START {						\
	if (pthread_detach((t)->self)) {			\
	    MUTEX_UNLOCK(&(t)->mutex);				\
	    Perl_croak_nocontext("panic: DETACH");		\
	}							\
    } STMT_END
#endif /* DETACH */

#ifndef JOIN
#  define JOIN(t, avp) \
    STMT_START {						\
	if (pthread_join((t)->self, (void**)(avp)))		\
	    Perl_croak_nocontext("panic: pthread_join");	\
    } STMT_END
#endif /* JOIN */

#ifndef PERL_GET_CONTEXT
#  define PERL_GET_CONTEXT	pthread_getspecific(PL_thr_key)
#endif

#ifndef PERL_SET_CONTEXT
#  define PERL_SET_CONTEXT(t) \
    STMT_START {						\
	if (pthread_setspecific(PL_thr_key, (void *)(t)))	\
	    Perl_croak_nocontext("panic: pthread_setspecific");	\
    } STMT_END
#endif /* PERL_SET_CONTEXT */

#ifndef INIT_THREADS
#  ifdef NEED_PTHREAD_INIT
#    define INIT_THREADS pthread_init()
#  endif
#endif

#ifndef ALLOC_THREAD_KEY
#  define ALLOC_THREAD_KEY \
    STMT_START {						\
	if (pthread_key_create(&PL_thr_key, 0))	{		\
	    PerlIO_printf(PerlIO_stderr(), "panic: pthread_key_create");	\
	    exit(1);						\
	}							\
    } STMT_END
#endif

#ifndef FREE_THREAD_KEY
#  define FREE_THREAD_KEY \
    STMT_START {						\
	pthread_key_delete(PL_thr_key);				\
    } STMT_END
#endif

#ifndef THREAD_RET_TYPE
#  define THREAD_RET_TYPE	void *
#  define THREAD_RET_CAST(p)	((void *)(p))
#endif /* THREAD_RET */

#if defined(USE_THREADS)

/* Accessor for per-thread SVs */
#  define THREADSV(i) (thr->threadsvp[i])

/*
 * LOCK_SV_MUTEX and UNLOCK_SV_MUTEX are performance-critical. Here, we
 * try only locking them if there may be more than one thread in existence.
 * Systems with very fast mutexes (and/or slow conditionals) may wish to
 * remove the "if (threadnum) ..." test.
 * XXX do NOT use C<if (PL_threadnum) ...> -- it sets up race conditions!
 */
#  define LOCK_SV_MUTEX		MUTEX_LOCK(&PL_sv_mutex)
#  define UNLOCK_SV_MUTEX	MUTEX_UNLOCK(&PL_sv_mutex)
#  define LOCK_STRTAB_MUTEX	MUTEX_LOCK(&PL_strtab_mutex)
#  define UNLOCK_STRTAB_MUTEX	MUTEX_UNLOCK(&PL_strtab_mutex)
#  define LOCK_CRED_MUTEX	MUTEX_LOCK(&PL_cred_mutex)
#  define UNLOCK_CRED_MUTEX	MUTEX_UNLOCK(&PL_cred_mutex)
#  define LOCK_FDPID_MUTEX	MUTEX_LOCK(&PL_fdpid_mutex)
#  define UNLOCK_FDPID_MUTEX	MUTEX_UNLOCK(&PL_fdpid_mutex)
#  define LOCK_SV_LOCK_MUTEX	MUTEX_LOCK(&PL_sv_lock_mutex)
#  define UNLOCK_SV_LOCK_MUTEX	MUTEX_UNLOCK(&PL_sv_lock_mutex)

/* Values and macros for thr->flags */
#define THRf_STATE_MASK	7
#define THRf_R_JOINABLE	0
#define THRf_R_JOINED	1
#define THRf_R_DETACHED	2
#define THRf_ZOMBIE	3
#define THRf_DEAD	4

#define THRf_DID_DIE	8

/* ThrSTATE(t) and ThrSETSTATE(t) must only be called while holding t->mutex */
#define ThrSTATE(t) ((t)->flags & THRf_STATE_MASK)
#define ThrSETSTATE(t, s) STMT_START {		\
	(t)->flags &= ~THRf_STATE_MASK;		\
	(t)->flags |= (s);			\
	DEBUG_S(PerlIO_printf(Perl_debug_log,	\
			      "thread %p set to state %d\n", (t), (s))); \
    } STMT_END

typedef struct condpair {
    perl_mutex	mutex;		/* Protects all other fields */
    perl_cond	owner_cond;	/* For when owner changes at all */
    perl_cond	cond;		/* For cond_signal and cond_broadcast */
    Thread	owner;		/* Currently owning thread */
} condpair_t;

#define MgMUTEXP(mg) (&((condpair_t *)(mg->mg_ptr))->mutex)
#define MgOWNERCONDP(mg) (&((condpair_t *)(mg->mg_ptr))->owner_cond)
#define MgCONDP(mg) (&((condpair_t *)(mg->mg_ptr))->cond)
#define MgOWNER(mg) ((condpair_t *)(mg->mg_ptr))->owner

#endif /* USE_THREADS */
#endif /* USE_THREADS || USE_ITHREADS */

#ifndef MUTEX_LOCK
#  define MUTEX_LOCK(m)
#endif

#ifndef MUTEX_UNLOCK
#  define MUTEX_UNLOCK(m)
#endif

#ifndef MUTEX_INIT
#  define MUTEX_INIT(m)
#endif

#ifndef MUTEX_DESTROY
#  define MUTEX_DESTROY(m)
#endif

#ifndef COND_INIT
#  define COND_INIT(c)
#endif

#ifndef COND_SIGNAL
#  define COND_SIGNAL(c)
#endif

#ifndef COND_BROADCAST
#  define COND_BROADCAST(c)
#endif

#ifndef COND_WAIT
#  define COND_WAIT(c, m)
#endif

#ifndef COND_DESTROY
#  define COND_DESTROY(c)
#endif

#ifndef LOCK_SV_MUTEX
#  define LOCK_SV_MUTEX
#endif

#ifndef UNLOCK_SV_MUTEX
#  define UNLOCK_SV_MUTEX
#endif

#ifndef LOCK_STRTAB_MUTEX
#  define LOCK_STRTAB_MUTEX
#endif

#ifndef UNLOCK_STRTAB_MUTEX
#  define UNLOCK_STRTAB_MUTEX
#endif

#ifndef LOCK_CRED_MUTEX
#  define LOCK_CRED_MUTEX
#endif

#ifndef UNLOCK_CRED_MUTEX
#  define UNLOCK_CRED_MUTEX
#endif

#ifndef LOCK_FDPID_MUTEX
#  define LOCK_FDPID_MUTEX
#endif

#ifndef UNLOCK_FDPID_MUTEX
#  define UNLOCK_FDPID_MUTEX
#endif

#ifndef LOCK_SV_LOCK_MUTEX
#  define LOCK_SV_LOCK_MUTEX
#endif

#ifndef UNLOCK_SV_LOCK_MUTEX
#  define UNLOCK_SV_LOCK_MUTEX
#endif

/* THR, SET_THR, and dTHR are there for compatibility with old versions */
#ifndef THR
#  define THR		PERL_GET_THX
#endif

#ifndef SET_THR
#  define SET_THR(t)	PERL_SET_THX(t)
#endif

#ifndef dTHR
#  define dTHR dNOOP
#endif

#ifndef INIT_THREADS
#  define INIT_THREADS NOOP
#endif
