# $FreeBSD$

# POSIX thread man files

.PATH: ${.CURDIR}/man

MAN+=	pthread_attr.3 \
	pthread_attr_get_np.3 \
	pthread_attr_setcreatesuspend_np.3 \
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
	pthread_mutex_trylock.3 \
	pthread_mutex_unlock.3 \
	pthread_once.3 \
	pthread_resume_all_np.3 \
	pthread_resume_np.3 \
	pthread_rwlock_destroy.3 \
	pthread_rwlock_init.3 \
	pthread_rwlock_rdlock.3 \
	pthread_rwlock_unlock.3 \
	pthread_rwlock_wrlock.3 \
	pthread_rwlockattr_destroy.3 \
	pthread_rwlockattr_getpshared.3 \
	pthread_rwlockattr_init.3 \
	pthread_rwlockattr_setpshared.3 \
	pthread_schedparam.3 \
	pthread_self.3 \
	pthread_set_name_np.3 \
	pthread_setspecific.3 \
	pthread_sigmask.3 \
	pthread_suspend_all_np.3 \
	pthread_suspend_np.3 \
	pthread_switch_add_np.3 \
	pthread_testcancel.3 \
	sem_destroy.3 \
	sem_getvalue.3 \
	sem_init.3 \
	sem_open.3 \
	sem_post.3 \
	sem_wait.3 \
	sigwait.3

MLINKS+= \
	pthread_attr.3 pthread_attr_destroy.3 \
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
	pthread_attr.3 pthread_attr_setsstack.3 \
	pthread_attr.3 pthread_attr_setstackaddr.3 \
	pthread_attr.3 pthread_attr_setstacksize.3 \
	pthread_condattr.3 pthread_condattr_init.3 \
	pthread_condattr.3 pthread_condattr_destroy.3 \
	pthread_multi_np.3 pthread_single_np.3 \
	pthread_mutexattr.3 pthread_mutexattr_init.3 \
	pthread_mutexattr.3 pthread_mutexattr_destroy.3 \
	pthread_mutexattr.3 pthread_mutexattr_getprioceiling.3 \
	pthread_mutexattr.3 pthread_mutexattr_getprotocol.3 \
	pthread_mutexattr.3 pthread_mutexattr_gettype.3 \
	pthread_mutexattr.3 pthread_mutexattr_setprioceiling.3 \
	pthread_mutexattr.3 pthread_mutexattr_setprotocol.3 \
	pthread_mutexattr.3 pthread_mutexattr_settype.3 \
	pthread_mutexattr_getkind_np.3 pthread_mutexattr_setkind_np.3 \
	pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3 \
	pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3 \
	pthread_schedparam.3 pthread_getschedparam.3 \
	pthread_schedparam.3 pthread_setschedparam.3 \
	pthread_switch_add_np.3 pthread_switch_delete_np.3 \
	pthread_testcancel.3 pthread_setcancelstate.3 \
	pthread_testcancel.3 pthread_setcanceltype.3 \
	sem_open.3 sem_close.3 \
	sem_open.3 sem_unlink.3 \
	sem_wait.3 sem_trywait.3
