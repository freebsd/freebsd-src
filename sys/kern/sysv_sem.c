/* $FreeBSD$ */

/*
 * Implementation of SVID semaphores
 *
 * Author:  Daniel Boulet
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sem.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/jail.h>

static MALLOC_DEFINE(M_SEM, "sem", "SVID compatible semaphores");

static void seminit(void);
static int sysvsem_modload(struct module *, int, void *);
static int semunload(void);
static void semexit_myhook(struct proc *p);
static int sysctl_sema(SYSCTL_HANDLER_ARGS);

#ifndef _SYS_SYSPROTO_H_
struct __semctl_args;
int __semctl(struct thread *td, struct __semctl_args *uap);
struct semget_args;
int semget(struct thread *td, struct semget_args *uap);
struct semop_args;
int semop(struct thread *td, struct semop_args *uap);
#endif

static struct sem_undo *semu_alloc(struct thread *td);
static int semundo_adjust(struct thread *td, struct sem_undo **supptr, 
		int semid, int semnum, int adjval);
static void semundo_clear(int semid, int semnum);

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *semcalls[] = {
	(sy_call_t *)__semctl, (sy_call_t *)semget,
	(sy_call_t *)semop
};

static int	semtot = 0;
static struct semid_ds *sema;	/* semaphore id pool */
static struct sem *sem;		/* semaphore pool */
static struct sem_undo *semu_list; /* list of active undo structures */
static int	*semu;		/* undo structure pool */

struct sem {
	u_short	semval;		/* semaphore value */
	pid_t	sempid;		/* pid of last operation */
	u_short	semncnt;	/* # awaiting semval > cval */
	u_short	semzcnt;	/* # awaiting semval = 0 */
};

/*
 * Undo structure (one per process)
 */
struct sem_undo {
	struct	sem_undo *un_next;	/* ptr to next active undo structure */
	struct	proc *un_proc;		/* owner of this structure */
	short	un_cnt;			/* # of active entries */
	struct undo {
		short	un_adjval;	/* adjust on exit values */
		short	un_num;		/* semaphore # */
		int	un_id;		/* semid */
	} un_ent[1];			/* undo entries */
};

/*
 * Configuration parameters
 */
#ifndef SEMMNI
#define SEMMNI	10		/* # of semaphore identifiers */
#endif
#ifndef SEMMNS
#define SEMMNS	60		/* # of semaphores in system */
#endif
#ifndef SEMUME
#define SEMUME	10		/* max # of undo entries per process */
#endif
#ifndef SEMMNU
#define SEMMNU	30		/* # of undo structures in system */
#endif

/* shouldn't need tuning */
#ifndef SEMMAP
#define SEMMAP	30		/* # of entries in semaphore map */
#endif
#ifndef SEMMSL
#define SEMMSL	SEMMNS		/* max # of semaphores per id */
#endif
#ifndef SEMOPM
#define SEMOPM	100		/* max # of operations per semop call */
#endif

#define SEMVMX	32767		/* semaphore maximum value */
#define SEMAEM	16384		/* adjust on exit max value */

/*
 * Due to the way semaphore memory is allocated, we have to ensure that
 * SEMUSZ is properly aligned.
 */

#define SEM_ALIGN(bytes) (((bytes) + (sizeof(long) - 1)) & ~(sizeof(long) - 1))

/* actual size of an undo structure */
#define SEMUSZ	SEM_ALIGN(offsetof(struct sem_undo, un_ent[SEMUME]))

/*
 * Macro to find a particular sem_undo vector
 */
#define SEMU(ix)	((struct sem_undo *)(((intptr_t)semu)+ix * seminfo.semusz))

/*
 * semaphore info struct
 */
struct seminfo seminfo = {
                SEMMAP,         /* # of entries in semaphore map */
                SEMMNI,         /* # of semaphore identifiers */
                SEMMNS,         /* # of semaphores in system */
                SEMMNU,         /* # of undo structures in system */
                SEMMSL,         /* max # of semaphores per id */
                SEMOPM,         /* max # of operations per semop call */
                SEMUME,         /* max # of undo entries per process */
                SEMUSZ,         /* size in bytes of undo structure */
                SEMVMX,         /* semaphore maximum value */
                SEMAEM          /* adjust on exit max value */
};

SYSCTL_DECL(_kern_ipc);
SYSCTL_INT(_kern_ipc, OID_AUTO, semmap, CTLFLAG_RW, &seminfo.semmap, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmni, CTLFLAG_RD, &seminfo.semmni, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmns, CTLFLAG_RD, &seminfo.semmns, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmnu, CTLFLAG_RD, &seminfo.semmnu, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmsl, CTLFLAG_RW, &seminfo.semmsl, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semopm, CTLFLAG_RD, &seminfo.semopm, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semume, CTLFLAG_RD, &seminfo.semume, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semusz, CTLFLAG_RD, &seminfo.semusz, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semvmx, CTLFLAG_RW, &seminfo.semvmx, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semaem, CTLFLAG_RW, &seminfo.semaem, 0, "");
SYSCTL_PROC(_kern_ipc, OID_AUTO, sema, CTLFLAG_RD,
    NULL, 0, sysctl_sema, "", "");

static void
seminit(void)
{
	register int i;

	TUNABLE_INT_FETCH("kern.ipc.semmap", &seminfo.semmap);
	TUNABLE_INT_FETCH("kern.ipc.semmni", &seminfo.semmni);
	TUNABLE_INT_FETCH("kern.ipc.semmns", &seminfo.semmns);
	TUNABLE_INT_FETCH("kern.ipc.semmnu", &seminfo.semmnu);
	TUNABLE_INT_FETCH("kern.ipc.semmsl", &seminfo.semmsl);
	TUNABLE_INT_FETCH("kern.ipc.semopm", &seminfo.semopm);
	TUNABLE_INT_FETCH("kern.ipc.semume", &seminfo.semume);
	TUNABLE_INT_FETCH("kern.ipc.semusz", &seminfo.semusz);
	TUNABLE_INT_FETCH("kern.ipc.semvmx", &seminfo.semvmx);
	TUNABLE_INT_FETCH("kern.ipc.semaem", &seminfo.semaem);

	sem = malloc(sizeof(struct sem) * seminfo.semmns, M_SEM, M_WAITOK);
	if (sem == NULL)
		panic("sem is NULL");
	sema = malloc(sizeof(struct semid_ds) * seminfo.semmni, M_SEM, M_WAITOK);
	if (sema == NULL)
		panic("sema is NULL");
	semu = malloc(seminfo.semmnu * seminfo.semusz, M_SEM, M_WAITOK);
	if (semu == NULL)
		panic("semu is NULL");

	for (i = 0; i < seminfo.semmni; i++) {
		sema[i].sem_base = 0;
		sema[i].sem_perm.mode = 0;
	}
	for (i = 0; i < seminfo.semmnu; i++) {
		register struct sem_undo *suptr = SEMU(i);
		suptr->un_proc = NULL;
	}
	semu_list = NULL;
	at_exit(semexit_myhook);
}

static int
semunload(void)
{

	if (semtot != 0)
		return (EBUSY);

	free(sem, M_SEM);
	free(sema, M_SEM);
	free(semu, M_SEM);
	rm_at_exit(semexit_myhook);
	return (0);
}

static int
sysvsem_modload(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD:
		seminit();
		break;
	case MOD_UNLOAD:
		error = semunload();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static moduledata_t sysvsem_mod = {
	"sysvsem",
	&sysvsem_modload,
	NULL
};

SYSCALL_MODULE_HELPER(semsys);
SYSCALL_MODULE_HELPER(__semctl);
SYSCALL_MODULE_HELPER(semget);
SYSCALL_MODULE_HELPER(semop);

DECLARE_MODULE(sysvsem, sysvsem_mod,
	SI_SUB_SYSV_SEM, SI_ORDER_FIRST);
MODULE_VERSION(sysvsem, 1);

/*
 * Entry point for all SEM calls
 *
 * MPSAFE
 */
int
semsys(td, uap)
	struct thread *td;
	/* XXX actually varargs. */
	struct semsys_args /* {
		u_int	which;
		int	a2;
		int	a3;
		int	a4;
		int	a5;
	} */ *uap;
{
	int error;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	if (uap->which >= sizeof(semcalls)/sizeof(semcalls[0]))
		return (EINVAL);
	mtx_lock(&Giant);
	error = (*semcalls[uap->which])(td, &uap->a2);
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Allocate a new sem_undo structure for a process
 * (returns ptr to structure or NULL if no more room)
 */

static struct sem_undo *
semu_alloc(td)
	struct thread *td;
{
	register int i;
	register struct sem_undo *suptr;
	register struct sem_undo **supptr;
	int attempt;

	/*
	 * Try twice to allocate something.
	 * (we'll purge any empty structures after the first pass so
	 * two passes are always enough)
	 */

	for (attempt = 0; attempt < 2; attempt++) {
		/*
		 * Look for a free structure.
		 * Fill it in and return it if we find one.
		 */

		for (i = 0; i < seminfo.semmnu; i++) {
			suptr = SEMU(i);
			if (suptr->un_proc == NULL) {
				suptr->un_next = semu_list;
				semu_list = suptr;
				suptr->un_cnt = 0;
				suptr->un_proc = td->td_proc;
				return(suptr);
			}
		}

		/*
		 * We didn't find a free one, if this is the first attempt
		 * then try to free some structures.
		 */

		if (attempt == 0) {
			/* All the structures are in use - try to free some */
			int did_something = 0;

			supptr = &semu_list;
			while ((suptr = *supptr) != NULL) {
				if (suptr->un_cnt == 0)  {
					suptr->un_proc = NULL;
					*supptr = suptr->un_next;
					did_something = 1;
				} else
					supptr = &(suptr->un_next);
			}

			/* If we didn't free anything then just give-up */
			if (!did_something)
				return(NULL);
		} else {
			/*
			 * The second pass failed even though we freed
			 * something after the first pass!
			 * This is IMPOSSIBLE!
			 */
			panic("semu_alloc - second attempt failed");
		}
	}
	return (NULL);
}

/*
 * Adjust a particular entry for a particular proc
 */

static int
semundo_adjust(td, supptr, semid, semnum, adjval)
	register struct thread *td;
	struct sem_undo **supptr;
	int semid, semnum;
	int adjval;
{
	struct proc *p = td->td_proc;
	register struct sem_undo *suptr;
	register struct undo *sunptr;
	int i;

	/* Look for and remember the sem_undo if the caller doesn't provide
	   it */

	suptr = *supptr;
	if (suptr == NULL) {
		for (suptr = semu_list; suptr != NULL;
		    suptr = suptr->un_next) {
			if (suptr->un_proc == p) {
				*supptr = suptr;
				break;
			}
		}
		if (suptr == NULL) {
			if (adjval == 0)
				return(0);
			suptr = semu_alloc(td);
			if (suptr == NULL)
				return(ENOSPC);
			*supptr = suptr;
		}
	}

	/*
	 * Look for the requested entry and adjust it (delete if adjval becomes
	 * 0).
	 */
	sunptr = &suptr->un_ent[0];
	for (i = 0; i < suptr->un_cnt; i++, sunptr++) {
		if (sunptr->un_id != semid || sunptr->un_num != semnum)
			continue;
		if (adjval != 0) {
			adjval += sunptr->un_adjval;
			if (adjval > seminfo.semaem || adjval < -seminfo.semaem)
				return (ERANGE);
		}
		sunptr->un_adjval = adjval;
		if (sunptr->un_adjval == 0) {
			suptr->un_cnt--;
			if (i < suptr->un_cnt)
				suptr->un_ent[i] =
				    suptr->un_ent[suptr->un_cnt];
		}
		return(0);
	}

	/* Didn't find the right entry - create it */
	if (adjval == 0)
		return(0);
	if (adjval > seminfo.semaem || adjval < -seminfo.semaem)
		return (ERANGE);
	if (suptr->un_cnt != seminfo.semume) {
		sunptr = &suptr->un_ent[suptr->un_cnt];
		suptr->un_cnt++;
		sunptr->un_adjval = adjval;
		sunptr->un_id = semid; sunptr->un_num = semnum;
	} else
		return(EINVAL);
	return(0);
}

static void
semundo_clear(semid, semnum)
	int semid, semnum;
{
	register struct sem_undo *suptr;

	for (suptr = semu_list; suptr != NULL; suptr = suptr->un_next) {
		register struct undo *sunptr = &suptr->un_ent[0];
		register int i = 0;

		while (i < suptr->un_cnt) {
			if (sunptr->un_id == semid) {
				if (semnum == -1 || sunptr->un_num == semnum) {
					suptr->un_cnt--;
					if (i < suptr->un_cnt) {
						suptr->un_ent[i] =
						  suptr->un_ent[suptr->un_cnt];
						continue;
					}
				}
				if (semnum != -1)
					break;
			}
			i++, sunptr++;
		}
	}
}

/*
 * Note that the user-mode half of this passes a union, not a pointer
 */
#ifndef _SYS_SYSPROTO_H_
struct __semctl_args {
	int	semid;
	int	semnum;
	int	cmd;
	union	semun *arg;
};
#endif

/*
 * MPSAFE
 */
int
__semctl(td, uap)
	struct thread *td;
	register struct __semctl_args *uap;
{
	int semid = uap->semid;
	int semnum = uap->semnum;
	int cmd = uap->cmd;
	union semun *arg = uap->arg;
	union semun real_arg;
	struct ucred *cred = td->td_ucred;
	int i, rval, error;
	struct semid_ds sbuf;
	register struct semid_ds *semaptr;
	u_short usval;

#ifdef SEM_DEBUG
	printf("call to semctl(%d, %d, %d, 0x%x)\n", semid, semnum, cmd, arg);
#endif
	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&Giant);
	switch(cmd) {
	case SEM_STAT:
		if (semid < 0 || semid >= seminfo.semmni)
			UGAR(EINVAL);
		semaptr = &sema[semid];
		if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0 )
			UGAR(EINVAL);
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			UGAR(error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			UGAR(error);
		error = copyout(semaptr, real_arg.buf, sizeof(struct semid_ds));
		rval = IXSEQ_TO_IPCID(semid,semaptr->sem_perm);
		if (error == 0)
			td->td_retval[0] = rval;
		goto done2;
	}

	semid = IPCID_TO_IX(semid);
	if (semid < 0 || semid >= seminfo.semmni) {
		error = EINVAL;
		goto done2;
	}

	semaptr = &sema[semid];
	if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0 ||
	    semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid)) {
		error = EINVAL;
		goto done2;
	}

	error = 0;
	rval = 0;

	switch (cmd) {
	case IPC_RMID:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_M)))
			goto done2;
		semaptr->sem_perm.cuid = cred->cr_uid;
		semaptr->sem_perm.uid = cred->cr_uid;
		semtot -= semaptr->sem_nsems;
		for (i = semaptr->sem_base - sem; i < semtot; i++)
			sem[i] = sem[i + semaptr->sem_nsems];
		for (i = 0; i < seminfo.semmni; i++) {
			if ((sema[i].sem_perm.mode & SEM_ALLOC) &&
			    sema[i].sem_base > semaptr->sem_base)
				sema[i].sem_base -= semaptr->sem_nsems;
		}
		semaptr->sem_perm.mode = 0;
		semundo_clear(semid, -1);
		wakeup(semaptr);
		break;

	case IPC_SET:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_M)))
			goto done2;
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			goto done2;
		if ((error = copyin(real_arg.buf, &sbuf, sizeof(sbuf))) != 0) {
			goto done2;
		}
		semaptr->sem_perm.uid = sbuf.sem_perm.uid;
		semaptr->sem_perm.gid = sbuf.sem_perm.gid;
		semaptr->sem_perm.mode = (semaptr->sem_perm.mode & ~0777) |
		    (sbuf.sem_perm.mode & 0777);
		semaptr->sem_ctime = time_second;
		break;

	case IPC_STAT:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			goto done2;
		error = copyout(semaptr, real_arg.buf,
				sizeof(struct semid_ds));
		break;

	case GETNCNT:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if (semnum < 0 || semnum >= semaptr->sem_nsems) {
			error = EINVAL;
			goto done2;
		}
		rval = semaptr->sem_base[semnum].semncnt;
		break;

	case GETPID:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if (semnum < 0 || semnum >= semaptr->sem_nsems) {
			error = EINVAL;
			goto done2;
		}
		rval = semaptr->sem_base[semnum].sempid;
		break;

	case GETVAL:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if (semnum < 0 || semnum >= semaptr->sem_nsems) {
			error = EINVAL;
			goto done2;
		}
		rval = semaptr->sem_base[semnum].semval;
		break;

	case GETALL:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			goto done2;
		for (i = 0; i < semaptr->sem_nsems; i++) {
			error = copyout(&semaptr->sem_base[i].semval,
			    &real_arg.array[i], sizeof(real_arg.array[0]));
			if (error != 0)
				break;
		}
		break;

	case GETZCNT:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_R)))
			goto done2;
		if (semnum < 0 || semnum >= semaptr->sem_nsems) {
			error = EINVAL;
			goto done2;
		}
		rval = semaptr->sem_base[semnum].semzcnt;
		break;

	case SETVAL:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_W)))
			goto done2;
		if (semnum < 0 || semnum >= semaptr->sem_nsems) {
			error = EINVAL;
			goto done2;
		}
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			goto done2;
		if (real_arg.val < 0 || real_arg.val > seminfo.semvmx) {
			error = ERANGE;
			goto done2;
		}
		semaptr->sem_base[semnum].semval = real_arg.val;
		semundo_clear(semid, semnum);
		wakeup(semaptr);
		break;

	case SETALL:
		if ((error = ipcperm(td, &semaptr->sem_perm, IPC_W)))
			goto done2;
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			goto done2;
		for (i = 0; i < semaptr->sem_nsems; i++) {
			error = copyin(&real_arg.array[i],
			    &usval, sizeof(real_arg.array[0]));
			if (error != 0)
				break;
			if (usval > seminfo.semvmx) {
				error = ERANGE;
				break;
			}
			semaptr->sem_base[i].semval = usval;
		}
		semundo_clear(semid, -1);
		wakeup(semaptr);
		break;

	default:
		error = EINVAL;
		break;
	}

	if (error == 0)
		td->td_retval[0] = rval;
done2:
	mtx_unlock(&Giant);
	return(error);
}

#ifndef _SYS_SYSPROTO_H_
struct semget_args {
	key_t	key;
	int	nsems;
	int	semflg;
};
#endif

/*
 * MPSAFE
 */
int
semget(td, uap)
	struct thread *td;
	register struct semget_args *uap;
{
	int semid, error = 0;
	int key = uap->key;
	int nsems = uap->nsems;
	int semflg = uap->semflg;
	struct ucred *cred = td->td_ucred;

#ifdef SEM_DEBUG
	printf("semget(0x%x, %d, 0%o)\n", key, nsems, semflg);
#endif
	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&Giant);
	if (key != IPC_PRIVATE) {
		for (semid = 0; semid < seminfo.semmni; semid++) {
			if ((sema[semid].sem_perm.mode & SEM_ALLOC) &&
			    sema[semid].sem_perm.key == key)
				break;
		}
		if (semid < seminfo.semmni) {
#ifdef SEM_DEBUG
			printf("found public key\n");
#endif
			if ((error = ipcperm(td, &sema[semid].sem_perm,
			    semflg & 0700))) {
				goto done2;
			}
			if (nsems > 0 && sema[semid].sem_nsems < nsems) {
#ifdef SEM_DEBUG
				printf("too small\n");
#endif
				error = EINVAL;
				goto done2;
			}
			if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
#ifdef SEM_DEBUG
				printf("not exclusive\n");
#endif
				error = EEXIST;
				goto done2;
			}
			goto found;
		}
	}

#ifdef SEM_DEBUG
	printf("need to allocate the semid_ds\n");
#endif
	if (key == IPC_PRIVATE || (semflg & IPC_CREAT)) {
		if (nsems <= 0 || nsems > seminfo.semmsl) {
#ifdef SEM_DEBUG
			printf("nsems out of range (0<%d<=%d)\n", nsems,
			    seminfo.semmsl);
#endif
			error = EINVAL;
			goto done2;
		}
		if (nsems > seminfo.semmns - semtot) {
#ifdef SEM_DEBUG
			printf("not enough semaphores left (need %d, got %d)\n",
			    nsems, seminfo.semmns - semtot);
#endif
			error = ENOSPC;
			goto done2;
		}
		for (semid = 0; semid < seminfo.semmni; semid++) {
			if ((sema[semid].sem_perm.mode & SEM_ALLOC) == 0)
				break;
		}
		if (semid == seminfo.semmni) {
#ifdef SEM_DEBUG
			printf("no more semid_ds's available\n");
#endif
			error = ENOSPC;
			goto done2;
		}
#ifdef SEM_DEBUG
		printf("semid %d is available\n", semid);
#endif
		sema[semid].sem_perm.key = key;
		sema[semid].sem_perm.cuid = cred->cr_uid;
		sema[semid].sem_perm.uid = cred->cr_uid;
		sema[semid].sem_perm.cgid = cred->cr_gid;
		sema[semid].sem_perm.gid = cred->cr_gid;
		sema[semid].sem_perm.mode = (semflg & 0777) | SEM_ALLOC;
		sema[semid].sem_perm.seq =
		    (sema[semid].sem_perm.seq + 1) & 0x7fff;
		sema[semid].sem_nsems = nsems;
		sema[semid].sem_otime = 0;
		sema[semid].sem_ctime = time_second;
		sema[semid].sem_base = &sem[semtot];
		semtot += nsems;
		bzero(sema[semid].sem_base,
		    sizeof(sema[semid].sem_base[0])*nsems);
#ifdef SEM_DEBUG
		printf("sembase = 0x%x, next = 0x%x\n", sema[semid].sem_base,
		    &sem[semtot]);
#endif
	} else {
#ifdef SEM_DEBUG
		printf("didn't find it and wasn't asked to create it\n");
#endif
		error = ENOENT;
		goto done2;
	}

found:
	td->td_retval[0] = IXSEQ_TO_IPCID(semid, sema[semid].sem_perm);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct semop_args {
	int	semid;
	struct	sembuf *sops;
	u_int	nsops;
};
#endif

/*
 * MPSAFE
 */
int
semop(td, uap)
	struct thread *td;
	register struct semop_args *uap;
{
	int semid = uap->semid;
	u_int nsops = uap->nsops;
	struct sembuf *sops = NULL;
	register struct semid_ds *semaptr;
	register struct sembuf *sopptr = 0;
	register struct sem *semptr = 0;
	struct sem_undo *suptr;
	int i, j, error;
	int do_wakeup, do_undos;

#ifdef SEM_DEBUG
	printf("call to semop(%d, 0x%x, %u)\n", semid, sops, nsops);
#endif

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&Giant);
	semid = IPCID_TO_IX(semid);	/* Convert back to zero origin */

	if (semid < 0 || semid >= seminfo.semmni) {
		error = EINVAL;
		goto done2;
	}

	semaptr = &sema[semid];
	if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0) {
		error = EINVAL;
		goto done2;
	}
	if (semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid)) {
		error = EINVAL;
		goto done2;
	}
	if (nsops > seminfo.semopm) {
#ifdef SEM_DEBUG
		printf("too many sops (max=%d, nsops=%d)\n", seminfo.semopm,
		    nsops);
#endif
		error = E2BIG;
		goto done2;
	}

	/* Allocate memory for sem_ops */
	sops = malloc(nsops * sizeof(sops[0]), M_SEM, M_WAITOK);
	if (!sops)
		panic("Failed to allocate %d sem_ops", nsops);

	if ((error = copyin(uap->sops, sops, nsops * sizeof(sops[0]))) != 0) {
#ifdef SEM_DEBUG
		printf("error = %d from copyin(%08x, %08x, %d)\n", error,
		    uap->sops, sops, nsops * sizeof(sops[0]));
#endif
		goto done2;
	}

	/*
	 * Initial pass thru sops to see what permissions are needed.
	 * Also perform any checks that don't need repeating on each
	 * attempt to satisfy the request vector.
	 */
	j = 0;		/* permission needed */
	do_undos = 0;
	for (i = 0; i < nsops; i++) {
		sopptr = &sops[i];
		if (sopptr->sem_num >= semaptr->sem_nsems) {
			error = EFBIG;
			goto done2;
		}
		if (sopptr->sem_flg & SEM_UNDO && sopptr->sem_op != 0)
			do_undos = 1;
		j |= (sopptr->sem_op == 0) ? SEM_R : SEM_A;
	}

	if ((error = ipcperm(td, &semaptr->sem_perm, j))) {
#ifdef SEM_DEBUG
		printf("error = %d from ipaccess\n", error);
#endif
		goto done2;
	}

	/*
	 * Loop trying to satisfy the vector of requests.
	 * If we reach a point where we must wait, any requests already
	 * performed are rolled back and we go to sleep until some other
	 * process wakes us up.  At this point, we start all over again.
	 *
	 * This ensures that from the perspective of other tasks, a set
	 * of requests is atomic (never partially satisfied).
	 */
	for (;;) {
		do_wakeup = 0;
		error = 0;	/* error return if necessary */

		for (i = 0; i < nsops; i++) {
			sopptr = &sops[i];
			semptr = &semaptr->sem_base[sopptr->sem_num];

#ifdef SEM_DEBUG
			printf("semop:  semaptr=%x, sem_base=%x, semptr=%x, sem[%d]=%d : op=%d, flag=%s\n",
			    semaptr, semaptr->sem_base, semptr,
			    sopptr->sem_num, semptr->semval, sopptr->sem_op,
			    (sopptr->sem_flg & IPC_NOWAIT) ? "nowait" : "wait");
#endif

			if (sopptr->sem_op < 0) {
				if (semptr->semval + sopptr->sem_op < 0) {
#ifdef SEM_DEBUG
					printf("semop:  can't do it now\n");
#endif
					break;
				} else {
					semptr->semval += sopptr->sem_op;
					if (semptr->semval == 0 &&
					    semptr->semzcnt > 0)
						do_wakeup = 1;
				}
			} else if (sopptr->sem_op == 0) {
				if (semptr->semval != 0) {
#ifdef SEM_DEBUG
					printf("semop:  not zero now\n");
#endif
					break;
				}
			} else if (semptr->semval + sopptr->sem_op >
			    seminfo.semvmx) {
				error = ERANGE;
				break;
			} else {
				if (semptr->semncnt > 0)
					do_wakeup = 1;
				semptr->semval += sopptr->sem_op;
			}
		}

		/*
		 * Did we get through the entire vector?
		 */
		if (i >= nsops)
			goto done;

		/*
		 * No ... rollback anything that we've already done
		 */
#ifdef SEM_DEBUG
		printf("semop:  rollback 0 through %d\n", i-1);
#endif
		for (j = 0; j < i; j++)
			semaptr->sem_base[sops[j].sem_num].semval -=
			    sops[j].sem_op;

		/* If we detected an error, return it */
		if (error != 0)
			goto done2;

		/*
		 * If the request that we couldn't satisfy has the
		 * NOWAIT flag set then return with EAGAIN.
		 */
		if (sopptr->sem_flg & IPC_NOWAIT) {
			error = EAGAIN;
			goto done2;
		}

		if (sopptr->sem_op == 0)
			semptr->semzcnt++;
		else
			semptr->semncnt++;

#ifdef SEM_DEBUG
		printf("semop:  good night!\n");
#endif
		error = tsleep(semaptr, (PZERO - 4) | PCATCH, "semwait", 0);
#ifdef SEM_DEBUG
		printf("semop:  good morning (error=%d)!\n", error);
#endif

		if (error != 0) {
			error = EINTR;
			goto done2;
		}
#ifdef SEM_DEBUG
		printf("semop:  good morning!\n");
#endif

		/*
		 * Make sure that the semaphore still exists
		 */
		if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0 ||
		    semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid)) {
			error = EIDRM;
			goto done2;
		}

		/*
		 * The semaphore is still alive.  Readjust the count of
		 * waiting processes.
		 */
		if (sopptr->sem_op == 0)
			semptr->semzcnt--;
		else
			semptr->semncnt--;
	}

done:
	/*
	 * Process any SEM_UNDO requests.
	 */
	if (do_undos) {
		suptr = NULL;
		for (i = 0; i < nsops; i++) {
			/*
			 * We only need to deal with SEM_UNDO's for non-zero
			 * op's.
			 */
			int adjval;

			if ((sops[i].sem_flg & SEM_UNDO) == 0)
				continue;
			adjval = sops[i].sem_op;
			if (adjval == 0)
				continue;
			error = semundo_adjust(td, &suptr, semid,
			    sops[i].sem_num, -adjval);
			if (error == 0)
				continue;

			/*
			 * Oh-Oh!  We ran out of either sem_undo's or undo's.
			 * Rollback the adjustments to this point and then
			 * rollback the semaphore ups and down so we can return
			 * with an error with all structures restored.  We
			 * rollback the undo's in the exact reverse order that
			 * we applied them.  This guarantees that we won't run
			 * out of space as we roll things back out.
			 */
			for (j = i - 1; j >= 0; j--) {
				if ((sops[j].sem_flg & SEM_UNDO) == 0)
					continue;
				adjval = sops[j].sem_op;
				if (adjval == 0)
					continue;
				if (semundo_adjust(td, &suptr, semid,
				    sops[j].sem_num, adjval) != 0)
					panic("semop - can't undo undos");
			}

			for (j = 0; j < nsops; j++)
				semaptr->sem_base[sops[j].sem_num].semval -=
				    sops[j].sem_op;

#ifdef SEM_DEBUG
			printf("error = %d from semundo_adjust\n", error);
#endif
			goto done2;
		} /* loop through the sops */
	} /* if (do_undos) */

	/* We're definitely done - set the sempid's and time */
	for (i = 0; i < nsops; i++) {
		sopptr = &sops[i];
		semptr = &semaptr->sem_base[sopptr->sem_num];
		semptr->sempid = td->td_proc->p_pid;
	}
	semaptr->sem_otime = time_second;

	/*
	 * Do a wakeup if any semaphore was up'd whilst something was
	 * sleeping on it.
	 */
	if (do_wakeup) {
#ifdef SEM_DEBUG
		printf("semop:  doing wakeup\n");
#endif
		wakeup(semaptr);
#ifdef SEM_DEBUG
		printf("semop:  back from wakeup\n");
#endif
	}
#ifdef SEM_DEBUG
	printf("semop:  done\n");
#endif
	td->td_retval[0] = 0;
done2:
	if (sops)
	    free(sops, M_SEM);
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Go through the undo structures for this process and apply the adjustments to
 * semaphores.
 */
static void
semexit_myhook(p)
	struct proc *p;
{
	register struct sem_undo *suptr;
	register struct sem_undo **supptr;

	/*
	 * Go through the chain of undo vectors looking for one
	 * associated with this process.
	 */

	for (supptr = &semu_list; (suptr = *supptr) != NULL;
	    supptr = &suptr->un_next) {
		if (suptr->un_proc == p)
			break;
	}

	if (suptr == NULL)
		return;

#ifdef SEM_DEBUG
	printf("proc @%08x has undo structure with %d entries\n", p,
	    suptr->un_cnt);
#endif

	/*
	 * If there are any active undo elements then process them.
	 */
	if (suptr->un_cnt > 0) {
		int ix;

		for (ix = 0; ix < suptr->un_cnt; ix++) {
			int semid = suptr->un_ent[ix].un_id;
			int semnum = suptr->un_ent[ix].un_num;
			int adjval = suptr->un_ent[ix].un_adjval;
			struct semid_ds *semaptr;

			semaptr = &sema[semid];
			if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0)
				panic("semexit - semid not allocated");
			if (semnum >= semaptr->sem_nsems)
				panic("semexit - semnum out of range");

#ifdef SEM_DEBUG
			printf("semexit:  %08x id=%d num=%d(adj=%d) ; sem=%d\n",
			    suptr->un_proc, suptr->un_ent[ix].un_id,
			    suptr->un_ent[ix].un_num,
			    suptr->un_ent[ix].un_adjval,
			    semaptr->sem_base[semnum].semval);
#endif

			if (adjval < 0) {
				if (semaptr->sem_base[semnum].semval < -adjval)
					semaptr->sem_base[semnum].semval = 0;
				else
					semaptr->sem_base[semnum].semval +=
					    adjval;
			} else
				semaptr->sem_base[semnum].semval += adjval;

			wakeup(semaptr);
#ifdef SEM_DEBUG
			printf("semexit:  back from wakeup\n");
#endif
		}
	}

	/*
	 * Deallocate the undo vector.
	 */
#ifdef SEM_DEBUG
	printf("removing vector\n");
#endif
	suptr->un_proc = NULL;
	*supptr = suptr->un_next;
}

static int
sysctl_sema(SYSCTL_HANDLER_ARGS)
{

	return (SYSCTL_OUT(req, sema,
	    sizeof(struct semid_ds) * seminfo.semmni));
}
