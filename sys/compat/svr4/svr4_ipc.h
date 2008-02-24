/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1995 Christos Zoulas.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
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
 * $FreeBSD: src/sys/compat/svr4/svr4_ipc.h,v 1.4 2005/01/05 22:34:36 imp Exp $
 */

#ifndef _SVR4_IPC_H_
#define _SVR4_IPC_H_

/*
 * General IPC
 */
#define	SVR4_IPC_RMID		10
#define	SVR4_IPC_SET		11
#define	SVR4_IPC_STAT		12

struct svr4_ipc_perm {
	svr4_uid_t	uid;
	svr4_gid_t	gid;
	svr4_uid_t	cuid;
	svr4_gid_t	cgid;
	svr4_mode_t	mode;
	u_long		seq;
	svr4_key_t	key;
	long		pad[4];
};

/*
 * Message queues
 */
#define SVR4_msgget	0
#define SVR4_msgctl	1
#define SVR4_msgrcv	2
#define SVR4_msgsnd	3

struct svr4_msg {
	struct svr4_msg	*msg_next;
	long		msg_type;
	u_short		msg_ts;	
	short		msg_spot;
};

struct svr4_msqid_ds {
	struct svr4_ipc_perm msg_perm;
	struct svr4_msg	*msg_first;
	struct svr4_msg	*msg_last;
	u_long		msg_cbytes;
	u_long		msg_qnum;
	u_long		msg_qbytes;
	svr4_pid_t	msg_lspid;
	svr4_pid_t	msg_lrpid;
	svr4_time_t	msg_stime;
	long		msg_pad1;	
	svr4_time_t	msg_rtime;
	long		msg_pad2;
	svr4_time_t	msg_ctime;
	long		msg_pad3;
	short		msg_cv;
	short		msg_qnum_cv;
	long		msg_pad4[3];
};

struct svr4_msgbuf {
	long	mtype;		/* message type */
	char	mtext[1];	/* message text */
};

struct svr4_msginfo {
	int	msgmap;
	int	msgmax;
	int	msgmnb;
	int	msgmni;
	int	msgssz;
	int	msgtql;
	u_short	msgseg;
};

/*
 * Shared memory
 */
#define SVR4_shmat	0
#define SVR4_shmctl	1
#define SVR4_shmdt	2
#define SVR4_shmget	3

/* shmctl() operations */
#define	SVR4_SHM_LOCK		 3
#define	SVR4_SHM_UNLOCK		 4

struct svr4_shmid_ds {
	struct svr4_ipc_perm	shm_perm;
	int		shm_segsz;
	void		*shm_amp;
	u_short		shm_lkcnt;
	svr4_pid_t	shm_lpid;
	svr4_pid_t	shm_cpid;
	u_long		shm_nattch;
	u_long		shm_cnattch;
	svr4_time_t	shm_atime;
	long		shm_pad1;
	svr4_time_t	shm_dtime;
	long		shm_pad2;
	svr4_time_t	shm_ctime;
	long		shm_pad3;
	long		shm_pad4[4];
};

/*
 * Semaphores
 */
#define SVR4_semctl	0
#define SVR4_semget	1
#define SVR4_semop	2

/* semctl() operations */
#define	SVR4_SEM_GETNCNT	 3
#define	SVR4_SEM_GETPID		 4
#define	SVR4_SEM_GETVAL		 5
#define	SVR4_SEM_GETALL		 6
#define	SVR4_SEM_GETZCNT	 7
#define	SVR4_SEM_SETVAL		 8
#define	SVR4_SEM_SETALL		 9

struct svr4_sem {
	u_short		semval;
	svr4_pid_t	sempid;
	u_short		semncnt;
	u_short		semzcnt;
	u_short		semncnt_cv;
	u_short		semzcnt_cv;
};

struct svr4_semid_ds {
	struct svr4_ipc_perm sem_perm;
	struct svr4_sem	*sem_base;
	u_short		sem_nsems;
	svr4_time_t	sem_otime;
	long		sem_pad1;
	svr4_time_t	sem_ctime;
	long		sem_pad2;
	long		sem_pad3[4];
};

struct svr4_sembuf {
	u_short		sem_num;
	short		sem_op;
	short		sem_flg;
};

#endif	/* _SVR4_IPC_H */
