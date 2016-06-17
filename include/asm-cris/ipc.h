#ifndef __CRIS_IPC_H__
#define __CRIS_IPC_H__

/* 
 * These are used to wrap system calls on CRIS.
 *
 * See arch/cris/kernel/sys_cris.c for ugly details..
 *
 * Same as x86 version.
 *
 */
struct ipc_kludge {
	struct msgbuf *msgp;
	long msgtyp;
};

#define SEMOP		 1
#define SEMGET		 2
#define SEMCTL		 3
#define SEMTIMEDOP	 4
#define MSGSND		11
#define MSGRCV		12
#define MSGGET		13
#define MSGCTL		14
#define SHMAT		21
#define SHMDT		22
#define SHMGET		23
#define SHMCTL		24

/* Used by the DIPC package, try and avoid reusing it */
#define DIPC            25

#define IPCCALL(version,op)	((version)<<16 | (op))

#endif
