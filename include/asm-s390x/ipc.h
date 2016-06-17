/*
 *  include/asm-s390/ipc.h
 *
 *  S390 version
 *
 *  Derived from "include/asm-i386/ipc.h"
 */

#ifndef __s390_IPC_H__
#define __s390_IPC_H__

/* 
 * These are used to wrap system calls on S390.
 *
 * See arch/s390/kernel/sys_s390.c for ugly details..
 */
struct ipc_kludge {
	struct msgbuf *msgp;
	long msgtyp;
};

#define SEMOP		 1
#define SEMGET		 2
#define SEMCTL		 3
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
