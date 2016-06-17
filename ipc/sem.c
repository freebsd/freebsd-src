/*
 * linux/ipc/sem.c
 * Copyright (C) 1992 Krishna Balasubramanian
 * Copyright (C) 1995 Eric Schenk, Bruno Haible
 *
 * IMPLEMENTATION NOTES ON CODE REWRITE (Eric Schenk, January 1995):
 * This code underwent a massive rewrite in order to solve some problems
 * with the original code. In particular the original code failed to
 * wake up processes that were waiting for semval to go to 0 if the
 * value went to 0 and was then incremented rapidly enough. In solving
 * this problem I have also modified the implementation so that it
 * processes pending operations in a FIFO manner, thus give a guarantee
 * that processes waiting for a lock on the semaphore won't starve
 * unless another locking process fails to unlock.
 * In addition the following two changes in behavior have been introduced:
 * - The original implementation of semop returned the value
 *   last semaphore element examined on success. This does not
 *   match the manual page specifications, and effectively
 *   allows the user to read the semaphore even if they do not
 *   have read permissions. The implementation now returns 0
 *   on success as stated in the manual page.
 * - There is some confusion over whether the set of undo adjustments
 *   to be performed at exit should be done in an atomic manner.
 *   That is, if we are attempting to decrement the semval should we queue
 *   up and wait until we can do so legally?
 *   The original implementation attempted to do this.
 *   The current implementation does not do so. This is because I don't
 *   think it is the right thing (TM) to do, and because I couldn't
 *   see a clean way to get the old behavior with the new design.
 *   The POSIX standard and SVID should be consulted to determine
 *   what behavior is mandated.
 *
 * Further notes on refinement (Christoph Rohland, December 1998):
 * - The POSIX standard says, that the undo adjustments simply should
 *   redo. So the current implementation is o.K.
 * - The previous code had two flaws:
 *   1) It actively gave the semaphore to the next waiting process
 *      sleeping on the semaphore. Since this process did not have the
 *      cpu this led to many unnecessary context switches and bad
 *      performance. Now we only check which process should be able to
 *      get the semaphore and if this process wants to reduce some
 *      semaphore value we simply wake it up without doing the
 *      operation. So it has to try to get it later. Thus e.g. the
 *      running process may reacquire the semaphore during the current
 *      time slice. If it only waits for zero or increases the semaphore,
 *      we do the operation in advance and wake it up.
 *   2) It did not wake up all zero waiting processes. We try to do
 *      better but only get the semops right which only wait for zero or
 *      increase. If there are decrement operations in the operations
 *      array we do the same as before.
 *
 * /proc/sysvipc/sem support (c) 1999 Dragos Acostachioaie <dragos@iname.com>
 *
 * SMP-threaded, sysctl's added
 * (c) 1999 Manfred Spraul <manfreds@colorfullife.com>
 * Enforced range limit on SEM_UNDO
 * (c) 2001 Red Hat Inc <alan@redhat.com>
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include "util.h"


#define sem_lock(id)	((struct sem_array*)ipc_lock(&sem_ids,id))
#define sem_unlock(id)	ipc_unlock(&sem_ids,id)
#define sem_rmid(id)	((struct sem_array*)ipc_rmid(&sem_ids,id))
#define sem_checkid(sma, semid)	\
	ipc_checkid(&sem_ids,&sma->sem_perm,semid)
#define sem_buildid(id, seq) \
	ipc_buildid(&sem_ids, id, seq)
static struct ipc_ids sem_ids;

static int newary (key_t, int, int);
static void freeary (int id);
#ifdef CONFIG_PROC_FS
static int sysvipc_sem_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data);
#endif

#define SEMMSL_FAST	256 /* 512 bytes on stack */
#define SEMOPM_FAST	64  /* ~ 372 bytes on stack */

/*
 * linked list protection:
 *	sem_undo.id_next,
 *	sem_array.sem_pending{,last},
 *	sem_array.sem_undo: sem_lock() for read/write
 *	sem_undo.proc_next: only "current" is allowed to read/write that field.
 *	
 */

int sem_ctls[4] = {SEMMSL, SEMMNS, SEMOPM, SEMMNI};
#define sc_semmsl	(sem_ctls[0])
#define sc_semmns	(sem_ctls[1])
#define sc_semopm	(sem_ctls[2])
#define sc_semmni	(sem_ctls[3])

static int used_sems;

void __init sem_init (void)
{
	used_sems = 0;
	ipc_init_ids(&sem_ids,sc_semmni);

#ifdef CONFIG_PROC_FS
	create_proc_read_entry("sysvipc/sem", 0, 0, sysvipc_sem_read_proc, NULL);
#endif
}

static int newary (key_t key, int nsems, int semflg)
{
	int id;
	struct sem_array *sma;
	int size;

	if (!nsems)
		return -EINVAL;
	if (used_sems + nsems > sc_semmns)
		return -ENOSPC;

	size = sizeof (*sma) + nsems * sizeof (struct sem);
	sma = (struct sem_array *) ipc_alloc(size);
	if (!sma) {
		return -ENOMEM;
	}
	memset (sma, 0, size);
	id = ipc_addid(&sem_ids, &sma->sem_perm, sc_semmni);
	if(id == -1) {
		ipc_free(sma, size);
		return -ENOSPC;
	}
	used_sems += nsems;

	sma->sem_perm.mode = (semflg & S_IRWXUGO);
	sma->sem_perm.key = key;

	sma->sem_base = (struct sem *) &sma[1];
	/* sma->sem_pending = NULL; */
	sma->sem_pending_last = &sma->sem_pending;
	/* sma->undo = NULL; */
	sma->sem_nsems = nsems;
	sma->sem_ctime = CURRENT_TIME;
	sem_unlock(id);

	return sem_buildid(id, sma->sem_perm.seq);
}

asmlinkage long sys_semget (key_t key, int nsems, int semflg)
{
	int id, err = -EINVAL;
	struct sem_array *sma;

	if (nsems < 0 || nsems > sc_semmsl)
		return -EINVAL;
	down(&sem_ids.sem);
	
	if (key == IPC_PRIVATE) {
		err = newary(key, nsems, semflg);
	} else if ((id = ipc_findkey(&sem_ids, key)) == -1) {  /* key not used */
		if (!(semflg & IPC_CREAT))
			err = -ENOENT;
		else
			err = newary(key, nsems, semflg);
	} else if (semflg & IPC_CREAT && semflg & IPC_EXCL) {
		err = -EEXIST;
	} else {
		sma = sem_lock(id);
		if(sma==NULL)
			BUG();
		if (nsems > sma->sem_nsems)
			err = -EINVAL;
		else if (ipcperms(&sma->sem_perm, semflg))
			err = -EACCES;
		else
			err = sem_buildid(id, sma->sem_perm.seq);
		sem_unlock(id);
	}

	up(&sem_ids.sem);
	return err;
}

/* doesn't acquire the sem_lock on error! */
static int sem_revalidate(int semid, struct sem_array* sma, int nsems, short flg)
{
	struct sem_array* smanew;

	smanew = sem_lock(semid);
	if(smanew==NULL)
		return -EIDRM;
	if(smanew != sma || sem_checkid(sma,semid) || sma->sem_nsems != nsems) {
		sem_unlock(semid);
		return -EIDRM;
	}

	if (ipcperms(&sma->sem_perm, flg)) {
		sem_unlock(semid);
		return -EACCES;
	}
	return 0;
}
/* Manage the doubly linked list sma->sem_pending as a FIFO:
 * insert new queue elements at the tail sma->sem_pending_last.
 */
static inline void append_to_queue (struct sem_array * sma,
				    struct sem_queue * q)
{
	*(q->prev = sma->sem_pending_last) = q;
	*(sma->sem_pending_last = &q->next) = NULL;
}

static inline void prepend_to_queue (struct sem_array * sma,
				     struct sem_queue * q)
{
	q->next = sma->sem_pending;
	*(q->prev = &sma->sem_pending) = q;
	if (q->next)
		q->next->prev = &q->next;
	else /* sma->sem_pending_last == &sma->sem_pending */
		sma->sem_pending_last = &q->next;
}

static inline void remove_from_queue (struct sem_array * sma,
				      struct sem_queue * q)
{
	*(q->prev) = q->next;
	if (q->next)
		q->next->prev = q->prev;
	else /* sma->sem_pending_last == &q->next */
		sma->sem_pending_last = q->prev;
	q->prev = NULL; /* mark as removed */
}

/*
 * Determine whether a sequence of semaphore operations would succeed
 * all at once. Return 0 if yes, 1 if need to sleep, else return error code.
 */

static int try_atomic_semop (struct sem_array * sma, struct sembuf * sops,
			     int nsops, struct sem_undo *un, int pid,
			     int do_undo)
{
	int result, sem_op;
	struct sembuf *sop;
	struct sem * curr;

	for (sop = sops; sop < sops + nsops; sop++) {
		curr = sma->sem_base + sop->sem_num;
		sem_op = sop->sem_op;
		result = curr->semval;
  
		if (!sem_op && result)
			goto would_block;

		result += sem_op;
		if (result < 0)
			goto would_block;
		if (result > SEMVMX)
			goto out_of_range;
		if (sop->sem_flg & SEM_UNDO) {
			int undo = un->semadj[sop->sem_num] - sem_op;
			/*
	 		 *	Exceeding the undo range is an error.
			 */
			if (undo < (-SEMAEM - 1) || undo > SEMAEM)
				goto out_of_range;
		}
		curr->semval = result;
	}

	if (do_undo) {
		result = 0;
		goto undo;
	}
	sop--;
	while (sop >= sops) {
		sma->sem_base[sop->sem_num].sempid = pid;
		if (sop->sem_flg & SEM_UNDO)
			un->semadj[sop->sem_num] -= sop->sem_op;
		sop--;
	}
	sma->sem_otime = CURRENT_TIME;
	return 0;

out_of_range:
	result = -ERANGE;
	goto undo;

would_block:
	if (sop->sem_flg & IPC_NOWAIT)
		result = -EAGAIN;
	else
		result = 1;

undo:
	sop--;
	while (sop >= sops) {
		sma->sem_base[sop->sem_num].semval -= sop->sem_op;
		sop--;
	}

	return result;
}

/* Go through the pending queue for the indicated semaphore
 * looking for tasks that can be completed.
 */
static void update_queue (struct sem_array * sma)
{
	int error;
	struct sem_queue * q;

	for (q = sma->sem_pending; q; q = q->next) {
			
		if (q->status == 1)
			continue;	/* this one was woken up before */

		error = try_atomic_semop(sma, q->sops, q->nsops,
					 q->undo, q->pid, q->alter);

		/* Does q->sleeper still need to sleep? */
		if (error <= 0) {
				/* Found one, wake it up */
			wake_up_process(q->sleeper);
			if (error == 0 && q->alter) {
				/* if q-> alter let it self try */
				q->status = 1;
				return;
			}
			q->status = error;
			remove_from_queue(sma,q);
		}
	}
}

/* The following counts are associated to each semaphore:
 *   semncnt        number of tasks waiting on semval being nonzero
 *   semzcnt        number of tasks waiting on semval being zero
 * This model assumes that a task waits on exactly one semaphore.
 * Since semaphore operations are to be performed atomically, tasks actually
 * wait on a whole sequence of semaphores simultaneously.
 * The counts we return here are a rough approximation, but still
 * warrant that semncnt+semzcnt>0 if the task is on the pending queue.
 */
static int count_semncnt (struct sem_array * sma, ushort semnum)
{
	int semncnt;
	struct sem_queue * q;

	semncnt = 0;
	for (q = sma->sem_pending; q; q = q->next) {
		struct sembuf * sops = q->sops;
		int nsops = q->nsops;
		int i;
		for (i = 0; i < nsops; i++)
			if (sops[i].sem_num == semnum
			    && (sops[i].sem_op < 0)
			    && !(sops[i].sem_flg & IPC_NOWAIT))
				semncnt++;
	}
	return semncnt;
}
static int count_semzcnt (struct sem_array * sma, ushort semnum)
{
	int semzcnt;
	struct sem_queue * q;

	semzcnt = 0;
	for (q = sma->sem_pending; q; q = q->next) {
		struct sembuf * sops = q->sops;
		int nsops = q->nsops;
		int i;
		for (i = 0; i < nsops; i++)
			if (sops[i].sem_num == semnum
			    && (sops[i].sem_op == 0)
			    && !(sops[i].sem_flg & IPC_NOWAIT))
				semzcnt++;
	}
	return semzcnt;
}

/* Free a semaphore set. */
static void freeary (int id)
{
	struct sem_array *sma;
	struct sem_undo *un;
	struct sem_queue *q;
	int size;

	sma = sem_rmid(id);

	/* Invalidate the existing undo structures for this semaphore set.
	 * (They will be freed without any further action in sem_exit()
	 * or during the next semop.)
	 */
	for (un = sma->undo; un; un = un->id_next)
		un->semid = -1;

	/* Wake up all pending processes and let them fail with EIDRM. */
	for (q = sma->sem_pending; q; q = q->next) {
		q->status = -EIDRM;
		q->prev = NULL;
		wake_up_process(q->sleeper); /* doesn't sleep */
	}
	sem_unlock(id);

	used_sems -= sma->sem_nsems;
	size = sizeof (*sma) + sma->sem_nsems * sizeof (struct sem);
	ipc_free(sma, size);
}

static unsigned long copy_semid_to_user(void *buf, struct semid64_ds *in, int version)
{
	switch(version) {
	case IPC_64:
		return copy_to_user(buf, in, sizeof(*in));
	case IPC_OLD:
	    {
		struct semid_ds out;

		ipc64_perm_to_ipc_perm(&in->sem_perm, &out.sem_perm);

		out.sem_otime	= in->sem_otime;
		out.sem_ctime	= in->sem_ctime;
		out.sem_nsems	= in->sem_nsems;

		return copy_to_user(buf, &out, sizeof(out));
	    }
	default:
		return -EINVAL;
	}
}

static int semctl_nolock(int semid, int semnum, int cmd, int version, union semun arg)
{
	int err = -EINVAL;

	switch(cmd) {
	case IPC_INFO:
	case SEM_INFO:
	{
		struct seminfo seminfo;
		int max_id;

		memset(&seminfo,0,sizeof(seminfo));
		seminfo.semmni = sc_semmni;
		seminfo.semmns = sc_semmns;
		seminfo.semmsl = sc_semmsl;
		seminfo.semopm = sc_semopm;
		seminfo.semvmx = SEMVMX;
		seminfo.semmnu = SEMMNU;
		seminfo.semmap = SEMMAP;
		seminfo.semume = SEMUME;
		down(&sem_ids.sem);
		if (cmd == SEM_INFO) {
			seminfo.semusz = sem_ids.in_use;
			seminfo.semaem = used_sems;
		} else {
			seminfo.semusz = SEMUSZ;
			seminfo.semaem = SEMAEM;
		}
		max_id = sem_ids.max_id;
		up(&sem_ids.sem);
		if (copy_to_user (arg.__buf, &seminfo, sizeof(struct seminfo))) 
			return -EFAULT;
		return (max_id < 0) ? 0: max_id;
	}
	case SEM_STAT:
	{
		struct sem_array *sma;
		struct semid64_ds tbuf;
		int id;

		if(semid >= sem_ids.size)
			return -EINVAL;

		memset(&tbuf,0,sizeof(tbuf));

		sma = sem_lock(semid);
		if(sma == NULL)
			return -EINVAL;

		err = -EACCES;
		if (ipcperms (&sma->sem_perm, S_IRUGO))
			goto out_unlock;
		id = sem_buildid(semid, sma->sem_perm.seq);

		kernel_to_ipc64_perm(&sma->sem_perm, &tbuf.sem_perm);
		tbuf.sem_otime  = sma->sem_otime;
		tbuf.sem_ctime  = sma->sem_ctime;
		tbuf.sem_nsems  = sma->sem_nsems;
		sem_unlock(semid);
		if (copy_semid_to_user (arg.buf, &tbuf, version))
			return -EFAULT;
		return id;
	}
	default:
		return -EINVAL;
	}
	return err;
out_unlock:
	sem_unlock(semid);
	return err;
}

static int semctl_main(int semid, int semnum, int cmd, int version, union semun arg)
{
	struct sem_array *sma;
	struct sem* curr;
	int err;
	ushort fast_sem_io[SEMMSL_FAST];
	ushort* sem_io = fast_sem_io;
	int nsems;

	sma = sem_lock(semid);
	if(sma==NULL)
		return -EINVAL;

	nsems = sma->sem_nsems;

	err=-EIDRM;
	if (sem_checkid(sma,semid))
		goto out_unlock;

	err = -EACCES;
	if (ipcperms (&sma->sem_perm, (cmd==SETVAL||cmd==SETALL)?S_IWUGO:S_IRUGO))
		goto out_unlock;

	switch (cmd) {
	case GETALL:
	{
		ushort *array = arg.array;
		int i;

		if(nsems > SEMMSL_FAST) {
			sem_unlock(semid);			
			sem_io = ipc_alloc(sizeof(ushort)*nsems);
			if(sem_io == NULL)
				return -ENOMEM;
			err = sem_revalidate(semid, sma, nsems, S_IRUGO);
			if(err)
				goto out_free;
		}

		for (i = 0; i < sma->sem_nsems; i++)
			sem_io[i] = sma->sem_base[i].semval;
		sem_unlock(semid);
		err = 0;
		if(copy_to_user(array, sem_io, nsems*sizeof(ushort)))
			err = -EFAULT;
		goto out_free;
	}
	case SETALL:
	{
		int i;
		struct sem_undo *un;

		sem_unlock(semid);

		if(nsems > SEMMSL_FAST) {
			sem_io = ipc_alloc(sizeof(ushort)*nsems);
			if(sem_io == NULL)
				return -ENOMEM;
		}

		if (copy_from_user (sem_io, arg.array, nsems*sizeof(ushort))) {
			err = -EFAULT;
			goto out_free;
		}

		for (i = 0; i < nsems; i++) {
			if (sem_io[i] > SEMVMX) {
				err = -ERANGE;
				goto out_free;
			}
		}
		err = sem_revalidate(semid, sma, nsems, S_IWUGO);
		if(err)
			goto out_free;

		for (i = 0; i < nsems; i++)
			sma->sem_base[i].semval = sem_io[i];
		for (un = sma->undo; un; un = un->id_next)
			for (i = 0; i < nsems; i++)
				un->semadj[i] = 0;
		sma->sem_ctime = CURRENT_TIME;
		/* maybe some queued-up processes were waiting for this */
		update_queue(sma);
		err = 0;
		goto out_unlock;
	}
	case IPC_STAT:
	{
		struct semid64_ds tbuf;
		memset(&tbuf,0,sizeof(tbuf));
		kernel_to_ipc64_perm(&sma->sem_perm, &tbuf.sem_perm);
		tbuf.sem_otime  = sma->sem_otime;
		tbuf.sem_ctime  = sma->sem_ctime;
		tbuf.sem_nsems  = sma->sem_nsems;
		sem_unlock(semid);
		if (copy_semid_to_user (arg.buf, &tbuf, version))
			return -EFAULT;
		return 0;
	}
	/* GETVAL, GETPID, GETNCTN, GETZCNT, SETVAL: fall-through */
	}
	err = -EINVAL;
	if(semnum < 0 || semnum >= nsems)
		goto out_unlock;

	curr = &sma->sem_base[semnum];

	switch (cmd) {
	case GETVAL:
		err = curr->semval;
		goto out_unlock;
	case GETPID:
		err = curr->sempid;
		goto out_unlock;
	case GETNCNT:
		err = count_semncnt(sma,semnum);
		goto out_unlock;
	case GETZCNT:
		err = count_semzcnt(sma,semnum);
		goto out_unlock;
	case SETVAL:
	{
		int val = arg.val;
		struct sem_undo *un;
		err = -ERANGE;
		if (val > SEMVMX || val < 0)
			goto out_unlock;

		for (un = sma->undo; un; un = un->id_next)
			un->semadj[semnum] = 0;
		curr->semval = val;
		curr->sempid = current->pid;
		sma->sem_ctime = CURRENT_TIME;
		/* maybe some queued-up processes were waiting for this */
		update_queue(sma);
		err = 0;
		goto out_unlock;
	}
	}
out_unlock:
	sem_unlock(semid);
out_free:
	if(sem_io != fast_sem_io)
		ipc_free(sem_io, sizeof(ushort)*nsems);
	return err;
}

struct sem_setbuf {
	uid_t	uid;
	gid_t	gid;
	mode_t	mode;
};

static inline unsigned long copy_semid_from_user(struct sem_setbuf *out, void *buf, int version)
{
	switch(version) {
	case IPC_64:
	    {
		struct semid64_ds tbuf;

		if(copy_from_user(&tbuf, buf, sizeof(tbuf)))
			return -EFAULT;

		out->uid	= tbuf.sem_perm.uid;
		out->gid	= tbuf.sem_perm.gid;
		out->mode	= tbuf.sem_perm.mode;

		return 0;
	    }
	case IPC_OLD:
	    {
		struct semid_ds tbuf_old;

		if(copy_from_user(&tbuf_old, buf, sizeof(tbuf_old)))
			return -EFAULT;

		out->uid	= tbuf_old.sem_perm.uid;
		out->gid	= tbuf_old.sem_perm.gid;
		out->mode	= tbuf_old.sem_perm.mode;

		return 0;
	    }
	default:
		return -EINVAL;
	}
}

static int semctl_down(int semid, int semnum, int cmd, int version, union semun arg)
{
	struct sem_array *sma;
	int err;
	struct sem_setbuf setbuf;
	struct kern_ipc_perm *ipcp;

	if(cmd == IPC_SET) {
		if(copy_semid_from_user (&setbuf, arg.buf, version))
			return -EFAULT;
	}
	sma = sem_lock(semid);
	if(sma==NULL)
		return -EINVAL;

	if (sem_checkid(sma,semid)) {
		err=-EIDRM;
		goto out_unlock;
	}	
	ipcp = &sma->sem_perm;
	
	if (current->euid != ipcp->cuid && 
	    current->euid != ipcp->uid && !capable(CAP_SYS_ADMIN)) {
	    	err=-EPERM;
		goto out_unlock;
	}

	switch(cmd){
	case IPC_RMID:
		freeary(semid);
		err = 0;
		break;
	case IPC_SET:
		ipcp->uid = setbuf.uid;
		ipcp->gid = setbuf.gid;
		ipcp->mode = (ipcp->mode & ~S_IRWXUGO)
				| (setbuf.mode & S_IRWXUGO);
		sma->sem_ctime = CURRENT_TIME;
		sem_unlock(semid);
		err = 0;
		break;
	default:
		sem_unlock(semid);
		err = -EINVAL;
		break;
	}
	return err;

out_unlock:
	sem_unlock(semid);
	return err;
}

asmlinkage long sys_semctl (int semid, int semnum, int cmd, union semun arg)
{
	int err = -EINVAL;
	int version;

	if (semid < 0)
		return -EINVAL;

	version = ipc_parse_version(&cmd);

	switch(cmd) {
	case IPC_INFO:
	case SEM_INFO:
	case SEM_STAT:
		err = semctl_nolock(semid,semnum,cmd,version,arg);
		return err;
	case GETALL:
	case GETVAL:
	case GETPID:
	case GETNCNT:
	case GETZCNT:
	case IPC_STAT:
	case SETVAL:
	case SETALL:
		err = semctl_main(semid,semnum,cmd,version,arg);
		return err;
	case IPC_RMID:
	case IPC_SET:
		down(&sem_ids.sem);
		err = semctl_down(semid,semnum,cmd,version,arg);
		up(&sem_ids.sem);
		return err;
	default:
		return -EINVAL;
	}
}

static struct sem_undo* freeundos(struct sem_array *sma, struct sem_undo* un)
{
	struct sem_undo* u;
	struct sem_undo** up;

	for(up = &current->semundo;(u=*up);up=&u->proc_next) {
		if(un==u) {
			un=u->proc_next;
			*up=un;
			kfree(u);
			return un;
		}
	}
	printk ("freeundos undo list error id=%d\n", un->semid);
	return un->proc_next;
}

/* returns without sem_lock on error! */
static int alloc_undo(struct sem_array *sma, struct sem_undo** unp, int semid, int alter)
{
	int size, nsems, error;
	struct sem_undo *un;

	nsems = sma->sem_nsems;
	size = sizeof(struct sem_undo) + sizeof(short)*nsems;
	sem_unlock(semid);

	un = (struct sem_undo *) kmalloc(size, GFP_KERNEL);
	if (!un)
		return -ENOMEM;

	memset(un, 0, size);
	error = sem_revalidate(semid, sma, nsems, alter ? S_IWUGO : S_IRUGO);
	if(error) {
		kfree(un);
		return error;
	}

	un->semadj = (short *) &un[1];
	un->semid = semid;
	un->proc_next = current->semundo;
	current->semundo = un;
	un->id_next = sma->undo;
	sma->undo = un;
	*unp = un;
	return 0;
}

asmlinkage long sys_semop (int semid, struct sembuf *tsops, unsigned nsops)
{
	return sys_semtimedop(semid, tsops, nsops, NULL);
}

asmlinkage long sys_semtimedop (int semid, struct sembuf *tsops,
			unsigned nsops, const struct timespec *timeout)
{
	int error = -EINVAL;
	struct sem_array *sma;
	struct sembuf fast_sops[SEMOPM_FAST];
	struct sembuf* sops = fast_sops, *sop;
	struct sem_undo *un;
	int undos = 0, decrease = 0, alter = 0;
	struct sem_queue queue;
	unsigned long jiffies_left = 0;

	if (nsops < 1 || semid < 0)
		return -EINVAL;
	if (nsops > sc_semopm)
		return -E2BIG;
	if(nsops > SEMOPM_FAST) {
		sops = kmalloc(sizeof(*sops)*nsops,GFP_KERNEL);
		if(sops==NULL)
			return -ENOMEM;
	}
	if (copy_from_user (sops, tsops, nsops * sizeof(*tsops))) {
		error=-EFAULT;
		goto out_free;
	}
	if (timeout) {
		struct timespec _timeout;
		if (copy_from_user(&_timeout, timeout, sizeof(*timeout))) {
			error = -EFAULT;
			goto out_free;
		}
		if (_timeout.tv_sec < 0 || _timeout.tv_nsec < 0 ||
		    _timeout.tv_nsec >= 1000000000L) {
			error = -EINVAL;
			goto out_free;
		}
		jiffies_left = timespec_to_jiffies(&_timeout);
	}
	sma = sem_lock(semid);
	error=-EINVAL;
	if(sma==NULL)
		goto out_free;
	error = -EIDRM;
	if (sem_checkid(sma,semid))
		goto out_unlock_free;
	error = -EFBIG;
	for (sop = sops; sop < sops + nsops; sop++) {
		if (sop->sem_num >= sma->sem_nsems)
			goto out_unlock_free;
		if (sop->sem_flg & SEM_UNDO)
			undos++;
		if (sop->sem_op < 0)
			decrease = 1;
		if (sop->sem_op > 0)
			alter = 1;
	}
	alter |= decrease;

	error = -EACCES;
	if (ipcperms(&sma->sem_perm, alter ? S_IWUGO : S_IRUGO))
		goto out_unlock_free;
	if (undos) {
		/* Make sure we have an undo structure
		 * for this process and this semaphore set.
		 */
		un=current->semundo;
		while(un != NULL) {
			if(un->semid==semid)
				break;
			if(un->semid==-1)
				un=freeundos(sma,un);
			 else
				un=un->proc_next;
		}
		if (!un) {
			error = alloc_undo(sma,&un,semid,alter);
			if(error)
				goto out_free;
		}
	} else
		un = NULL;

	error = try_atomic_semop (sma, sops, nsops, un, current->pid, 0);
	if (error <= 0)
		goto update;

	/* We need to sleep on this operation, so we put the current
	 * task into the pending queue and go to sleep.
	 */
		
	queue.sma = sma;
	queue.sops = sops;
	queue.nsops = nsops;
	queue.undo = un;
	queue.pid = current->pid;
	queue.alter = decrease;
	queue.id = semid;
	if (alter)
		append_to_queue(sma ,&queue);
	else
		prepend_to_queue(sma ,&queue);
	current->semsleeping = &queue;

	for (;;) {
		struct sem_array* tmp;
		queue.status = -EINTR;
		queue.sleeper = current;
		current->state = TASK_INTERRUPTIBLE;
		sem_unlock(semid);

		if (timeout)
			jiffies_left = schedule_timeout(jiffies_left);
		else
			schedule();

		tmp = sem_lock(semid);
		if(tmp==NULL) {
			if(queue.prev != NULL)
				BUG();
			current->semsleeping = NULL;
			error = -EIDRM;
			goto out_free;
		}
		/*
		 * If queue.status == 1 we where woken up and
		 * have to retry else we simply return.
		 * If an interrupt occurred we have to clean up the
		 * queue
		 *
		 */
		if (queue.status == 1)
		{
			error = try_atomic_semop (sma, sops, nsops, un,
						  current->pid,0);
			if (error <= 0) 
				break;
		} else {
			error = queue.status;
			if (error == -EINTR && timeout && jiffies_left == 0)
				error = -EAGAIN;
			if (queue.prev) /* got Interrupt */
				break;
			/* Everything done by update_queue */
			current->semsleeping = NULL;
			goto out_unlock_free;
		}
	}
	current->semsleeping = NULL;
	remove_from_queue(sma,&queue);
update:
	if (alter)
		update_queue (sma);
out_unlock_free:
	sem_unlock(semid);
out_free:
	if(sops != fast_sops)
		kfree(sops);
	return error;
}

/*
 * add semadj values to semaphores, free undo structures.
 * undo structures are not freed when semaphore arrays are destroyed
 * so some of them may be out of date.
 * IMPLEMENTATION NOTE: There is some confusion over whether the
 * set of adjustments that needs to be done should be done in an atomic
 * manner or not. That is, if we are attempting to decrement the semval
 * should we queue up and wait until we can do so legally?
 * The original implementation attempted to do this (queue and wait).
 * The current implementation does not do so. The POSIX standard
 * and SVID should be consulted to determine what behavior is mandated.
 */
void sem_exit (void)
{
	struct sem_queue *q;
	struct sem_undo *u, *un = NULL, **up, **unp;
	struct sem_array *sma;
	int nsems, i;

	/* If the current process was sleeping for a semaphore,
	 * remove it from the queue.
	 */
	if ((q = current->semsleeping)) {
		int semid = q->id;
		sma = sem_lock(semid);
		current->semsleeping = NULL;

		if (q->prev) {
			if(sma==NULL)
				BUG();
			remove_from_queue(q->sma,q);
		}
		if(sma!=NULL)
			sem_unlock(semid);
	}

	for (up = &current->semundo; (u = *up); *up = u->proc_next, kfree(u)) {
		int semid = u->semid;
		if(semid == -1)
			continue;
		sma = sem_lock(semid);
		if (sma == NULL)
			continue;

		if (u->semid == -1)
			goto next_entry;

		if (sem_checkid(sma,u->semid))
			goto next_entry;

		/* remove u from the sma->undo list */
		for (unp = &sma->undo; (un = *unp); unp = &un->id_next) {
			if (u == un)
				goto found;
		}
		printk ("sem_exit undo list error id=%d\n", u->semid);
		goto next_entry;
found:
		*unp = un->id_next;
		/* perform adjustments registered in u */
		nsems = sma->sem_nsems;
		for (i = 0; i < nsems; i++) {
			struct sem * sem = &sma->sem_base[i];
			sem->semval += u->semadj[i];
			if (sem->semval < 0)
				sem->semval = 0; /* shouldn't happen */
			sem->sempid = current->pid;
		}
		sma->sem_otime = CURRENT_TIME;
		/* maybe some queued-up processes were waiting for this */
		update_queue(sma);
next_entry:
		sem_unlock(semid);
	}
	current->semundo = NULL;
}

#ifdef CONFIG_PROC_FS
static int sysvipc_sem_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	off_t pos = 0;
	off_t begin = 0;
	int i, len = 0;

	len += sprintf(buffer, "       key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n");
	down(&sem_ids.sem);

	for(i = 0; i <= sem_ids.max_id; i++) {
		struct sem_array *sma;
		sma = sem_lock(i);
		if(sma) {
			len += sprintf(buffer + len, "%10d %10d  %4o %10lu %5u %5u %5u %5u %10lu %10lu\n",
				sma->sem_perm.key,
				sem_buildid(i,sma->sem_perm.seq),
				sma->sem_perm.mode,
				sma->sem_nsems,
				sma->sem_perm.uid,
				sma->sem_perm.gid,
				sma->sem_perm.cuid,
				sma->sem_perm.cgid,
				sma->sem_otime,
				sma->sem_ctime);
			sem_unlock(i);

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
	up(&sem_ids.sem);
	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if(len > length)
		len = length;
	if(len < 0)
		len = 0;
	return len;
}
#endif
