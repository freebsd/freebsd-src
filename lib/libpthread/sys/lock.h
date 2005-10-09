/*
 * Copyright (c) 2001, 2003 Daniel Eischen <deischen@freebsd.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
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

#ifndef _LOCK_H_
#define	_LOCK_H_

struct lockreq;
struct lockuser;
struct lock;

enum lock_type {
	LCK_DEFAULT	= 0x0000,	/* default is FIFO spin locks */
	LCK_PRIORITY	= 0x0001,
	LCK_ADAPTIVE 	= 0x0002	/* call user-supplied handlers */
};

typedef void lock_handler_t(struct lock *, struct lockuser *);

struct lock {
	struct lockreq	*l_head;
	struct lockreq	*l_tail;	/* only used for priority locks */
	enum lock_type	l_type;
	lock_handler_t	*l_wait;	/* only used for adaptive locks */
	lock_handler_t	*l_wakeup;	/* only used for adaptive locks */
};

/* Try to make this >= CACHELINESIZE */
struct lockreq {
	struct lockuser	*lr_watcher;	/* only used for priority locks */
	struct lockuser	*lr_owner;	/* only used for priority locks */
	volatile int	lr_locked;	/* lock granted = 0, busy otherwise */
	volatile int	lr_active;	/* non-zero if the lock is last lock for thread */
};

struct lockuser {
	struct lockreq	*lu_myreq;	/* request to give up/trade */
	struct lockreq	*lu_watchreq;	/* watch this request */
	int		lu_priority;	/* only used for priority locks */
	void		*lu_private1;	/* private{1,2} are initialized to */
	void		*lu_private2;	/*   NULL and can be used by caller */
#define	lu_private	lu_private1
};

#define	_LCK_INITIALIZER(lck_req)	{ &lck_req, NULL, LCK_DEFAULT, \
					  NULL, NULL }
#define	_LCK_REQUEST_INITIALIZER	{ 0, NULL, NULL, 0 }

#define	_LCK_BUSY(lu)			((lu)->lu_watchreq->lr_locked != 0)
#define	_LCK_ACTIVE(lu)			((lu)->lu_watchreq->lr_active != 0)
#define	_LCK_GRANTED(lu)		((lu)->lu_watchreq->lr_locked == 3)

#define	_LCK_SET_PRIVATE(lu, p)		(lu)->lu_private = (void *)(p)
#define	_LCK_GET_PRIVATE(lu)		(lu)->lu_private
#define	_LCK_SET_PRIVATE2(lu, p)	(lu)->lu_private2 = (void *)(p)
#define	_LCK_GET_PRIVATE2(lu)		(lu)->lu_private2

void	_lock_acquire(struct lock *, struct lockuser *, int);
void	_lock_destroy(struct lock *);
void	_lock_grant(struct lock *, struct lockuser *);
int	_lock_init(struct lock *, enum lock_type,
	    lock_handler_t *, lock_handler_t *);
int	_lock_reinit(struct lock *, enum lock_type,
	    lock_handler_t *, lock_handler_t *);
void	_lock_release(struct lock *, struct lockuser *);
int	_lockuser_init(struct lockuser *lu, void *priv);
void	_lockuser_destroy(struct lockuser *lu);
int	_lockuser_reinit(struct lockuser *lu, void *priv);
void	_lockuser_setactive(struct lockuser *lu, int active);

#endif
