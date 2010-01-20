/*-
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_IPC_H_
#define _LINUX_IPC_H_

/*
 * Version flags for semctl, msgctl, and shmctl commands
 * These are passed as bitflags or-ed with the actual command
 */
#define	LINUX_IPC_OLD	0	/* Old version (no 32-bit UID support on many
				   architectures) */
#define	LINUX_IPC_64	0x0100	/* New version (support 32-bit UIDs, bigger
				   message sizes, etc. */

#if defined(__i386__) || defined(__amd64__)

struct linux_msgctl_args 
{
	l_int		msqid;
	l_int		cmd;
	struct l_msqid_ds *buf;
};

struct linux_msgget_args
{
	l_key_t		key;
	l_int		msgflg;
};

struct linux_msgrcv_args
{
	l_int		msqid;
	struct l_msgbuf *msgp;
	l_size_t	msgsz;
	l_long		msgtyp;
	l_int		msgflg;
};

struct linux_msgsnd_args
{
	l_int		msqid;
	struct l_msgbuf *msgp;
	l_size_t	msgsz;
	l_int		msgflg;
};

struct linux_semctl_args
{
	l_int		semid;
	l_int		semnum;
	l_int		cmd;
	union l_semun	arg;
};

struct linux_semget_args
{
	l_key_t		key;
	l_int		nsems;
	l_int		semflg;
};

struct linux_semop_args
{
	l_int		semid;
	struct l_sembuf *tsops;
	l_uint		nsops;
};

struct linux_shmat_args
{
	l_int		shmid;
	char		*shmaddr;
	l_int		shmflg;
	l_ulong		*raddr;
};

struct linux_shmctl_args
{
	l_int		shmid;
	l_int		cmd;
	struct l_shmid_ds *buf;
};

struct linux_shmdt_args
{
	char *shmaddr;
};

struct linux_shmget_args
{
	l_key_t		key;
	l_size_t	size;
	l_int		shmflg;
};

int linux_msgctl(struct thread *, struct linux_msgctl_args *);
int linux_msgget(struct thread *, struct linux_msgget_args *);
int linux_msgrcv(struct thread *, struct linux_msgrcv_args *);
int linux_msgsnd(struct thread *, struct linux_msgsnd_args *);

int linux_semctl(struct thread *, struct linux_semctl_args *);
int linux_semget(struct thread *, struct linux_semget_args *);
int linux_semop(struct thread *, struct linux_semop_args *);

int linux_shmat(struct thread *, struct linux_shmat_args *);
int linux_shmctl(struct thread *, struct linux_shmctl_args *);
int linux_shmdt(struct thread *, struct linux_shmdt_args *);
int linux_shmget(struct thread *, struct linux_shmget_args *);

#define	LINUX_MSG_INFO	12

#endif	/* __i386__ || __amd64__ */

#endif /* _LINUX_IPC_H_ */
