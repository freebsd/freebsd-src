#ifdef USE_THREADS

#ifdef WIN32
#  include <win32thread.h>
#else

#ifndef DJGPP
/* POSIXish threads */
#ifdef OLD_PTHREADS_API
#  define pthread_mutexattr_init(a) pthread_mutexattr_create(a)
#  define pthread_mutexattr_settype(a,t) pthread_mutexattr_setkind_np(a,t)
#  define pthread_key_create(k,d) pthread_keycreate(k,(pthread_destructor_t)(d))
#  define YIELD pthread_yield()
#  define DETACH(t)				\
    STMT_START {				\
	if (pthread_detach(&(t)->self)) {	\
	    MUTEX_UNLOCK(&(t)->mutex);		\
	    croak("panic: DETACH");		\
	}					\
    } STMT_END
#else
#  define pthread_mutexattr_default NULL
#  define pthread_condattr_default NULL
#endif /* OLD_PTHREADS_API */
#endif
#endif

#ifdef PTHREADS_CREATED_JOINABLE
#  define ATTR_JOINABLE PTHREAD_CREATE_JOINABLE
#else
#  ifdef PTHREAD_CREATE_UNDETACHED
#    define ATTR_JOINABLE PTHREAD_CREATE_UNDETACHED
#  else
#    define ATTR_JOINABLE PTHREAD_CREATE_JOINABLE
#  endif
#endif

#ifdef I_MACH_CTHREADS

/* cthreads interface */

/* #include <mach/cthreads.h> is in perl.h #ifdef I_MACH_CTHREADS */

#define MUTEX_INIT(m)					\
	STMT_START {					\
		*m = mutex_alloc();			\
		if (*m) {				\
			mutex_init(*m);			\
		} else {				\
			croak("panic: MUTEX_INIT");	\
		}					\
	} STMT_END

#define MUTEX_LOCK(m)		mutex_lock(*m)
#define MUTEX_UNLOCK(m)		mutex_unlock(*m)
#define MUTEX_DESTROY(m)				\
	STMT_START {					\
		mutex_free(*m);				\
		*m = 0;					\
	} STMT_END

#define COND_INIT(c)					\
	STMT_START {					\
		*c = condition_alloc();			\
		if (*c) {				\
			condition_init(*c);		\
		} else {				\
			croak("panic: COND_INIT");	\
		}					\
	} STMT_END

#define COND_SIGNAL(c)		condition_signal(*c)
#define COND_BROADCAST(c)	condition_broadcast(*c)
#define COND_WAIT(c, m)		condition_wait(*c, *m)
#define COND_DESTROY(c)				\
	STMT_START {				\
		condition_free(*c);		\
		*c = 0;				\
	} STMT_END

#define THREAD_CREATE(thr, f)	(thr->self = cthread_fork(f, thr), 0)
#define THREAD_POST_CREATE(thr)

#define THREAD_RET_TYPE		any_t
#define THREAD_RET_CAST(x)	((any_t) x)

#define DETACH(t)		cthread_detach(t->self)
#define JOIN(t, avp)		(*(avp) = (AV *)cthread_join(t->self))

#define SET_THR(thr)		cthread_set_data(cthread_self(), thr)
#define THR			cthread_data(cthread_self())

#define INIT_THREADS		cthread_init()
#define YIELD			cthread_yield()
#define ALLOC_THREAD_KEY
#define SET_THREAD_SELF(thr)	(thr->self = cthread_self())

#endif /* I_MACH_CTHREADS */

#ifndef YIELD
#  ifdef HAS_SCHED_YIELD
#    define YIELD sched_yield()
#  else
#    ifdef HAS_PTHREAD_YIELD
#      define YIELD pthread_yield()
#    endif
#  endif
#endif

#ifdef __hpux
#  define MUTEX_INIT_NEEDS_MUTEX_ZEROED
#endif

#ifndef MUTEX_INIT
#ifdef MUTEX_INIT_NEEDS_MUTEX_ZEROED
    /* Temporary workaround, true bug is deeper. --jhi 1999-02-25 */
#define MUTEX_INIT(m)						\
    STMT_START {						\
	Zero((m), 1, perl_mutex);                               \
 	if (pthread_mutex_init((m), pthread_mutexattr_default))	\
	    croak("panic: MUTEX_INIT");				\
    } STMT_END
#else
#define MUTEX_INIT(m)						\
    STMT_START {						\
	if (pthread_mutex_init((m), pthread_mutexattr_default))	\
	    croak("panic: MUTEX_INIT");				\
    } STMT_END
#endif
#define MUTEX_LOCK(m)				\
    STMT_START {				\
	if (pthread_mutex_lock((m)))		\
	    croak("panic: MUTEX_LOCK");		\
    } STMT_END
#define MUTEX_UNLOCK(m)				\
    STMT_START {				\
	if (pthread_mutex_unlock((m)))		\
	    croak("panic: MUTEX_UNLOCK");	\
    } STMT_END
#define MUTEX_DESTROY(m)			\
    STMT_START {				\
	if (pthread_mutex_destroy((m)))		\
	    croak("panic: MUTEX_DESTROY");	\
    } STMT_END
#endif /* MUTEX_INIT */

#ifndef COND_INIT
#define COND_INIT(c)						\
    STMT_START {						\
	if (pthread_cond_init((c), pthread_condattr_default))	\
	    croak("panic: COND_INIT");				\
    } STMT_END
#define COND_SIGNAL(c)				\
    STMT_START {				\
	if (pthread_cond_signal((c)))		\
	    croak("panic: COND_SIGNAL");	\
    } STMT_END
#define COND_BROADCAST(c)			\
    STMT_START {				\
	if (pthread_cond_broadcast((c)))	\
	    croak("panic: COND_BROADCAST");	\
    } STMT_END
#define COND_WAIT(c, m)				\
    STMT_START {				\
	if (pthread_cond_wait((c), (m)))	\
	    croak("panic: COND_WAIT");		\
    } STMT_END
#define COND_DESTROY(c)				\
    STMT_START {				\
	if (pthread_cond_destroy((c)))		\
	    croak("panic: COND_DESTROY");	\
    } STMT_END
#endif /* COND_INIT */

/* DETACH(t) must only be called while holding t->mutex */
#ifndef DETACH
#define DETACH(t)				\
    STMT_START {				\
	if (pthread_detach((t)->self)) {	\
	    MUTEX_UNLOCK(&(t)->mutex);		\
	    croak("panic: DETACH");		\
	}					\
    } STMT_END
#endif /* DETACH */

#ifndef JOIN
#define JOIN(t, avp) 					\
    STMT_START {					\
	if (pthread_join((t)->self, (void**)(avp)))	\
	    croak("panic: pthread_join");		\
    } STMT_END
#endif /* JOIN */

#ifndef SET_THR
#define SET_THR(t)					\
    STMT_START {					\
	if (pthread_setspecific(PL_thr_key, (void *) (t)))	\
	    croak("panic: pthread_setspecific");	\
    } STMT_END
#endif /* SET_THR */

#ifndef THR
#  ifdef OLD_PTHREADS_API
struct perl_thread *getTHR _((void));
#    define THR getTHR()
#  else
#    define THR ((struct perl_thread *) pthread_getspecific(PL_thr_key))
#  endif /* OLD_PTHREADS_API */
#endif /* THR */

/*
 * dTHR is performance-critical. Here, we only do the pthread_get_specific
 * if there may be more than one thread in existence, otherwise we get thr
 * from thrsv which is cached in the per-interpreter structure.
 * Systems with very fast pthread_get_specific (which should be all systems
 * but unfortunately isn't) may wish to simplify to "...*thr = THR".
 *
 * The use of PL_threadnum should be safe here.
 */
#ifndef dTHR
#  define dTHR \
    struct perl_thread *thr = PL_threadnum? THR : (struct perl_thread*)SvPVX(PL_thrsv)
#endif /* dTHR */

#ifndef INIT_THREADS
#  ifdef NEED_PTHREAD_INIT
#    define INIT_THREADS pthread_init()
#  else
#    define INIT_THREADS NOOP
#  endif
#endif

/* Accessor for per-thread SVs */
#define THREADSV(i) (thr->threadsvp[i])

/*
 * LOCK_SV_MUTEX and UNLOCK_SV_MUTEX are performance-critical. Here, we
 * try only locking them if there may be more than one thread in existence.
 * Systems with very fast mutexes (and/or slow conditionals) may wish to
 * remove the "if (threadnum) ..." test.
 * XXX do NOT use C<if (PL_threadnum) ...> -- it sets up race conditions!
 */
#define LOCK_SV_MUTEX				\
    STMT_START {				\
	MUTEX_LOCK(&PL_sv_mutex);		\
    } STMT_END

#define UNLOCK_SV_MUTEX				\
    STMT_START {				\
	MUTEX_UNLOCK(&PL_sv_mutex);		\
    } STMT_END

/* Likewise for strtab_mutex */
#define LOCK_STRTAB_MUTEX			\
    STMT_START {				\
	MUTEX_LOCK(&PL_strtab_mutex);	\
    } STMT_END

#define UNLOCK_STRTAB_MUTEX			\
    STMT_START {				\
	MUTEX_UNLOCK(&PL_strtab_mutex);	\
    } STMT_END

#ifndef THREAD_RET_TYPE
#  define THREAD_RET_TYPE	void *
#  define THREAD_RET_CAST(p)	((void *)(p))
#endif /* THREAD_RET */


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
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),	\
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

#else
/* USE_THREADS is not defined */
#define MUTEX_LOCK(m)
#define MUTEX_UNLOCK(m)
#define MUTEX_INIT(m)
#define MUTEX_DESTROY(m)
#define COND_INIT(c)
#define COND_SIGNAL(c)
#define COND_BROADCAST(c)
#define COND_WAIT(c, m)
#define COND_DESTROY(c)
#define LOCK_SV_MUTEX
#define UNLOCK_SV_MUTEX
#define LOCK_STRTAB_MUTEX
#define UNLOCK_STRTAB_MUTEX

#define THR
/* Rats: if dTHR is just blank then the subsequent ";" throws an error */
#ifdef WIN32
#define dTHR extern int Perl___notused
#else
#define dTHR extern int errno
#endif
#endif /* USE_THREADS */
