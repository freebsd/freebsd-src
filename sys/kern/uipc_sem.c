/*-
 * Copyright (c) 2002 Alfred Perlstein <alfred@FreeBSD.org>
 * Copyright (c) 2003-2005 SPARTA, Inc.
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"
#include "opt_posix.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/condvar.h>
#include <sys/sem.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <posix4/ksem.h>
#include <posix4/posix4.h>
#include <posix4/semaphore.h>
#include <posix4/_semaphore.h>

static int sem_count_proc(struct proc *p);
static struct ksem *sem_lookup_byname(const char *name);
static int sem_create(struct thread *td, const char *name,
    struct ksem **ksret, mode_t mode, unsigned int value);
static void sem_free(struct ksem *ksnew);
static int sem_perm(struct thread *td, struct ksem *ks);
static void sem_enter(struct proc *p, struct ksem *ks);
static int sem_leave(struct proc *p, struct ksem *ks);
static void sem_exithook(void *arg, struct proc *p);
static void sem_forkhook(void *arg, struct proc *p1, struct proc *p2,
    int flags);
static int sem_hasopen(struct thread *td, struct ksem *ks);

static int kern_sem_close(struct thread *td, semid_t id);
static int kern_sem_post(struct thread *td, semid_t id);
static int kern_sem_wait(struct thread *td, semid_t id, int tryflag,
    struct timespec *abstime);
static int kern_sem_init(struct thread *td, int dir, unsigned int value,
    semid_t *idp);
static int kern_sem_open(struct thread *td, int dir, const char *name,
    int oflag, mode_t mode, unsigned int value, semid_t *idp);
static int kern_sem_unlink(struct thread *td, const char *name);

#ifndef SEM_MAX
#define SEM_MAX	30
#endif

#define SEM_MAX_NAMELEN	14

#define SEM_TO_ID(x)	((intptr_t)(x))
#define ID_TO_SEM(x)	id_to_sem(x)

/*
 * available semaphores go here, this includes sem_init and any semaphores
 * created via sem_open that have not yet been unlinked.
 */
LIST_HEAD(, ksem) ksem_head = LIST_HEAD_INITIALIZER(&ksem_head);
/*
 * semaphores still in use but have been sem_unlink()'d go here.
 */
LIST_HEAD(, ksem) ksem_deadhead = LIST_HEAD_INITIALIZER(&ksem_deadhead);

static struct mtx sem_lock;
static MALLOC_DEFINE(M_SEM, "sems", "semaphore data");

static int nsems = 0;
SYSCTL_DECL(_p1003_1b);
SYSCTL_INT(_p1003_1b, OID_AUTO, nsems, CTLFLAG_RD, &nsems, 0, "");

static eventhandler_tag sem_exit_tag, sem_exec_tag, sem_fork_tag;

#ifdef SEM_DEBUG
#define DP(x)	printf x
#else
#define DP(x)
#endif

static __inline
void
sem_ref(struct ksem *ks)
{

	mtx_assert(&sem_lock, MA_OWNED);
	ks->ks_ref++;
	DP(("sem_ref: ks = %p, ref = %d\n", ks, ks->ks_ref));
}

static __inline
void
sem_rel(struct ksem *ks)
{

	mtx_assert(&sem_lock, MA_OWNED);
	DP(("sem_rel: ks = %p, ref = %d\n", ks, ks->ks_ref - 1));
	if (--ks->ks_ref == 0)
		sem_free(ks);
}

static __inline struct ksem *id_to_sem(semid_t id);

static __inline
struct ksem *
id_to_sem(id)
	semid_t id;
{
	struct ksem *ks;

	mtx_assert(&sem_lock, MA_OWNED);
	DP(("id_to_sem: id = %0x,%p\n", id, (struct ksem *)id));
	LIST_FOREACH(ks, &ksem_head, ks_entry) {
		DP(("id_to_sem: ks = %p\n", ks));
		if (ks == (struct ksem *)id)
			return (ks);
	}
	return (NULL);
}

static struct ksem *
sem_lookup_byname(name)
	const char *name;
{
	struct ksem *ks;

	mtx_assert(&sem_lock, MA_OWNED);
	LIST_FOREACH(ks, &ksem_head, ks_entry)
		if (ks->ks_name != NULL && strcmp(ks->ks_name, name) == 0)
			return (ks);
	return (NULL);
}

static int
sem_create(td, name, ksret, mode, value)
	struct thread *td;
	const char *name;
	struct ksem **ksret;
	mode_t mode;
	unsigned int value;
{
	struct ksem *ret;
	struct proc *p;
	struct ucred *uc;
	size_t len;
	int error;

	DP(("sem_create\n"));
	p = td->td_proc;
	uc = td->td_ucred;
	if (value > SEM_VALUE_MAX)
		return (EINVAL);
	ret = malloc(sizeof(*ret), M_SEM, M_WAITOK | M_ZERO);
	if (name != NULL) {
		len = strlen(name);
		if (len > SEM_MAX_NAMELEN) {
			free(ret, M_SEM);
			return (ENAMETOOLONG);
		}
		/* name must start with a '/' but not contain one. */
		if (*name != '/' || len < 2 || index(name + 1, '/') != NULL) {
			free(ret, M_SEM);
			return (EINVAL);
		}
		ret->ks_name = malloc(len + 1, M_SEM, M_WAITOK);
		strcpy(ret->ks_name, name);
	} else {
		ret->ks_name = NULL;
	}
	ret->ks_mode = mode;
	ret->ks_value = value;
	ret->ks_ref = 1;
	ret->ks_waiters = 0;
	ret->ks_uid = uc->cr_uid;
	ret->ks_gid = uc->cr_gid;
	ret->ks_onlist = 0;
	cv_init(&ret->ks_cv, "sem");
	LIST_INIT(&ret->ks_users);
#ifdef MAC
	mac_init_posix_sem(ret);
	mac_create_posix_sem(uc, ret);
#endif
	if (name != NULL)
		sem_enter(td->td_proc, ret);
	*ksret = ret;
	mtx_lock(&sem_lock);
	if (nsems >= p31b_getcfg(CTL_P1003_1B_SEM_NSEMS_MAX)) {
		sem_leave(td->td_proc, ret);
		sem_free(ret);
		error = ENFILE;
	} else {
		nsems++;
		error = 0;
	}
	mtx_unlock(&sem_lock);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_init_args {
	unsigned int value;
	semid_t *idp;
};
int ksem_init(struct thread *td, struct ksem_init_args *uap);
#endif
int
ksem_init(td, uap)
	struct thread *td;
	struct ksem_init_args *uap;
{
	int error;

	error = kern_sem_init(td, UIO_USERSPACE, uap->value, uap->idp);
	return (error);
}

static int
kern_sem_init(td, dir, value, idp)
	struct thread *td;
	int dir;
	unsigned int value;
	semid_t *idp;
{
	struct ksem *ks;
	semid_t id;
	int error;

	error = sem_create(td, NULL, &ks, S_IRWXU | S_IRWXG, value);
	if (error)
		return (error);
	id = SEM_TO_ID(ks);
	if (dir == UIO_USERSPACE) {
		error = copyout(&id, idp, sizeof(id));
		if (error) {
			mtx_lock(&sem_lock);
			sem_rel(ks);
			mtx_unlock(&sem_lock);
			return (error);
		}
	} else {
		*idp = id;
	}
	mtx_lock(&sem_lock);
	LIST_INSERT_HEAD(&ksem_head, ks, ks_entry);
	ks->ks_onlist = 1;
	mtx_unlock(&sem_lock);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_open_args {
	char *name;
	int oflag;
	mode_t mode;
	unsigned int value;
	semid_t *idp;	
};
int ksem_open(struct thread *td, struct ksem_open_args *uap);
#endif
int
ksem_open(td, uap)
	struct thread *td;
	struct ksem_open_args *uap;
{
	char name[SEM_MAX_NAMELEN + 1];
	size_t done;
	int error;

	error = copyinstr(uap->name, name, SEM_MAX_NAMELEN + 1, &done);
	if (error)
		return (error);
	DP((">>> sem_open start\n"));
	error = kern_sem_open(td, UIO_USERSPACE,
	    name, uap->oflag, uap->mode, uap->value, uap->idp);
	DP(("<<< sem_open end\n"));
	return (error);
}

static int
kern_sem_open(td, dir, name, oflag, mode, value, idp)
	struct thread *td;
	int dir;
	const char *name;
	int oflag;
	mode_t mode;
	unsigned int value;
	semid_t *idp;
{
	struct ksem *ksnew, *ks;
	int error;
	semid_t id;

	ksnew = NULL;
	mtx_lock(&sem_lock);
	ks = sem_lookup_byname(name);
	/*
	 * If we found it but O_EXCL is set, error.
	 */
	if (ks != NULL && (oflag & O_EXCL) != 0) {
		mtx_unlock(&sem_lock);
		return (EEXIST);
	}
	/*
	 * If we didn't find it...
	 */
	if (ks == NULL) {
		/*
		 * didn't ask for creation? error.
		 */
		if ((oflag & O_CREAT) == 0) {
			mtx_unlock(&sem_lock);
			return (ENOENT);
		}
		/*
		 * We may block during creation, so drop the lock.
		 */
		mtx_unlock(&sem_lock);
		error = sem_create(td, name, &ksnew, mode, value);
		if (error != 0)
			return (error);
		id = SEM_TO_ID(ksnew);
		if (dir == UIO_USERSPACE) {
			DP(("about to copyout! %d to %p\n", id, idp));
			error = copyout(&id, idp, sizeof(id));
			if (error) {
				mtx_lock(&sem_lock);
				sem_leave(td->td_proc, ksnew);
				sem_rel(ksnew);
				mtx_unlock(&sem_lock);
				return (error);
			}
		} else {
			DP(("about to set! %d to %p\n", id, idp));
			*idp = id;
		}
		/*
		 * We need to make sure we haven't lost a race while
		 * allocating during creation.
		 */
		mtx_lock(&sem_lock);
		ks = sem_lookup_byname(name);
		if (ks != NULL) {
			/* we lost... */
			sem_leave(td->td_proc, ksnew);
			sem_rel(ksnew);
			/* we lost and we can't loose... */
			if ((oflag & O_EXCL) != 0) {
				mtx_unlock(&sem_lock);
				return (EEXIST);
			}
		} else {
			DP(("sem_create: about to add to list...\n"));
			LIST_INSERT_HEAD(&ksem_head, ksnew, ks_entry); 
			DP(("sem_create: setting list bit...\n"));
			ksnew->ks_onlist = 1;
			DP(("sem_create: done, about to unlock...\n"));
		}
	} else {
#ifdef MAC
		error = mac_check_posix_sem_open(td->td_ucred, ks);
		if (error)
			goto err_open;
#endif
		/*
		 * if we aren't the creator, then enforce permissions.
		 */
		error = sem_perm(td, ks);
		if (error)
			goto err_open;
		sem_ref(ks);
		mtx_unlock(&sem_lock);
		id = SEM_TO_ID(ks);
		if (dir == UIO_USERSPACE) {
			error = copyout(&id, idp, sizeof(id));
			if (error) {
				mtx_lock(&sem_lock);
				sem_rel(ks);
				mtx_unlock(&sem_lock);
				return (error);
			}
		} else {
			*idp = id;
		}
		sem_enter(td->td_proc, ks);
		mtx_lock(&sem_lock);
		sem_rel(ks);
	}
err_open:
	mtx_unlock(&sem_lock);
	return (error);
}

static int
sem_perm(td, ks)
	struct thread *td;
	struct ksem *ks;
{
	struct ucred *uc;

	uc = td->td_ucred;
	DP(("sem_perm: uc(%d,%d) ks(%d,%d,%o)\n",
	    uc->cr_uid, uc->cr_gid,
	     ks->ks_uid, ks->ks_gid, ks->ks_mode));
	if ((uc->cr_uid == ks->ks_uid && (ks->ks_mode & S_IWUSR) != 0) ||
	    (uc->cr_gid == ks->ks_gid && (ks->ks_mode & S_IWGRP) != 0) ||
	    (ks->ks_mode & S_IWOTH) != 0 || suser(td) == 0)
		return (0);
	return (EPERM);
}

static void
sem_free(struct ksem *ks)
{

	nsems--;
	if (ks->ks_onlist)
		LIST_REMOVE(ks, ks_entry);
	if (ks->ks_name != NULL)
		free(ks->ks_name, M_SEM);
	cv_destroy(&ks->ks_cv);
	free(ks, M_SEM);
}

static __inline struct kuser *sem_getuser(struct proc *p, struct ksem *ks);

static __inline struct kuser *
sem_getuser(p, ks)
	struct proc *p;
	struct ksem *ks;
{
	struct kuser *k;

	LIST_FOREACH(k, &ks->ks_users, ku_next)
		if (k->ku_pid == p->p_pid)
			return (k);
	return (NULL);
}

static int
sem_hasopen(td, ks)
	struct thread *td;
	struct ksem *ks;
{
	
	return ((ks->ks_name == NULL && sem_perm(td, ks) == 0)
	    || sem_getuser(td->td_proc, ks) != NULL);
}

static int
sem_leave(p, ks)
	struct proc *p;
	struct ksem *ks;
{
	struct kuser *k;

	DP(("sem_leave: ks = %p\n", ks));
	k = sem_getuser(p, ks);
	DP(("sem_leave: ks = %p, k = %p\n", ks, k));
	if (k != NULL) {
		LIST_REMOVE(k, ku_next);
		sem_rel(ks);
		DP(("sem_leave: about to free k\n"));
		free(k, M_SEM);
		DP(("sem_leave: returning\n"));
		return (0);
	}
	return (EINVAL);
}

static void
sem_enter(p, ks)
	struct proc *p;
	struct ksem *ks;
{
	struct kuser *ku, *k;

	ku = malloc(sizeof(*ku), M_SEM, M_WAITOK);
	ku->ku_pid = p->p_pid;
	mtx_lock(&sem_lock);
	k = sem_getuser(p, ks);
	if (k != NULL) {
		mtx_unlock(&sem_lock);
		free(ku, M_TEMP);
		return;
	}
	LIST_INSERT_HEAD(&ks->ks_users, ku, ku_next);
	sem_ref(ks);
	mtx_unlock(&sem_lock);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_unlink_args {
	char *name;
};
int ksem_unlink(struct thread *td, struct ksem_unlink_args *uap);
#endif
	
int
ksem_unlink(td, uap)
	struct thread *td;
	struct ksem_unlink_args *uap;
{
	char name[SEM_MAX_NAMELEN + 1];
	size_t done;
	int error;

	error = copyinstr(uap->name, name, SEM_MAX_NAMELEN + 1, &done);
	return (error ? error :
	    kern_sem_unlink(td, name));
}

static int
kern_sem_unlink(td, name)
	struct thread *td;
	const char *name;
{
	struct ksem *ks;
	int error;

	mtx_lock(&sem_lock);
	ks = sem_lookup_byname(name);
	if (ks != NULL) {
#ifdef MAC
		error = mac_check_posix_sem_unlink(td->td_ucred, ks);
		if (error) {
			mtx_unlock(&sem_lock);
			return (error);
		}
#endif
		error = sem_perm(td, ks);
	} else
		error = ENOENT;
	DP(("sem_unlink: '%s' ks = %p, error = %d\n", name, ks, error));
	if (error == 0) {
		LIST_REMOVE(ks, ks_entry);
		LIST_INSERT_HEAD(&ksem_deadhead, ks, ks_entry); 
		sem_rel(ks);
	}
	mtx_unlock(&sem_lock);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_close_args {
	semid_t id;
};
int ksem_close(struct thread *td, struct ksem_close_args *uap);
#endif

int
ksem_close(struct thread *td, struct ksem_close_args *uap)
{

	return (kern_sem_close(td, uap->id));
}

static int
kern_sem_close(td, id)
	struct thread *td;
	semid_t id;
{
	struct ksem *ks;
	int error;

	error = EINVAL;
	mtx_lock(&sem_lock);
	ks = ID_TO_SEM(id);
	/* this is not a valid operation for unnamed sems */
	if (ks != NULL && ks->ks_name != NULL)
		error = sem_leave(td->td_proc, ks);
	mtx_unlock(&sem_lock);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_post_args {
	semid_t id;
};
int ksem_post(struct thread *td, struct ksem_post_args *uap);
#endif
int
ksem_post(td, uap)
	struct thread *td;
	struct ksem_post_args *uap;
{

	return (kern_sem_post(td, uap->id));
}

static int
kern_sem_post(td, id)
	struct thread *td;
	semid_t id;
{
	struct ksem *ks;
	int error;

	mtx_lock(&sem_lock);
	ks = ID_TO_SEM(id);
	if (ks == NULL || !sem_hasopen(td, ks)) {
		error = EINVAL;
		goto err;
	}
#ifdef MAC
	error = mac_check_posix_sem_post(td->td_ucred, ks);
	if (error)
		goto err;
#endif
	if (ks->ks_value == SEM_VALUE_MAX) {
		error = EOVERFLOW;
		goto err;
	}
	++ks->ks_value;
	if (ks->ks_waiters > 0)
		cv_signal(&ks->ks_cv);
	error = 0;
err:
	mtx_unlock(&sem_lock);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_wait_args {
	semid_t id;
};
int ksem_wait(struct thread *td, struct ksem_wait_args *uap);
#endif

int
ksem_wait(td, uap)
	struct thread *td;
	struct ksem_wait_args *uap;
{

	return (kern_sem_wait(td, uap->id, 0, NULL));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_timedwait_args {
	semid_t id;
	struct timespec *abstime;
};
int ksem_timedwait(struct thread *td, struct ksem_timedwait_args *uap);
#endif
int
ksem_timedwait(td, uap)
	struct thread *td;
	struct ksem_timedwait_args *uap;
{
	struct timespec abstime;
	struct timespec *ts;
	int error;

	/* We allow a null timespec (wait forever). */
	if (uap->abstime == NULL)
		ts = NULL;
	else {
		error = copyin(uap->abstime, &abstime, sizeof(abstime));
		if (error != 0)
			return (error);
		if (abstime.tv_nsec >= 1000000000 || abstime.tv_nsec < 0)
			return (EINVAL);
		ts = &abstime;
	}
	return (kern_sem_wait(td, uap->id, 0, ts));
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_trywait_args {
	semid_t id;
};
int ksem_trywait(struct thread *td, struct ksem_trywait_args *uap);
#endif
int
ksem_trywait(td, uap)
	struct thread *td;
	struct ksem_trywait_args *uap;
{

	return (kern_sem_wait(td, uap->id, 1, NULL));
}

static int
kern_sem_wait(td, id, tryflag, abstime)
	struct thread *td;
	semid_t id;
	int tryflag;
	struct timespec *abstime;
{
	struct timespec ts1, ts2;
	struct timeval tv;
	struct ksem *ks;
	int error;

	DP((">>> kern_sem_wait entered!\n"));
	mtx_lock(&sem_lock);
	ks = ID_TO_SEM(id);
	if (ks == NULL) {
		DP(("kern_sem_wait ks == NULL\n"));
		error = EINVAL;
		goto err;
	}
	sem_ref(ks);
	if (!sem_hasopen(td, ks)) {
		DP(("kern_sem_wait hasopen failed\n"));
		error = EINVAL;
		goto err;
	}
#ifdef MAC
	error = mac_check_posix_sem_wait(td->td_ucred, ks);
	if (error) {
		DP(("kern_sem_wait mac failed\n"));
		goto err;
	}
#endif
	DP(("kern_sem_wait value = %d, tryflag %d\n", ks->ks_value, tryflag));
	if (ks->ks_value == 0) {
		ks->ks_waiters++;
		if (tryflag != 0)
			error = EAGAIN;
		else if (abstime == NULL)
			error = cv_wait_sig(&ks->ks_cv, &sem_lock);
		else {
			for (;;) {
				ts1 = *abstime;
				getnanotime(&ts2);
				timespecsub(&ts1, &ts2);
				TIMESPEC_TO_TIMEVAL(&tv, &ts1);
				if (tv.tv_sec < 0) {
					error = ETIMEDOUT;
					break;
				}
				error = cv_timedwait_sig(&ks->ks_cv,
				    &sem_lock, tvtohz(&tv));
				if (error != EWOULDBLOCK)
					break;
			}
		}
		ks->ks_waiters--;
		if (error)
			goto err;
	}
	ks->ks_value--;
	error = 0;
err:
	if (ks != NULL)
		sem_rel(ks);
	mtx_unlock(&sem_lock);
	DP(("<<< kern_sem_wait leaving, error = %d\n", error));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_getvalue_args {
	semid_t id;
	int *val;
};
int ksem_getvalue(struct thread *td, struct ksem_getvalue_args *uap);
#endif
int
ksem_getvalue(td, uap)
	struct thread *td;
	struct ksem_getvalue_args *uap;
{
	struct ksem *ks;
	int error, val;

	mtx_lock(&sem_lock);
	ks = ID_TO_SEM(uap->id);
	if (ks == NULL || !sem_hasopen(td, ks)) {
		mtx_unlock(&sem_lock);
		return (EINVAL);
	}
#ifdef MAC
	error = mac_check_posix_sem_getvalue(td->td_ucred, ks);
	if (error) {
		mtx_unlock(&sem_lock);
		return (error);
	}
#endif
	val = ks->ks_value;
	mtx_unlock(&sem_lock);
	error = copyout(&val, uap->val, sizeof(val));
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct ksem_destroy_args {
	semid_t id;
};
int ksem_destroy(struct thread *td, struct ksem_destroy_args *uap);
#endif
int
ksem_destroy(td, uap)
	struct thread *td;
	struct ksem_destroy_args *uap;
{
	struct ksem *ks;
	int error;

	mtx_lock(&sem_lock);
	ks = ID_TO_SEM(uap->id);
	if (ks == NULL || !sem_hasopen(td, ks) ||
	    ks->ks_name != NULL) {
		error = EINVAL;
		goto err;
	}
#ifdef MAC
	error = mac_check_posix_sem_destroy(td->td_ucred, ks);
	if (error)
		goto err;
#endif
	if (ks->ks_waiters != 0) {
		error = EBUSY;
		goto err;
	}
	sem_rel(ks);
	error = 0;
err:
	mtx_unlock(&sem_lock);
	return (error);
}

/*
 * Count the number of kusers associated with a proc, so as to guess at how
 * many to allocate when forking.
 */
static int
sem_count_proc(p)
	struct proc *p;
{
	struct ksem *ks;
	struct kuser *ku;
	int count;

	mtx_assert(&sem_lock, MA_OWNED);

	count = 0;
	LIST_FOREACH(ks, &ksem_head, ks_entry) {
		LIST_FOREACH(ku, &ks->ks_users, ku_next) {
			if (ku->ku_pid == p->p_pid)
				count++;
		}
	}
	LIST_FOREACH(ks, &ksem_deadhead, ks_entry) {
		LIST_FOREACH(ku, &ks->ks_users, ku_next) {
			if (ku->ku_pid == p->p_pid)
				count++;
		}
	}
	return (count);
}

/*
 * When a process forks, the child process must gain a reference to each open
 * semaphore in the parent process, whether it is unlinked or not.  This
 * requires allocating a kuser structure for each semaphore reference in the
 * new process.  Because the set of semaphores in the parent can change while
 * the fork is in progress, we have to handle races -- first we attempt to
 * allocate enough storage to acquire references to each of the semaphores,
 * then we enter the semaphores and release the temporary references.
 */
static void
sem_forkhook(arg, p1, p2, flags)
	void *arg;
	struct proc *p1;
	struct proc *p2;
	int flags;
{
	struct ksem *ks, **sem_array;
	int count, i, new_count;
	struct kuser *ku;

	mtx_lock(&sem_lock);
	count = sem_count_proc(p1);
	if (count == 0) {
		mtx_unlock(&sem_lock);
		return;
	}
race_lost:
	mtx_assert(&sem_lock, MA_OWNED);
	mtx_unlock(&sem_lock);
	sem_array = malloc(sizeof(struct ksem *) * count, M_TEMP, M_WAITOK);
	mtx_lock(&sem_lock);
	new_count = sem_count_proc(p1);
	if (count < new_count) {
		/* Lost race, repeat and allocate more storage. */
		free(sem_array, M_TEMP);
		count = new_count;
		goto race_lost;
	}
	/*
	 * Given an array capable of storing an adequate number of semaphore
	 * references, now walk the list of semaphores and acquire a new
	 * reference for any semaphore opened by p1.
	 */
	count = new_count;
	i = 0;
	LIST_FOREACH(ks, &ksem_head, ks_entry) {
		LIST_FOREACH(ku, &ks->ks_users, ku_next) {
			if (ku->ku_pid == p1->p_pid) {
				sem_ref(ks);
				sem_array[i] = ks;
				i++;
				break;
			}
		}
	}
	LIST_FOREACH(ks, &ksem_deadhead, ks_entry) {
		LIST_FOREACH(ku, &ks->ks_users, ku_next) {
			if (ku->ku_pid == p1->p_pid) {
				sem_ref(ks);
				sem_array[i] = ks;
				i++;
				break;
			}
		}
	}
	mtx_unlock(&sem_lock);
	KASSERT(i == count, ("sem_forkhook: i != count (%d, %d)", i, count));
	/*
	 * Now cause p2 to enter each of the referenced semaphores, then
	 * release our temporary reference.  This is pretty inefficient.
	 * Finally, free our temporary array.
	 */
	for (i = 0; i < count; i++) {
		sem_enter(p2, sem_array[i]);
		mtx_lock(&sem_lock);
		sem_rel(sem_array[i]);
		mtx_unlock(&sem_lock);
	}
	free(sem_array, M_TEMP);
}

static void
sem_exithook(arg, p)
	void *arg;
	struct proc *p;
{
	struct ksem *ks, *ksnext;

	mtx_lock(&sem_lock);
	ks = LIST_FIRST(&ksem_head);
	while (ks != NULL) {
		ksnext = LIST_NEXT(ks, ks_entry);
		sem_leave(p, ks);
		ks = ksnext;
	}
	ks = LIST_FIRST(&ksem_deadhead);
	while (ks != NULL) {
		ksnext = LIST_NEXT(ks, ks_entry);
		sem_leave(p, ks);
		ks = ksnext;
	}
	mtx_unlock(&sem_lock);
}

static int
sem_modload(struct module *module, int cmd, void *arg)
{
        int error = 0;

        switch (cmd) {
        case MOD_LOAD:
		mtx_init(&sem_lock, "sem", "semaphore", MTX_DEF);
		p31b_setcfg(CTL_P1003_1B_SEM_NSEMS_MAX, SEM_MAX);
		p31b_setcfg(CTL_P1003_1B_SEM_VALUE_MAX, SEM_VALUE_MAX);
		sem_exit_tag = EVENTHANDLER_REGISTER(process_exit, sem_exithook,
		    NULL, EVENTHANDLER_PRI_ANY);
		sem_exec_tag = EVENTHANDLER_REGISTER(process_exec, sem_exithook,
		    NULL, EVENTHANDLER_PRI_ANY);
		sem_fork_tag = EVENTHANDLER_REGISTER(process_fork, sem_forkhook, NULL, EVENTHANDLER_PRI_ANY);
                break;
        case MOD_UNLOAD:
		if (nsems != 0) {
			error = EOPNOTSUPP;
			break;
		}
		EVENTHANDLER_DEREGISTER(process_exit, sem_exit_tag);
		EVENTHANDLER_DEREGISTER(process_exec, sem_exec_tag);
		EVENTHANDLER_DEREGISTER(process_fork, sem_fork_tag);
		mtx_destroy(&sem_lock);
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                error = EINVAL;
                break;
        }
        return (error);
}

static moduledata_t sem_mod = {
        "sem",
        &sem_modload,
        NULL
};

SYSCALL_MODULE_HELPER(ksem_init);
SYSCALL_MODULE_HELPER(ksem_open);
SYSCALL_MODULE_HELPER(ksem_unlink);
SYSCALL_MODULE_HELPER(ksem_close);
SYSCALL_MODULE_HELPER(ksem_post);
SYSCALL_MODULE_HELPER(ksem_wait);
SYSCALL_MODULE_HELPER(ksem_timedwait);
SYSCALL_MODULE_HELPER(ksem_trywait);
SYSCALL_MODULE_HELPER(ksem_getvalue);
SYSCALL_MODULE_HELPER(ksem_destroy);

DECLARE_MODULE(sem, sem_mod, SI_SUB_SYSV_SEM, SI_ORDER_FIRST);
MODULE_VERSION(sem, 1);
