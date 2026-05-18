/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2026 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysent.h>
#include <sys/user.h>
#include <dev/ntsync/ntsyncvar.h>

static struct cdev *ntsync_cdev;
MALLOC_DEFINE(M_NTSYNC, "ntsync", "ntsync");

static void ntsync_free_priv(struct ntsync_priv *priv);

/*
 * Returning error from an ioctl handler prevents the generic ioctl
 * code from copying out the result.  Use direct access to ioctl(2)
 * args to get the parameters block pointer to implement Linux
 * semantic of both returning an error and updating the parameters
 * block.
 */
static int
ntsync_ioctl_copyout(struct thread *td, const void *ptr, size_t sz)
{
	void *uptr;

	if (SV_PROC_ABI(td->td_proc) != SV_ABI_FREEBSD)
		return (0);
	uptr = (void *)(uintptr_t)td->td_sa.args[2];
	return (copyout(ptr, uptr, sz));
}

static bool
ntsync_wait_any(struct ntsync_wait_state *state)
{
	struct ntsync_obj *obj;
	int i;

	MPASS(state->any);
	NTSYNC_PRIV_ASSERT(state->owner);

	for (i = 0; i < state->obj_count; i++) {
		obj = state->objs[i];
		if (obj->is_signaled(obj, state, i)) {
			state->index = i;
			obj->consume(obj, state, state->index);
			return (true);
		}
	}
	return (false);
}

static bool
ntsync_wait_all_prepare(struct ntsync_wait_state *state, bool *stop)
{
	struct ntsync_obj *obj;
	int alerti, i;
	bool first;

	MPASS(state->all);
	MPASS(state->error == 0);
	MPASS(!*stop);
	NTSYNC_PRIV_ASSERT(state->owner);

	alerti = state->alert_event == NULL ? 0 : 1;
	first = true;

	for (i = 0; i < state->obj_count - alerti; i++) {
		obj = state->objs[i];
		if (!obj->prepare(obj, state, i, stop))
			return (false);
		if (*stop) {
			MPASS(state->error != 0);
			return (false);
		}
		MPASS (state->error == 0);
		if (first) {
			first = false;
			state->index = i;
		}
	}
	return (true);
}

static void
ntsync_wait_all_commit(struct ntsync_wait_state *state)
{
	struct ntsync_obj *obj;
	int i, alerti;

	MPASS(state->all);
	NTSYNC_PRIV_ASSERT(state->owner);
	alerti = state->alert_event == NULL ? 0 : 1;

	for (i = 0; i < state->obj_count - alerti; i++) {
		obj = state->objs[i];
		obj->commit(obj, state, i);
	}
}

static void
ntsync_wait_link_waiters(struct ntsync_wait_state *state)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_waiter *waiter;
	int i;

	NTSYNC_PRIV_ASSERT(state->owner);

	for (i = 0; i < state->obj_count; i++) {
		obj = state->objs[i];
		waiter = &state->waiters[i];
		waiter->state = state;
		TAILQ_INSERT_TAIL(&obj->waiters, waiter, link);
	}
}

static void
ntsync_wait_unlink_waiters(struct ntsync_wait_state *state)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_waiter *waiter;
	int i;

	NTSYNC_PRIV_ASSERT(state->owner);

	for (i = 0; i < state->obj_count; i++) {
		obj = state->objs[i];
		waiter = &state->waiters[i];
		TAILQ_REMOVE(&obj->waiters, waiter, link);
	}
}

static void
ntsync_wait_post_commit(struct ntsync_wait_state *state)
{
	struct ntsync_obj *obj;
	int alerti, i;

	NTSYNC_PRIV_ASSERT(state->owner);

	alerti = state->alert_event == NULL ? 0 : 1;
	for (i = 0; i < state->obj_count - alerti; i++) {
		obj = state->objs[i];
		obj->post_commit(obj, state, i);
	}
}

static void
ntsync_wait_check_ready(struct ntsync_wait_state *state)
{
	struct ntsync_obj *ae;
	int index;
	bool stop;

	NTSYNC_PRIV_ASSERT(state->owner);

	if (state->ready)
		return;

	if (state->all) {
		stop = false;
		if (ntsync_wait_all_prepare(state, &stop)) {
			MPASS(!stop);
			ntsync_wait_all_commit(state);
			state->ready = true;
			ntsync_wait_post_commit(state);
		} else if (stop) {
			/* skip */
		} else if (state->alert_event != NULL) {
			ae = &state->alert_event->obj;
			index = state->obj_count - 1;
			if (ae->is_signaled(ae, state, index)) {
				state->index = index;
				ae->consume(ae, state, index);
				ae->post_commit(ae, state, index);
				state->ready = true;
			}
		}
	} else {	/* state->any */
		if (ntsync_wait_any(state))
			state->ready = true;
	}
}

/*
 * Perform the wait.  Errors returned through state->error still
 * result in the copyout of the ntsync_wait_args after the wait, while
 * errors returned as the function result do not.
 */
static int
ntsync_wait_locked(struct ntsync_wait_state *state, struct thread *td)
{
	int error;

	NTSYNC_PRIV_ASSERT(state->owner);

	for (;;) {
		ntsync_wait_check_ready(state);
		if (state->ready)
			break;
		error = msleep_sbt(state, &state->owner->lock,
		    PCATCH, "ntsync", state->sb, 0,
		    C_ABSOLUTE /* | C_HARDCLOCK XXXKIB */);

		/*
		 * Check state->ready before checking error from
		 * msleep().  If there was a wake up that set the
		 * readiness before us receiving a signal or timeout,
		 * the objects states are modified to reflect wakeup.
		 * Due to this, ready should result in normal return.
		 */
		if (state->ready) {
			error = 0;
			break;
		}

		if (error != 0) {
			if (error == EAGAIN)
				error = ETIMEDOUT;
			break;
		}
	}
	return (error);
}

static int
ntsync_wait(struct ntsync_wait_state *state, struct thread *td)
{
	int error;

	NTSYNC_PRIV_LOCK(state->owner);
	ntsync_wait_link_waiters(state);
	error = ntsync_wait_locked(state, td);
	ntsync_wait_unlink_waiters(state);
	NTSYNC_PRIV_UNLOCK(state->owner);
	return (error);
}

static void
ntsync_wakeup_waiters(struct ntsync_obj *obj)
{
	struct ntsync_obj_waiter *w;

	NTSYNC_PRIV_ASSERT(obj->owner);

	TAILQ_FOREACH(w, &obj->waiters, link) {
		ntsync_wait_check_ready(w->state);
		if (w->state->ready)
			wakeup(w->state);
	}
}

static int
ntsync_create_obj(struct ntsync_obj *obj, struct fileops *fops,
    struct ntsync_priv *priv, struct thread *td)
{
	struct file *fp;
	int error, fd;

	error = falloc_noinstall(td, &fp);
	if (error != 0)
		return (error);

	/*
	 * The priv fd cannot be closed during object creation since
	 * it is fget-ed around ioctl.
	 */
	obj->owner = priv;

	TAILQ_INIT(&obj->waiters);
	NTSYNC_PRIV_LOCK(priv);
	MPASS(!priv->closed);
	if (priv->objs_cnt == UINT_MAX) {
		NTSYNC_PRIV_UNLOCK(priv);
		fdrop(fp, td);
		return (EMFILE);
	}
	priv->objs_cnt++;
	NTSYNC_PRIV_UNLOCK(priv);

	finit(fp, FREAD | FWRITE, DTYPE_NTSYNC, obj, fops);
	error = finstall(td, fp, &fd, 0, NULL);
	if (error != 0) {
		NTSYNC_PRIV_LOCK(priv);
		MPASS(priv->objs_cnt > 0);
		priv->objs_cnt--;
		NTSYNC_PRIV_UNLOCK(priv);
	} else {
		td->td_retval[0] = fd;
	}
	fdrop(fp, td);
	return (error);
}

static void
ntsync_close_obj(struct ntsync_obj *obj, struct thread *td)
{
	struct ntsync_priv *priv;

	priv = obj->owner;
	NTSYNC_PRIV_LOCK(priv);
	MPASS(priv->objs_cnt > 0);
	MPASS(TAILQ_EMPTY(&obj->waiters));
	priv->objs_cnt--;
	NTSYNC_PRIV_UNLOCK(priv);
	ntsync_free_priv(priv);
}

static bool
ntsync_sem_is_signaled(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_sem *sem;

	MPASS(obj->type == NTSYNC_OBJ_SEM);
	NTSYNC_PRIV_ASSERT(obj->owner);
	sem = OBJ_TO_SEM(obj);
	return (sem->a.count != 0);
}

static void
ntsync_sem_consume(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_sem *sem;

	MPASS(obj->type == NTSYNC_OBJ_SEM);
	NTSYNC_PRIV_ASSERT(obj->owner);
	sem = OBJ_TO_SEM(obj);
	MPASS(sem->a.count != 0);
	sem->a.count--;
}

static bool
ntsync_sem_prepare(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index, bool *stop)
{
	struct ntsync_obj_sem *sem;

	MPASS(obj->type == NTSYNC_OBJ_SEM);
	NTSYNC_PRIV_ASSERT(obj->owner);
	sem = OBJ_TO_SEM(obj);
	if (sem->a.count == 0)
		return (false);
	sem->a1 = sem->a;
	sem->a1.count--;
	return (true);
}

static void
ntsync_sem_commit(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_sem *sem;

	MPASS(obj->type == NTSYNC_OBJ_SEM);
	NTSYNC_PRIV_ASSERT(obj->owner);
	sem = OBJ_TO_SEM(obj);
	sem->a = sem->a1;
}

static void
ntsync_sem_post_commit(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
}

static int
ntsync_sem_close(struct file *fp, struct thread *td)
{
	struct ntsync_obj_sem *sem;

	sem = fp->f_data;
	ntsync_close_obj(&sem->obj, td);
	free(sem, M_NTSYNC);
	return (0);
}

int
ntsync_sem_release(struct thread *td, struct file *fp, uint32_t *val)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_sem *sem;
	struct ntsync_priv *priv;
	uint32_t prev;
	int error;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_SEM)
		return (EINVAL);
	sem = OBJ_TO_SEM(obj);
	priv = obj->owner;
	error = 0;

	NTSYNC_PRIV_LOCK(priv);
	if (sem->a.count + *val < sem->a.count ||
	    sem->a.count + *val > sem->a.max) {
		error = EOVERFLOW;
	} else {
		prev = sem->a.count;
		sem->a.count += *val;
		if (sem->a.count != 0)
			ntsync_wakeup_waiters(obj);
		*val = prev;
	}
	NTSYNC_PRIV_UNLOCK(priv);
	return (error);
}

int
ntsync_sem_read(struct thread *td, struct file *fp, struct ntsync_sem_args *a)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_sem *sem;
	struct ntsync_priv *priv;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_SEM)
		return (EINVAL);
	sem = OBJ_TO_SEM(obj);
	priv = obj->owner;
	NTSYNC_PRIV_LOCK(priv);
	*a = sem->a;
	NTSYNC_PRIV_UNLOCK(priv);
	return (0);
}

static int
ntsync_sem_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{
	int error;

	switch (com) {
	case NTSYNC_IOC_SEM_RELEASE:
		error = ntsync_sem_release(td, fp, data);
		break;
	case NTSYNC_IOC_SEM_READ:
		error = ntsync_sem_read(td, fp, data);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ntsync_sem_stat(struct file *fp, struct stat *sbp, struct ucred *cred)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_sem *sem;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_SEM);
	sem = OBJ_TO_SEM(obj);

	memset(sbp, 0, sizeof(*sbp));
	sbp->st_mode = S_IFREG /* XXXKIB */ | S_IRUSR | S_IWUSR;
	NTSYNC_PRIV_LOCK(obj->owner);
	sbp->st_size = sem->a.max;
	sbp->st_nlink = sem->a.count;
	NTSYNC_PRIV_UNLOCK(obj->owner);
	return (0);
}

static int
ntsync_sem_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_sem *sem;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_SEM);
	sem = OBJ_TO_SEM(obj);

	kif->kf_type = KF_TYPE_NTSYNC;
	kif->kf_un.kf_ntsync.kf_ntsync_type = KF_NTSYNC_TYPE_SEM;
	kif->kf_un.kf_ntsync.kf_ntsync_dev = (uintptr_t)obj->owner;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_sem.count = sem->a.count;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_sem.max = sem->a.max;
	return (0);
}

struct fileops ntsync_sem_fops = {
	.fo_read = invfo_rdwr,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = ntsync_sem_ioctl,
	.fo_poll = invfo_poll,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = ntsync_sem_stat,
	.fo_close = ntsync_sem_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = ntsync_sem_fill_kinfo,
	.fo_flags = DFLAG_PASSABLE,
};

static int
ntsync_create_sem(struct ntsync_sem_args *args, struct ntsync_priv *priv,
    struct thread *td)
{
	struct ntsync_obj_sem *sem;
	int error;

	if (args->count > args->max)
		return (EINVAL);

	sem = malloc(sizeof(*sem), M_NTSYNC, M_WAITOK | M_ZERO);
	sem->obj.type = NTSYNC_OBJ_SEM;
	sem->obj.is_signaled = ntsync_sem_is_signaled;
	sem->obj.consume = ntsync_sem_consume;
	sem->obj.prepare = ntsync_sem_prepare;
	sem->obj.commit = ntsync_sem_commit;
	sem->obj.post_commit = ntsync_sem_post_commit;
	sem->a = *args;

	error = ntsync_create_obj(&sem->obj, &ntsync_sem_fops, priv, td);
	if (error != 0)
		free(sem, M_NTSYNC);

	return (error);
}

static bool
ntsync_mutex_can_lock(struct ntsync_obj_mutex *mutex, uint32_t nwa_owner)
{
	return (mutex->a.owner == 0 ||
	    (mutex->a.owner == nwa_owner && mutex->a.count < UINT32_MAX) ||
	    mutex->abandoned);
}

static bool
ntsync_mutex_is_signaled(struct ntsync_obj *obj,
    struct ntsync_wait_state *state, int index)
{
	struct ntsync_obj_mutex *mutex;

	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	NTSYNC_PRIV_ASSERT(obj->owner);
	mutex = OBJ_TO_MUTEX(obj);
	return (ntsync_mutex_can_lock(mutex, state->nwa->owner));
}

static void
ntsync_mutex_consume(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_mutex *mutex;

	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	NTSYNC_PRIV_ASSERT(obj->owner);
	mutex = OBJ_TO_MUTEX(obj);
	MPASS(ntsync_mutex_can_lock(mutex, state->nwa->owner));
	if (state->nwa->owner == 0) {
		state->error = EINVAL;
		return;
	}
	if (mutex->a.owner == 0 || mutex->abandoned)
		mutex->a.count = 1;
	else
		mutex->a.count++;
	mutex->a.owner = state->nwa->owner;
	if (mutex->abandoned && state->error == 0)
		state->error = EOWNERDEAD;
	mutex->abandoned = false;
}

static bool
ntsync_mutex_prepare(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index, bool *stop)
{
	struct ntsync_obj_mutex *mutex;

	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	NTSYNC_PRIV_ASSERT(obj->owner);
	mutex = OBJ_TO_MUTEX(obj);
	if (!ntsync_mutex_can_lock(mutex, state->nwa->owner))
		return (false);
	if (state->nwa->owner == 0) {
		state->error = EINVAL;
		*stop = true;
		return (false);
	}
	mutex->a1 = mutex->a;
	if (mutex->a.owner == 0 || mutex->abandoned)
		mutex->a1.count = 1;
	else
		mutex->a1.count++;
	mutex->a1.owner = state->nwa->owner;
	return (true);
}

static void
ntsync_mutex_commit(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_mutex *mutex;

	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	NTSYNC_PRIV_ASSERT(obj->owner);
	mutex = OBJ_TO_MUTEX(obj);
	mutex->a = mutex->a1;
	if (mutex->abandoned)
		state->error = EOWNERDEAD;
	mutex->abandoned = false;
}

static void
ntsync_mutex_post_commit(struct ntsync_obj *obj,
    struct ntsync_wait_state *state, int index)
{
}

static int
ntsync_mutex_close(struct file *fp, struct thread *td)
{
	struct ntsync_obj_mutex *mutex;

	mutex = fp->f_data;
	ntsync_close_obj(&mutex->obj, td);
	free(mutex, M_NTSYNC);
	return (0);
}

int
ntsync_mutex_unlock(struct thread *td, struct file *fp,
    struct ntsync_mutex_args *a)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_mutex *mutex;
	struct ntsync_priv *priv;
	uint32_t prev;
	int error;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_MUTEX)
		return (EINVAL);
	mutex = OBJ_TO_MUTEX(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	if (a->owner == 0) {
		error = EINVAL;
	} else if (a->owner != mutex->a.owner) {
		error = EPERM;
	} else {
		error = 0;
		prev = mutex->a.count;
		MPASS(mutex->a.count > 0);
		mutex->a.count--;
		a->count = prev;
		if (mutex->a.count == 0) {
			mutex->a.owner = 0;
			ntsync_wakeup_waiters(obj);
		}
	}
	NTSYNC_PRIV_UNLOCK(priv);
	return (error);
}

int
ntsync_mutex_kill(struct thread *td, struct file *fp, uint32_t val)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_mutex *mutex;
	struct ntsync_priv *priv;
	int error;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_MUTEX)
		return (EINVAL);
	mutex = OBJ_TO_MUTEX(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	if (val == 0) {
		error = EINVAL;
	} else if (mutex->a.owner != val) {
		error = EPERM;
	} else {
		error = 0;
		mutex->a.owner = 0;
		mutex->a.count = 0;
		mutex->abandoned = true;
		ntsync_wakeup_waiters(obj);
	}
	NTSYNC_PRIV_UNLOCK(priv);
	return (error);
}

int
ntsync_mutex_read(struct thread *td, struct file *fp,
    struct ntsync_mutex_args *a, bool *doco)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_mutex *mutex;
	struct ntsync_priv *priv;
	int error;

	*doco = false;
	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_MUTEX)
		return (EINVAL);
	mutex = OBJ_TO_MUTEX(obj);
	priv = obj->owner;
	error = 0;

	NTSYNC_PRIV_LOCK(priv);
	*a = mutex->a;
	if (mutex->abandoned)
		error = EOWNERDEAD;
	NTSYNC_PRIV_UNLOCK(priv);
	*doco = true;
	return (error);
}

static int
ntsync_mutex_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{
	struct ntsync_mutex_args aa;
	int error, error1;
	bool doco;

	doco = false;
	switch (com) {
	case NTSYNC_IOC_MUTEX_UNLOCK:
		error = ntsync_mutex_unlock(td, fp, data);
		break;
	case NTSYNC_IOC_MUTEX_KILL:
		error = ntsync_mutex_kill(td, fp, *(uint32_t *)data);
		break;
	case NTSYNC_IOC_MUTEX_READ:
		error = ntsync_mutex_read(td, fp, &aa, &doco);
		if (doco) {
			error1 = ntsync_ioctl_copyout(td, &aa, sizeof(aa));
			if (error1 != 0)
				error = error1;
		}
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ntsync_mutex_stat(struct file *fp, struct stat *sbp, struct ucred *cred)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_mutex *mutex;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	mutex = OBJ_TO_MUTEX(obj);

	memset(sbp, 0, sizeof(*sbp));
	sbp->st_mode = S_IFREG /* XXXKIB */ | S_IRUSR | S_IWUSR;
	NTSYNC_PRIV_LOCK(obj->owner);
	sbp->st_size = mutex->a.owner;
	sbp->st_nlink = mutex->a.count;
	NTSYNC_PRIV_UNLOCK(obj->owner);
	return (0);
}

static int
ntsync_mutex_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_mutex *mutex;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_MUTEX);
	mutex = OBJ_TO_MUTEX(obj);

	kif->kf_type = KF_TYPE_NTSYNC;
	kif->kf_un.kf_ntsync.kf_ntsync_type = KF_NTSYNC_TYPE_MUTEX;
	kif->kf_un.kf_ntsync.kf_ntsync_dev = (uintptr_t)obj->owner;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_mutex.owner =
	    mutex->a.owner;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_mutex.count =
	    mutex->a.count;
	return (0);
}

struct fileops ntsync_mutex_fops = {
	.fo_read = invfo_rdwr,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = ntsync_mutex_ioctl,
	.fo_poll = invfo_poll,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = ntsync_mutex_stat,
	.fo_close = ntsync_mutex_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = ntsync_mutex_fill_kinfo,
	.fo_flags = DFLAG_PASSABLE,
};

static int
ntsync_create_mutex(struct ntsync_mutex_args *args, struct ntsync_priv *priv,
    struct thread *td)
{
	struct ntsync_obj_mutex *mutex;
	int error;

	if ((args->owner != 0 && args->count == 0) ||
	    (args->owner == 0 && args->count != 0))
		return (EINVAL);

	mutex = malloc(sizeof(*mutex), M_NTSYNC, M_WAITOK | M_ZERO);
	mutex->obj.type = NTSYNC_OBJ_MUTEX;
	mutex->obj.is_signaled = ntsync_mutex_is_signaled;
	mutex->obj.consume = ntsync_mutex_consume;
	mutex->obj.prepare = ntsync_mutex_prepare;
	mutex->obj.commit = ntsync_mutex_commit;
	mutex->obj.post_commit = ntsync_mutex_post_commit;
	mutex->a = *args;
	mutex->abandoned = false;

	error = ntsync_create_obj(&mutex->obj, &ntsync_mutex_fops, priv, td);
	if (error != 0)
		free(mutex, M_NTSYNC);

	return (error);
}

static bool
ntsync_event_is_signaled(struct ntsync_obj *obj,
    struct ntsync_wait_state *state, int index)
{
	struct ntsync_obj_event *event;

	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	NTSYNC_PRIV_ASSERT(obj->owner);
	event = OBJ_TO_EVENT(obj);
	return (event->a.signaled != 0);
}

static void
ntsync_event_consume(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_event *event;

	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	NTSYNC_PRIV_ASSERT(obj->owner);
	MPASS(ntsync_event_is_signaled(obj, state, index));

	event = OBJ_TO_EVENT(obj);
	if (event->a.manual == 0)
		event->a.signaled = 0;
}

static bool
ntsync_event_prepare(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index, bool *stop)
{
	struct ntsync_obj_event *event;

	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	NTSYNC_PRIV_ASSERT(obj->owner);
	event = OBJ_TO_EVENT(obj);
	if (!ntsync_event_is_signaled(obj, state, index))
		return (false);
	event->a1 = event->a;
	return (true);
}

static void
ntsync_event_commit(struct ntsync_obj *obj, struct ntsync_wait_state *state,
    int index)
{
	struct ntsync_obj_event *event;

	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	NTSYNC_PRIV_ASSERT(obj->owner);
	event = OBJ_TO_EVENT(obj);
	event->a = event->a1;
	if (event->pulse && event->a.manual == 0) {
		event->a.signaled = 0;
		event->pulse = false;
	}
}

static void
ntsync_event_post_commit(struct ntsync_obj *obj,
    struct ntsync_wait_state *state, int index)
{
	struct ntsync_obj_event *event;

	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	NTSYNC_PRIV_ASSERT(obj->owner);
	event = OBJ_TO_EVENT(obj);
	if (event->a.manual == 0)
		event->a.signaled = 0;
}

static int
ntsync_event_close(struct file *fp, struct thread *td)
{
	struct ntsync_obj_event *event;

	event = fp->f_data;
	ntsync_close_obj(&event->obj, td);
	free(event, M_NTSYNC);
	return (0);
}

int
ntsync_event_set(struct thread *td, struct file *fp, uint32_t *val)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;
	struct ntsync_priv *priv;
	uint32_t prev;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_EVENT)
		return (EINVAL);
	event = OBJ_TO_EVENT(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	prev = event->a.signaled;
	event->a.signaled = 1;
	ntsync_wakeup_waiters(obj);
	NTSYNC_PRIV_UNLOCK(priv);

	*val = prev;
	return (0);
}

int
ntsync_event_reset(struct thread *td, struct file *fp, uint32_t *val)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;
	struct ntsync_priv *priv;
	uint32_t prev;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_EVENT)
		return (EINVAL);
	event = OBJ_TO_EVENT(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	prev = event->a.signaled;
	event->a.signaled = 0;
	NTSYNC_PRIV_UNLOCK(priv);

	*val = prev;
	return (0);
}

int
ntsync_event_pulse(struct thread *td, struct file *fp, uint32_t *val)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;
	struct ntsync_priv *priv;
	uint32_t prev;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_EVENT)
		return (EINVAL);
	event = OBJ_TO_EVENT(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	prev = event->a.signaled;
	event->a.signaled = 1;
	event->pulse = true;
	ntsync_wakeup_waiters(obj);
	event->a.signaled = 0;
	event->pulse = false;
	NTSYNC_PRIV_UNLOCK(priv);

	*val = prev;
	return (0);
}

int
ntsync_event_read(struct thread *td, struct file *fp,
    struct ntsync_event_args *a)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;
	struct ntsync_priv *priv;

	obj = fp->f_data;
	if (obj->type != NTSYNC_OBJ_EVENT)
		return (EINVAL);
	event = OBJ_TO_EVENT(obj);
	priv = obj->owner;

	NTSYNC_PRIV_LOCK(priv);
	*a = event->a;
	NTSYNC_PRIV_UNLOCK(priv);

	return (0);
}

static int
ntsync_event_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{
	int error;

	switch (com) {
	case NTSYNC_IOC_EVENT_SET:
		error = ntsync_event_set(td, fp, data);
		break;
	case NTSYNC_IOC_EVENT_RESET:
		error = ntsync_event_reset(td, fp, data);
		break;
	case NTSYNC_IOC_EVENT_PULSE:
		error = ntsync_event_pulse(td, fp, data);
		break;
	case NTSYNC_IOC_EVENT_READ:
		error = ntsync_event_read(td, fp, data);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ntsync_event_stat(struct file *fp, struct stat *sbp, struct ucred *cred)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	event = OBJ_TO_EVENT(obj);

	memset(sbp, 0, sizeof(*sbp));
	sbp->st_mode = S_IFREG /* XXXKIB */ | S_IRUSR | S_IWUSR;
	NTSYNC_PRIV_LOCK(obj->owner);
	sbp->st_size = event->a.signaled;
	sbp->st_nlink = event->a.manual;
	NTSYNC_PRIV_UNLOCK(obj->owner);
	return (0);
}

static int
ntsync_event_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct ntsync_obj *obj;
	struct ntsync_obj_event *event;

	MPASS(fp->f_type == DTYPE_NTSYNC);
	obj = fp->f_data;
	MPASS(obj->type == NTSYNC_OBJ_EVENT);
	event = OBJ_TO_EVENT(obj);

	kif->kf_type = KF_TYPE_NTSYNC;
	kif->kf_un.kf_ntsync.kf_ntsync_type = KF_NTSYNC_TYPE_EVENT;
	kif->kf_un.kf_ntsync.kf_ntsync_dev = (uintptr_t)obj->owner;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_event.signaled =
		event->a.signaled;
	kif->kf_un.kf_ntsync.kf_ntsync_un.kf_ntsync_event.manual =
		event->a.manual;
	return (0);
}

struct fileops ntsync_event_fops = {
	.fo_read = invfo_rdwr,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = ntsync_event_ioctl,
	.fo_poll = invfo_poll,
	.fo_kqfilter = invfo_kqfilter,
	.fo_stat = ntsync_event_stat,
	.fo_close = ntsync_event_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = ntsync_event_fill_kinfo,
	.fo_flags = DFLAG_PASSABLE,
};

static int
ntsync_create_event(struct ntsync_event_args *args, struct ntsync_priv *priv,
    struct thread *td)
{
	struct ntsync_obj_event *event;
	int error;

	event = malloc(sizeof(*event), M_NTSYNC, M_WAITOK | M_ZERO);
	event->obj.type = NTSYNC_OBJ_EVENT;
	event->obj.is_signaled = ntsync_event_is_signaled;
	event->obj.consume = ntsync_event_consume;
	event->obj.prepare = ntsync_event_prepare;
	event->obj.commit = ntsync_event_commit;
	event->obj.post_commit = ntsync_event_post_commit;
	event->a = *args;

	error = ntsync_create_obj(&event->obj, &ntsync_event_fops, priv, td);
	if (error != 0)
		free(event, M_NTSYNC);

	return (error);
}

static void
ntsync_free_priv(struct ntsync_priv *priv)
{
	bool do_free;

	NTSYNC_PRIV_LOCK(priv);
	do_free = priv->closed && priv->objs_cnt == 0;
	NTSYNC_PRIV_UNLOCK(priv);
	if (do_free) {
		mtx_destroy(&priv->lock);
		free(priv, M_NTSYNC);
	}
}

static void
ntsync_priv_dtr(void *data)
{
	ntsync_free_priv(data);
}

static int
ntsync_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct ntsync_priv *priv;

	priv = malloc(sizeof(*priv), M_NTSYNC, M_WAITOK);
	priv->closed = false;
	priv->objs_cnt = 0;
	mtx_init(&priv->lock, "ntsync", "ntsync", MTX_DEF | MTX_NEW);
	devfs_set_cdevpriv(priv, ntsync_priv_dtr);
	return (0);
}

static int
ntsync_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct ntsync_priv *priv;
	void *a;
	int error;

	error = devfs_get_cdevpriv(&a);
	if (error == 0) {
		priv = a;
		NTSYNC_PRIV_LOCK(priv);
		priv->closed = true;
		NTSYNC_PRIV_UNLOCK(priv);
	}
	devfs_clear_cdevpriv();
	return (0);
}

static int
ntsync_wait_state_get(struct ntsync_wait_args *nwa, u_long cmd,
    struct ntsync_priv *owner, struct ntsync_wait_state **statep,
    struct thread *td)
{
	struct ntsync_wait_state *state;
	struct ntsync_obj *obj;
	struct bintime btb;
	int error, i, j;

	if (nwa->count > NTSYNC_MAX_WAIT_COUNT)
		return (EINVAL);
	if ((nwa->flags & ~NTSYNC_WAIT_REALTIME) != 0)
		return (EINVAL);

	state = malloc(sizeof(*state), M_NTSYNC, M_WAITOK | M_ZERO);
	state->nwa = nwa;
	state->owner = owner;
	state->all = cmd == NTSYNC_IOC_WAIT_ALL;
	state->any = !state->all;
	error = copyin((void *)(uintptr_t)nwa->objs, &state->fds[0],
	    nwa->count * sizeof(state->fds[0]));
	if (error != 0)
		return (error);

	i = 0;
	if (nwa->alert != 0) {
		error = fget_cap(td, nwa->alert, &cap_no_rights, NULL,
		    &state->fp_alert, NULL);
		if (error != 0) {
			state->fp_alert = NULL;
			goto error_out;
		}
		if (state->fp_alert->f_type != DTYPE_NTSYNC) {
			error = EINVAL;
			goto error_out;
		}
		obj = state->fp_alert->f_data;
		if (obj->type != NTSYNC_OBJ_EVENT || obj->owner != owner) {
			error = EINVAL;
			goto error_out;
		}
		state->alert_event = OBJ_TO_EVENT(obj);
	}

	for (; i < nwa->count; i++) {
		error = fget_cap(td, state->fds[i], &cap_no_rights, NULL,
		    &state->fps[i], NULL);
		if (error != 0) {
			state->fps[i] = NULL;
			goto error_out;
		}
		if (state->fps[i]->f_type != DTYPE_NTSYNC ||
		    (obj = state->fps[i]->f_data)->owner != owner) {
			i++;
			error = EINVAL;
			goto error_out;
		}
	}

	state->obj_count = nwa->count;
	for (i = 0; i < nwa->count; i++)
		state->objs[i] = state->fps[i]->f_data;
	if (state->alert_event != NULL) {
		state->objs[i] = &state->alert_event->obj;
		state->obj_count++;
	}

	if (state->all) {
		/* Check no dups */
		for (i = 0; i < state->obj_count; i++) {
			obj = state->objs[i];
			for (j = i + 1; j < state->obj_count; j++) {
				if (obj == state->objs[j]) {
					i = state->obj_count;
					error = EINVAL;
					goto error_out;
				}
			}
		}
	}

	if (nwa->timeout == UINT64_MAX) {
		state->sb = 0;
	} else {
		state->sb = nstosbt(nwa->timeout);
		if ((nwa->flags & NTSYNC_WAIT_REALTIME) != 0) {
			getboottimebin(&btb);
			state->sb += bttosbt(btb);
		}
	}

	*statep = state;
	return (0);

error_out:
	for (j = 0; j < i; j++)
		fdrop(state->fps[j], td);
	if (state->fp_alert != NULL)
		fdrop(state->fp_alert, td);
	return (error);
}

static void
ntsync_wait_state_put(struct ntsync_wait_state *state, struct thread *td)
{
	int i;

	for (i = 0; i < state->nwa->count; i++)
		fdrop(state->fps[i], td);
	if (state->fp_alert != NULL)
		fdrop(state->fp_alert, td);
	free(state, M_NTSYNC);
}

static int
ntsync_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct ntsync_priv *owner;
	struct ntsync_wait_args *nwa;
	struct ntsync_wait_state *state;
	void *a;
	int error;

	error = devfs_get_cdevpriv(&a);
	if (error != 0)
		return (error);
	owner = a;

	switch (cmd) {
	case NTSYNC_IOC_CREATE_SEM:
		error = ntsync_create_sem((struct ntsync_sem_args *)data,
		    owner, td);
		break;
	case NTSYNC_IOC_CREATE_MUTEX:
		error = ntsync_create_mutex((struct ntsync_mutex_args *)data,
		    owner, td);
		break;
	case NTSYNC_IOC_CREATE_EVENT:
		error = ntsync_create_event((struct ntsync_event_args *)data,
		    owner, td);
		break;
	case NTSYNC_IOC_WAIT_ANY:
		nwa = (struct ntsync_wait_args *)data;
		error = ntsync_wait_state_get(nwa, cmd, owner, &state, td);
		if (error != 0)
			break;
		error = ntsync_wait(state, td);
		if (error == 0) {
			nwa->index = state->index;
			error = ntsync_ioctl_copyout(td, nwa, sizeof(*nwa));
			if (error == 0)
				error = state->error;
		}
		ntsync_wait_state_put(state, td);
		break;
	case NTSYNC_IOC_WAIT_ALL:
		nwa = (struct ntsync_wait_args *)data;
		error = ntsync_wait_state_get(nwa, cmd, owner, &state, td);
		if (error != 0)
			break;
		error = ntsync_wait(state, td);
		if (error == 0) {
			nwa->index = state->index;
			error = ntsync_ioctl_copyout(td, nwa, sizeof(*nwa));
			if (error == 0)
				error = state->error;
		}
		ntsync_wait_state_put(state, td);
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

struct cdevsw ntsync_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	ntsync_open,
	.d_close =	ntsync_close,
	.d_ioctl =	ntsync_ioctl,
	.d_name =	"ntsync",
};

static int
ntsync_modevent(module_t mod __unused, int type, void *data __unused)
{
	struct make_dev_args mda;
	int error;

	error = 0;
	switch (type) {
	case MOD_LOAD:
		make_dev_args_init(&mda);
		mda.mda_flags = MAKEDEV_WAITOK | MAKEDEV_CHECKNAME;
		mda.mda_devsw = &ntsync_cdevsw;
		mda.mda_uid = UID_ROOT;
		mda.mda_gid = GID_GAMES;
		mda.mda_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |
		    S_IROTH | S_IWOTH;

		error = make_dev_s(&mda, &ntsync_cdev, "ntsync");
		if (error != 0) {
			printf("cannot create ntsync dev err %d\n", error);
			break;
		}
		if (bootverbose)
			printf("ntsync\n");
		break;

	case MOD_UNLOAD:
		destroy_dev(ntsync_cdev);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
	}

	return (error);
}

DEV_MODULE(ntsync, ntsync_modevent, NULL);
MODULE_VERSION(ntsync, 1);
