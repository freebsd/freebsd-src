/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef __DEV_NTSYNCVAR_H__
#define	__DEV_NTSYNCVAR_H__

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <dev/ntsync/ntsync.h>

enum ntsync_obj_type {
	NTSYNC_OBJ_SEM,
	NTSYNC_OBJ_MUTEX,
	NTSYNC_OBJ_EVENT,
};

struct ntsync_wait_state;

struct ntsync_obj_waiter {
	struct ntsync_wait_state *state;
	TAILQ_ENTRY(ntsync_obj_waiter) link;
};

struct ntsync_obj {
	enum ntsync_obj_type type;
	struct ntsync_priv *owner;
	TAILQ_HEAD(, ntsync_obj_waiter) waiters;
	/* any */
	bool (*is_signaled)(struct ntsync_obj *,
	    struct ntsync_wait_state *state, int index);
	void (*consume)(struct ntsync_obj *, struct ntsync_wait_state *,
	    int index);
	/* all */
	bool (*prepare)(struct ntsync_obj *, struct ntsync_wait_state *state,
	    int index, bool *stop);
	void (*commit)(struct ntsync_obj *, struct ntsync_wait_state *state,
	    int index);
	void (*post_commit)(struct ntsync_obj *,
	    struct ntsync_wait_state *state, int index);
};

struct ntsync_obj_sem {
	struct ntsync_obj obj;
	struct ntsync_sem_args a;
	struct ntsync_sem_args a1;
};
#define	OBJ_TO_SEM(obj)		__containerof(obj, struct ntsync_obj_sem, obj)

struct ntsync_obj_mutex {
	struct ntsync_obj obj;
	struct ntsync_mutex_args a;
	struct ntsync_mutex_args a1;
	bool abandoned;
};
#define	OBJ_TO_MUTEX(obj)	__containerof(obj, struct ntsync_obj_mutex, obj)

struct ntsync_obj_event {
	struct ntsync_obj obj;
	struct ntsync_event_args a;
	struct ntsync_event_args a1;
	bool pulse;
};
#define	OBJ_TO_EVENT(obj)	__containerof(obj, struct ntsync_obj_event, obj)

struct ntsync_wait_state {
	struct ntsync_wait_args *nwa;
	struct ntsync_priv *owner;
	struct ntsync_obj_waiter waiters[NTSYNC_MAX_WAIT_COUNT + 1];
	int fds[NTSYNC_MAX_WAIT_COUNT];
	struct file *fps[NTSYNC_MAX_WAIT_COUNT];
	struct file *fp_alert;
	int obj_count;
	struct ntsync_obj *objs[NTSYNC_MAX_WAIT_COUNT + 1];
	struct ntsync_obj_event *alert_event;
	sbintime_t sb;
	sbintime_t prec;
	int error;
	int index;
	bool any;
	bool all;
	bool ready;
};

struct ntsync_priv {
	struct mtx lock;
	unsigned objs_cnt;
	bool closed;
};

#define	NTSYNC_PRIV_LOCK(priv)		mtx_lock(&priv->lock)
#define	NTSYNC_PRIV_UNLOCK(priv)	mtx_unlock(&priv->lock)
#define	NTSYNC_PRIV_ASSERT(priv)	mtx_assert(&priv->lock, MA_OWNED)

extern struct cdevsw ntsync_cdevsw;

struct file;
struct thread;
int ntsync_sem_release(struct thread *td, struct file *fp, uint32_t *val);
int ntsync_sem_read(struct thread *td, struct file *fp,
    struct ntsync_sem_args *a);
int ntsync_mutex_unlock(struct thread *td, struct file *fp,
    struct ntsync_mutex_args *a);
int ntsync_mutex_kill(struct thread *td, struct file *fp, uint32_t val);
int ntsync_mutex_read(struct thread *td, struct file *fp,
    struct ntsync_mutex_args *a, bool *doco);
int ntsync_event_set(struct thread *td, struct file *fp, uint32_t *val);
int ntsync_event_reset(struct thread *td, struct file *fp, uint32_t *val);
int ntsync_event_pulse(struct thread *td, struct file *fp, uint32_t *val);
int ntsync_event_read(struct thread *td, struct file *fp,
    struct ntsync_event_args *a);

#endif
