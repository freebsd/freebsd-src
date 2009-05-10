/*-
 * Implementation of SVID messages
 *
 * Author:  Daniel Boulet
 *
 * Copyright 1993 Daniel Boulet and RTMX Inc.
 *
 * This system call was implemented by Daniel Boulet under contract from RTMX.
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */
/*-
 * Copyright (c) 2003-2005 McAfee, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Security Research Division of McAfee, Inc under DARPA/SPAWAR
 * contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA CHATS research
 * program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_sysvipc.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/msg.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/jail.h>

#include <security/mac/mac_framework.h>

static MALLOC_DEFINE(M_MSG, "msg", "SVID compatible message queues");

static void msginit(void);
static int msgunload(void);
static int sysvmsg_modload(struct module *, int, void *);

#ifdef MSG_DEBUG
#define DPRINTF(a)	printf a
#else
#define DPRINTF(a)
#endif

static void msg_freehdr(struct msg *msghdr);

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *msgcalls[] = {
	(sy_call_t *)msgctl, (sy_call_t *)msgget,
	(sy_call_t *)msgsnd, (sy_call_t *)msgrcv
};

#ifndef MSGSSZ
#define MSGSSZ	8		/* Each segment must be 2^N long */
#endif
#ifndef MSGSEG
#define MSGSEG	2048		/* must be less than 32767 */
#endif
#define MSGMAX	(MSGSSZ*MSGSEG)
#ifndef MSGMNB
#define MSGMNB	2048		/* max # of bytes in a queue */
#endif
#ifndef MSGMNI
#define MSGMNI	40
#endif
#ifndef MSGTQL
#define MSGTQL	40
#endif

/*
 * Based on the configuration parameters described in an SVR2 (yes, two)
 * config(1m) man page.
 *
 * Each message is broken up and stored in segments that are msgssz bytes
 * long.  For efficiency reasons, this should be a power of two.  Also,
 * it doesn't make sense if it is less than 8 or greater than about 256.
 * Consequently, msginit in kern/sysv_msg.c checks that msgssz is a power of
 * two between 8 and 1024 inclusive (and panic's if it isn't).
 */
struct msginfo msginfo = {
                MSGMAX,         /* max chars in a message */
                MSGMNI,         /* # of message queue identifiers */
                MSGMNB,         /* max chars in a queue */
                MSGTQL,         /* max messages in system */
                MSGSSZ,         /* size of a message segment */
                		/* (must be small power of 2 greater than 4) */
                MSGSEG          /* number of message segments */
};

/*
 * macros to convert between msqid_ds's and msqid's.
 * (specific to this implementation)
 */
#define MSQID(ix,ds)	((ix) & 0xffff | (((ds).msg_perm.seq << 16) & 0xffff0000))
#define MSQID_IX(id)	((id) & 0xffff)
#define MSQID_SEQ(id)	(((id) >> 16) & 0xffff)

/*
 * The rest of this file is specific to this particular implementation.
 */

struct msgmap {
	short	next;		/* next segment in buffer */
    				/* -1 -> available */
    				/* 0..(MSGSEG-1) -> index of next segment */
};

#define MSG_LOCKED	01000	/* Is this msqid_ds locked? */

static int nfree_msgmaps;	/* # of free map entries */
static short free_msgmaps;	/* head of linked list of free map entries */
static struct msg *free_msghdrs;/* list of free msg headers */
static char *msgpool;		/* MSGMAX byte long msg buffer pool */
static struct msgmap *msgmaps;	/* MSGSEG msgmap structures */
static struct msg *msghdrs;	/* MSGTQL msg headers */
static struct msqid_kernel *msqids;	/* MSGMNI msqid_kernel struct's */
static struct mtx msq_mtx;	/* global mutex for message queues. */

static void
msginit()
{
	register int i;

	TUNABLE_INT_FETCH("kern.ipc.msgseg", &msginfo.msgseg);
	TUNABLE_INT_FETCH("kern.ipc.msgssz", &msginfo.msgssz);
	msginfo.msgmax = msginfo.msgseg * msginfo.msgssz;
	TUNABLE_INT_FETCH("kern.ipc.msgmni", &msginfo.msgmni);
	TUNABLE_INT_FETCH("kern.ipc.msgmnb", &msginfo.msgmnb);
	TUNABLE_INT_FETCH("kern.ipc.msgtql", &msginfo.msgtql);

	msgpool = malloc(msginfo.msgmax, M_MSG, M_WAITOK);
	if (msgpool == NULL)
		panic("msgpool is NULL");
	msgmaps = malloc(sizeof(struct msgmap) * msginfo.msgseg, M_MSG, M_WAITOK);
	if (msgmaps == NULL)
		panic("msgmaps is NULL");
	msghdrs = malloc(sizeof(struct msg) * msginfo.msgtql, M_MSG, M_WAITOK);
	if (msghdrs == NULL)
		panic("msghdrs is NULL");
	msqids = malloc(sizeof(struct msqid_kernel) * msginfo.msgmni, M_MSG,
	    M_WAITOK);
	if (msqids == NULL)
		panic("msqids is NULL");

	/*
	 * msginfo.msgssz should be a power of two for efficiency reasons.
	 * It is also pretty silly if msginfo.msgssz is less than 8
	 * or greater than about 256 so ...
	 */

	i = 8;
	while (i < 1024 && i != msginfo.msgssz)
		i <<= 1;
    	if (i != msginfo.msgssz) {
		DPRINTF(("msginfo.msgssz=%d (0x%x)\n", msginfo.msgssz,
		    msginfo.msgssz));
		panic("msginfo.msgssz not a small power of 2");
	}

	if (msginfo.msgseg > 32767) {
		DPRINTF(("msginfo.msgseg=%d\n", msginfo.msgseg));
		panic("msginfo.msgseg > 32767");
	}

	if (msgmaps == NULL)
		panic("msgmaps is NULL");

	for (i = 0; i < msginfo.msgseg; i++) {
		if (i > 0)
			msgmaps[i-1].next = i;
		msgmaps[i].next = -1;	/* implies entry is available */
	}
	free_msgmaps = 0;
	nfree_msgmaps = msginfo.msgseg;

	if (msghdrs == NULL)
		panic("msghdrs is NULL");

	for (i = 0; i < msginfo.msgtql; i++) {
		msghdrs[i].msg_type = 0;
		if (i > 0)
			msghdrs[i-1].msg_next = &msghdrs[i];
		msghdrs[i].msg_next = NULL;
#ifdef MAC
		mac_sysvmsg_init(&msghdrs[i]);
#endif
    	}
	free_msghdrs = &msghdrs[0];

	if (msqids == NULL)
		panic("msqids is NULL");

	for (i = 0; i < msginfo.msgmni; i++) {
		msqids[i].u.msg_qbytes = 0;	/* implies entry is available */
		msqids[i].u.msg_perm.seq = 0;	/* reset to a known value */
		msqids[i].u.msg_perm.mode = 0;
#ifdef MAC
		mac_sysvmsq_init(&msqids[i]);
#endif
	}
	mtx_init(&msq_mtx, "msq", NULL, MTX_DEF);
}

static int
msgunload()
{
	struct msqid_kernel *msqkptr;
	int msqid;
#ifdef MAC
	int i;
#endif

	for (msqid = 0; msqid < msginfo.msgmni; msqid++) {
		/*
		 * Look for an unallocated and unlocked msqid_ds.
		 * msqid_ds's can be locked by msgsnd or msgrcv while
		 * they are copying the message in/out.  We can't
		 * re-use the entry until they release it.
		 */
		msqkptr = &msqids[msqid];
		if (msqkptr->u.msg_qbytes != 0 ||
		    (msqkptr->u.msg_perm.mode & MSG_LOCKED) != 0)
			break;
	}
	if (msqid != msginfo.msgmni)
		return (EBUSY);

#ifdef MAC
	for (i = 0; i < msginfo.msgtql; i++)
		mac_sysvmsg_destroy(&msghdrs[i]);
	for (msqid = 0; msqid < msginfo.msgmni; msqid++)
		mac_sysvmsq_destroy(&msqids[msqid]);
#endif
	free(msgpool, M_MSG);
	free(msgmaps, M_MSG);
	free(msghdrs, M_MSG);
	free(msqids, M_MSG);
	mtx_destroy(&msq_mtx);
	return (0);
}


static int
sysvmsg_modload(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD:
		msginit();
		break;
	case MOD_UNLOAD:
		error = msgunload();
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static moduledata_t sysvmsg_mod = {
	"sysvmsg",
	&sysvmsg_modload,
	NULL
};

SYSCALL_MODULE_HELPER(msgsys);
SYSCALL_MODULE_HELPER(msgctl);
SYSCALL_MODULE_HELPER(msgget);
SYSCALL_MODULE_HELPER(msgsnd);
SYSCALL_MODULE_HELPER(msgrcv);

DECLARE_MODULE(sysvmsg, sysvmsg_mod,
	SI_SUB_SYSV_MSG, SI_ORDER_FIRST);
MODULE_VERSION(sysvmsg, 1);

/*
 * Entry point for all MSG calls.
 */
int
msgsys(td, uap)
	struct thread *td;
	/* XXX actually varargs. */
	struct msgsys_args /* {
		int	which;
		int	a2;
		int	a3;
		int	a4;
		int	a5;
		int	a6;
	} */ *uap;
{
	int error;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);
	if (uap->which < 0 ||
	    uap->which >= sizeof(msgcalls)/sizeof(msgcalls[0]))
		return (EINVAL);
	error = (*msgcalls[uap->which])(td, &uap->a2);
	return (error);
}

static void
msg_freehdr(msghdr)
	struct msg *msghdr;
{
	while (msghdr->msg_ts > 0) {
		short next;
		if (msghdr->msg_spot < 0 || msghdr->msg_spot >= msginfo.msgseg)
			panic("msghdr->msg_spot out of range");
		next = msgmaps[msghdr->msg_spot].next;
		msgmaps[msghdr->msg_spot].next = free_msgmaps;
		free_msgmaps = msghdr->msg_spot;
		nfree_msgmaps++;
		msghdr->msg_spot = next;
		if (msghdr->msg_ts >= msginfo.msgssz)
			msghdr->msg_ts -= msginfo.msgssz;
		else
			msghdr->msg_ts = 0;
	}
	if (msghdr->msg_spot != -1)
		panic("msghdr->msg_spot != -1");
	msghdr->msg_next = free_msghdrs;
	free_msghdrs = msghdr;
#ifdef MAC
	mac_sysvmsg_cleanup(msghdr);
#endif
}

#ifndef _SYS_SYSPROTO_H_
struct msgctl_args {
	int	msqid;
	int	cmd;
	struct	msqid_ds *buf;
};
#endif
int
msgctl(td, uap)
	struct thread *td;
	register struct msgctl_args *uap;
{
	int msqid = uap->msqid;
	int cmd = uap->cmd;
	struct msqid_ds msqbuf;
	int error;

	DPRINTF(("call to msgctl(%d, %d, %p)\n", msqid, cmd, uap->buf));
	if (cmd == IPC_SET &&
	    (error = copyin(uap->buf, &msqbuf, sizeof(msqbuf))) != 0)
		return (error);
	error = kern_msgctl(td, msqid, cmd, &msqbuf);
	if (cmd == IPC_STAT && error == 0)
		error = copyout(&msqbuf, uap->buf, sizeof(struct msqid_ds));
	return (error);
}

int
kern_msgctl(td, msqid, cmd, msqbuf)
	struct thread *td;
	int msqid;
	int cmd;
	struct msqid_ds *msqbuf;
{
	int rval, error, msqix;
	register struct msqid_kernel *msqkptr;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	msqix = IPCID_TO_IX(msqid);

	if (msqix < 0 || msqix >= msginfo.msgmni) {
		DPRINTF(("msqid (%d) out of range (0<=msqid<%d)\n", msqix,
		    msginfo.msgmni));
		return (EINVAL);
	}

	msqkptr = &msqids[msqix];

	mtx_lock(&msq_mtx);
	if (msqkptr->u.msg_qbytes == 0) {
		DPRINTF(("no such msqid\n"));
		error = EINVAL;
		goto done2;
	}
	if (msqkptr->u.msg_perm.seq != IPCID_TO_SEQ(msqid)) {
		DPRINTF(("wrong sequence number\n"));
		error = EINVAL;
		goto done2;
	}
#ifdef MAC
	error = mac_sysvmsq_check_msqctl(td->td_ucred, msqkptr, cmd);
	if (error != 0)
		goto done2;
#endif

	error = 0;
	rval = 0;

	switch (cmd) {

	case IPC_RMID:
	{
		struct msg *msghdr;
		if ((error = ipcperm(td, &msqkptr->u.msg_perm, IPC_M)))
			goto done2;

#ifdef MAC
		/*
		 * Check that the thread has MAC access permissions to
		 * individual msghdrs.  Note: We need to do this in a
		 * separate loop because the actual loop alters the
		 * msq/msghdr info as it progresses, and there is no going
		 * back if half the way through we discover that the
		 * thread cannot free a certain msghdr.  The msq will get
		 * into an inconsistent state.
		 */
		for (msghdr = msqkptr->u.msg_first; msghdr != NULL;
		    msghdr = msghdr->msg_next) {
			error = mac_sysvmsq_check_msgrmid(td->td_ucred, msghdr);
			if (error != 0)
				goto done2;
		}
#endif

		/* Free the message headers */
		msghdr = msqkptr->u.msg_first;
		while (msghdr != NULL) {
			struct msg *msghdr_tmp;

			/* Free the segments of each message */
			msqkptr->u.msg_cbytes -= msghdr->msg_ts;
			msqkptr->u.msg_qnum--;
			msghdr_tmp = msghdr;
			msghdr = msghdr->msg_next;
			msg_freehdr(msghdr_tmp);
		}

		if (msqkptr->u.msg_cbytes != 0)
			panic("msg_cbytes is screwed up");
		if (msqkptr->u.msg_qnum != 0)
			panic("msg_qnum is screwed up");

		msqkptr->u.msg_qbytes = 0;	/* Mark it as free */

#ifdef MAC
		mac_sysvmsq_cleanup(msqkptr);
#endif

		wakeup(msqkptr);
	}

		break;

	case IPC_SET:
		if ((error = ipcperm(td, &msqkptr->u.msg_perm, IPC_M)))
			goto done2;
		if (msqbuf->msg_qbytes > msqkptr->u.msg_qbytes) {
			error = priv_check(td, PRIV_IPC_MSGSIZE);
			if (error)
				goto done2;
		}
		if (msqbuf->msg_qbytes > msginfo.msgmnb) {
			DPRINTF(("can't increase msg_qbytes beyond %d"
			    "(truncating)\n", msginfo.msgmnb));
			msqbuf->msg_qbytes = msginfo.msgmnb;	/* silently restrict qbytes to system limit */
		}
		if (msqbuf->msg_qbytes == 0) {
			DPRINTF(("can't reduce msg_qbytes to 0\n"));
			error = EINVAL;		/* non-standard errno! */
			goto done2;
		}
		msqkptr->u.msg_perm.uid = msqbuf->msg_perm.uid;	/* change the owner */
		msqkptr->u.msg_perm.gid = msqbuf->msg_perm.gid;	/* change the owner */
		msqkptr->u.msg_perm.mode = (msqkptr->u.msg_perm.mode & ~0777) |
		    (msqbuf->msg_perm.mode & 0777);
		msqkptr->u.msg_qbytes = msqbuf->msg_qbytes;
		msqkptr->u.msg_ctime = time_second;
		break;

	case IPC_STAT:
		if ((error = ipcperm(td, &msqkptr->u.msg_perm, IPC_R))) {
			DPRINTF(("requester doesn't have read access\n"));
			goto done2;
		}
		*msqbuf = msqkptr->u;
		break;

	default:
		DPRINTF(("invalid command %d\n", cmd));
		error = EINVAL;
		goto done2;
	}

	if (error == 0)
		td->td_retval[0] = rval;
done2:
	mtx_unlock(&msq_mtx);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct msgget_args {
	key_t	key;
	int	msgflg;
};
#endif
int
msgget(td, uap)
	struct thread *td;
	register struct msgget_args *uap;
{
	int msqid, error = 0;
	int key = uap->key;
	int msgflg = uap->msgflg;
	struct ucred *cred = td->td_ucred;
	register struct msqid_kernel *msqkptr = NULL;

	DPRINTF(("msgget(0x%x, 0%o)\n", key, msgflg));

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&msq_mtx);
	if (key != IPC_PRIVATE) {
		for (msqid = 0; msqid < msginfo.msgmni; msqid++) {
			msqkptr = &msqids[msqid];
			if (msqkptr->u.msg_qbytes != 0 &&
			    msqkptr->u.msg_perm.key == key)
				break;
		}
		if (msqid < msginfo.msgmni) {
			DPRINTF(("found public key\n"));
			if ((msgflg & IPC_CREAT) && (msgflg & IPC_EXCL)) {
				DPRINTF(("not exclusive\n"));
				error = EEXIST;
				goto done2;
			}
			if ((error = ipcperm(td, &msqkptr->u.msg_perm,
			    msgflg & 0700))) {
				DPRINTF(("requester doesn't have 0%o access\n",
				    msgflg & 0700));
				goto done2;
			}
#ifdef MAC
			error = mac_sysvmsq_check_msqget(cred, msqkptr);
			if (error != 0)
				goto done2;
#endif
			goto found;
		}
	}

	DPRINTF(("need to allocate the msqid_ds\n"));
	if (key == IPC_PRIVATE || (msgflg & IPC_CREAT)) {
		for (msqid = 0; msqid < msginfo.msgmni; msqid++) {
			/*
			 * Look for an unallocated and unlocked msqid_ds.
			 * msqid_ds's can be locked by msgsnd or msgrcv while
			 * they are copying the message in/out.  We can't
			 * re-use the entry until they release it.
			 */
			msqkptr = &msqids[msqid];
			if (msqkptr->u.msg_qbytes == 0 &&
			    (msqkptr->u.msg_perm.mode & MSG_LOCKED) == 0)
				break;
		}
		if (msqid == msginfo.msgmni) {
			DPRINTF(("no more msqid_ds's available\n"));
			error = ENOSPC;
			goto done2;
		}
		DPRINTF(("msqid %d is available\n", msqid));
		msqkptr->u.msg_perm.key = key;
		msqkptr->u.msg_perm.cuid = cred->cr_uid;
		msqkptr->u.msg_perm.uid = cred->cr_uid;
		msqkptr->u.msg_perm.cgid = cred->cr_gid;
		msqkptr->u.msg_perm.gid = cred->cr_gid;
		msqkptr->u.msg_perm.mode = (msgflg & 0777);
		/* Make sure that the returned msqid is unique */
		msqkptr->u.msg_perm.seq = (msqkptr->u.msg_perm.seq + 1) & 0x7fff;
		msqkptr->u.msg_first = NULL;
		msqkptr->u.msg_last = NULL;
		msqkptr->u.msg_cbytes = 0;
		msqkptr->u.msg_qnum = 0;
		msqkptr->u.msg_qbytes = msginfo.msgmnb;
		msqkptr->u.msg_lspid = 0;
		msqkptr->u.msg_lrpid = 0;
		msqkptr->u.msg_stime = 0;
		msqkptr->u.msg_rtime = 0;
		msqkptr->u.msg_ctime = time_second;
#ifdef MAC
		mac_sysvmsq_create(cred, msqkptr);
#endif
	} else {
		DPRINTF(("didn't find it and wasn't asked to create it\n"));
		error = ENOENT;
		goto done2;
	}

found:
	/* Construct the unique msqid */
	td->td_retval[0] = IXSEQ_TO_IPCID(msqid, msqkptr->u.msg_perm);
done2:
	mtx_unlock(&msq_mtx);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct msgsnd_args {
	int	msqid;
	const void	*msgp;
	size_t	msgsz;
	int	msgflg;
};
#endif
int
kern_msgsnd(td, msqid, msgp, msgsz, msgflg, mtype)
	struct thread *td;
	int msqid;
	const void *msgp;	/* XXX msgp is actually mtext. */
	size_t msgsz;
	int msgflg;
	long mtype;
{
	int msqix, segs_needed, error = 0;
	register struct msqid_kernel *msqkptr;
	register struct msg *msghdr;
	short next;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	mtx_lock(&msq_mtx);
	msqix = IPCID_TO_IX(msqid);

	if (msqix < 0 || msqix >= msginfo.msgmni) {
		DPRINTF(("msqid (%d) out of range (0<=msqid<%d)\n", msqix,
		    msginfo.msgmni));
		error = EINVAL;
		goto done2;
	}

	msqkptr = &msqids[msqix];
	if (msqkptr->u.msg_qbytes == 0) {
		DPRINTF(("no such message queue id\n"));
		error = EINVAL;
		goto done2;
	}
	if (msqkptr->u.msg_perm.seq != IPCID_TO_SEQ(msqid)) {
		DPRINTF(("wrong sequence number\n"));
		error = EINVAL;
		goto done2;
	}

	if ((error = ipcperm(td, &msqkptr->u.msg_perm, IPC_W))) {
		DPRINTF(("requester doesn't have write access\n"));
		goto done2;
	}

#ifdef MAC
	error = mac_sysvmsq_check_msqsnd(td->td_ucred, msqkptr);
	if (error != 0)
		goto done2;
#endif

	segs_needed = (msgsz + msginfo.msgssz - 1) / msginfo.msgssz;
	DPRINTF(("msgsz=%zu, msgssz=%d, segs_needed=%d\n", msgsz,
	    msginfo.msgssz, segs_needed));
	for (;;) {
		int need_more_resources = 0;

		/*
		 * check msgsz
		 * (inside this loop in case msg_qbytes changes while we sleep)
		 */

		if (msgsz > msqkptr->u.msg_qbytes) {
			DPRINTF(("msgsz > msqkptr->u.msg_qbytes\n"));
			error = EINVAL;
			goto done2;
		}

		if (msqkptr->u.msg_perm.mode & MSG_LOCKED) {
			DPRINTF(("msqid is locked\n"));
			need_more_resources = 1;
		}
		if (msgsz + msqkptr->u.msg_cbytes > msqkptr->u.msg_qbytes) {
			DPRINTF(("msgsz + msg_cbytes > msg_qbytes\n"));
			need_more_resources = 1;
		}
		if (segs_needed > nfree_msgmaps) {
			DPRINTF(("segs_needed > nfree_msgmaps\n"));
			need_more_resources = 1;
		}
		if (free_msghdrs == NULL) {
			DPRINTF(("no more msghdrs\n"));
			need_more_resources = 1;
		}

		if (need_more_resources) {
			int we_own_it;

			if ((msgflg & IPC_NOWAIT) != 0) {
				DPRINTF(("need more resources but caller "
				    "doesn't want to wait\n"));
				error = EAGAIN;
				goto done2;
			}

			if ((msqkptr->u.msg_perm.mode & MSG_LOCKED) != 0) {
				DPRINTF(("we don't own the msqid_ds\n"));
				we_own_it = 0;
			} else {
				/* Force later arrivals to wait for our
				   request */
				DPRINTF(("we own the msqid_ds\n"));
				msqkptr->u.msg_perm.mode |= MSG_LOCKED;
				we_own_it = 1;
			}
			DPRINTF(("msgsnd:  goodnight\n"));
			error = msleep(msqkptr, &msq_mtx, (PZERO - 4) | PCATCH,
			    "msgsnd", hz);
			DPRINTF(("msgsnd:  good morning, error=%d\n", error));
			if (we_own_it)
				msqkptr->u.msg_perm.mode &= ~MSG_LOCKED;
			if (error == EWOULDBLOCK) {
				DPRINTF(("msgsnd:  timed out\n"));
				continue;
			}
			if (error != 0) {
				DPRINTF(("msgsnd:  interrupted system call\n"));
				error = EINTR;
				goto done2;
			}

			/*
			 * Make sure that the msq queue still exists
			 */

			if (msqkptr->u.msg_qbytes == 0) {
				DPRINTF(("msqid deleted\n"));
				error = EIDRM;
				goto done2;
			}

		} else {
			DPRINTF(("got all the resources that we need\n"));
			break;
		}
	}

	/*
	 * We have the resources that we need.
	 * Make sure!
	 */

	if (msqkptr->u.msg_perm.mode & MSG_LOCKED)
		panic("msg_perm.mode & MSG_LOCKED");
	if (segs_needed > nfree_msgmaps)
		panic("segs_needed > nfree_msgmaps");
	if (msgsz + msqkptr->u.msg_cbytes > msqkptr->u.msg_qbytes)
		panic("msgsz + msg_cbytes > msg_qbytes");
	if (free_msghdrs == NULL)
		panic("no more msghdrs");

	/*
	 * Re-lock the msqid_ds in case we page-fault when copying in the
	 * message
	 */

	if ((msqkptr->u.msg_perm.mode & MSG_LOCKED) != 0)
		panic("msqid_ds is already locked");
	msqkptr->u.msg_perm.mode |= MSG_LOCKED;

	/*
	 * Allocate a message header
	 */

	msghdr = free_msghdrs;
	free_msghdrs = msghdr->msg_next;
	msghdr->msg_spot = -1;
	msghdr->msg_ts = msgsz;
	msghdr->msg_type = mtype;
#ifdef MAC
	/*
	 * XXXMAC: Should the mac_sysvmsq_check_msgmsq check follow here
	 * immediately?  Or, should it be checked just before the msg is
	 * enqueued in the msgq (as it is done now)?
	 */
	mac_sysvmsg_create(td->td_ucred, msqkptr, msghdr);
#endif

	/*
	 * Allocate space for the message
	 */

	while (segs_needed > 0) {
		if (nfree_msgmaps <= 0)
			panic("not enough msgmaps");
		if (free_msgmaps == -1)
			panic("nil free_msgmaps");
		next = free_msgmaps;
		if (next <= -1)
			panic("next too low #1");
		if (next >= msginfo.msgseg)
			panic("next out of range #1");
		DPRINTF(("allocating segment %d to message\n", next));
		free_msgmaps = msgmaps[next].next;
		nfree_msgmaps--;
		msgmaps[next].next = msghdr->msg_spot;
		msghdr->msg_spot = next;
		segs_needed--;
	}

	/*
	 * Validate the message type
	 */

	if (msghdr->msg_type < 1) {
		msg_freehdr(msghdr);
		msqkptr->u.msg_perm.mode &= ~MSG_LOCKED;
		wakeup(msqkptr);
		DPRINTF(("mtype (%ld) < 1\n", msghdr->msg_type));
		error = EINVAL;
		goto done2;
	}

	/*
	 * Copy in the message body
	 */

	next = msghdr->msg_spot;
	while (msgsz > 0) {
		size_t tlen;
		if (msgsz > msginfo.msgssz)
			tlen = msginfo.msgssz;
		else
			tlen = msgsz;
		if (next <= -1)
			panic("next too low #2");
		if (next >= msginfo.msgseg)
			panic("next out of range #2");
		mtx_unlock(&msq_mtx);
		if ((error = copyin(msgp, &msgpool[next * msginfo.msgssz],
		    tlen)) != 0) {
			mtx_lock(&msq_mtx);
			DPRINTF(("error %d copying in message segment\n",
			    error));
			msg_freehdr(msghdr);
			msqkptr->u.msg_perm.mode &= ~MSG_LOCKED;
			wakeup(msqkptr);
			goto done2;
		}
		mtx_lock(&msq_mtx);
		msgsz -= tlen;
		msgp = (const char *)msgp + tlen;
		next = msgmaps[next].next;
	}
	if (next != -1)
		panic("didn't use all the msg segments");

	/*
	 * We've got the message.  Unlock the msqid_ds.
	 */

	msqkptr->u.msg_perm.mode &= ~MSG_LOCKED;

	/*
	 * Make sure that the msqid_ds is still allocated.
	 */

	if (msqkptr->u.msg_qbytes == 0) {
		msg_freehdr(msghdr);
		wakeup(msqkptr);
		error = EIDRM;
		goto done2;
	}

#ifdef MAC
	/*
	 * Note: Since the task/thread allocates the msghdr and usually
	 * primes it with its own MAC label, for a majority of policies, it
	 * won't be necessary to check whether the msghdr has access
	 * permissions to the msgq.  The mac_sysvmsq_check_msqsnd check would
	 * suffice in that case.  However, this hook may be required where
	 * individual policies derive a non-identical label for the msghdr
	 * from the current thread label and may want to check the msghdr
	 * enqueue permissions, along with read/write permissions to the
	 * msgq.
	 */
	error = mac_sysvmsq_check_msgmsq(td->td_ucred, msghdr, msqkptr);
	if (error != 0) {
		msg_freehdr(msghdr);
		wakeup(msqkptr);
		goto done2;
	}
#endif

	/*
	 * Put the message into the queue
	 */
	if (msqkptr->u.msg_first == NULL) {
		msqkptr->u.msg_first = msghdr;
		msqkptr->u.msg_last = msghdr;
	} else {
		msqkptr->u.msg_last->msg_next = msghdr;
		msqkptr->u.msg_last = msghdr;
	}
	msqkptr->u.msg_last->msg_next = NULL;

	msqkptr->u.msg_cbytes += msghdr->msg_ts;
	msqkptr->u.msg_qnum++;
	msqkptr->u.msg_lspid = td->td_proc->p_pid;
	msqkptr->u.msg_stime = time_second;

	wakeup(msqkptr);
	td->td_retval[0] = 0;
done2:
	mtx_unlock(&msq_mtx);
	return (error);
}

int
msgsnd(td, uap)
	struct thread *td;
	register struct msgsnd_args *uap;
{
	int error;
	long mtype;

	DPRINTF(("call to msgsnd(%d, %p, %zu, %d)\n", uap->msqid, uap->msgp,
	    uap->msgsz, uap->msgflg));

	if ((error = copyin(uap->msgp, &mtype, sizeof(mtype))) != 0) {
		DPRINTF(("error %d copying the message type\n", error));
		return (error);
	}
	return (kern_msgsnd(td, uap->msqid,
	    (const char *)uap->msgp + sizeof(mtype),
	    uap->msgsz, uap->msgflg, mtype));
}

#ifndef _SYS_SYSPROTO_H_
struct msgrcv_args {
	int	msqid;
	void	*msgp;
	size_t	msgsz;
	long	msgtyp;
	int	msgflg;
};
#endif
int
kern_msgrcv(td, msqid, msgp, msgsz, msgtyp, msgflg, mtype)
	struct thread *td;
	int msqid;
	void *msgp;	/* XXX msgp is actually mtext. */
	size_t msgsz;
	long msgtyp;
	int msgflg;
	long *mtype;
{
	size_t len;
	register struct msqid_kernel *msqkptr;
	register struct msg *msghdr;
	int msqix, error = 0;
	short next;

	if (!jail_sysvipc_allowed && jailed(td->td_ucred))
		return (ENOSYS);

	msqix = IPCID_TO_IX(msqid);

	if (msqix < 0 || msqix >= msginfo.msgmni) {
		DPRINTF(("msqid (%d) out of range (0<=msqid<%d)\n", msqix,
		    msginfo.msgmni));
		return (EINVAL);
	}

	msqkptr = &msqids[msqix];
	mtx_lock(&msq_mtx);
	if (msqkptr->u.msg_qbytes == 0) {
		DPRINTF(("no such message queue id\n"));
		error = EINVAL;
		goto done2;
	}
	if (msqkptr->u.msg_perm.seq != IPCID_TO_SEQ(msqid)) {
		DPRINTF(("wrong sequence number\n"));
		error = EINVAL;
		goto done2;
	}

	if ((error = ipcperm(td, &msqkptr->u.msg_perm, IPC_R))) {
		DPRINTF(("requester doesn't have read access\n"));
		goto done2;
	}

#ifdef MAC
	error = mac_sysvmsq_check_msqrcv(td->td_ucred, msqkptr);
	if (error != 0)
		goto done2;
#endif

	msghdr = NULL;
	while (msghdr == NULL) {
		if (msgtyp == 0) {
			msghdr = msqkptr->u.msg_first;
			if (msghdr != NULL) {
				if (msgsz < msghdr->msg_ts &&
				    (msgflg & MSG_NOERROR) == 0) {
					DPRINTF(("first message on the queue "
					    "is too big (want %zu, got %d)\n",
					    msgsz, msghdr->msg_ts));
					error = E2BIG;
					goto done2;
				}
#ifdef MAC
				error = mac_sysvmsq_check_msgrcv(td->td_ucred,
				    msghdr);
				if (error != 0)
					goto done2;
#endif
				if (msqkptr->u.msg_first == msqkptr->u.msg_last) {
					msqkptr->u.msg_first = NULL;
					msqkptr->u.msg_last = NULL;
				} else {
					msqkptr->u.msg_first = msghdr->msg_next;
					if (msqkptr->u.msg_first == NULL)
						panic("msg_first/last screwed up #1");
				}
			}
		} else {
			struct msg *previous;
			struct msg **prev;

			previous = NULL;
			prev = &(msqkptr->u.msg_first);
			while ((msghdr = *prev) != NULL) {
				/*
				 * Is this message's type an exact match or is
				 * this message's type less than or equal to
				 * the absolute value of a negative msgtyp?
				 * Note that the second half of this test can
				 * NEVER be true if msgtyp is positive since
				 * msg_type is always positive!
				 */

				if (msgtyp == msghdr->msg_type ||
				    msghdr->msg_type <= -msgtyp) {
					DPRINTF(("found message type %ld, "
					    "requested %ld\n",
					    msghdr->msg_type, msgtyp));
					if (msgsz < msghdr->msg_ts &&
					    (msgflg & MSG_NOERROR) == 0) {
						DPRINTF(("requested message "
						    "on the queue is too big "
						    "(want %zu, got %hu)\n",
						    msgsz, msghdr->msg_ts));
						error = E2BIG;
						goto done2;
					}
#ifdef MAC
					error = mac_sysvmsq_check_msgrcv(
					    td->td_ucred, msghdr);
					if (error != 0)
						goto done2;
#endif
					*prev = msghdr->msg_next;
					if (msghdr == msqkptr->u.msg_last) {
						if (previous == NULL) {
							if (prev !=
							    &msqkptr->u.msg_first)
								panic("msg_first/last screwed up #2");
							msqkptr->u.msg_first =
							    NULL;
							msqkptr->u.msg_last =
							    NULL;
						} else {
							if (prev ==
							    &msqkptr->u.msg_first)
								panic("msg_first/last screwed up #3");
							msqkptr->u.msg_last =
							    previous;
						}
					}
					break;
				}
				previous = msghdr;
				prev = &(msghdr->msg_next);
			}
		}

		/*
		 * We've either extracted the msghdr for the appropriate
		 * message or there isn't one.
		 * If there is one then bail out of this loop.
		 */

		if (msghdr != NULL)
			break;

		/*
		 * Hmph!  No message found.  Does the user want to wait?
		 */

		if ((msgflg & IPC_NOWAIT) != 0) {
			DPRINTF(("no appropriate message found (msgtyp=%ld)\n",
			    msgtyp));
			/* The SVID says to return ENOMSG. */
			error = ENOMSG;
			goto done2;
		}

		/*
		 * Wait for something to happen
		 */

		DPRINTF(("msgrcv:  goodnight\n"));
		error = msleep(msqkptr, &msq_mtx, (PZERO - 4) | PCATCH,
		    "msgrcv", 0);
		DPRINTF(("msgrcv:  good morning (error=%d)\n", error));

		if (error != 0) {
			DPRINTF(("msgrcv:  interrupted system call\n"));
			error = EINTR;
			goto done2;
		}

		/*
		 * Make sure that the msq queue still exists
		 */

		if (msqkptr->u.msg_qbytes == 0 ||
		    msqkptr->u.msg_perm.seq != IPCID_TO_SEQ(msqid)) {
			DPRINTF(("msqid deleted\n"));
			error = EIDRM;
			goto done2;
		}
	}

	/*
	 * Return the message to the user.
	 *
	 * First, do the bookkeeping (before we risk being interrupted).
	 */

	msqkptr->u.msg_cbytes -= msghdr->msg_ts;
	msqkptr->u.msg_qnum--;
	msqkptr->u.msg_lrpid = td->td_proc->p_pid;
	msqkptr->u.msg_rtime = time_second;

	/*
	 * Make msgsz the actual amount that we'll be returning.
	 * Note that this effectively truncates the message if it is too long
	 * (since msgsz is never increased).
	 */

	DPRINTF(("found a message, msgsz=%zu, msg_ts=%hu\n", msgsz,
	    msghdr->msg_ts));
	if (msgsz > msghdr->msg_ts)
		msgsz = msghdr->msg_ts;
	*mtype = msghdr->msg_type;

	/*
	 * Return the segments to the user
	 */

	next = msghdr->msg_spot;
	for (len = 0; len < msgsz; len += msginfo.msgssz) {
		size_t tlen;

		if (msgsz - len > msginfo.msgssz)
			tlen = msginfo.msgssz;
		else
			tlen = msgsz - len;
		if (next <= -1)
			panic("next too low #3");
		if (next >= msginfo.msgseg)
			panic("next out of range #3");
		mtx_unlock(&msq_mtx);
		error = copyout(&msgpool[next * msginfo.msgssz], msgp, tlen);
		mtx_lock(&msq_mtx);
		if (error != 0) {
			DPRINTF(("error (%d) copying out message segment\n",
			    error));
			msg_freehdr(msghdr);
			wakeup(msqkptr);
			goto done2;
		}
		msgp = (char *)msgp + tlen;
		next = msgmaps[next].next;
	}

	/*
	 * Done, return the actual number of bytes copied out.
	 */

	msg_freehdr(msghdr);
	wakeup(msqkptr);
	td->td_retval[0] = msgsz;
done2:
	mtx_unlock(&msq_mtx);
	return (error);
}

int
msgrcv(td, uap)
	struct thread *td;
	register struct msgrcv_args *uap;
{
	int error;
	long mtype;

	DPRINTF(("call to msgrcv(%d, %p, %zu, %ld, %d)\n", uap->msqid,
	    uap->msgp, uap->msgsz, uap->msgtyp, uap->msgflg));

	if ((error = kern_msgrcv(td, uap->msqid,
	    (char *)uap->msgp + sizeof(mtype), uap->msgsz,
	    uap->msgtyp, uap->msgflg, &mtype)) != 0)
		return (error);
	if ((error = copyout(&mtype, uap->msgp, sizeof(mtype))) != 0)
		DPRINTF(("error %d copying the message type\n", error));
	return (error);
}

static int
sysctl_msqids(SYSCTL_HANDLER_ARGS)
{

	return (SYSCTL_OUT(req, msqids,
	    sizeof(struct msqid_kernel) * msginfo.msgmni));
}

SYSCTL_INT(_kern_ipc, OID_AUTO, msgmax, CTLFLAG_RD, &msginfo.msgmax, 0,
    "Maximum message size");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgmni, CTLFLAG_RDTUN, &msginfo.msgmni, 0,
    "Number of message queue identifiers");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgmnb, CTLFLAG_RDTUN, &msginfo.msgmnb, 0,
    "Maximum number of bytes in a queue");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgtql, CTLFLAG_RDTUN, &msginfo.msgtql, 0,
    "Maximum number of messages in the system");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgssz, CTLFLAG_RDTUN, &msginfo.msgssz, 0,
    "Size of a message segment");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgseg, CTLFLAG_RDTUN, &msginfo.msgseg, 0,
    "Number of message segments");
SYSCTL_PROC(_kern_ipc, OID_AUTO, msqids, CTLFLAG_RD,
    NULL, 0, sysctl_msqids, "", "Message queue IDs");
