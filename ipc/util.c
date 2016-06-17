/*
 * linux/ipc/util.c
 * Copyright (C) 1992 Krishna Balasubramanian
 *
 * Sep 1997 - Call suser() last after "normal" permission checks so we
 *            get BSD style process accounting right.
 *            Occurs in several places in the IPC code.
 *            Chris Evans, <chris@ferret.lmh.ox.ac.uk>
 * Nov 1999 - ipc helper functions, unified SMP locking
 *	      Manfred Spraul <manfreds@colorfullife.com>
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/shm.h>
#include <linux/init.h>
#include <linux/msg.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/highuid.h>

#if defined(CONFIG_SYSVIPC)

#include "util.h"

/**
 *	ipc_init	-	initialise IPC subsystem
 *
 *	The various system5 IPC resources (semaphores, messages and shared
 *	memory are initialised
 */
 
void __init ipc_init (void)
{
	sem_init();
	msg_init();
	shm_init();
	return;
}

/**
 *	ipc_init_ids		-	initialise IPC identifiers
 *	@ids: Identifier set
 *	@size: Number of identifiers
 *
 *	Given a size for the ipc identifier range (limited below IPCMNI)
 *	set up the sequence range to use then allocate and initialise the
 *	array itself. 
 */
 
void __init ipc_init_ids(struct ipc_ids* ids, int size)
{
	int i;
	sema_init(&ids->sem,1);

	if(size > IPCMNI)
		size = IPCMNI;
	ids->size = size;
	ids->in_use = 0;
	ids->max_id = -1;
	ids->seq = 0;
	{
		int seq_limit = INT_MAX/SEQ_MULTIPLIER;
		if(seq_limit > USHRT_MAX)
			ids->seq_max = USHRT_MAX;
		 else
		 	ids->seq_max = seq_limit;
	}

	ids->entries = ipc_alloc(sizeof(struct ipc_id)*size);

	if(ids->entries == NULL) {
		printk(KERN_ERR "ipc_init_ids() failed, ipc service disabled.\n");
		ids->size = 0;
	}
	ids->ary = SPIN_LOCK_UNLOCKED;
	for(i=0;i<ids->size;i++)
		ids->entries[i].p = NULL;
}

/**
 *	ipc_findkey	-	find a key in an ipc identifier set	
 *	@ids: Identifier set
 *	@key: The key to find
 *
 *	Returns the identifier if found or -1 if not.
 */
 
int ipc_findkey(struct ipc_ids* ids, key_t key)
{
	int id;
	struct kern_ipc_perm* p;

	for (id = 0; id <= ids->max_id; id++) {
		p = ids->entries[id].p;
		if(p==NULL)
			continue;
		if (key == p->key)
			return id;
	}
	return -1;
}

static int grow_ary(struct ipc_ids* ids, int newsize)
{
	struct ipc_id* new;
	struct ipc_id* old;
	int i;

	if(newsize > IPCMNI)
		newsize = IPCMNI;
	if(newsize <= ids->size)
		return newsize;

	new = ipc_alloc(sizeof(struct ipc_id)*newsize);
	if(new == NULL)
		return ids->size;
	memcpy(new, ids->entries, sizeof(struct ipc_id)*ids->size);
	for(i=ids->size;i<newsize;i++) {
		new[i].p = NULL;
	}
	spin_lock(&ids->ary);

	old = ids->entries;
	ids->entries = new;
	i = ids->size;
	ids->size = newsize;
	spin_unlock(&ids->ary);
	ipc_free(old, sizeof(struct ipc_id)*i);
	return ids->size;
}

/**
 *	ipc_addid 	-	add an IPC identifier
 *	@ids: IPC identifier set
 *	@new: new IPC permission set
 *	@size: new size limit for the id array
 *
 *	Add an entry 'new' to the IPC arrays. The permissions object is
 *	initialised and the first free entry is set up and the id assigned
 *	is returned. The list is returned in a locked state on success.
 *	On failure the list is not locked and -1 is returned.
 */
 
int ipc_addid(struct ipc_ids* ids, struct kern_ipc_perm* new, int size)
{
	int id;

	size = grow_ary(ids,size);
	for (id = 0; id < size; id++) {
		if(ids->entries[id].p == NULL)
			goto found;
	}
	return -1;
found:
	ids->in_use++;
	if (id > ids->max_id)
		ids->max_id = id;

	new->cuid = new->uid = current->euid;
	new->gid = new->cgid = current->egid;

	new->seq = ids->seq++;
	if(ids->seq > ids->seq_max)
		ids->seq = 0;

	spin_lock(&ids->ary);
	ids->entries[id].p = new;
	return id;
}

/**
 *	ipc_rmid	-	remove an IPC identifier
 *	@ids: identifier set
 *	@id: Identifier to remove
 *
 *	The identifier must be valid, and in use. The kernel will panic if
 *	fed an invalid identifier. The entry is removed and internal
 *	variables recomputed. The object associated with the identifier
 *	is returned.
 */
 
struct kern_ipc_perm* ipc_rmid(struct ipc_ids* ids, int id)
{
	struct kern_ipc_perm* p;
	int lid = id % SEQ_MULTIPLIER;
	if(lid >= ids->size)
		BUG();
	p = ids->entries[lid].p;
	ids->entries[lid].p = NULL;
	if(p==NULL)
		BUG();
	ids->in_use--;

	if (lid == ids->max_id) {
		do {
			lid--;
			if(lid == -1)
				break;
		} while (ids->entries[lid].p == NULL);
		ids->max_id = lid;
	}
	return p;
}

/**
 *	ipc_alloc	-	allocate ipc space
 *	@size: size desired
 *
 *	Allocate memory from the appropriate pools and return a pointer to it.
 *	NULL is returned if the allocation fails
 */
 
void* ipc_alloc(int size)
{
	void* out;
	if(size > PAGE_SIZE)
		out = vmalloc(size);
	else
		out = kmalloc(size, GFP_KERNEL);
	return out;
}

/**
 *	ipc_free	-	free ipc space
 *	@ptr: pointer returned by ipc_alloc
 *	@size: size of block
 *
 *	Free a block created with ipc_alloc. The caller must know the size
 *	used in the allocation call.
 */
 
void ipc_free(void* ptr, int size)
{
	if(size > PAGE_SIZE)
		vfree(ptr);
	else
		kfree(ptr);
}

/**
 *	ipcperms	-	check IPC permissions
 *	@ipcp: IPC permission set
 *	@flag: desired permission set.
 *
 *	Check user, group, other permissions for access
 *	to ipc resources. return 0 if allowed
 */
 
int ipcperms (struct kern_ipc_perm *ipcp, short flag)
{	/* flag will most probably be 0 or S_...UGO from <linux/stat.h> */
	int requested_mode, granted_mode;

	requested_mode = (flag >> 6) | (flag >> 3) | flag;
	granted_mode = ipcp->mode;
	if (current->euid == ipcp->cuid || current->euid == ipcp->uid)
		granted_mode >>= 6;
	else if (in_group_p(ipcp->cgid) || in_group_p(ipcp->gid))
		granted_mode >>= 3;
	/* is there some bit set in requested_mode but not in granted_mode? */
	if ((requested_mode & ~granted_mode & 0007) && 
	    !capable(CAP_IPC_OWNER))
		return -1;

	return 0;
}

/*
 * Functions to convert between the kern_ipc_perm structure and the
 * old/new ipc_perm structures
 */

/**
 *	kernel_to_ipc64_perm	-	convert kernel ipc permissions to user
 *	@in: kernel permissions
 *	@out: new style IPC permissions
 *
 *	Turn the kernel object 'in' into a set of permissions descriptions
 *	for returning to userspace (out).
 */
 

void kernel_to_ipc64_perm (struct kern_ipc_perm *in, struct ipc64_perm *out)
{
	out->key	= in->key;
	out->uid	= in->uid;
	out->gid	= in->gid;
	out->cuid	= in->cuid;
	out->cgid	= in->cgid;
	out->mode	= in->mode;
	out->seq	= in->seq;
}

/**
 *	ipc64_perm_to_ipc_perm	-	convert old ipc permissions to new
 *	@in: new style IPC permissions
 *	@out: old style IPC permissions
 *
 *	Turn the new style permissions object in into a compatibility
 *	object and store it into the 'out' pointer.
 */
 
void ipc64_perm_to_ipc_perm (struct ipc64_perm *in, struct ipc_perm *out)
{
	out->key	= in->key;
	out->uid	= NEW_TO_OLD_UID(in->uid);
	out->gid	= NEW_TO_OLD_GID(in->gid);
	out->cuid	= NEW_TO_OLD_UID(in->cuid);
	out->cgid	= NEW_TO_OLD_GID(in->cgid);
	out->mode	= in->mode;
	out->seq	= in->seq;
}

#if !defined(__ia64__) && !defined(__hppa__)

/**
 *	ipc_parse_version	-	IPC call version
 *	@cmd: pointer to command
 *
 *	Return IPC_64 for new style IPC and IPC_OLD for old style IPC. 
 *	The cmd value is turned from an encoding command and version into
 *	just the command code.
 */
 
int ipc_parse_version (int *cmd)
{
#ifdef __x86_64__
	if (!(current->thread.flags & THREAD_IA32))
		return IPC_64; 
#endif
	if (*cmd & IPC_64) {
		*cmd ^= IPC_64;
		return IPC_64;
	} else {
		return IPC_OLD;
	}
}

#endif /* __ia64__ */

#else
/*
 * Dummy functions when SYSV IPC isn't configured
 */

void sem_exit (void)
{
    return;
}

asmlinkage long sys_semget (key_t key, int nsems, int semflg)
{
	return -ENOSYS;
}

asmlinkage long sys_semop (int semid, struct sembuf *sops, unsigned nsops)
{
	return -ENOSYS;
}

asmlinkage long sys_semtimedop(int semid, struct sembuf *sops, unsigned nsops,
			       const struct timespec *timeout)
{
	return -ENOSYS;
}

asmlinkage long sys_semctl (int semid, int semnum, int cmd, union semun arg)
{
	return -ENOSYS;
}

asmlinkage long sys_msgget (key_t key, int msgflg)
{
	return -ENOSYS;
}

asmlinkage long sys_msgsnd (int msqid, struct msgbuf *msgp, size_t msgsz, int msgflg)
{
	return -ENOSYS;
}

asmlinkage long sys_msgrcv (int msqid, struct msgbuf *msgp, size_t msgsz, long msgtyp,
		       int msgflg)
{
	return -ENOSYS;
}

asmlinkage long sys_msgctl (int msqid, int cmd, struct msqid_ds *buf)
{
	return -ENOSYS;
}

asmlinkage long sys_shmget (key_t key, size_t size, int shmflag)
{
	return -ENOSYS;
}

asmlinkage long sys_shmat (int shmid, char *shmaddr, int shmflg, ulong *addr)
{
	return -ENOSYS;
}

asmlinkage long sys_shmdt (char *shmaddr)
{
	return -ENOSYS;
}

asmlinkage long sys_shmctl (int shmid, int cmd, struct shmid_ds *buf)
{
	return -ENOSYS;
}

#endif /* CONFIG_SYSVIPC */
