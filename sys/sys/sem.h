/* $Id: sem.h,v 1.12 1997/02/22 09:45:51 peter Exp $ */
/*	$NetBSD: sem.h,v 1.5 1994/06/29 06:45:15 cgd Exp $	*/

/*
 * SVID compatible sem.h file
 *
 * Author:  Daniel Boulet
 */

#ifndef _SYS_SEM_H_
#define _SYS_SEM_H_

#include <sys/ipc.h>

struct sem {
	u_short	semval;		/* semaphore value */
	pid_t	sempid;		/* pid of last operation */
	u_short	semncnt;	/* # awaiting semval > cval */
	u_short	semzcnt;	/* # awaiting semval = 0 */
};

struct semid_ds {
	struct	ipc_perm sem_perm;	/* operation permission struct */
	struct	sem *sem_base;	/* pointer to first semaphore in set */
	u_short	sem_nsems;	/* number of sems in set */
	time_t	sem_otime;	/* last operation time */
	long	sem_pad1;	/* SVABI/386 says I need this here */
	time_t	sem_ctime;	/* last change time */
    				/* Times measured in secs since */
    				/* 00:00:00 GMT, Jan. 1, 1970 */
	long	sem_pad2;	/* SVABI/386 says I need this here */
	long	sem_pad3[4];	/* SVABI/386 says I need this here */
};

/*
 * semop's sops parameter structure
 */
struct sembuf {
	u_short	sem_num;	/* semaphore # */
	short	sem_op;		/* semaphore operation */
	short	sem_flg;	/* operation flags */
};
#define SEM_UNDO	010000

#define MAX_SOPS	5	/* maximum # of sembuf's per semop call */

/*
 * semctl's arg parameter structure
 */
union semun {
	int	val;		/* value for SETVAL */
	struct	semid_ds *buf;	/* buffer for IPC_STAT & IPC_SET */
	u_short	*array;		/* array for GETALL & SETALL */
};

/*
 * commands for semctl
 */
#define GETNCNT	3	/* Return the value of semncnt {READ} */
#define GETPID	4	/* Return the value of sempid {READ} */
#define GETVAL	5	/* Return the value of semval {READ} */
#define GETALL	6	/* Return semvals into arg.array {READ} */
#define GETZCNT	7	/* Return the value of semzcnt {READ} */
#define SETVAL	8	/* Set the value of semval to arg.val {ALTER} */
#define SETALL	9	/* Set semvals from arg.array {ALTER} */

/*
 * Permissions
 */
#define SEM_A		0200	/* alter permission */
#define SEM_R		0400	/* read permission */

#ifdef KERNEL
/*
 * Kernel implementation stuff
 */
#define SEMVMX	32767		/* semaphore maximum value */
#define SEMAEM	16384		/* adjust on exit max value */


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
 * semaphore info struct
 */
struct seminfo {
	int	semmap,		/* # of entries in semaphore map */
		semmni,		/* # of semaphore identifiers */
		semmns,		/* # of semaphores in system */
		semmnu,		/* # of undo structures in system */
		semmsl,		/* max # of semaphores per id */
		semopm,		/* max # of operations per semop call */
		semume,		/* max # of undo entries per process */
		semusz,		/* size in bytes of undo structure */
		semvmx,		/* semaphore maximum value */
		semaem;		/* adjust on exit max value */
};
extern struct seminfo	seminfo;

/* internal "mode" bits */
#define	SEM_ALLOC	01000	/* semaphore is allocated */
#define	SEM_DEST	02000	/* semaphore will be destroyed on last detach */

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

/* actual size of an undo structure */
#define SEMUSZ	(sizeof(struct sem_undo)+sizeof(struct undo)*SEMUME)

extern struct semid_ds *sema;	/* semaphore id pool */
extern struct sem *sem;		/* semaphore pool */
extern int	*semu;		/* undo structure pool */

/*
 * Macro to find a particular sem_undo vector
 */
#define SEMU(ix)	((struct sem_undo *)(((long)semu)+ix * SEMUSZ))

/*
 * Process sem_undo vectors at proc exit.
 */
void	semexit __P((struct proc *p));

/*
 * Parameters to the semconfig system call
 */
typedef enum {
	SEM_CONFIG_FREEZE,	/* Freeze the semaphore facility. */
	SEM_CONFIG_THAW		/* Thaw the semaphore facility. */
} semconfig_ctl_t;
#endif /* KERNEL */

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int semsys __P((int, ...));
int semctl __P((int, int, int, union semun));
int semget __P((key_t, int, int));
int semop __P((int, struct sembuf *,unsigned));
__END_DECLS
#endif /* !KERNEL */

#endif /* !_SEM_H_ */
