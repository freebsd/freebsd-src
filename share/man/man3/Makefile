#	@(#)Makefile	8.2 (Berkeley) 12/13/93
# $FreeBSD$

MAN=		assert.3 \
		bitstring.3 \
		end.3 \
		fpgetround.3 \
		intro.3 \
		${PTHREAD_MAN} \
		queue.3 \
		stdarg.3 \
		sysexits.3 \
		tgmath.3 \
		timeradd.3 \
		tree.3

MLINKS=		bitstring.3 bit_alloc.3 \
		bitstring.3 bit_clear.3 \
		bitstring.3 bit_decl.3 \
		bitstring.3 bit_ffc.3 \
		bitstring.3 bit_ffs.3 \
		bitstring.3 bit_nclear.3 \
		bitstring.3 bit_nset.3 \
		bitstring.3 bit_set.3 \
		bitstring.3 bitstr_size.3 \
		bitstring.3 bit_test.3
MLINKS+=	end.3 edata.3 \
		end.3 etext.3
MLINKS+=	fpgetround.3 fpgetmask.3 \
		fpgetround.3 fpgetprec.3 \
		fpgetround.3 fpgetsticky.3 \
		fpgetround.3 fpresetsticky.3 \
		fpgetround.3 fpsetmask.3 \
		fpgetround.3 fpsetprec.3 \
		fpgetround.3 fpsetround.3
MLINKS+=	${PTHREAD_MLINKS}
MLINKS+=	queue.3 LIST_EMPTY.3 \
		queue.3 LIST_ENTRY.3 \
		queue.3 LIST_FIRST.3 \
		queue.3 LIST_FOREACH.3 \
		queue.3 LIST_FOREACH_SAFE.3 \
		queue.3 LIST_HEAD.3 \
		queue.3 LIST_HEAD_INITIALIZER.3 \
		queue.3 LIST_INIT.3 \
		queue.3 LIST_INSERT_AFTER.3 \
		queue.3 LIST_INSERT_BEFORE.3 \
		queue.3 LIST_INSERT_HEAD.3 \
		queue.3 LIST_NEXT.3 \
		queue.3 LIST_REMOVE.3 \
		queue.3 SLIST_EMPTY.3 \
		queue.3 SLIST_ENTRY.3 \
		queue.3 SLIST_FIRST.3 \
		queue.3 SLIST_FOREACH.3 \
		queue.3 SLIST_FOREACH_SAFE.3 \
		queue.3 SLIST_HEAD.3 \
		queue.3 SLIST_HEAD_INITIALIZER.3 \
		queue.3 SLIST_INIT.3 \
		queue.3 SLIST_INSERT_AFTER.3 \
		queue.3 SLIST_INSERT_HEAD.3 \
		queue.3 SLIST_NEXT.3 \
		queue.3 SLIST_REMOVE.3 \
		queue.3 SLIST_REMOVE_HEAD.3 \
		queue.3 STAILQ_CONCAT.3 \
		queue.3 STAILQ_EMPTY.3 \
		queue.3 STAILQ_ENTRY.3 \
		queue.3 STAILQ_FIRST.3 \
		queue.3 STAILQ_FOREACH.3 \
		queue.3 STAILQ_FOREACH_SAFE.3 \
		queue.3 STAILQ_HEAD.3 \
		queue.3 STAILQ_HEAD_INITIALIZER.3 \
		queue.3 STAILQ_INIT.3 \
		queue.3 STAILQ_INSERT_AFTER.3 \
		queue.3 STAILQ_INSERT_HEAD.3 \
		queue.3 STAILQ_INSERT_TAIL.3 \
		queue.3 STAILQ_LAST.3 \
		queue.3 STAILQ_NEXT.3 \
		queue.3 STAILQ_REMOVE.3 \
		queue.3 STAILQ_REMOVE_HEAD.3 \
		queue.3 TAILQ_CONCAT.3 \
		queue.3 TAILQ_EMPTY.3 \
		queue.3 TAILQ_ENTRY.3 \
		queue.3 TAILQ_FIRST.3 \
		queue.3 TAILQ_FOREACH.3 \
		queue.3 TAILQ_FOREACH_REVERSE.3 \
		queue.3 TAILQ_FOREACH_REVERSE_SAFE.3 \
		queue.3 TAILQ_FOREACH_SAFE.3 \
		queue.3 TAILQ_HEAD.3 \
		queue.3 TAILQ_HEAD_INITIALIZER.3 \
		queue.3 TAILQ_INIT.3 \
		queue.3 TAILQ_INSERT_AFTER.3 \
		queue.3 TAILQ_INSERT_BEFORE.3 \
		queue.3 TAILQ_INSERT_HEAD.3 \
		queue.3 TAILQ_INSERT_TAIL.3 \
		queue.3 TAILQ_LAST.3 \
		queue.3 TAILQ_NEXT.3 \
		queue.3 TAILQ_PREV.3 \
		queue.3 TAILQ_REMOVE.3
MLINKS+=	stdarg.3 va_arg.3 \
		stdarg.3 va_copy.3 \
		stdarg.3 va_end.3 \
		stdarg.3 varargs.3 \
		stdarg.3 va_start.3
MLINKS+=	timeradd.3 timerclear.3 \
		timeradd.3 timercmp.3 \
		timeradd.3 timerisset.3 \
		timeradd.3 timersub.3
MLINKS+=	tree.3 RB_EMPTY.3 \
		tree.3 RB_ENTRY.3 \
		tree.3 RB_FIND.3 \
		tree.3 RB_FOREACH.3 \
		tree.3 RB_GENERATE.3 \
		tree.3 RB_HEAD.3 \
		tree.3 RB_INIT.3 \
		tree.3 RB_INITIALIZER.3 \
		tree.3 RB_INSERT.3 \
		tree.3 RB_LEFT.3 \
		tree.3 RB_MAX.3 \
		tree.3 RB_MIN.3 \
		tree.3 RB_NEXT.3 \
		tree.3 RB_PARENT.3 \
		tree.3 RB_PROTOTYPE.3 \
		tree.3 RB_REMOVE.3 \
		tree.3 RB_RIGHT.3 \
		tree.3 RB_ROOT.3 \
		tree.3 SPLAY_EMPTY.3 \
		tree.3 SPLAY_ENTRY.3 \
		tree.3 SPLAY_FIND.3 \
		tree.3 SPLAY_FOREACH.3 \
		tree.3 SPLAY_GENERATE.3 \
		tree.3 SPLAY_HEAD.3 \
		tree.3 SPLAY_INIT.3 \
		tree.3 SPLAY_INITIALIZER.3 \
		tree.3 SPLAY_INSERT.3 \
		tree.3 SPLAY_LEFT.3 \
		tree.3 SPLAY_MAX.3 \
		tree.3 SPLAY_MIN.3 \
		tree.3 SPLAY_NEXT.3 \
		tree.3 SPLAY_PROTOTYPE.3 \
		tree.3 SPLAY_REMOVE.3 \
		tree.3 SPLAY_RIGHT.3 \
		tree.3 SPLAY_ROOT.3

.if !defined(NO_LIBC_R) || !defined(NO_LIBPTHREAD) || !defined(NO_LIBTHR)
PTHREAD_MAN=	pthread.3 \
		pthread_atfork.3 \
		pthread_attr.3 \
		pthread_attr_get_np.3 \
		pthread_attr_setcreatesuspend_np.3 \
		pthread_barrierattr.3 \
		pthread_barrier_destroy.3 \
		pthread_cancel.3 \
		pthread_cleanup_pop.3 \
		pthread_cleanup_push.3 \
		pthread_condattr.3 \
		pthread_cond_broadcast.3 \
		pthread_cond_destroy.3 \
		pthread_cond_init.3 \
		pthread_cond_signal.3 \
		pthread_cond_timedwait.3 \
		pthread_cond_wait.3 \
		pthread_create.3 \
		pthread_detach.3 \
		pthread_equal.3 \
		pthread_exit.3 \
		pthread_getconcurrency.3 \
		pthread_getspecific.3 \
		pthread_join.3 \
		pthread_key_create.3 \
		pthread_key_delete.3 \
		pthread_kill.3 \
		pthread_main_np.3 \
		pthread_multi_np.3 \
		pthread_mutexattr.3 \
		pthread_mutexattr_getkind_np.3 \
		pthread_mutex_destroy.3 \
		pthread_mutex_init.3 \
		pthread_mutex_lock.3 \
		pthread_mutex_timedlock.3 \
		pthread_mutex_trylock.3 \
		pthread_mutex_unlock.3 \
		pthread_once.3 \
		pthread_resume_all_np.3 \
		pthread_resume_np.3 \
		pthread_rwlockattr_destroy.3 \
		pthread_rwlockattr_getpshared.3 \
		pthread_rwlockattr_init.3 \
		pthread_rwlockattr_setpshared.3 \
		pthread_rwlock_destroy.3 \
		pthread_rwlock_init.3 \
		pthread_rwlock_rdlock.3 \
		pthread_rwlock_timedrdlock.3 \
		pthread_rwlock_timedwrlock.3 \
		pthread_rwlock_unlock.3 \
		pthread_rwlock_wrlock.3 \
		pthread_schedparam.3 \
		pthread_self.3 \
		pthread_set_name_np.3 \
		pthread_setspecific.3 \
		pthread_sigmask.3 \
		pthread_spin_init.3 \
		pthread_spin_lock.3 \
		pthread_suspend_all_np.3 \
		pthread_suspend_np.3 \
		pthread_switch_add_np.3 \
		pthread_testcancel.3 \
		pthread_yield.3

PTHREAD_MLINKS=	pthread_attr.3 pthread_attr_destroy.3 \
		pthread_attr.3 pthread_attr_getdetachstate.3 \
		pthread_attr.3 pthread_attr_getguardsize.3 \
		pthread_attr.3 pthread_attr_getinheritsched.3 \
		pthread_attr.3 pthread_attr_getschedparam.3 \
		pthread_attr.3 pthread_attr_getschedpolicy.3 \
		pthread_attr.3 pthread_attr_getscope.3 \
		pthread_attr.3 pthread_attr_getstack.3 \
		pthread_attr.3 pthread_attr_getstackaddr.3 \
		pthread_attr.3 pthread_attr_getstacksize.3 \
		pthread_attr.3 pthread_attr_init.3 \
		pthread_attr.3 pthread_attr_setdetachstate.3 \
		pthread_attr.3 pthread_attr_setguardsize.3 \
		pthread_attr.3 pthread_attr_setinheritsched.3 \
		pthread_attr.3 pthread_attr_setschedparam.3 \
		pthread_attr.3 pthread_attr_setschedpolicy.3 \
		pthread_attr.3 pthread_attr_setscope.3 \
		pthread_attr.3 pthread_attr_setstack.3 \
		pthread_attr.3 pthread_attr_setstackaddr.3 \
		pthread_attr.3 pthread_attr_setstacksize.3
PTHREAD_MLINKS+=pthread_barrierattr.3 pthread_barrierattr_destroy.3 \
		pthread_barrierattr.3 pthread_barrierattr_getpshared.3 \
		pthread_barrierattr.3 pthread_barrierattr_init.3 \
		pthread_barrierattr.3 pthread_barrierattr_setpshared.3
PTHREAD_MLINKS+=pthread_barrier_destroy.3 pthread_barrier_init.3 \
		pthread_barrier_destroy.3 pthread_barrier_wait.3
PTHREAD_MLINKS+=pthread_condattr.3 pthread_condattr_destroy.3 \
		pthread_condattr.3 pthread_condattr_init.3
PTHREAD_MLINKS+=pthread_getconcurrency.3 pthread_setconcurrency.3
PTHREAD_MLINKS+=pthread_multi_np.3 pthread_single_np.3
PTHREAD_MLINKS+=pthread_mutexattr.3 pthread_mutexattr_destroy.3 \
		pthread_mutexattr.3 pthread_mutexattr_getprioceiling.3 \
		pthread_mutexattr.3 pthread_mutexattr_getprotocol.3 \
		pthread_mutexattr.3 pthread_mutexattr_gettype.3 \
		pthread_mutexattr.3 pthread_mutexattr_init.3 \
		pthread_mutexattr.3 pthread_mutexattr_setprioceiling.3 \
		pthread_mutexattr.3 pthread_mutexattr_setprotocol.3 \
		pthread_mutexattr.3 pthread_mutexattr_settype.3
PTHREAD_MLINKS+=pthread_mutexattr_getkind_np.3 pthread_mutexattr_setkind_np.3
PTHREAD_MLINKS+=pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3
PTHREAD_MLINKS+=pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3
PTHREAD_MLINKS+=pthread_schedparam.3 pthread_getschedparam.3 \
		pthread_schedparam.3 pthread_setschedparam.3
PTHREAD_MLINKS+=pthread_spin_init.3 pthread_spin_destroy.3 \
		pthread_spin_lock.3 pthread_spin_trylock.3 \
		pthread_spin_lock.3 pthread_spin_unlock.3
PTHREAD_MLINKS+=pthread_switch_add_np.3 pthread_switch_delete_np.3
PTHREAD_MLINKS+=pthread_testcancel.3 pthread_setcancelstate.3 \
		pthread_testcancel.3 pthread_setcanceltype.3
.endif

.include <bsd.prog.mk>
