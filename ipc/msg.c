/*
 * linux/ipc/msg.c
 * Copyright (C) 1992 Krishna Balasubramanian 
 *
 * Removed all the remaining kerneld mess
 * Catch the -EFAULT stuff properly
 * Use GFP_KERNEL for messages as in 1.2
 * Fixed up the unchecked user space derefs
 * Copyright (C) 1998 Alan Cox & Andi Kleen
 *
 * /proc/sysvipc/msg support (c) 1999 Dragos Acostachioaie <dragos@iname.com>
 *
 * mostly rewritten, threaded and wake-one semantics added
 * MSGMAX limit removed, sysctl's added
 * (c) 1999 Manfred Spraul <manfreds@colorfullife.com>
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/msg.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include "util.h"

/* sysctl: */
int msg_ctlmax = MSGMAX;
int msg_ctlmnb = MSGMNB;
int msg_ctlmni = MSGMNI;

/* one msg_receiver structure for each sleeping receiver */
struct msg_receiver {
	struct list_head r_list;
	struct task_struct* r_tsk;

	int r_mode;
	long r_msgtype;
	long r_maxsize;

	struct msg_msg* volatile r_msg;
};

/* one msg_sender for each sleeping sender */
struct msg_sender {
	struct list_head list;
	struct task_struct* tsk;
};

struct msg_msgseg {
	struct msg_msgseg* next;
	/* the next part of the message follows immediately */
};
/* one msg_msg structure for each message */
struct msg_msg {
	struct list_head m_list; 
	long  m_type;          
	int m_ts;           /* message text size */
	struct msg_msgseg* next;
	/* the actual message follows immediately */
};

#define DATALEN_MSG	(PAGE_SIZE-sizeof(struct msg_msg))
#define DATALEN_SEG	(PAGE_SIZE-sizeof(struct msg_msgseg))

/* one msq_queue structure for each present queue on the system */
struct msg_queue {
	struct kern_ipc_perm q_perm;
	time_t q_stime;			/* last msgsnd time */
	time_t q_rtime;			/* last msgrcv time */
	time_t q_ctime;			/* last change time */
	unsigned long q_cbytes;		/* current number of bytes on queue */
	unsigned long q_qnum;		/* number of messages in queue */
	unsigned long q_qbytes;		/* max number of bytes on queue */
	pid_t q_lspid;			/* pid of last msgsnd */
	pid_t q_lrpid;			/* last receive pid */

	struct list_head q_messages;
	struct list_head q_receivers;
	struct list_head q_senders;
};

#define SEARCH_ANY		1
#define SEARCH_EQUAL		2
#define SEARCH_NOTEQUAL		3
#define SEARCH_LESSEQUAL	4

static atomic_t msg_bytes = ATOMIC_INIT(0);
static atomic_t msg_hdrs = ATOMIC_INIT(0);

static struct ipc_ids msg_ids;

#define msg_lock(id)	((struct msg_queue*)ipc_lock(&msg_ids,id))
#define msg_unlock(id)	ipc_unlock(&msg_ids,id)
#define msg_rmid(id)	((struct msg_queue*)ipc_rmid(&msg_ids,id))
#define msg_checkid(msq, msgid)	\
	ipc_checkid(&msg_ids,&msq->q_perm,msgid)
#define msg_buildid(id, seq) \
	ipc_buildid(&msg_ids, id, seq)

static void freeque (int id);
static int newque (key_t key, int msgflg);
#ifdef CONFIG_PROC_FS
static int sysvipc_msg_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data);
#endif

void __init msg_init (void)
{
	ipc_init_ids(&msg_ids,msg_ctlmni);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("sysvipc/msg", 0, 0, sysvipc_msg_read_proc, NULL);
#endif
}

static int newque (key_t key, int msgflg)
{
	int id;
	struct msg_queue *msq;

	msq  = (struct msg_queue *) kmalloc (sizeof (*msq), GFP_KERNEL);
	if (!msq) 
		return -ENOMEM;
	id = ipc_addid(&msg_ids, &msq->q_perm, msg_ctlmni);
	if(id == -1) {
		kfree(msq);
		return -ENOSPC;
	}
	msq->q_perm.mode = (msgflg & S_IRWXUGO);
	msq->q_perm.key = key;

	msq->q_stime = msq->q_rtime = 0;
	msq->q_ctime = CURRENT_TIME;
	msq->q_cbytes = msq->q_qnum = 0;
	msq->q_qbytes = msg_ctlmnb;
	msq->q_lspid = msq->q_lrpid = 0;
	INIT_LIST_HEAD(&msq->q_messages);
	INIT_LIST_HEAD(&msq->q_receivers);
	INIT_LIST_HEAD(&msq->q_senders);
	msg_unlock(id);

	return msg_buildid(id,msq->q_perm.seq);
}

static void free_msg(struct msg_msg* msg)
{
	struct msg_msgseg* seg;
	seg = msg->next;
	kfree(msg);
	while(seg != NULL) {
		struct msg_msgseg* tmp = seg->next;
		kfree(seg);
		seg = tmp;
	}
}

static struct msg_msg* load_msg(void* src, int len)
{
	struct msg_msg* msg;
	struct msg_msgseg** pseg;
	int err;
	int alen;

	alen = len;
	if(alen > DATALEN_MSG)
		alen = DATALEN_MSG;

	msg = (struct msg_msg *) kmalloc (sizeof(*msg) + alen, GFP_KERNEL);
	if(msg==NULL)
		return ERR_PTR(-ENOMEM);

	msg->next = NULL;

	if (copy_from_user(msg+1, src, alen)) {
		err = -EFAULT;
		goto out_err;
	}

	len -= alen;
	src = ((char*)src)+alen;
	pseg = &msg->next;
	while(len > 0) {
		struct msg_msgseg* seg;
		alen = len;
		if(alen > DATALEN_SEG)
			alen = DATALEN_SEG;
		seg = (struct msg_msgseg *) kmalloc (sizeof(*seg) + alen, GFP_KERNEL);
		if(seg==NULL) {
			err=-ENOMEM;
			goto out_err;
		}
		*pseg = seg;
		seg->next = NULL;
		if(copy_from_user (seg+1, src, alen)) {
			err = -EFAULT;
			goto out_err;
		}
		pseg = &seg->next;
		len -= alen;
		src = ((char*)src)+alen;
	}
	return msg;

out_err:
	free_msg(msg);
	return ERR_PTR(err);
}

static int store_msg(void* dest, struct msg_msg* msg, int len)
{
	int alen;
	struct msg_msgseg *seg;

	alen = len;
	if(alen > DATALEN_MSG)
		alen = DATALEN_MSG;
	if(copy_to_user (dest, msg+1, alen))
		return -1;

	len -= alen;
	dest = ((char*)dest)+alen;
	seg = msg->next;
	while(len > 0) {
		alen = len;
		if(alen > DATALEN_SEG)
			alen = DATALEN_SEG;
		if(copy_to_user (dest, seg+1, alen))
			return -1;
		len -= alen;
		dest = ((char*)dest)+alen;
		seg=seg->next;
	}
	return 0;
}

static inline void ss_add(struct msg_queue* msq, struct msg_sender* mss)
{
	mss->tsk=current;
	current->state=TASK_INTERRUPTIBLE;
	list_add_tail(&mss->list,&msq->q_senders);
}

static inline void ss_del(struct msg_sender* mss)
{
	if(mss->list.next != NULL)
		list_del(&mss->list);
}

static void ss_wakeup(struct list_head* h, int kill)
{
	struct list_head *tmp;

	tmp = h->next;
	while (tmp != h) {
		struct msg_sender* mss;
		
		mss = list_entry(tmp,struct msg_sender,list);
		tmp = tmp->next;
		if(kill)
			mss->list.next=NULL;
		wake_up_process(mss->tsk);
	}
}

static void expunge_all(struct msg_queue* msq, int res)
{
	struct list_head *tmp;

	tmp = msq->q_receivers.next;
	while (tmp != &msq->q_receivers) {
		struct msg_receiver* msr;
		
		msr = list_entry(tmp,struct msg_receiver,r_list);
		tmp = tmp->next;
		msr->r_msg = ERR_PTR(res);
		wake_up_process(msr->r_tsk);
	}
}

static void freeque (int id)
{
	struct msg_queue *msq;
	struct list_head *tmp;

	msq = msg_rmid(id);

	expunge_all(msq,-EIDRM);
	ss_wakeup(&msq->q_senders,1);
	msg_unlock(id);
		
	tmp = msq->q_messages.next;
	while(tmp != &msq->q_messages) {
		struct msg_msg* msg = list_entry(tmp,struct msg_msg,m_list);
		tmp = tmp->next;
		atomic_dec(&msg_hdrs);
		free_msg(msg);
	}
	atomic_sub(msq->q_cbytes, &msg_bytes);
	kfree(msq);
}

asmlinkage long sys_msgget (key_t key, int msgflg)
{
	int id, ret = -EPERM;
	struct msg_queue *msq;
	
	down(&msg_ids.sem);
	if (key == IPC_PRIVATE) 
		ret = newque(key, msgflg);
	else if ((id = ipc_findkey(&msg_ids, key)) == -1) { /* key not used */
		if (!(msgflg & IPC_CREAT))
			ret = -ENOENT;
		else
			ret = newque(key, msgflg);
	} else if (msgflg & IPC_CREAT && msgflg & IPC_EXCL) {
		ret = -EEXIST;
	} else {
		msq = msg_lock(id);
		if(msq==NULL)
			BUG();
		if (ipcperms(&msq->q_perm, msgflg))
			ret = -EACCES;
		else
			ret = msg_buildid(id, msq->q_perm.seq);
		msg_unlock(id);
	}
	up(&msg_ids.sem);
	return ret;
}

static inline unsigned long copy_msqid_to_user(void *buf, struct msqid64_ds *in, int version)
{
	switch(version) {
	case IPC_64:
		return copy_to_user (buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct msqid_ds out;

		memset(&out,0,sizeof(out));

		ipc64_perm_to_ipc_perm(&in->msg_perm, &out.msg_perm);

		out.msg_stime		= in->msg_stime;
		out.msg_rtime		= in->msg_rtime;
		out.msg_ctime		= in->msg_ctime;

		if(in->msg_cbytes > USHRT_MAX)
			out.msg_cbytes	= USHRT_MAX;
		else
			out.msg_cbytes	= in->msg_cbytes;
		out.msg_lcbytes		= in->msg_cbytes;

		if(in->msg_qnum > USHRT_MAX)
			out.msg_qnum	= USHRT_MAX;
		else
			out.msg_qnum	= in->msg_qnum;

		if(in->msg_qbytes > USHRT_MAX)
			out.msg_qbytes	= USHRT_MAX;
		else
			out.msg_qbytes	= in->msg_qbytes;
		out.msg_lqbytes		= in->msg_qbytes;

		out.msg_lspid		= in->msg_lspid;
		out.msg_lrpid		= in->msg_lrpid;

		return copy_to_user (buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

struct msq_setbuf {
	unsigned long	qbytes;
	uid_t		uid;
	gid_t		gid;
	mode_t		mode;
};

static inline unsigned long copy_msqid_from_user(struct msq_setbuf *out, void *buf, int version)
{
	switch(version) {
	case IPC_64:
	    {
		struct msqid64_ds tbuf;

		if (copy_from_user (&tbuf, buf, sizeof (tbuf)))
			return -EFAULT;

		out->qbytes		= tbuf.msg_qbytes;
		out->uid		= tbuf.msg_perm.uid;
		out->gid		= tbuf.msg_perm.gid;
		out->mode		= tbuf.msg_perm.mode;

		return 0;
	    }
	case IPC_OLD:
	    {
		struct msqid_ds tbuf_old;

		if (copy_from_user (&tbuf_old, buf, sizeof (tbuf_old)))
			return -EFAULT;

		out->uid		= tbuf_old.msg_perm.uid;
		out->gid		= tbuf_old.msg_perm.gid;
		out->mode		= tbuf_old.msg_perm.mode;

		if(tbuf_old.msg_qbytes == 0)
			out->qbytes	= tbuf_old.msg_lqbytes;
		else
			out->qbytes	= tbuf_old.msg_qbytes;

		return 0;
	    }
	default:
		return -EINVAL;
	}
}

asmlinkage long sys_msgctl (int msqid, int cmd, struct msqid_ds *buf)
{
	int err, version;
	struct msg_queue *msq;
	struct msq_setbuf setbuf;
	struct kern_ipc_perm *ipcp;
	
	if (msqid < 0 || cmd < 0)
		return -EINVAL;

	version = ipc_parse_version(&cmd);

	switch (cmd) {
	case IPC_INFO: 
	case MSG_INFO: 
	{ 
		struct msginfo msginfo;
		int max_id;
		if (!buf)
			return -EFAULT;
		/* We must not return kernel stack data.
		 * due to padding, it's not enough
		 * to set all member fields.
		 */
		memset(&msginfo,0,sizeof(msginfo));	
		msginfo.msgmni = msg_ctlmni;
		msginfo.msgmax = msg_ctlmax;
		msginfo.msgmnb = msg_ctlmnb;
		msginfo.msgssz = MSGSSZ;
		msginfo.msgseg = MSGSEG;
		down(&msg_ids.sem);
		if (cmd == MSG_INFO) {
			msginfo.msgpool = msg_ids.in_use;
			msginfo.msgmap = atomic_read(&msg_hdrs);
			msginfo.msgtql = atomic_read(&msg_bytes);
		} else {
			msginfo.msgmap = MSGMAP;
			msginfo.msgpool = MSGPOOL;
			msginfo.msgtql = MSGTQL;
		}
		max_id = msg_ids.max_id;
		up(&msg_ids.sem);
		if (copy_to_user (buf, &msginfo, sizeof(struct msginfo)))
			return -EFAULT;
		return (max_id < 0) ? 0: max_id;
	}
	case MSG_STAT:
	case IPC_STAT:
	{
		struct msqid64_ds tbuf;
		int success_return;
		if (!buf)
			return -EFAULT;
		if(cmd == MSG_STAT && msqid >= msg_ids.size)
			return -EINVAL;

		memset(&tbuf,0,sizeof(tbuf));

		msq = msg_lock(msqid);
		if (msq == NULL)
			return -EINVAL;

		if(cmd == MSG_STAT) {
			success_return = msg_buildid(msqid, msq->q_perm.seq);
		} else {
			err = -EIDRM;
			if (msg_checkid(msq,msqid))
				goto out_unlock;
			success_return = 0;
		}
		err = -EACCES;
		if (ipcperms (&msq->q_perm, S_IRUGO))
			goto out_unlock;

		kernel_to_ipc64_perm(&msq->q_perm, &tbuf.msg_perm);
		tbuf.msg_stime  = msq->q_stime;
		tbuf.msg_rtime  = msq->q_rtime;
		tbuf.msg_ctime  = msq->q_ctime;
		tbuf.msg_cbytes = msq->q_cbytes;
		tbuf.msg_qnum   = msq->q_qnum;
		tbuf.msg_qbytes = msq->q_qbytes;
		tbuf.msg_lspid  = msq->q_lspid;
		tbuf.msg_lrpid  = msq->q_lrpid;
		msg_unlock(msqid);
		if (copy_msqid_to_user(buf, &tbuf, version))
			return -EFAULT;
		return success_return;
	}
	case IPC_SET:
		if (!buf)
			return -EFAULT;
		if (copy_msqid_from_user (&setbuf, buf, version))
			return -EFAULT;
		break;
	case IPC_RMID:
		break;
	default:
		return  -EINVAL;
	}

	down(&msg_ids.sem);
	msq = msg_lock(msqid);
	err=-EINVAL;
	if (msq == NULL)
		goto out_up;

	err = -EIDRM;
	if (msg_checkid(msq,msqid))
		goto out_unlock_up;
	ipcp = &msq->q_perm;
	err = -EPERM;
	if (current->euid != ipcp->cuid && 
	    current->euid != ipcp->uid && !capable(CAP_SYS_ADMIN))
	    /* We _could_ check for CAP_CHOWN above, but we don't */
		goto out_unlock_up;

	switch (cmd) {
	case IPC_SET:
	{
		if (setbuf.qbytes > msg_ctlmnb && !capable(CAP_SYS_RESOURCE))
			goto out_unlock_up;
		msq->q_qbytes = setbuf.qbytes;

		ipcp->uid = setbuf.uid;
		ipcp->gid = setbuf.gid;
		ipcp->mode = (ipcp->mode & ~S_IRWXUGO) | 
			(S_IRWXUGO & setbuf.mode);
		msq->q_ctime = CURRENT_TIME;
		/* sleeping receivers might be excluded by
		 * stricter permissions.
		 */
		expunge_all(msq,-EAGAIN);
		/* sleeping senders might be able to send
		 * due to a larger queue size.
		 */
		ss_wakeup(&msq->q_senders,0);
		msg_unlock(msqid);
		break;
	}
	case IPC_RMID:
		freeque (msqid); 
		break;
	}
	err = 0;
out_up:
	up(&msg_ids.sem);
	return err;
out_unlock_up:
	msg_unlock(msqid);
	goto out_up;
out_unlock:
	msg_unlock(msqid);
	return err;
}

static int testmsg(struct msg_msg* msg,long type,int mode)
{
	switch(mode)
	{
		case SEARCH_ANY:
			return 1;
		case SEARCH_LESSEQUAL:
			if(msg->m_type <=type)
				return 1;
			break;
		case SEARCH_EQUAL:
			if(msg->m_type == type)
				return 1;
			break;
		case SEARCH_NOTEQUAL:
			if(msg->m_type != type)
				return 1;
			break;
	}
	return 0;
}

static int inline pipelined_send(struct msg_queue* msq, struct msg_msg* msg)
{
	struct list_head* tmp;

	tmp = msq->q_receivers.next;
	while (tmp != &msq->q_receivers) {
		struct msg_receiver* msr;
		msr = list_entry(tmp,struct msg_receiver,r_list);
		tmp = tmp->next;
		if(testmsg(msg,msr->r_msgtype,msr->r_mode)) {
			list_del(&msr->r_list);
			if(msr->r_maxsize < msg->m_ts) {
				msr->r_msg = ERR_PTR(-E2BIG);
				wake_up_process(msr->r_tsk);
			} else {
				msr->r_msg = msg;
				msq->q_lrpid = msr->r_tsk->pid;
				msq->q_rtime = CURRENT_TIME;
				wake_up_process(msr->r_tsk);
				return 1;
			}
		}
	}
	return 0;
}

asmlinkage long sys_msgsnd (int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
	struct msg_queue *msq;
	struct msg_msg *msg;
	long mtype;
	int err;
	
	if (msgsz > msg_ctlmax || (long) msgsz < 0 || msqid < 0)
		return -EINVAL;
	if (get_user(mtype, &msgp->mtype))
		return -EFAULT; 
	if (mtype < 1)
		return -EINVAL;

	msg = load_msg(msgp->mtext, msgsz);
	if(IS_ERR(msg))
		return PTR_ERR(msg);

	msg->m_type = mtype;
	msg->m_ts = msgsz;

	msq = msg_lock(msqid);
	err=-EINVAL;
	if(msq==NULL)
		goto out_free;
retry:
	err= -EIDRM;
	if (msg_checkid(msq,msqid))
		goto out_unlock_free;

	err=-EACCES;
	if (ipcperms(&msq->q_perm, S_IWUGO)) 
		goto out_unlock_free;

	if(msgsz + msq->q_cbytes > msq->q_qbytes ||
		1 + msq->q_qnum > msq->q_qbytes) {
		struct msg_sender s;

		if(msgflg&IPC_NOWAIT) {
			err=-EAGAIN;
			goto out_unlock_free;
		}
		ss_add(msq, &s);
		msg_unlock(msqid);
		schedule();
		current->state= TASK_RUNNING;

		msq = msg_lock(msqid);
		err = -EIDRM;
		if(msq==NULL)
			goto out_free;
		ss_del(&s);
		
		if (signal_pending(current)) {
			err=-EINTR;
			goto out_unlock_free;
		}
		goto retry;
	}

	msq->q_lspid = current->pid;
	msq->q_stime = CURRENT_TIME;

	if(!pipelined_send(msq,msg)) {
		/* noone is waiting for this message, enqueue it */
		list_add_tail(&msg->m_list,&msq->q_messages);
		msq->q_cbytes += msgsz;
		msq->q_qnum++;
		atomic_add(msgsz,&msg_bytes);
		atomic_inc(&msg_hdrs);
	}
	
	err = 0;
	msg = NULL;

out_unlock_free:
	msg_unlock(msqid);
out_free:
	if(msg!=NULL)
		free_msg(msg);
	return err;
}

static int inline convert_mode(long* msgtyp, int msgflg)
{
	/* 
	 *  find message of correct type.
	 *  msgtyp = 0 => get first.
	 *  msgtyp > 0 => get first message of matching type.
	 *  msgtyp < 0 => get message with least type must be < abs(msgtype).  
	 */
	if(*msgtyp==0)
		return SEARCH_ANY;
	if(*msgtyp<0) {
		*msgtyp=-(*msgtyp);
		return SEARCH_LESSEQUAL;
	}
	if(msgflg & MSG_EXCEPT)
		return SEARCH_NOTEQUAL;
	return SEARCH_EQUAL;
}

asmlinkage long sys_msgrcv (int msqid, struct msgbuf *msgp, size_t msgsz,
			    long msgtyp, int msgflg)
{
	struct msg_queue *msq;
	struct msg_receiver msr_d;
	struct list_head* tmp;
	struct msg_msg* msg, *found_msg;
	int err;
	int mode;

	if (msqid < 0 || (long) msgsz < 0)
		return -EINVAL;
	mode = convert_mode(&msgtyp,msgflg);

	msq = msg_lock(msqid);
	if(msq==NULL)
		return -EINVAL;
retry:
	err = -EIDRM;
	if (msg_checkid(msq,msqid))
		goto out_unlock;

	err=-EACCES;
	if (ipcperms (&msq->q_perm, S_IRUGO))
		goto out_unlock;

	tmp = msq->q_messages.next;
	found_msg=NULL;
	while (tmp != &msq->q_messages) {
		msg = list_entry(tmp,struct msg_msg,m_list);
		if(testmsg(msg,msgtyp,mode)) {
			found_msg = msg;
			if(mode == SEARCH_LESSEQUAL && msg->m_type != 1) {
				found_msg=msg;
				msgtyp=msg->m_type-1;
			} else {
				found_msg=msg;
				break;
			}
		}
		tmp = tmp->next;
	}
	if(found_msg) {
		msg=found_msg;
		if ((msgsz < msg->m_ts) && !(msgflg & MSG_NOERROR)) {
			err=-E2BIG;
			goto out_unlock;
		}
		list_del(&msg->m_list);
		msq->q_qnum--;
		msq->q_rtime = CURRENT_TIME;
		msq->q_lrpid = current->pid;
		msq->q_cbytes -= msg->m_ts;
		atomic_sub(msg->m_ts,&msg_bytes);
		atomic_dec(&msg_hdrs);
		ss_wakeup(&msq->q_senders,0);
		msg_unlock(msqid);
out_success:
		msgsz = (msgsz > msg->m_ts) ? msg->m_ts : msgsz;
		if (put_user (msg->m_type, &msgp->mtype) ||
		    store_msg(msgp->mtext, msg, msgsz)) {
			    msgsz = -EFAULT;
		}
		free_msg(msg);
		return msgsz;
	} else
	{
		struct msg_queue *t;
		/* no message waiting. Prepare for pipelined
		 * receive.
		 */
		if (msgflg & IPC_NOWAIT) {
			err=-ENOMSG;
			goto out_unlock;
		}
		list_add_tail(&msr_d.r_list,&msq->q_receivers);
		msr_d.r_tsk = current;
		msr_d.r_msgtype = msgtyp;
		msr_d.r_mode = mode;
		if(msgflg & MSG_NOERROR)
			msr_d.r_maxsize = INT_MAX;
		 else
		 	msr_d.r_maxsize = msgsz;
		msr_d.r_msg = ERR_PTR(-EAGAIN);
		current->state = TASK_INTERRUPTIBLE;
		msg_unlock(msqid);

		schedule();
		current->state = TASK_RUNNING;

		/* This introduces a race so we must always take
		   the slow path
		msg = (struct msg_msg*) msr_d.r_msg;
		if(!IS_ERR(msg)) 
			goto out_success;
		*/
		t = msg_lock(msqid);
		if(t==NULL)
			msqid=-1;
		msg = (struct msg_msg*)msr_d.r_msg;
		if(!IS_ERR(msg)) {
			/* our message arived while we waited for
			 * the spinlock. Process it.
			 */
			if(msqid!=-1)
				msg_unlock(msqid);
			goto out_success;
		}
		err = PTR_ERR(msg);
		if(err == -EAGAIN) {
			if(msqid==-1)
				BUG();
			list_del(&msr_d.r_list);
			if (signal_pending(current))
				err=-EINTR;
			 else
				goto retry;
		}
	}
out_unlock:
	if(msqid!=-1)
		msg_unlock(msqid);
	return err;
}

#ifdef CONFIG_PROC_FS
static int sysvipc_msg_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	off_t pos = 0;
	off_t begin = 0;
	int i, len = 0;

	down(&msg_ids.sem);
	len += sprintf(buffer, "       key      msqid perms      cbytes       qnum lspid lrpid   uid   gid  cuid  cgid      stime      rtime      ctime\n");

	for(i = 0; i <= msg_ids.max_id; i++) {
		struct msg_queue * msq;
		msq = msg_lock(i);
		if(msq != NULL) {
			len += sprintf(buffer + len, "%10d %10d  %4o  %10lu %10lu %5u %5u %5u %5u %5u %5u %10lu %10lu %10lu\n",
				msq->q_perm.key,
				msg_buildid(i,msq->q_perm.seq),
				msq->q_perm.mode,
				msq->q_cbytes,
				msq->q_qnum,
				msq->q_lspid,
				msq->q_lrpid,
				msq->q_perm.uid,
				msq->q_perm.gid,
				msq->q_perm.cuid,
				msq->q_perm.cgid,
				msq->q_stime,
				msq->q_rtime,
				msq->q_ctime);
			msg_unlock(i);

			pos += len;
			if(pos < offset) {
				len = 0;
				begin = pos;
			}
			if(pos > offset + length)
				goto done;
		}

	}
	*eof = 1;
done:
	up(&msg_ids.sem);
	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if(len > length)
		len = length;
	if(len < 0)
		len = 0;
	return len;
}
#endif
