/*
 * Copyright (c) 2001 Daniel Eischen <deischen@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <signal.h>
#include <pthread.h>
#include <pthread_np.h>

/*
 * Weak symbols: All libc internal usage of these functions should
 * use the weak symbol versions (_pthread_XXX).  If libpthread is
 * linked, it will override these functions with (non-weak) routines.
 * The _pthread_XXX functions are provided solely for internal libc
 * usage to avoid unwanted cancellation points and to differentiate
 * between application locks and libc locks (threads holding the
 * latter can't be allowed to exit/terminate).
 */
__weak_reference(_pthread_cond_init_stub,	_pthread_cond_init);
__weak_reference(_pthread_cond_signal_stub,	_pthread_cond_signal);
__weak_reference(_pthread_cond_wait_stub,	_pthread_cond_wait);
__weak_reference(_pthread_getspecific_stub,	_pthread_getspecific);
__weak_reference(_pthread_key_create_stub,	_pthread_key_create);
__weak_reference(_pthread_key_delete_stub,	_pthread_key_delete);
__weak_reference(_pthread_main_np_stub,		_pthread_main_np);
__weak_reference(_pthread_mutex_destroy_stub,	_pthread_mutex_destroy);
__weak_reference(_pthread_mutex_init_stub,	_pthread_mutex_init);
__weak_reference(_pthread_mutex_lock_stub,	_pthread_mutex_lock);
__weak_reference(_pthread_mutex_trylock_stub,	_pthread_mutex_trylock);
__weak_reference(_pthread_mutex_unlock_stub,	_pthread_mutex_unlock);
__weak_reference(_pthread_mutexattr_init_stub,	_pthread_mutexattr_init);
__weak_reference(_pthread_mutexattr_destroy_stub, _pthread_mutexattr_destroy);
__weak_reference(_pthread_mutexattr_settype_stub, _pthread_mutexattr_settype);
__weak_reference(_pthread_once_stub,		_pthread_once);
__weak_reference(_pthread_self_stub,		_pthread_self);
__weak_reference(_pthread_rwlock_init_stub,	_pthread_rwlock_init);
__weak_reference(_pthread_rwlock_rdlock_stub,	_pthread_rwlock_rdlock);
__weak_reference(_pthread_rwlock_tryrdlock_stub, _pthread_rwlock_tryrdlock);
__weak_reference(_pthread_rwlock_trywrlock_stub, _pthread_rwlock_trywrloc);
__weak_reference(_pthread_rwlock_unlock_stub,	_pthread_rwlock_unlock);
__weak_reference(_pthread_rwlock_wrlock_stub,	_pthread_rwlock_wrlock); 
__weak_reference(_pthread_setspecific_stub,	_pthread_setspecific);
__weak_reference(_pthread_sigmask_stub,		_pthread_sigmask);

/* Define a null pthread structure just to satisfy _pthread_self. */
struct pthread {
};

static struct pthread	main_thread;

int
_pthread_cond_init_stub(pthread_cond_t *cond,
    const pthread_condattr_t *cond_attr)
{
	return (0);
}

int
_pthread_cond_signal_stub(pthread_cond_t *cond)
{
	return (0);
}

int
_pthread_cond_wait_stub(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	return (0);
}

void *
_pthread_getspecific_stub(pthread_key_t key)
{
	return (NULL);
}

int
_pthread_key_create_stub(pthread_key_t *key, void (*destructor) (void *))
{
	return (0);
}

int
_pthread_key_delete_stub(pthread_key_t key)
{
	return (0);
}

int
_pthread_main_np_stub()
{
	return (-1);
}

int
_pthread_mutex_destroy_stub(pthread_mutex_t *mattr)
{
	return (0);
}

int
_pthread_mutex_init_stub(pthread_mutex_t *mutex, const pthread_mutexattr_t *mattr)
{
	return (0);
}

int
_pthread_mutex_lock_stub(pthread_mutex_t *mutex)
{
	return (0);
}

int
_pthread_mutex_trylock_stub(pthread_mutex_t *mutex)
{
	return (0);
}

int
_pthread_mutex_unlock_stub(pthread_mutex_t *mutex)
{
	return (0);
}

int
_pthread_mutexattr_init_stub(pthread_mutexattr_t *mattr)
{
	return (0);
}

int
_pthread_mutexattr_destroy_stub(pthread_mutexattr_t *mattr)
{
	return (0);
}

int
_pthread_mutexattr_settype_stub(pthread_mutexattr_t *mattr, int type)
{
	return (0);
}

int
_pthread_once_stub(pthread_once_t *once_control, void (*init_routine) (void))
{
	return (0);
}

int
_pthread_rwlock_init_stub(pthread_rwlock_t *rwlock,
    const pthread_rwlockattr_t *attr)
{
	return (0); 
}

int
_pthread_rwlock_destroy_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_rdlock_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_tryrdlock_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_trywrlock_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_unlock_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

int
_pthread_rwlock_wrlock_stub(pthread_rwlock_t *rwlock)
{
	return (0);
}

pthread_t
_pthread_self_stub(void)
{
	return (&main_thread);
}

int
_pthread_setspecific_stub(pthread_key_t key, const void *value)
{
	return (0);
}

int
_pthread_sigmask_stub(int how, const sigset_t *set, sigset_t *oset)
{
	return (0);
}
