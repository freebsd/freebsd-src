# $Id: Makefile.inc,v 1.4 1998/07/31 09:09:19 phk Exp $

# POSIX thread man files

.PATH: ${.CURDIR}/man

MAN3+=	pthread_cleanup_pop.3 \
	pthread_cleanup_push.3 \
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
	pthread_mutex_destroy.3 \
	pthread_mutex_init.3 \
	pthread_mutex_lock.3 \
	pthread_mutex_trylock.3 \
	pthread_mutex_unlock.3 \
	pthread_once.3 \
	pthread_self.3 \
	pthread_setspecific.3
