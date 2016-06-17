#ifndef __SPARC64_IPC_H__
#define __SPARC64_IPC_H__

/* 
 * These are used to wrap system calls on the sparc.
 *
 * See arch/sparc64/kernel/sys_sparc32.c for ugly details..
 */
struct ipc_kludge {
	u32 msgp;
	s32 msgtyp;
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

/* We don't need to maintain backward compatibility on 64bit, we've started fresh */
#define IPCCALL(version,op)	(op)

#endif
