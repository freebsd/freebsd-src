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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _NAMESPACE_H_
#define _NAMESPACE_H_

/*
 * Adjust names so that headers declare "hidden" names.
 */

/*
 * ISO C (C90) section.  Most names in libc aren't in ISO C, so they
 * should be here.  Most aren't here...
 */
#define		err				_err
#define		warn				_warn

/*
 * Prototypes for syscalls/functions that need to be overridden
 * in libc_r/libpthread.
 */
#define		accept				_accept
#define		__acl_aclcheck_fd		___acl_aclcheck_fd
#define		__acl_delete_fd			___acl_delete_fd
#define		__acl_get_fd			___acl_get_fd
#define		__acl_set_fd			___acl_set_fd
#define		bind				_bind
#define		__cap_get_fd			___cap_get_fd
#define		__cap_set_fd			___cap_set_fd
#define		close				_close
#define		connect				_connect
#define		dup				_dup
#define		dup2				_dup2
#define		execve				_execve
#define		fcntl				_fcntl
/*#define		flock				_flock */
#define		flockfile			_flockfile
#define		fstat				_fstat
#define		fstatfs				_fstatfs
#define		fsync				_fsync
#define		funlockfile			_funlockfile
#define		getdirentries			_getdirentries
#define		getlogin			_getlogin
#define		getpeername			_getpeername
#define		getsockname			_getsockname
#define		getsockopt			_getsockopt
#define		ioctl				_ioctl
/* #define		kevent				_kevent */
#define		listen				_listen
#define		nanosleep			_nanosleep
#define		open				_open
#define		poll				_poll
#define		pthread_cond_signal		_pthread_cond_signal
#define		pthread_cond_wait		_pthread_cond_wait
#define		pthread_cond_init		_pthread_cond_init
#define		pthread_exit			_pthread_exit
#define		pthread_getspecific		_pthread_getspecific
#define		pthread_key_create		_pthread_key_create
#define		pthread_key_delete		_pthread_key_delete
#define		pthread_main_np			_pthread_main_np
#define		pthread_mutex_destroy		_pthread_mutex_destroy
#define		pthread_mutex_init		_pthread_mutex_init
#define		pthread_mutex_lock		_pthread_mutex_lock
#define		pthread_mutex_trylock		_pthread_mutex_trylock
#define		pthread_mutex_unlock		_pthread_mutex_unlock
#define		pthread_mutexattr_init		_pthread_mutexattr_init
#define		pthread_mutexattr_destroy	_pthread_mutexattr_destroy
#define		pthread_mutexattr_settype	_pthread_mutexattr_settype
#define		pthread_once			_pthread_once
#define		pthread_rwlock_init		_pthread_rwlock_init
#define		pthread_rwlock_rdlock		_pthread_rwlock_rdlock
#define		pthread_rwlock_wrlock		_pthread_rwlock_wrlock
#define		pthread_rwlock_unlock		_pthread_rwlock_unlock
#define		pthread_self			_pthread_self
#define		pthread_setspecific		_pthread_setspecific
#define		pthread_sigmask			_pthread_sigmask
#define		read				_read
#define		readv				_readv
#define		recvfrom			_recvfrom
#define		recvmsg				_recvmsg
#define		select				_select
#define		sendmsg				_sendmsg
#define		sendto				_sendto
#define		setsockopt			_setsockopt
/*#define		sigaction			_sigaction*/
#define		sigprocmask			_sigprocmask
#define		sigsuspend			_sigsuspend
#define		socket				_socket
#define		socketpair			_socketpair
#define		wait4				_wait4
#define		write				_write
#define		writev				_writev


/*
 * Other hidden syscalls/functions that libc_r needs to override
 * but are not used internally by libc.
 *
 * XXX - When modifying libc to use one of the following, remove
 * the prototype from below and place it in the list above.
 */
#if 0
#define		creat				_creat
#define		fchflags			_fchflags
#define		fchmod				_fchmod
#define		fpathconf			_fpathconf
#define		ftrylockfile			_ftrylockfile
#define		msync				_msync
#define		nfssvc				_nfssvc
#define		pause				_pause
#define		pthread_rwlock_destroy		_pthread_rwlock_destroy
#define		pthread_rwlock_tryrdlock	_pthread_rwlock_tryrdlock
#define		pthread_rwlock_trywrlock	_pthread_rwlock_trywrlock
#define		pthread_rwlockattr_init		_pthread_rwlockattr_init
#define		pthread_rwlockattr_destroy	_pthread_rwlockattr_destroy
#define		sched_yield			_sched_yield
#define		sendfile			_sendfile
#define		shutdown			_shutdown
#define		sigaltstack			_sigaltstack
#define		sigpending			_sigpending
#define		sigreturn			_sigreturn
#define		sigsetmask			_sigsetmask
#define		sleep				_sleep
#define		system				_system
#define		tcdrain				_tcdrain
#define		wait				_wait
#define		waitpid				_waitpid
#endif

#endif /* _NAMESPACE_H_ */
