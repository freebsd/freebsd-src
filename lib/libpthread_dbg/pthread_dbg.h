/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _PTHREAD_DBG_H
#define _PTHREAD_DBG_H

#include <sys/types.h>
#include <signal.h>

typedef struct td_proc		td_proc_t;
typedef struct td_thread	td_thread_t;

typedef struct td_proc_callbacks {
	int (*proc_read)(void *arg, caddr_t addr, void *buf, size_t size);
	int (*proc_readstring)(void *arg, caddr_t addr, void *buf, size_t size);
	int (*proc_write)(void *arg, caddr_t addr, void *buf, size_t size);
	int (*proc_lookup)(void *arg, char *sym, caddr_t *addr);
	int (*proc_regsize)(void *arg, int regset, size_t *size);
	int (*proc_getregs)(void *arg, int regset, void *lwp, void *buf);
	int (*proc_setregs)(void *arg, int regset, void *lwp, void *buf);
	int (*proc_sstep)(void *arg, void *lwp, int onoff);
} td_proc_callbacks_t;

typedef struct td_thread_info {
	caddr_t	thread_addr;		/* Address of data structure */
	int	thread_state;		/* TD_STATE_*; see below */
	int	thread_type;		/* TD_TYPE_*; see below */
	int	thread_scope;		/* TD_SCOPE_*; see below */
	long	thread_id;
	stack_t	thread_stack;
	caddr_t	thread_joiner;
	caddr_t	thread_tls;
	int	thread_tlscount;
	int	thread_errno;
	sigset_t thread_sigmask;
	sigset_t thread_sigpend;
	stack_t	thread_sigstk;
	char	thread_base_priority;
	char	thread_inherited_priority;
	char	thread_active_priority;
	int	thread_cancelflags;	/* TD_CANCEL_*; see below */
	caddr_t	thread_retval;
	long	pad[32];
} td_thread_info_t;

#define TD_STATE_UNKNOWN	0
#define TD_STATE_RUNNING	1
#define TD_STATE_LOCKWAIT	2
#define TD_STATE_MUTEXWAIT	3
#define TD_STATE_CONDWAIT	4
#define TD_STATE_SLEEPING	5
#define TD_STATE_SIGSUSPEND	6
#define TD_STATE_SIGWAIT	7
#define TD_STATE_JOIN		8
#define TD_STATE_SUSPENDED	9
#define TD_STATE_DEAD		10
#define TD_STATE_DEADLOCK	11

#define TD_TYPE_NORMAL		0
#define TD_TYPE_UPCALL		1

#define TD_SCOPE_PROCESS	0
#define TD_SCOPE_SYSTEM		1

#define TD_CANCEL_DISABLED	1
#define TD_CANCEL_ASYNCHRONOUS	2
#define TD_CANCEL_AT_POINT	4
#define TD_CANCEL_CANCELLING	8
#define TD_CANCEL_NEEDED	10

/* Error return codes */
#define TD_ERR_OK		0
#define TD_ERR_ERR		1  /* Generic error */
#define TD_ERR_NOSYM		2  /* Symbol not found (proc_lookup) */
#define TD_ERR_NOOBJ		3  /* No object matched the request */
#define TD_ERR_BADTHREAD        4  /* Thread structure damaged */
#define TD_ERR_INUSE		5  /* The process is already being debugged */
#define TD_ERR_NOLIB		6  /* The process is not using libpthread */
#define TD_ERR_NOMEM		7  /* malloc() failed */
#define TD_ERR_IO		8  /* A callback failed to read or write */
#define TD_ERR_INVAL		9  /* Invalid parameter */

/* Make a connection to a threaded process */
int td_open(td_proc_callbacks_t *, void *arg, td_proc_t **);

/* Release proc object */
int td_close(td_proc_t *);

/* Iterate over the threads in the process */
int td_thr_iter(td_proc_t *, int (*)(td_thread_t *, void *), void *);

/* Check if threaded mode is activated */
int td_activated(td_proc_t *);

/* Get information on a thread */
int td_thr_info(td_thread_t *, td_thread_info_t *);

/* Get name of a thread */
int td_thr_getname(td_thread_t *, char *, int);

/* Get registers set of a thread */
int td_thr_getregs(td_thread_t *, int, void *);

/* Set registers set of a thread */
int td_thr_setregs(td_thread_t *, int, void *);

/* Set/clear single step status */
int td_thr_sstep(td_thread_t *, int step);

/* Map error code to error message */
char *td_err_string (int errcode);

#endif

