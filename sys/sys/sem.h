/* $FreeBSD$ */
/*	$NetBSD: sem.h,v 1.5 1994/06/29 06:45:15 cgd Exp $	*/

/*
 * SVID compatible sem.h file
 *
 * Author:  Daniel Boulet
 */

#ifndef _SYS_SEM_H_
#define _SYS_SEM_H_

#include <sys/ipc.h>

struct sem;

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
#define SEM_STAT 10	/* Like IPC_STAT but treats semid as sema-index */
#define SEM_INFO 11 /* Like IPC_INFO but treats semid as sema-index */

/*
 * Permissions
 */
#define SEM_A		IPC_W	/* alter permission */
#define SEM_R		IPC_R	/* read permission */

#ifdef _KERNEL

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
 * Process sem_undo vectors at proc exit.
 */
void	semexit(struct proc *p);
#endif /* _KERNEL */

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int semsys(int, ...);
int semctl(int, int, int, ...);
int semget(key_t, int, int);
int semop(int, struct sembuf *,unsigned);
__END_DECLS
#endif /* !_KERNEL */

#endif /* !_SEM_H_ */
