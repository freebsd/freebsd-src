/*
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

#ifdef SYSVMSG

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "proc.h"
#include "msg.h"
#include "malloc.h"

static int	msgctl(), msgget(), msgsnd(), msgrcv();

int	(*msgcalls[])() = { msgctl, msgget, msgsnd, msgrcv };

int nfree_msgmaps;		/* # of free map entries */
short free_msgmaps;		/* head of linked list of free map entries */
struct msg *free_msghdrs;	/* list of free msg headers */

void
msginit()
{
    register int i;
    vm_offset_t whocares1, whocares2;

    /*
     * msginfo.msgssz should be a power of two for efficiency reasons.
     * It is also pretty silly if msginfo.msgssz is less than 8
     * or greater than about 256 so ...
     */

    i = 8;
    while ( i < 1024 && i != msginfo.msgssz ) {
	i <<= 1;
    }
    if ( i != msginfo.msgssz ) {
	printf("msginfo.msgssz=%d (0x%x)\n",msginfo.msgssz,msginfo.msgssz);
	panic("msginfo.msgssz not a small power of 2");
    }

    if ( msginfo.msgseg > 32767 ) {
	printf("msginfo.msgseg=%d\n",msginfo.msgseg);
	panic("msginfo.msgseg > 32767");
    }

    if ( msgmaps == NULL ) {
	panic("msgmaps is NULL");
    }
    for ( i = 0; i < msginfo.msgseg; i += 1 ) {
	if ( i > 0 ) {
	    msgmaps[i-1].next = i;
	}
	msgmaps[i].next = -1;		/* implies entry is available */
    }
    free_msgmaps = 0;
    nfree_msgmaps = msginfo.msgseg;

    if ( msghdrs == NULL ) {
	panic("msghdrs is NULL");
    }
    for ( i = 0; i < msginfo.msgtql; i += 1 ) {
	msghdrs[i].msg_type = 0;
	if ( i > 0 ) {
	    msghdrs[i-1].msg_next = &msghdrs[i];
	}
	msghdrs[i].msg_next = NULL;
    }
    free_msghdrs = &msghdrs[0];

    if ( msqids == NULL ) {
	panic("msqids is NULL");
    }
    for ( i = 0; i < msginfo.msgmni; i += 1 ) {
	msqids[i].msg_qbytes = 0;	/* implies entry is available */
	msqids[i].msg_perm.seq = 0;	/* reset to a known value */
    }

}

TEXT_SET(pseudo_set, msginit);

/*
 * Entry point for all MSG calls
 */

struct msgsys_args {
	u_int	which;
};

int
msgsys(p, uap, retval)
	struct caller *p;
	struct msgsys_args *uap;
	int *retval;
{
	if (uap->which >= sizeof(msgcalls)/sizeof(msgcalls[0]))
		return (EINVAL);
	return ((*msgcalls[uap->which])(p, &uap[1], retval));
}

static
void
msg_freehdr(msghdr)
struct msg *msghdr;
{
    while ( msghdr->msg_ts > 0 ) {
	short next;
	if ( msghdr->msg_spot < 0 || msghdr->msg_spot >= msginfo.msgseg ) {
	    panic("msghdr->msg_spot out of range");
	}
	next = msgmaps[msghdr->msg_spot].next;
	msgmaps[msghdr->msg_spot].next = free_msgmaps;
	free_msgmaps = msghdr->msg_spot;
	nfree_msgmaps += 1;
	msghdr->msg_spot = next;
	if ( msghdr->msg_ts >= msginfo.msgssz ) {
	    msghdr->msg_ts -= msginfo.msgssz;
	} else {
	    msghdr->msg_ts = 0;
	}
    }
    if ( msghdr->msg_spot != -1 ) {
	panic("msghdr->msg_spot != -1");
    }
    msghdr->msg_next = free_msghdrs;
    free_msghdrs = msghdr;
}

struct msgctl_args {
	int	msqid;
	int	cmd;
	struct	msqid_ds *user_msqptr;
};

int
msgctl(p, uap, retval)
	struct proc *p;
	register struct msgctl_args *uap;
	int *retval;
{
    int msqid = uap->msqid;
    int cmd = uap->cmd;
    struct msqid_ds *user_msqptr = uap->user_msqptr;
    struct ucred *cred = p->p_ucred;
    int i, rval, eval;
    struct msqid_ds msqbuf;
    register struct msqid_ds *msqptr;

#ifdef MSG_DEBUG
    printf("call to msgctl(%d,%d,0x%x)\n",msqid,cmd,user_msqptr);
#endif

    msqid = IPCID_TO_IX(msqid);

    if ( msqid < 0 || msqid >= msginfo.msgmni ) {
#ifdef MSG_DEBUG
	printf("msqid (%d) out of range (0<=msqid<%d)\n",msqid,msginfo.msgmni);
#endif
	return(EINVAL);
    }

    msqptr = &msqids[msqid];

    if ( msqptr->msg_qbytes == 0 ) {
#ifdef MSG_DEBUG
	printf("no such msqid\n");
#endif
	return(EINVAL);
    }
    if ( msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid) ) {
#ifdef MSG_DEBUG
	printf("wrong sequence number\n");
#endif
	return(EINVAL);
    }

    eval = 0;
    rval = 0;

    switch (cmd) {

    case IPC_RMID:
#ifdef MSG_DEBUG
	printf("IPC_RMID\n");
#endif
	{
	    struct msg *msghdr;

	    if ( cred->cr_uid != 0
	    && msqptr->msg_perm.cuid != cred->cr_uid
	    && msqptr->msg_perm.uid != cred->cr_uid ) {
		return(EPERM);
	    }
	    msghdr = msqptr->msg_first;

	    /* Free the message headers */

	    while ( msghdr != NULL ) {
		struct msg *msghdr_tmp;

		/* Free the segments of each message */

		msqptr->msg_cbytes -= msghdr->msg_ts;
		msqptr->msg_qnum -= 1;
		msghdr_tmp = msghdr;
		msghdr = msghdr->msg_next;
		msg_freehdr(msghdr_tmp);

	    }

	    if ( msqptr->msg_cbytes != 0 ) {
		panic("msg_cbytes is screwed up");
	    }
	    if ( msqptr->msg_qnum != 0 ) {
		panic("msg_qnum is screwed up");
	    }

	    msqptr->msg_qbytes = 0;	/* Mark it as free */

	    /* Make sure that anybody who is waiting notices the deletion */

	    wakeup( (caddr_t)msqptr );
	}

	break;

    case IPC_SET:
#ifdef MSG_DEBUG
	printf("IPC_SET\n");
#endif
	if ( cred->cr_uid != 0
	&& msqptr->msg_perm.cuid != cred->cr_uid
	&& msqptr->msg_perm.uid != cred->cr_uid ) {
	    return(EPERM);
	}
	if ( (eval = copyin(user_msqptr, &msqbuf, sizeof(msqbuf))) != 0 ) {
	    return(eval);
	}
	if ( msqbuf.msg_qbytes > msqptr->msg_qbytes
	&& cred->cr_uid != 0 ) {
	    return(EPERM);
	}
	if ( msqbuf.msg_qbytes > msginfo.msgmnb ) {
#ifdef MSG_DEBUG
	    printf("can't increase msg_qbytes beyond %d (truncating)\n",msginfo.msgmnb);
#endif
	    msqbuf.msg_qbytes = msginfo.msgmnb;	/* silently restrict qbytes to system limit */
	}
	if ( msqbuf.msg_qbytes == 0 ) {
#ifdef MSG_DEBUG
	    printf("can't reduce msg_qbytes to 0\n");
#endif
	    return(EINVAL);		/* non-standard errno! */
	}
	msqptr->msg_perm.uid = msqbuf.msg_perm.uid;	/* change the owner */
	msqptr->msg_perm.gid = msqbuf.msg_perm.gid;	/* change the owner */
	msqptr->msg_perm.mode = (msqptr->msg_perm.mode & ~0777)
	| (msqbuf.msg_perm.mode & 0777);
	msqptr->msg_qbytes = msqbuf.msg_qbytes;
	msqptr->msg_ctime = time.tv_sec;
	break;

    case IPC_STAT:
#ifdef MSG_DEBUG
	printf("IPC_STAT\n");
#endif
	if ( (eval = ipcaccess(&msqptr->msg_perm, IPC_R, cred)) ) {
#ifdef MSG_DEBUG
	    printf("requester doesn't have read access\n");
#endif
	    return(eval);
	}
	rval = 0;
	eval = copyout((caddr_t)msqptr, user_msqptr, sizeof(struct msqid_ds));
	break;

    default:
#ifdef MSG_DEBUG
	printf("invalid command %d\n",cmd);
#endif
	return(EINVAL);
    }

    if ( eval == 0 ) {
	*retval = rval;
    }
    return(eval);
}

struct msgget_args {
	key_t	key;
	int	msgflg;
};

int
msgget(p, uap, retval)
	struct proc *p;
	register struct msgget_args *uap;
	int *retval;
{
    int msqid, eval;
    int key = uap->key;
    int msgflg = uap->msgflg;
    struct ucred *cred = p->p_ucred;
    register struct msqid_ds *msqptr = NULL;

#ifdef MSG_DEBUG
    printf("msgget(0x%x,0%o)\n",key,msgflg);
#endif

    if ( key == IPC_PRIVATE ) {
#ifdef MSG_DEBUG
	printf("private key\n");
#endif
	msqid = msginfo.msgmni;
    } else {
	for ( msqid = 0; msqid < msginfo.msgmni; msqid += 1 ) {
	    msqptr = &msqids[msqid];
	    if ( msqptr->msg_qbytes != 0 && msqptr->msg_perm.key == key ) {
		break;
	    }
	}
	if ( msqid < msginfo.msgmni ) {
#ifdef MSG_DEBUG
	    printf("found public key\n");
#endif
	    if ( (msgflg & IPC_CREAT) && (msgflg & IPC_EXCL) ) {
#ifdef MSG_DEBUG
		printf("not exclusive\n");
#endif
		return(EEXIST);
	    }
	    if ( (eval = ipcaccess(&msqptr->msg_perm, msgflg & 0700, cred)) ) {
#ifdef MSG_DEBUG
		printf("requester doesn't have 0%o access\n",msgflg & 0700);
#endif
		return(eval);
	    }
	} else {
#ifdef MSG_DEBUG
	    printf("didn't find public key\n");
#endif
	}
    }

    if ( msqid == msginfo.msgmni ) {
#ifdef MSG_DEBUG
	printf("need to allocate the msqid_ds\n");
#endif
	if ( key == IPC_PRIVATE || (msgflg & IPC_CREAT) ) {
	    for ( msqid = 0; msqid < msginfo.msgmni; msqid += 1 ) {
		/*
		 * Look for an unallocated and unlocked msqid_ds.
		 * msqid_ds's can be locked by msgsnd or msgrcv while they
		 * are copying the message in/out.  We can't re-use the
		 * entry until they release it.
		 */

		msqptr = &msqids[msqid];
		if ( msqptr->msg_qbytes == 0
		&& (msqptr->msg_perm.mode & MSG_LOCKED) == 0 ) {
		    break;
		}
	    }
	    if ( msqid == msginfo.msgmni ) {
#ifdef MSG_DEBUG
		printf("no more msqid_ds's available\n");
#endif
		return(ENOSPC);	
	    }
#ifdef MSG_DEBUG
	    printf("msqid %d is available\n",msqid+1);
#endif
	    msqptr->msg_perm.key = key;
	    msqptr->msg_perm.cuid = cred->cr_uid;
	    msqptr->msg_perm.uid = cred->cr_uid;
	    msqptr->msg_perm.cgid = cred->cr_gid;
	    msqptr->msg_perm.gid = cred->cr_gid;
	    msqptr->msg_perm.mode = (msgflg & 0777);
	    msqptr->msg_perm.seq += 1;		/* Make sure that the returned msqid is unique */
	    msqptr->msg_first = NULL;
	    msqptr->msg_last = NULL;
	    msqptr->msg_cbytes = 0;
	    msqptr->msg_qnum = 0;
	    msqptr->msg_qbytes = msginfo.msgmnb;
	    msqptr->msg_lspid = 0;
	    msqptr->msg_lrpid = 0;
	    msqptr->msg_stime = 0;
	    msqptr->msg_rtime = 0;
	    msqptr->msg_ctime = time.tv_sec;
	} else {
#ifdef MSG_DEBUG
	    printf("didn't find it and wasn't asked to create it\n");
#endif
	    return(ENOENT);
	}
    }

    *retval = IXSEQ_TO_IPCID(msqid,msqptr->msg_perm);	/* Construct the unique msqid */
    return(0);
}

struct msgsnd_args {
	int	msqid;
	void	*user_msgp;
	size_t	msgsz;
	int	msgflg;
};

int
msgsnd(p, uap, retval)
	struct proc *p;
	register struct msgsnd_args *uap;
	int *retval;
{
    int msqid = uap->msqid;
    void *user_msgp = uap->user_msgp;
    size_t msgsz = uap->msgsz;
    int msgflg = uap->msgflg;
    int segs_needed, eval;
    struct ucred *cred = p->p_ucred;
    register struct msqid_ds *msqptr;
    register struct msg *msghdr;
    short next;

#ifdef MSG_DEBUG
    printf("call to msgsnd(%d,0x%x,%d,%d)\n",msqid,user_msgp,msgsz,msgflg);
#endif

    msqid = IPCID_TO_IX(msqid);

    if ( msqid < 0 || msqid >= msginfo.msgmni ) {
#ifdef MSG_DEBUG
	printf("msqid (%d) out of range (0<=msqid<%d)\n",msqid,msginfo.msgmni);
#endif
	return(EINVAL);
    }

    msqptr = &msqids[msqid];
    if ( msqptr->msg_qbytes == 0 ) {
#ifdef MSG_DEBUG
	printf("no such message queue id\n");
#endif
	return(EINVAL);
    }
    if ( msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid) ) {
#ifdef MSG_DEBUG
	printf("wrong sequence number\n");
#endif
	return(EINVAL);
    }

    if ( (eval = ipcaccess(&msqptr->msg_perm, IPC_W, cred)) ) {
#ifdef MSG_DEBUG
	printf("requester doesn't have write access\n");
#endif
	return(eval);
    }

    segs_needed = (msgsz + msginfo.msgssz - 1) / msginfo.msgssz;
#ifdef MSG_DEBUG
    printf("msgsz=%d, msgssz=%d, segs_needed=%d\n",msgsz,msginfo.msgssz,segs_needed);
#endif
    while ( 1 ) {
	int need_more_resources = 0;

	/*
	 * check msgsz
	 * (inside this loop in case msg_qbytes changes while we sleep)
	 */

	if ( msgsz > msqptr->msg_qbytes ) {
#ifdef MSG_DEBUG
	    printf("msgsz > msqptr->msg_qbytes\n");
#endif
	    return(EINVAL);
	}

	if ( msqptr->msg_perm.mode & MSG_LOCKED ) {
#ifdef MSG_DEBUG
	    printf("msqid is locked\n");
#endif
	    need_more_resources = 1;
	}
	if ( msgsz + msqptr->msg_cbytes > msqptr->msg_qbytes ) {
#ifdef MSG_DEBUG
	    printf("msgsz + msg_cbytes > msg_qbytes\n");
#endif
	    need_more_resources = 1;
	}
	if ( segs_needed > nfree_msgmaps ) {
#ifdef MSG_DEBUG
	    printf("segs_needed > nfree_msgmaps\n");
#endif
	    need_more_resources = 1;
	}
	if ( free_msghdrs == NULL ) {
#ifdef MSG_DEBUG
	    printf("no more msghdrs\n");
#endif
	    need_more_resources = 1;
	}

	if ( need_more_resources ) {

	    int we_own_it;

	    if ( (msgflg & IPC_NOWAIT) != 0 ) {
#ifdef MSG_DEBUG
		printf("need more resources but caller doesn't want to wait\n");
#endif
		return(EAGAIN);
	    }

	    if ( (msqptr->msg_perm.mode & MSG_LOCKED) != 0 ) {
#ifdef MSG_DEBUG
		printf("we don't own the msqid_ds\n");
#endif
		we_own_it = 0;
	    } else {
		/* Force later arrivals to wait for our request */
#ifdef MSG_DEBUG
		printf("we own the msqid_ds\n");
#endif
		msqptr->msg_perm.mode |= MSG_LOCKED;
		we_own_it = 1;
	    }
#ifdef MSG_DEBUG
	    printf("goodnight\n");
#endif
	    eval = tsleep( (caddr_t)msqptr, (PZERO - 4) | PCATCH, "msg wait", 0 );
#ifdef MSG_DEBUG
	    printf("good morning, eval=%d\n",eval);
#endif
	    if ( we_own_it ) {
		msqptr->msg_perm.mode &= ~MSG_LOCKED;
	    }
	    if ( eval != 0 ) {
#ifdef MSG_DEBUG
		printf("msgsnd:  interrupted system call\n");
#endif
		return( EINTR );
	    }

	    /*
	     * Make sure that the msq queue still exists
	     */

	    if ( msqptr->msg_qbytes == 0 ) {
#ifdef MSG_DEBUG
		printf("msqid deleted\n");
#endif
		/* The SVID says to return EIDRM. */
#ifdef EIDRM
		return(EIDRM);
#else
		/* Unfortunately, BSD doesn't define that code (yet)! */
		return(EINVAL);
#endif
	    }

	} else {
#ifdef MSG_DEBUG
	    printf("got all the resources that we need\n");
#endif
	    break;
	}

    }

    /*
     * We have the resources that we need.
     * Make sure!
     */

    if ( msqptr->msg_perm.mode & MSG_LOCKED ) {
	panic("msg_perm.mode & MSG_LOCKED");		/* bug somewhere */
    }
    if ( segs_needed > nfree_msgmaps ) {
	panic("segs_needed > nfree_msgmaps");		/* bug somewhere */
    }
    if ( msgsz + msqptr->msg_cbytes > msqptr->msg_qbytes ) {
	panic("msgsz + msg_cbytes > msg_qbytes");	/* bug somewhere */
    }
    if ( free_msghdrs == NULL ) {
	panic("no more msghdrs");			/* bug somewhere */
    }

    /*
     * Re-lock the msqid_ds in case we page-fault when copying in the message
     */

    if ( (msqptr->msg_perm.mode & MSG_LOCKED) != 0 ) {
	panic("msqid_ds is already locked");
    }
    msqptr->msg_perm.mode |= MSG_LOCKED;

    /*
     * Allocate a message header
     */

    msghdr = free_msghdrs;
    free_msghdrs = msghdr->msg_next;
    msghdr->msg_spot = -1;
    msghdr->msg_ts = msgsz;

    /*
     * Allocate space for the message
     */

    while ( segs_needed > 0 ) {
	if ( nfree_msgmaps <= 0 ) {
	    panic("not enough msgmaps");
	}
	if ( free_msgmaps == -1 ) {
	    panic("nil free_msgmaps");
	}
	next = free_msgmaps;
	if ( next <= -1 ) {
	    panic("next too low #1");
	}
	if ( next >= msginfo.msgseg ) {
	    panic("next out of range #1");
	}
#ifdef MSG_DEBUG
	printf("allocating segment %d to message\n",next);
#endif
	free_msgmaps = msgmaps[next].next;
	nfree_msgmaps -= 1;
	msgmaps[next].next = msghdr->msg_spot;
	msghdr->msg_spot = next;
	segs_needed -= 1;
    }

    /*
     * Copy in the message type
     */

    if ( (eval = copyin(user_msgp,&msghdr->msg_type,sizeof(msghdr->msg_type))) != 0 ) {
#ifdef MSG_DEBUG
	printf("error %d copying the message type\n",eval);
#endif
	msg_freehdr(msghdr);
	msqptr->msg_perm.mode &= ~MSG_LOCKED;
	wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
	return(eval);
    }
    user_msgp += sizeof(msghdr->msg_type);

    /*
     * Validate the message type
     */

    if ( msghdr->msg_type < 1 ) {
	msg_freehdr(msghdr);
	msqptr->msg_perm.mode &= ~MSG_LOCKED;
	wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
#ifdef MSG_DEBUG
	printf("mtype (%d) < 1\n",msghdr->msg_type);
#endif
	return(EINVAL);
    }

    /*
     * Copy in the message body
     */

    next = msghdr->msg_spot;
    while ( msgsz > 0 ) {
	size_t tlen;
	if ( msgsz > msginfo.msgssz ) {
	    tlen = msginfo.msgssz;
	} else {
	    tlen = msgsz;
	}
	if ( next <= -1 ) {
	    panic("next too low #2");
	}
	if ( next >= msginfo.msgseg ) {
	    panic("next out of range #2");
	}
	if ( (eval = copyin(user_msgp, &msgpool[next * msginfo.msgssz], tlen)) != 0 ) {
#ifdef MSG_DEBUG
	    printf("error %d copying in message segment\n",eval);
#endif
	    msg_freehdr(msghdr);
	    msqptr->msg_perm.mode &= ~MSG_LOCKED;
	    wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
	    return(eval);
	}
	msgsz -= tlen;
	user_msgp += tlen;
	next = msgmaps[next].next;
    }
    if ( next != -1 ) {
	panic("didn't use all the msg segments");
    }

    /*
     * We've got the message.  Unlock the msqid_ds.
     */

    msqptr->msg_perm.mode &= ~MSG_LOCKED;

    /*
     * Make sure that the msqid_ds is still allocated.
     */

    if ( msqptr->msg_qbytes == 0 ) {
	msg_freehdr(msghdr);
	wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
	/* The SVID says to return EIDRM. */
#ifdef EIDRM
	return(EIDRM);
#else
	/* Unfortunately, BSD doesn't define that code (yet)! */
	return(EINVAL);
#endif
    }

    /*
     * Put the message into the queue
     */

    if ( msqptr->msg_first == NULL ) {
	msqptr->msg_first = msghdr;
	msqptr->msg_last = msghdr;
    } else {
	msqptr->msg_last->msg_next = msghdr;
	msqptr->msg_last = msghdr;
    }
    msqptr->msg_last->msg_next = NULL;

    msqptr->msg_cbytes += msghdr->msg_ts;
    msqptr->msg_qnum += 1;
    msqptr->msg_lspid = p->p_pid;
    msqptr->msg_stime = time.tv_sec;

    wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
    *retval = 0;
    return(0);
}

struct msgrcv_args {
	int	msqid;
	void	*msgp;
	size_t	msgsz;
	long	msgtyp;
	int	msgflg;
};

int
msgrcv(p, uap, retval)
	struct proc *p;
	register struct msgrcv_args *uap;
	int *retval;
{
    int msqid = uap->msqid;
    void *user_msgp = uap->msgp;
    size_t msgsz = uap->msgsz;
    long msgtyp = uap->msgtyp;
    int msgflg = uap->msgflg;
    size_t len;
    struct ucred *cred = p->p_ucred;
    register struct msqid_ds *msqptr;
    register struct msg *msghdr;
    int eval;
    short next;

#ifdef MSG_DEBUG
    printf("call to msgrcv(%d,0x%x,%d,%ld,%d)\n",msqid,user_msgp,msgsz,msgtyp,msgflg);
#endif

    msqid = IPCID_TO_IX(msqid);

    if ( msqid < 0 || msqid >= msginfo.msgmni ) {
#ifdef MSG_DEBUG
	printf("msqid (%d) out of range (0<=msqid<%d)\n",msqid,msginfo.msgmni);
#endif
	return(EINVAL);
    }

    msqptr = &msqids[msqid];
    if ( msqptr->msg_qbytes == 0 ) {
#ifdef MSG_DEBUG
	printf("no such message queue id\n");
#endif
	return(EINVAL);
    }
    if ( msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid) ) {
#ifdef MSG_DEBUG
	printf("wrong sequence number\n");
#endif
	return(EINVAL);
    }

    if ( (eval = ipcaccess(&msqptr->msg_perm, IPC_R, cred)) ) {
#ifdef MSG_DEBUG
	printf("requester doesn't have read access\n");
#endif
	return(eval);
    }

    msghdr = NULL;
    while ( msghdr == NULL ) {

	if ( msgtyp == 0 ) {

	    msghdr = msqptr->msg_first;
	    if ( msghdr != NULL ) {
		if ( msgsz < msghdr->msg_ts && (msgflg & MSG_NOERROR) == 0 ) {
#ifdef MSG_DEBUG
		    printf("first message on the queue is too big (want %d, got %d)\n",msgsz,msghdr->msg_ts);
#endif
		    return(E2BIG);
		}
		if ( msqptr->msg_first == msqptr->msg_last ) {
		    msqptr->msg_first = NULL;
		    msqptr->msg_last = NULL;
		} else {
		    msqptr->msg_first = msghdr->msg_next;
		    if ( msqptr->msg_first == NULL ) {
			panic("msg_first/last screwed up #1");
		    }
		}
	    }

	} else {
	    struct msg *previous;
	    struct msg **prev;

	    previous = NULL;
	    prev = &(msqptr->msg_first);
	    while ( (msghdr = *prev) != NULL ) {

		/*
		 * Is this message's type an exact match or is this message's
		 * type less than or equal to the absolute value of a negative msgtyp?
		 * Note that the second half of this test can NEVER be true
		 * if msgtyp is positive since msg_type is always positive!
		 */

		if ( msgtyp == msghdr->msg_type || msghdr->msg_type <= -msgtyp ) {
#ifdef MSG_DEBUG
		    printf("found message type %d, requested %d\n",msghdr->msg_type,msgtyp);
#endif
		    if ( msgsz < msghdr->msg_ts && (msgflg & MSG_NOERROR) == 0 ) {
#ifdef MSG_DEBUG
			printf("requested message on the queue is too big (want %d, got %d)\n",msgsz,msghdr->msg_ts);
#endif
			return(E2BIG);
		    }
		    *prev = msghdr->msg_next;
		    if ( msghdr == msqptr->msg_last ) {
			if ( previous == NULL ) {
			    if ( prev != &msqptr->msg_first ) {
				panic("msg_first/last screwed up #2");
			    }
			    msqptr->msg_first = NULL;
			    msqptr->msg_last = NULL;
			} else {
			    if ( prev == &msqptr->msg_first ) {
				panic("msg_first/last screwed up #3");
			    }
			    msqptr->msg_last = previous;
			}
		    }
		    break;
		}
		previous = msghdr;
		prev = &(msghdr->msg_next);
	    }

	}

	/*
	 * We've either extracted the msghdr for the appropriate message
	 * or there isn't one.
	 * If there is one then bail out of this loop.
	 */

	if ( msghdr != NULL ) {
	    break;
	}

	/*
	 * Hmph!  No message found.  Does the user want to wait?
	 */

	if ( (msgflg & IPC_NOWAIT) != 0 ) {
#ifdef MSG_DEBUG
	    printf("no appropriate message found (msgtyp=%d)\n",msgtyp);
#endif
	    /* The SVID says to return ENOMSG. */
#ifdef ENOMSG
	    return(ENOMSG);
#else
	    /* Unfortunately, BSD doesn't define that code (yet)! */
	    return(EAGAIN);
#endif
	}

	/*
	 * Wait for something to happen
	 */

#ifdef MSG_DEBUG
	printf("msgrcv:  goodnight\n");
#endif
	eval = tsleep( (caddr_t)msqptr, (PZERO - 4) | PCATCH, "msg wait", 0 );
#ifdef MSG_DEBUG
	printf("msgrcv:  good morning (eval=%d)\n",eval);
#endif

	if ( eval != 0 ) {
#ifdef MSG_DEBUG
	    printf("msgsnd:  interrupted system call\n");
#endif
	    return( EINTR );
	}

	/*
	 * Make sure that the msq queue still exists
	 */

	if ( msqptr->msg_qbytes == 0
	|| msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid) ) {
#ifdef MSG_DEBUG
	    printf("msqid deleted\n");
#endif
	    /* The SVID says to return EIDRM. */
#ifdef EIDRM
	    return(EIDRM);
#else
	    /* Unfortunately, BSD doesn't define that code (yet)! */
	    return(EINVAL);
#endif
	}

    }

    /*
     * Return the message to the user.
     *
     * First, do the bookkeeping (before we risk being interrupted).
     */

    msqptr->msg_cbytes -= msghdr->msg_ts;
    msqptr->msg_qnum -= 1;
    msqptr->msg_lrpid = p->p_pid;
    msqptr->msg_rtime = time.tv_sec;

    /*
     * Make msgsz the actual amount that we'll be returning.
     * Note that this effectively truncates the message if it is too long
     * (since msgsz is never increased).
     */

#ifdef MSG_DEBUG
    printf("found a message, msgsz=%d, msg_ts=%d\n",msgsz,msghdr->msg_ts);
#endif
    if ( msgsz > msghdr->msg_ts ) {
	msgsz = msghdr->msg_ts;
    }

    /*
     * Return the type to the user.
     */

    eval = copyout((caddr_t)&(msghdr->msg_type), user_msgp, sizeof(msghdr->msg_type));
    if ( eval != 0 ) {
#ifdef MSG_DEBUG
	printf("error (%d) copying out message type\n",eval);
#endif
	msg_freehdr(msghdr);
	wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
	return(eval);
    }
    user_msgp += sizeof(msghdr->msg_type);

    /*
     * Return the segments to the user
     */

    next = msghdr->msg_spot;
    for ( len = 0; len < msgsz; len += msginfo.msgssz ) {
	size_t tlen;
	if ( msgsz > msginfo.msgssz ) {
	    tlen = msginfo.msgssz;
	} else {
	    tlen = msgsz;
	}
	if ( next <= -1 ) {
	    panic("next too low #3");
	}
	if ( next >= msginfo.msgseg ) {
	    panic("next out of range #3");
	}
	eval = copyout((caddr_t)&msgpool[next * msginfo.msgssz], user_msgp, tlen);
	if ( eval != 0 ) {
#ifdef MSG_DEBUG
	    printf("error (%d) copying out message segment\n",eval);
#endif
	    msg_freehdr(msghdr);
	    wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
	    return(eval);
	}
	user_msgp += tlen;
	next = msgmaps[next].next;
    }

    /*
     * Done, return the actual number of bytes copied out.
     */

    msg_freehdr(msghdr);
    wakeup( (caddr_t)msqptr );		/* Somebody might care - we should check! */
    *retval = msgsz;
    return(0);
}
#endif
