#ifndef _ASM_IPC_H
#define _ASM_IPC_H

/*
 * These are used to wrap system calls on MIPS32.
 *
 * See arch/mips/kernel/sysmips.c for ugly details..
 * FIXME: split up into ordinary syscalls ...
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
#define DIPC		25

#define IPCCALL(version,op)	((version)<<16 | (op))

#endif /* _ASM_IPC_H */
