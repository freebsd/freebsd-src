/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
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
 *      This product includes software developed by Steven Wallace.
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
 * $FreeBSD$
 */


struct ibcs2_ipc_perm {
	u_short	uid;	/* user id */
	u_short	gid;	/* group id */
	u_short	cuid;	/* creator user id */
	u_short	cgid;	/* creator group id */
	u_short	mode;	/* r/w permission */
	u_short	seq;	/* sequence # (to generate unique msg/sem/shm id) */
	ibcs2_key_t key; /* user specified msg/sem/shm key */
};

struct ibcs2_msqid_ds {
	struct ibcs2_ipc_perm msg_perm;
	struct msg *msg_first;
	struct msg *msg_last;
	u_short msg_cbytes;
	u_short msg_qnum;
	u_short msg_qbytes;
	u_short msg_lspid;
	u_short msg_lrpid;
	ibcs2_time_t msg_stime;
	ibcs2_time_t msg_rtime;
	ibcs2_time_t msg_ctime;
};

struct ibcs2_semid_ds {
        struct ibcs2_ipc_perm sem_perm;
	struct ibcs2_sem *sem_base;
	u_short sem_nsems;
	ibcs2_time_t sem_otime;
	ibcs2_time_t sem_ctime;
};

struct ibcs2_sem {
        u_short semval;
	ibcs2_pid_t sempid;
	u_short semncnt;
	u_short semzcnt;
};

struct ibcs2_shmid_ds {
        struct ibcs2_ipc_perm shm_perm;
	int shm_segsz;
	int pad1;
	char pad2[4];
	u_short shm_lpid;
	u_short shm_cpid;
	u_short shm_nattch;
	u_short shm_cnattch;
	ibcs2_time_t shm_atime;
	ibcs2_time_t shm_dtime;
	ibcs2_time_t shm_ctime;
};
