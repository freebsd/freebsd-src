/*
 *  arch/s390x/kernel/linux32.c
 *
 *  S390 version
 *    Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Gerhard Tonn (ton@de.ibm.com)   
 *
 *  Conversion between 31bit and 64bit native syscalls.
 *
 * Heavily inspired by the 32-bit Sparc compat code which is 
 * Copyright (C) 1997,1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 * Copyright (C) 1997 David S. Miller (davem@caip.rutgers.edu)
 *
 */


#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h> 
#include <linux/mm.h> 
#include <linux/file.h> 
#include <linux/signal.h>
#include <linux/utime.h>
#include <linux/resource.h>
#include <linux/times.h>
#include <linux/utsname.h>
#include <linux/timex.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sem.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/slab.h>
#include <linux/uio.h>
#include <linux/nfs_fs.h>
#include <linux/smb_fs.h>
#include <linux/smb_mount.h>
#include <linux/ncp_fs.h>
#include <linux/quota.h>
#include <linux/quotacompat.h>
#include <linux/module.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/cache.h>
#include <linux/nfsd/xdr.h>
#include <linux/nfsd/syscall.h>
#include <linux/poll.h>
#include <linux/personality.h>
#include <linux/stat.h>
#include <linux/filter.h>
#include <linux/highmem.h>
#include <linux/highuid.h>
#include <linux/mman.h>
#include <linux/ipv6.h>
#include <linux/in.h>
#include <linux/icmpv6.h>
#include <linux/sysctl.h>

#include <asm/types.h>
#include <asm/ipc.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>

#include <net/scm.h>
#include <net/sock.h>

#include "linux32.h"

extern asmlinkage long sys_chown(const char *, uid_t,gid_t);
extern asmlinkage long sys_lchown(const char *, uid_t,gid_t);
extern asmlinkage long sys_fchown(unsigned int, uid_t,gid_t);
extern asmlinkage long sys_setregid(gid_t, gid_t);
extern asmlinkage long sys_setgid(gid_t);
extern asmlinkage long sys_setreuid(uid_t, uid_t);
extern asmlinkage long sys_setuid(uid_t);
extern asmlinkage long sys_setresuid(uid_t, uid_t, uid_t);
extern asmlinkage long sys_setresgid(gid_t, gid_t, gid_t);
extern asmlinkage long sys_setfsuid(uid_t);
extern asmlinkage long sys_setfsgid(gid_t);
 
/* For this source file, we want overflow handling. */

#undef high2lowuid
#undef high2lowgid
#undef low2highuid
#undef low2highgid
#undef SET_UID16
#undef SET_GID16
#undef NEW_TO_OLD_UID
#undef NEW_TO_OLD_GID
#undef SET_OLDSTAT_UID
#undef SET_OLDSTAT_GID
#undef SET_STAT_UID
#undef SET_STAT_GID

#define high2lowuid(uid) ((uid) > 65535) ? (u16)overflowuid : (u16)(uid)
#define high2lowgid(gid) ((gid) > 65535) ? (u16)overflowgid : (u16)(gid)
#define low2highuid(uid) ((uid) == (u16)-1) ? (uid_t)-1 : (uid_t)(uid)
#define low2highgid(gid) ((gid) == (u16)-1) ? (gid_t)-1 : (gid_t)(gid)
#define SET_UID16(var, uid)	var = high2lowuid(uid)
#define SET_GID16(var, gid)	var = high2lowgid(gid)
#define NEW_TO_OLD_UID(uid)	high2lowuid(uid)
#define NEW_TO_OLD_GID(gid)	high2lowgid(gid)
#define SET_OLDSTAT_UID(stat, uid)	(stat).st_uid = high2lowuid(uid)
#define SET_OLDSTAT_GID(stat, gid)	(stat).st_gid = high2lowgid(gid)
#define SET_STAT_UID(stat, uid)		(stat).st_uid = high2lowuid(uid)
#define SET_STAT_GID(stat, gid)		(stat).st_gid = high2lowgid(gid)

asmlinkage long sys32_chown16(const char * filename, u16 user, u16 group)
{
	return sys_chown(filename, low2highuid(user), low2highgid(group));
}

asmlinkage long sys32_lchown16(const char * filename, u16 user, u16 group)
{
	return sys_lchown(filename, low2highuid(user), low2highgid(group));
}

asmlinkage long sys32_fchown16(unsigned int fd, u16 user, u16 group)
{
	return sys_fchown(fd, low2highuid(user), low2highgid(group));
}

asmlinkage long sys32_setregid16(u16 rgid, u16 egid)
{
	return sys_setregid(low2highgid(rgid), low2highgid(egid));
}

asmlinkage long sys32_setgid16(u16 gid)
{
	return sys_setgid((gid_t)gid);
}

asmlinkage long sys32_setreuid16(u16 ruid, u16 euid)
{
	return sys_setreuid(low2highuid(ruid), low2highuid(euid));
}

asmlinkage long sys32_setuid16(u16 uid)
{
	return sys_setuid((uid_t)uid);
}

asmlinkage long sys32_setresuid16(u16 ruid, u16 euid, u16 suid)
{
	return sys_setresuid(low2highuid(ruid), low2highuid(euid),
		low2highuid(suid));
}

asmlinkage long sys32_getresuid16(u16 *ruid, u16 *euid, u16 *suid)
{
	int retval;

	if (!(retval = put_user(high2lowuid(current->uid), ruid)) &&
	    !(retval = put_user(high2lowuid(current->euid), euid)))
		retval = put_user(high2lowuid(current->suid), suid);

	return retval;
}

asmlinkage long sys32_setresgid16(u16 rgid, u16 egid, u16 sgid)
{
	return sys_setresgid(low2highgid(rgid), low2highgid(egid),
		low2highgid(sgid));
}

asmlinkage long sys32_getresgid16(u16 *rgid, u16 *egid, u16 *sgid)
{
	int retval;

	if (!(retval = put_user(high2lowgid(current->gid), rgid)) &&
	    !(retval = put_user(high2lowgid(current->egid), egid)))
		retval = put_user(high2lowgid(current->sgid), sgid);

	return retval;
}

asmlinkage long sys32_setfsuid16(u16 uid)
{
	return sys_setfsuid((uid_t)uid);
}

asmlinkage long sys32_setfsgid16(u16 gid)
{
	return sys_setfsgid((gid_t)gid);
}

asmlinkage long sys32_getgroups16(int gidsetsize, u16 *grouplist)
{
	u16 groups[NGROUPS];
	int i,j;

	if (gidsetsize < 0)
		return -EINVAL;
	i = current->ngroups;
	if (gidsetsize) {
		if (i > gidsetsize)
			return -EINVAL;
		for(j=0;j<i;j++)
			groups[j] = current->groups[j];
		if (copy_to_user(grouplist, groups, sizeof(u16)*i))
			return -EFAULT;
	}
	return i;
}

asmlinkage long sys32_setgroups16(int gidsetsize, u16 *grouplist)
{
	u16 groups[NGROUPS];
	int i;

	if (!capable(CAP_SETGID))
		return -EPERM;
	if ((unsigned) gidsetsize > NGROUPS)
		return -EINVAL;
	if (copy_from_user(groups, grouplist, gidsetsize * sizeof(u16)))
		return -EFAULT;
	for (i = 0 ; i < gidsetsize ; i++)
		current->groups[i] = (gid_t)groups[i];
	current->ngroups = gidsetsize;
	return 0;
}

asmlinkage long sys32_getuid16(void)
{
	return high2lowuid(current->uid);
}

asmlinkage long sys32_geteuid16(void)
{
	return high2lowuid(current->euid);
}

asmlinkage long sys32_getgid16(void)
{
	return high2lowgid(current->gid);
}

asmlinkage long sys32_getegid16(void)
{
	return high2lowgid(current->egid);
}

/* 32-bit timeval and related flotsam.  */

struct timeval32
{
    int tv_sec, tv_usec;
};

struct itimerval32
{
    struct timeval32 it_interval;
    struct timeval32 it_value;
};

static inline long get_tv32(struct timeval *o, struct timeval32 *i)
{
	return (!access_ok(VERIFY_READ, tv32, sizeof(*tv32)) ||
		(__get_user(o->tv_sec, &i->tv_sec) |
		 __get_user(o->tv_usec, &i->tv_usec)));
}

static inline long put_tv32(struct timeval32 *o, struct timeval *i)
{
	return (!access_ok(VERIFY_WRITE, o, sizeof(*o)) ||
		(__put_user(i->tv_sec, &o->tv_sec) |
		 __put_user(i->tv_usec, &o->tv_usec)));
}

static inline long get_it32(struct itimerval *o, struct itimerval32 *i)
{
	return (!access_ok(VERIFY_READ, i32, sizeof(*i32)) ||
		(__get_user(o->it_interval.tv_sec, &i->it_interval.tv_sec) |
		 __get_user(o->it_interval.tv_usec, &i->it_interval.tv_usec) |
		 __get_user(o->it_value.tv_sec, &i->it_value.tv_sec) |
		 __get_user(o->it_value.tv_usec, &i->it_value.tv_usec)));
}

static inline long put_it32(struct itimerval32 *o, struct itimerval *i)
{
	return (!access_ok(VERIFY_WRITE, i32, sizeof(*i32)) ||
		(__put_user(i->it_interval.tv_sec, &o->it_interval.tv_sec) |
		 __put_user(i->it_interval.tv_usec, &o->it_interval.tv_usec) |
		 __put_user(i->it_value.tv_sec, &o->it_value.tv_sec) |
		 __put_user(i->it_value.tv_usec, &o->it_value.tv_usec)));
}

struct msgbuf32 { s32 mtype; char mtext[1]; };

struct ipc64_perm_ds32
{
        __kernel_key_t          key;
        __kernel_uid32_t        uid;
        __kernel_gid32_t        gid;
        __kernel_uid32_t        cuid;
        __kernel_gid32_t        cgid;
        __kernel_mode_t32       mode;
        unsigned short          __pad1;
        unsigned short          seq;
        unsigned short          __pad2;
        unsigned int            __unused1;
        unsigned int            __unused2;
};

struct ipc_perm32
{
	key_t    	  key;
        __kernel_uid_t32  uid;
        __kernel_gid_t32  gid;
        __kernel_uid_t32  cuid;
        __kernel_gid_t32  cgid;
        __kernel_mode_t32 mode;
        unsigned short  seq;
};

struct semid_ds32 {
        struct ipc_perm32 sem_perm;               /* permissions .. see ipc.h */
        __kernel_time_t32 sem_otime;              /* last semop time */
        __kernel_time_t32 sem_ctime;              /* last change time */
        u32 sem_base;              /* ptr to first semaphore in array */
        u32 sem_pending;          /* pending operations to be processed */
        u32 sem_pending_last;    /* last pending operation */
        u32 undo;                  /* undo requests on this array */
        unsigned short  sem_nsems;              /* no. of semaphores in array */
};

struct semid64_ds32 {
	struct ipc64_perm_ds32 sem_perm;
	unsigned int	  __pad1;
	__kernel_time_t32 sem_otime;
	unsigned int	  __pad2;
	__kernel_time_t32 sem_ctime;
	u32 sem_nsems;
	u32 __unused1;
	u32 __unused2;
};

struct msqid_ds32
{
        struct ipc_perm32 msg_perm;
        u32 msg_first;
        u32 msg_last;
        __kernel_time_t32 msg_stime;
        __kernel_time_t32 msg_rtime;
        __kernel_time_t32 msg_ctime;
        u32 wwait;
        u32 rwait;
        unsigned short msg_cbytes;
        unsigned short msg_qnum;  
        unsigned short msg_qbytes;
        __kernel_ipc_pid_t32 msg_lspid;
        __kernel_ipc_pid_t32 msg_lrpid;
};

struct msqid64_ds32 {
	struct ipc64_perm_ds32 msg_perm;
	unsigned int   __pad1;
	__kernel_time_t32 msg_stime;
	unsigned int   __pad2;
	__kernel_time_t32 msg_rtime;
	unsigned int   __pad3;
	__kernel_time_t32 msg_ctime;
	unsigned int  msg_cbytes;
	unsigned int  msg_qnum;
	unsigned int  msg_qbytes;
	__kernel_pid_t32 msg_lspid;
	__kernel_pid_t32 msg_lrpid;
	unsigned int  __unused1;
	unsigned int  __unused2;
};


struct shmid_ds32 {
	struct ipc_perm32       shm_perm;
	int                     shm_segsz;
	__kernel_time_t32       shm_atime;
	__kernel_time_t32       shm_dtime;
	__kernel_time_t32       shm_ctime;
	__kernel_ipc_pid_t32    shm_cpid; 
	__kernel_ipc_pid_t32    shm_lpid; 
	unsigned short          shm_nattch;
};

struct shmid64_ds32 {
	struct ipc64_perm_ds32	shm_perm;
	__kernel_size_t32	shm_segsz;
	__kernel_time_t32	shm_atime;
	unsigned int		__unused1;
	__kernel_time_t32	shm_dtime;
	unsigned int		__unused2;
	__kernel_time_t32	shm_ctime;
	unsigned int		__unused3;
	__kernel_pid_t32	shm_cpid;
	__kernel_pid_t32	shm_lpid;
	unsigned int		shm_nattch;
	unsigned int		__unused4;
	unsigned int		__unused5;
};

                                                        
/*
 * sys32_ipc() is the de-multiplexer for the SysV IPC calls in 32bit emulation..
 *
 * This is really horribly ugly.
 */
#define IPCOP_MASK(__x)	(1UL << (__x))
static int do_sys32_semctl(int first, int second, int third, void *uptr)
{
	union semun fourth;
	u32 pad;
	int err = -EINVAL;

	if (!uptr)
		goto out;
	err = -EFAULT;
	if (get_user (pad, (u32 *)uptr))
		goto out;
	if(third == SETVAL)
		fourth.val = (int)pad;
	else
		fourth.__pad = (void *)A(pad);
	if (IPCOP_MASK (third) &
	    (IPCOP_MASK (IPC_INFO) | IPCOP_MASK (SEM_INFO) | IPCOP_MASK (GETVAL) |
	     IPCOP_MASK (GETPID) | IPCOP_MASK (GETNCNT) | IPCOP_MASK (GETZCNT) |
	     IPCOP_MASK (GETALL) | IPCOP_MASK (SETALL) | IPCOP_MASK (IPC_RMID))) {
		err = sys_semctl (first, second, third, fourth);
	} else if (third & IPC_64) {
		struct semid64_ds s;
		struct semid64_ds32 *usp = (struct semid64_ds32 *)A(pad);
		mm_segment_t old_fs;
		int need_back_translation;

		if (third == (IPC_SET|IPC_64)) {
			err = get_user (s.sem_perm.uid, &usp->sem_perm.uid);
			err |= __get_user (s.sem_perm.gid, &usp->sem_perm.gid);
			err |= __get_user (s.sem_perm.mode, &usp->sem_perm.mode);
			if (err)
				goto out;
			fourth.__pad = &s;
		}
		need_back_translation =
			(IPCOP_MASK (third) &
			 (IPCOP_MASK (SEM_STAT) | IPCOP_MASK (IPC_STAT))) != 0;
		if (need_back_translation)
			fourth.__pad = &s;
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_semctl (first, second, third, fourth);
		set_fs (old_fs);
		if (need_back_translation) {
			int err2 = put_user (s.sem_perm.key, &usp->sem_perm.key);
			err2 |= __put_user (high2lowuid(s.sem_perm.uid), &usp->sem_perm.uid);
			err2 |= __put_user (high2lowgid(s.sem_perm.gid), &usp->sem_perm.gid);
			err2 |= __put_user (high2lowuid(s.sem_perm.cuid), &usp->sem_perm.cuid);
			err2 |= __put_user (high2lowgid(s.sem_perm.cgid), &usp->sem_perm.cgid);
			err2 |= __put_user (s.sem_perm.mode, &usp->sem_perm.mode);
			err2 |= __put_user (s.sem_perm.seq, &usp->sem_perm.seq);
			err2 |= __put_user (s.sem_otime, &usp->sem_otime);
			err2 |= __put_user (s.sem_ctime, &usp->sem_ctime);
			err2 |= __put_user (s.sem_nsems, &usp->sem_nsems);
			if (err2) err = -EFAULT;
		}
	} else {
		struct semid_ds s;
		struct semid_ds32 *usp = (struct semid_ds32 *)A(pad);
		mm_segment_t old_fs;
		int need_back_translation;

		if (third == IPC_SET) {
			err = get_user (s.sem_perm.uid, &usp->sem_perm.uid);
			err |= __get_user (s.sem_perm.gid, &usp->sem_perm.gid);
			err |= __get_user (s.sem_perm.mode, &usp->sem_perm.mode);
			if (err)
				goto out;
			fourth.__pad = &s;
		}
		need_back_translation =
			(IPCOP_MASK (third) &
			 (IPCOP_MASK (SEM_STAT) | IPCOP_MASK (IPC_STAT))) != 0;
		if (need_back_translation)
			fourth.__pad = &s;
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_semctl (first, second, third, fourth);
		set_fs (old_fs);
		if (need_back_translation) {
			int err2 = put_user (s.sem_perm.key, &usp->sem_perm.key);
			err2 |= __put_user (high2lowuid(s.sem_perm.uid), &usp->sem_perm.uid);
			err2 |= __put_user (high2lowgid(s.sem_perm.gid), &usp->sem_perm.gid);
			err2 |= __put_user (high2lowuid(s.sem_perm.cuid), &usp->sem_perm.cuid);
			err2 |= __put_user (high2lowgid(s.sem_perm.cgid), &usp->sem_perm.cgid);
			err2 |= __put_user (s.sem_perm.mode, &usp->sem_perm.mode);
			err2 |= __put_user (s.sem_perm.seq, &usp->sem_perm.seq);
			err2 |= __put_user (s.sem_otime, &usp->sem_otime);
			err2 |= __put_user (s.sem_ctime, &usp->sem_ctime);
			err2 |= __put_user (s.sem_nsems, &usp->sem_nsems);
			if (err2) err = -EFAULT;
		}
	}
out:
	return err;
}

static int do_sys32_msgsnd (int first, int second, int third, void *uptr)
{
	struct msgbuf *p = kmalloc (second + sizeof (struct msgbuf), GFP_USER);
	struct msgbuf32 *up = (struct msgbuf32 *)uptr;
	mm_segment_t old_fs;
	int err;

	if (!p)
		return -ENOMEM;

	err = -EINVAL;
	if (second > MSGMAX || first < 0 || second < 0)
		goto out;

	err = -EFAULT;
	if (!uptr)
		goto out;
	if (get_user (p->mtype, &up->mtype) ||
	    __copy_from_user (p->mtext, &up->mtext, second))
		goto out;
	old_fs = get_fs ();
	set_fs (KERNEL_DS);
	err = sys_msgsnd (first, p, second, third);
	set_fs (old_fs);
out:
	kfree (p);
	return err;
}

static int do_sys32_msgrcv (int first, int second, int msgtyp, int third,
			    int version, void *uptr)
{
	struct msgbuf32 *up;
	struct msgbuf *p;
	mm_segment_t old_fs;
	int err;

	if (first < 0 || second < 0)
		return -EINVAL;

	if (!version) {
		struct ipc_kludge_32 *uipck = (struct ipc_kludge_32 *)uptr;
		struct ipc_kludge_32 ipck;

		err = -EINVAL;
		if (!uptr)
			goto out;
		err = -EFAULT;
		if (copy_from_user (&ipck, uipck, sizeof (struct ipc_kludge_32)))
			goto out;
		uptr = (void *)A(ipck.msgp);
		msgtyp = ipck.msgtyp;
	}
	err = -ENOMEM;
	p = kmalloc (second + sizeof (struct msgbuf), GFP_USER);
	if (!p)
		goto out;
	old_fs = get_fs ();
	set_fs (KERNEL_DS);
	err = sys_msgrcv (first, p, second, msgtyp, third);
	set_fs (old_fs);
	if (err < 0)
		goto free_then_out;
	up = (struct msgbuf32 *)uptr;
	if (put_user (p->mtype, &up->mtype) ||
	    __copy_to_user (&up->mtext, p->mtext, err))
		err = -EFAULT;
free_then_out:
	kfree (p);
out:
	return err;
}

static int do_sys32_msgctl (int first, int second, void *uptr)
{
	int err;

	if (IPCOP_MASK (second) &
	    (IPCOP_MASK (IPC_INFO) | IPCOP_MASK (MSG_INFO) |
	     IPCOP_MASK (IPC_RMID))) {
		err = sys_msgctl (first, second, (struct msqid_ds *)uptr);
	} else if (second & IPC_64) {
		struct msqid64_ds m;
		struct msqid64_ds32 *up = (struct msqid64_ds32 *)uptr;
		mm_segment_t old_fs;

		if (second == (IPC_SET|IPC_64)) {
			err = get_user (m.msg_perm.uid, &up->msg_perm.uid);
			err |= __get_user (m.msg_perm.gid, &up->msg_perm.gid);
			err |= __get_user (m.msg_perm.mode, &up->msg_perm.mode);
			err |= __get_user (m.msg_qbytes, &up->msg_qbytes);
			if (err)
				goto out;
		}
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_msgctl (first, second, (struct msqid_ds *)&m);
		set_fs (old_fs);
		if (IPCOP_MASK (second) &
		    (IPCOP_MASK (MSG_STAT) | IPCOP_MASK (IPC_STAT))) {
			int err2 = put_user (m.msg_perm.key, &up->msg_perm.key);
			err2 |= __put_user (high2lowuid(m.msg_perm.uid), &up->msg_perm.uid);
			err2 |= __put_user (high2lowgid(m.msg_perm.gid), &up->msg_perm.gid);
			err2 |= __put_user (high2lowuid(m.msg_perm.cuid), &up->msg_perm.cuid);
			err2 |= __put_user (high2lowgid(m.msg_perm.cgid), &up->msg_perm.cgid);
			err2 |= __put_user (m.msg_perm.mode, &up->msg_perm.mode);
			err2 |= __put_user (m.msg_perm.seq, &up->msg_perm.seq);
			err2 |= __put_user (m.msg_stime, &up->msg_stime);
			err2 |= __put_user (m.msg_rtime, &up->msg_rtime);
			err2 |= __put_user (m.msg_ctime, &up->msg_ctime);
			err2 |= __put_user (m.msg_cbytes, &up->msg_cbytes);
			err2 |= __put_user (m.msg_qnum, &up->msg_qnum);
			err2 |= __put_user (m.msg_qbytes, &up->msg_qbytes);
			err2 |= __put_user (m.msg_lspid, &up->msg_lspid);
			err2 |= __put_user (m.msg_lrpid, &up->msg_lrpid);
			if (err2)
				err = -EFAULT;
		}
	} else {
		struct msqid_ds m;
		struct msqid_ds32 *up = (struct msqid_ds32 *)uptr;
		mm_segment_t old_fs;

		if (second == IPC_SET) {
			err = get_user (m.msg_perm.uid, &up->msg_perm.uid);
			err |= __get_user (m.msg_perm.gid, &up->msg_perm.gid);
			err |= __get_user (m.msg_perm.mode, &up->msg_perm.mode);
			err |= __get_user (m.msg_qbytes, &up->msg_qbytes);
			if (err)
				goto out;
		}
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_msgctl (first, second, &m);
		set_fs (old_fs);
		if (IPCOP_MASK (second) &
		    (IPCOP_MASK (MSG_STAT) | IPCOP_MASK (IPC_STAT))) {
			int err2 = put_user (m.msg_perm.key, &up->msg_perm.key);
			err2 |= __put_user (high2lowuid(m.msg_perm.uid), &up->msg_perm.uid);
			err2 |= __put_user (high2lowgid(m.msg_perm.gid), &up->msg_perm.gid);
			err2 |= __put_user (high2lowuid(m.msg_perm.cuid), &up->msg_perm.cuid);
			err2 |= __put_user (high2lowgid(m.msg_perm.cgid), &up->msg_perm.cgid);
			err2 |= __put_user (m.msg_perm.mode, &up->msg_perm.mode);
			err2 |= __put_user (m.msg_perm.seq, &up->msg_perm.seq);
			err2 |= __put_user (m.msg_stime, &up->msg_stime);
			err2 |= __put_user (m.msg_rtime, &up->msg_rtime);
			err2 |= __put_user (m.msg_ctime, &up->msg_ctime);
			err2 |= __put_user (m.msg_cbytes, &up->msg_cbytes);
			err2 |= __put_user (m.msg_qnum, &up->msg_qnum);
			err2 |= __put_user (m.msg_qbytes, &up->msg_qbytes);
			err2 |= __put_user (m.msg_lspid, &up->msg_lspid);
			err2 |= __put_user (m.msg_lrpid, &up->msg_lrpid);
			if (err2)
				err = -EFAULT;
		}
	}

out:
	return err;
}

static int do_sys32_shmat (int first, int second, int third, int version, void *uptr)
{
	unsigned long raddr;
	u32 *uaddr = (u32 *)A((u32)third);
	int err = -EINVAL;

	if (version == 1)
		goto out;
	err = sys_shmat (first, uptr, second, &raddr);
	if (err)
		goto out;
	err = put_user (raddr, uaddr);
out:
	return err;
}

static int do_sys32_shmctl (int first, int second, void *uptr)
{
	int err;

	if (IPCOP_MASK (second) &
	    (IPCOP_MASK (IPC_INFO) | IPCOP_MASK (SHM_LOCK) | IPCOP_MASK (SHM_UNLOCK) |
	     IPCOP_MASK (IPC_RMID))) {
		if (second == (IPC_INFO|IPC_64))
			second = IPC_INFO; /* So that we don't have to translate it */
		err = sys_shmctl (first, second, (struct shmid_ds *)uptr);
	} else if ((second & IPC_64) && second != (SHM_INFO|IPC_64)) {
		struct shmid64_ds s;
		struct shmid64_ds32 *up = (struct shmid64_ds32 *)uptr;
		mm_segment_t old_fs;

		if (second == (IPC_SET|IPC_64)) {
			err = get_user (s.shm_perm.uid, &up->shm_perm.uid);
			err |= __get_user (s.shm_perm.gid, &up->shm_perm.gid);
			err |= __get_user (s.shm_perm.mode, &up->shm_perm.mode);
			if (err)
				goto out;
		}
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_shmctl (first, second, (struct shmid_ds *)&s);
		set_fs (old_fs);
		if (err < 0)
			goto out;

		/* Mask it even in this case so it becomes a CSE. */
		if (IPCOP_MASK (second) &
		    (IPCOP_MASK (SHM_STAT) | IPCOP_MASK (IPC_STAT))) {
			int err2 = put_user (s.shm_perm.key, &up->shm_perm.key);
			err2 |= __put_user (high2lowuid(s.shm_perm.uid), &up->shm_perm.uid);
			err2 |= __put_user (high2lowgid(s.shm_perm.gid), &up->shm_perm.gid);
			err2 |= __put_user (high2lowuid(s.shm_perm.cuid), &up->shm_perm.cuid);
			err2 |= __put_user (high2lowgid(s.shm_perm.cgid), &up->shm_perm.cgid);
			err2 |= __put_user (s.shm_perm.mode, &up->shm_perm.mode);
			err2 |= __put_user (s.shm_perm.seq, &up->shm_perm.seq);
			err2 |= __put_user (s.shm_atime, &up->shm_atime);
			err2 |= __put_user (s.shm_dtime, &up->shm_dtime);
			err2 |= __put_user (s.shm_ctime, &up->shm_ctime);
			err2 |= __put_user (s.shm_segsz, &up->shm_segsz);
			err2 |= __put_user (s.shm_nattch, &up->shm_nattch);
			err2 |= __put_user (s.shm_cpid, &up->shm_cpid);
			err2 |= __put_user (s.shm_lpid, &up->shm_lpid);
			if (err2)
				err = -EFAULT;
		}
	} else {
		struct shmid_ds s;
		struct shmid_ds32 *up = (struct shmid_ds32 *)uptr;
		mm_segment_t old_fs;

		second &= ~IPC_64;
		if (second == IPC_SET) {
			err = get_user (s.shm_perm.uid, &up->shm_perm.uid);
			err |= __get_user (s.shm_perm.gid, &up->shm_perm.gid);
			err |= __get_user (s.shm_perm.mode, &up->shm_perm.mode);
			if (err)
				goto out;
		}
		old_fs = get_fs ();
		set_fs (KERNEL_DS);
		err = sys_shmctl (first, second, &s);
		set_fs (old_fs);
		if (err < 0)
			goto out;

		/* Mask it even in this case so it becomes a CSE. */
		if (second == SHM_INFO) {
			struct shm_info32 {
				int used_ids;
				u32 shm_tot, shm_rss, shm_swp;
				u32 swap_attempts, swap_successes;
			} *uip = (struct shm_info32 *)uptr;
			struct shm_info *kp = (struct shm_info *)&s;
			int err2 = put_user (kp->used_ids, &uip->used_ids);
			err2 |= __put_user (kp->shm_tot, &uip->shm_tot);
			err2 |= __put_user (kp->shm_rss, &uip->shm_rss);
			err2 |= __put_user (kp->shm_swp, &uip->shm_swp);
			err2 |= __put_user (kp->swap_attempts, &uip->swap_attempts);
			err2 |= __put_user (kp->swap_successes, &uip->swap_successes);
			if (err2)
				err = -EFAULT;
		} else if (IPCOP_MASK (second) &
			   (IPCOP_MASK (SHM_STAT) | IPCOP_MASK (IPC_STAT))) {
			int err2 = put_user (s.shm_perm.key, &up->shm_perm.key);
			err2 |= __put_user (high2lowuid(s.shm_perm.uid), &up->shm_perm.uid);
			err2 |= __put_user (high2lowgid(s.shm_perm.gid), &up->shm_perm.gid);
			err2 |= __put_user (high2lowuid(s.shm_perm.cuid), &up->shm_perm.cuid);
			err2 |= __put_user (high2lowgid(s.shm_perm.cgid), &up->shm_perm.cgid);
			err2 |= __put_user (s.shm_perm.mode, &up->shm_perm.mode);
			err2 |= __put_user (s.shm_perm.seq, &up->shm_perm.seq);
			err2 |= __put_user (s.shm_atime, &up->shm_atime);
			err2 |= __put_user (s.shm_dtime, &up->shm_dtime);
			err2 |= __put_user (s.shm_ctime, &up->shm_ctime);
			err2 |= __put_user (s.shm_segsz, &up->shm_segsz);
			err2 |= __put_user (s.shm_nattch, &up->shm_nattch);
			err2 |= __put_user (s.shm_cpid, &up->shm_cpid);
			err2 |= __put_user (s.shm_lpid, &up->shm_lpid);
			if (err2)
				err = -EFAULT;
		}
	}
out:
	return err;
}

asmlinkage int sys32_ipc (u32 call, int first, int second, int third, u32 ptr, u32 fifth)
{
	int version, err;

	version = call >> 16; /* hack for backward compatibility */
	call &= 0xffff;

	if(version)
		return -EINVAL;

	if (call <= SEMCTL)
		switch (call) {
		case SEMOP:
			/* struct sembuf is the same on 32 and 64bit :)) */
			err = sys_semop (first, (struct sembuf *)AA(ptr), second);
			goto out;
		case SEMGET:
			err = sys_semget (first, second, third);
			goto out;
		case SEMCTL:
			err = do_sys32_semctl (first, second, third, (void *)AA(ptr));
			goto out;
		default:
			err = -EINVAL;
			goto out;
		};
	if (call <= MSGCTL) 
		switch (call) {
		case MSGSND:
			err = do_sys32_msgsnd (first, second, third, (void *)AA(ptr));
			goto out;
		case MSGRCV:
			err = do_sys32_msgrcv (first, second, 0, third,
					       version, (void *)AA(ptr));
			goto out;
		case MSGGET:
			err = sys_msgget ((key_t) first, second);
			goto out;
		case MSGCTL:
			err = do_sys32_msgctl (first, second, (void *)AA(ptr));
			goto out;
		default:
			err = -EINVAL;
			goto out;
		}
	if (call <= SHMCTL) 
		switch (call) {
		case SHMAT:
			err = do_sys32_shmat (first, second, third,
					      version, (void *)AA(ptr));
			goto out;
		case SHMDT: 
			err = sys_shmdt ((char *)AA(ptr));
			goto out;
		case SHMGET:
			err = sys_shmget (first, second, third);
			goto out;
		case SHMCTL:
			err = do_sys32_shmctl (first, second, (void *)AA(ptr));
			goto out;
		default:
			err = -EINVAL;
			goto out;
		}

	err = -EINVAL;

out:
	return err;
}

static inline int get_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;
	
	err = get_user(kfl->l_type, &ufl->l_type);
	err |= __get_user(kfl->l_whence, &ufl->l_whence);
	err |= __get_user(kfl->l_start, &ufl->l_start);
	err |= __get_user(kfl->l_len, &ufl->l_len);
	err |= __get_user(kfl->l_pid, &ufl->l_pid);
	return err;
}

static inline int put_flock(struct flock *kfl, struct flock32 *ufl)
{
	int err;
	
	err = __put_user(kfl->l_type, &ufl->l_type);
	err |= __put_user(kfl->l_whence, &ufl->l_whence);
	err |= __put_user(kfl->l_start, &ufl->l_start);
	err |= __put_user(kfl->l_len, &ufl->l_len);
	err |= __put_user(kfl->l_pid, &ufl->l_pid);
	return err;
}

extern asmlinkage long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);

asmlinkage long sys32_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case F_GETLK:
		{
			struct flock f;
			mm_segment_t old_fs;
			long ret;
			
			if(get_flock(&f, (struct flock32 *)A(arg)))
				return -EFAULT;
			old_fs = get_fs(); set_fs (KERNEL_DS);
			ret = sys_fcntl(fd, cmd, (unsigned long)&f);
			set_fs (old_fs);
			if (ret) return ret;
			if (f.l_start >= 0x7fffffffUL ||
			    f.l_start + f.l_len >= 0x7fffffffUL)
				return -EOVERFLOW;
			if(put_flock(&f, (struct flock32 *)A(arg)))
				return -EFAULT;
			return 0;
		}
	case F_SETLK:
	case F_SETLKW:
		{
			struct flock f;
			mm_segment_t old_fs;
			long ret;
			
			if(get_flock(&f, (struct flock32 *)A(arg)))
				return -EFAULT;
			old_fs = get_fs(); set_fs (KERNEL_DS);
			ret = sys_fcntl(fd, cmd, (unsigned long)&f);
			set_fs (old_fs);
			if (ret) return ret;
			return 0;
		}
	default:
		return sys_fcntl(fd, cmd, (unsigned long)arg);
	}
}

asmlinkage long sys32_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	if (cmd >= F_GETLK64 && cmd <= F_SETLKW64)
		return sys_fcntl(fd, cmd + F_GETLK - F_GETLK64, arg);
	return sys32_fcntl(fd, cmd, arg);
}

struct user_dqblk32 {
    __u32 dqb_bhardlimit;
    __u32 dqb_bsoftlimit;
    __u32 dqb_curblocks;
    __u32 dqb_ihardlimit;
    __u32 dqb_isoftlimit;
    __u32 dqb_curinodes;
    __kernel_time_t32 dqb_btime;
    __kernel_time_t32 dqb_itime;
};
                                
extern asmlinkage int sys_quotactl(int cmd, const char *special, int id, caddr_t addr);

asmlinkage int sys32_quotactl(int cmd, const char *special, int id, caddr_t addr)
{
	int cmds = cmd >> SUBCMDSHIFT;
	int err;
	struct v1c_mem_dqblk d;
	mm_segment_t old_fs;
	char *spec;
	
	switch (cmds) {
	case Q_V1_GETQUOTA:
		break;
	case Q_V1_SETQUOTA:
	case Q_V1_SETUSE:
	case Q_V1_SETQLIM:
		if (copy_from_user(&d, addr, sizeof (struct user_dqblk32)))
			return -EFAULT;
		d.dqb_itime = ((struct user_dqblk32 *)&d)->dqb_itime;
		d.dqb_btime = ((struct user_dqblk32 *)&d)->dqb_btime;
		break;
	default:
		return sys_quotactl(cmd, special, id, addr);
	}

	spec = getname(special);
	err = PTR_ERR(spec);
	if (IS_ERR(spec))
		return err;
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	err = sys_quotactl(cmd, (const char *)spec, id, (caddr_t)&d);
	set_fs(old_fs);
	putname(spec);
	if (err)
		return err;
	if (cmds == Q_V1_GETQUOTA) {
		__kernel_time_t b = d.dqb_btime, i = d.dqb_itime;
		((struct user_dqblk32 *)&d)->dqb_itime = i;
		((struct user_dqblk32 *)&d)->dqb_btime = b;
		if (copy_to_user(addr, &d, sizeof (struct user_dqblk32)))
			return -EFAULT;
	}
	return 0;
}

static inline int put_statfs (struct statfs32 *ubuf, struct statfs *kbuf)
{
	int err;
	
	err = put_user (kbuf->f_type, &ubuf->f_type);
	err |= __put_user (kbuf->f_bsize, &ubuf->f_bsize);
	err |= __put_user (kbuf->f_blocks, &ubuf->f_blocks);
	err |= __put_user (kbuf->f_bfree, &ubuf->f_bfree);
	err |= __put_user (kbuf->f_bavail, &ubuf->f_bavail);
	err |= __put_user (kbuf->f_files, &ubuf->f_files);
	err |= __put_user (kbuf->f_ffree, &ubuf->f_ffree);
	err |= __put_user (kbuf->f_namelen, &ubuf->f_namelen);
	err |= __put_user (kbuf->f_fsid.val[0], &ubuf->f_fsid.val[0]);
	err |= __put_user (kbuf->f_fsid.val[1], &ubuf->f_fsid.val[1]);
	return err;
}

extern asmlinkage int sys_statfs(const char * path, struct statfs * buf);

asmlinkage int sys32_statfs(const char * path, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();
	char *pth;
	
	pth = getname (path);
	ret = PTR_ERR(pth);
	if (!IS_ERR(pth)) {
		set_fs (KERNEL_DS);
		ret = sys_statfs((const char *)pth, &s);
		set_fs (old_fs);
		putname (pth);
		if (put_statfs(buf, &s))
			return -EFAULT;
	}
	return ret;
}

extern asmlinkage int sys_fstatfs(unsigned int fd, struct statfs * buf);

asmlinkage int sys32_fstatfs(unsigned int fd, struct statfs32 *buf)
{
	int ret;
	struct statfs s;
	mm_segment_t old_fs = get_fs();
	
	set_fs (KERNEL_DS);
	ret = sys_fstatfs(fd, &s);
	set_fs (old_fs);
	if (put_statfs(buf, &s))
		return -EFAULT;
	return ret;
}

extern asmlinkage long sys_truncate(const char * path, unsigned long length);
extern asmlinkage long sys_ftruncate(unsigned int fd, unsigned long length);

asmlinkage int sys32_truncate64(const char * path, unsigned long high, unsigned long low)
{
	if ((int)high < 0)
		return -EINVAL;
	else
		return sys_truncate(path, (high << 32) | low);
}

asmlinkage int sys32_ftruncate64(unsigned int fd, unsigned long high, unsigned long low)
{
	if ((int)high < 0)
		return -EINVAL;
	else
		return sys_ftruncate(fd, (high << 32) | low);
}

extern asmlinkage int sys_utime(char * filename, struct utimbuf * times);

struct utimbuf32 {
	__kernel_time_t32 actime, modtime;
};

asmlinkage int sys32_utime(char * filename, struct utimbuf32 *times)
{
	struct utimbuf t;
	mm_segment_t old_fs;
	int ret;
	char *filenam;
	
	if (!times)
		return sys_utime(filename, NULL);
	if (get_user (t.actime, &times->actime) ||
	    __get_user (t.modtime, &times->modtime))
		return -EFAULT;
	filenam = getname (filename);
	ret = PTR_ERR(filenam);
	if (!IS_ERR(filenam)) {
		old_fs = get_fs();
		set_fs (KERNEL_DS); 
		ret = sys_utime(filenam, &t);
		set_fs (old_fs);
		putname (filenam);
	}
	return ret;
}

struct iovec32 { u32 iov_base; __kernel_size_t32 iov_len; };

typedef ssize_t (*io_fn_t)(struct file *, char *, size_t, loff_t *);
typedef ssize_t (*iov_fn_t)(struct file *, const struct iovec *, unsigned long, loff_t *);

static long do_readv_writev32(int type, struct file *file,
			      const struct iovec32 *vector, u32 count)
{
	unsigned long tot_len;
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov=iovstack, *ivp;
	struct inode *inode;
	long retval, i;
	io_fn_t fn;
	iov_fn_t fnv;

	/* First get the "struct iovec" from user memory and
	 * verify all the pointers
	 */
	if (!count)
		return 0;
	if (verify_area(VERIFY_READ, vector, sizeof(struct iovec32)*count))
		return -EFAULT;
	if (count > UIO_MAXIOV)
		return -EINVAL;
	if (count > UIO_FASTIOV) {
		iov = kmalloc(count*sizeof(struct iovec), GFP_KERNEL);
		if (!iov)
			return -ENOMEM;
	}

	tot_len = 0;
	i = count;
	ivp = iov;
	while(i > 0) {
		u32 len;
		u32 buf;

		__get_user(len, &vector->iov_len);
		__get_user(buf, &vector->iov_base);
		tot_len += len;
		ivp->iov_base = (void *)A(buf);
		ivp->iov_len = (__kernel_size_t) len;
		vector++;
		ivp++;
		i--;
	}

	inode = file->f_dentry->d_inode;
	/* VERIFY_WRITE actually means a read, as we write to user space */
	retval = locks_verify_area((type == VERIFY_WRITE
				    ? FLOCK_VERIFY_READ : FLOCK_VERIFY_WRITE),
				   inode, file, file->f_pos, tot_len);
	if (retval)
		goto out;

	/* VERIFY_WRITE actually means a read, as we write to user space */
	fnv = (type == VERIFY_WRITE ? file->f_op->readv : file->f_op->writev);
	if (fnv) {
		retval = fnv(file, iov, count, &file->f_pos);
		goto out;
	}

	fn = (type == VERIFY_WRITE ? file->f_op->read :
	      (io_fn_t) file->f_op->write);

	ivp = iov;
	while (count > 0) {
		void * base;
		int len, nr;

		base = ivp->iov_base;
		len = ivp->iov_len;
		ivp++;
		count--;
		nr = fn(file, base, len, &file->f_pos);
		if (nr < 0) {
			if (!retval)
				retval = nr;
			break;
		}
		retval += nr;
		if (nr != len)
			break;
	}
out:
	if (iov != iovstack)
		kfree(iov);

	return retval;
}

asmlinkage long sys32_readv(int fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	long ret = -EBADF;

	file = fget(fd);
	if(!file)
		goto bad_file;

	if (file->f_op && (file->f_mode & FMODE_READ) &&
	    (file->f_op->readv || file->f_op->read))
		ret = do_readv_writev32(VERIFY_WRITE, file, vector, count);
	fput(file);

bad_file:
	return ret;
}

asmlinkage long sys32_writev(int fd, struct iovec32 *vector, u32 count)
{
	struct file *file;
	int ret = -EBADF;

	file = fget(fd);
	if(!file)
		goto bad_file;
	if (file->f_op && (file->f_mode & FMODE_WRITE) &&
	    (file->f_op->writev || file->f_op->write))
		ret = do_readv_writev32(VERIFY_READ, file, vector, count);
	fput(file);

bad_file:
	return ret;
}

/* readdir & getdents */

#define NAME_OFFSET(de) ((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x) (((x)+sizeof(u32)-1) & ~(sizeof(u32)-1))

struct old_linux_dirent32 {
	u32		d_ino;
	u32		d_offset;
	unsigned short	d_namlen;
	char		d_name[1];
};

struct readdir_callback32 {
	struct old_linux_dirent32 * dirent;
	int count;
};

static int fillonedir(void * __buf, const char * name, int namlen,
		      loff_t offset, ino_t ino, unsigned int d_type)
{
	struct readdir_callback32 * buf = (struct readdir_callback32 *) __buf;
	struct old_linux_dirent32 * dirent;

	if (buf->count)
		return -EINVAL;
	buf->count++;
	dirent = buf->dirent;
	put_user(ino, &dirent->d_ino);
	put_user(offset, &dirent->d_offset);
	put_user(namlen, &dirent->d_namlen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	return 0;
}

asmlinkage int old32_readdir(unsigned int fd, struct old_linux_dirent32 *dirent, unsigned int count)
{
	int error = -EBADF;
	struct file * file;
	struct readdir_callback32 buf;

	file = fget(fd);
	if (!file)
		goto out;

	buf.count = 0;
	buf.dirent = dirent;

	error = vfs_readdir(file, fillonedir, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.count;

out_putf:
	fput(file);
out:
	return error;
}

struct linux_dirent32 {
	u32		d_ino;
	u32		d_off;
	unsigned short	d_reclen;
	char		d_name[1];
};

struct getdents_callback32 {
	struct linux_dirent32 * current_dir;
	struct linux_dirent32 * previous;
	int count;
	int error;
};

static int filldir(void * __buf, const char * name, int namlen, loff_t offset, ino_t ino,
		   unsigned int d_type)
{
	struct linux_dirent32 * dirent;
	struct getdents_callback32 * buf = (struct getdents_callback32 *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent)
		put_user(offset, &dirent->d_off);
	dirent = buf->current_dir;
	buf->previous = dirent;
	put_user(ino, &dirent->d_ino);
	put_user(reclen, &dirent->d_reclen);
	copy_to_user(dirent->d_name, name, namlen);
	put_user(0, dirent->d_name + namlen);
	((char *) dirent) += reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
}

asmlinkage int sys32_getdents(unsigned int fd, struct linux_dirent32 *dirent, unsigned int count)
{
	struct file * file;
	struct linux_dirent32 * lastdirent;
	struct getdents_callback32 buf;
	int error = -EBADF;

	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;

	error = vfs_readdir(file, filldir, &buf);
	if (error < 0)
		goto out_putf;
	lastdirent = buf.previous;
	error = buf.error;
	if(lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}
out_putf:
	fput(file);
out:
	return error;
}

/* end of readdir & getdents */

/*
 * Ooo, nasty.  We need here to frob 32-bit unsigned longs to
 * 64-bit unsigned longs.
 */

static inline int
get_fd_set32(unsigned long n, unsigned long *fdset, u32 *ufdset)
{
	if (ufdset) {
		unsigned long odd;

		if (verify_area(VERIFY_WRITE, ufdset, n*sizeof(u32)))
			return -EFAULT;

		odd = n & 1UL;
		n &= ~1UL;
		while (n) {
			unsigned long h, l;
			__get_user(l, ufdset);
			__get_user(h, ufdset+1);
			ufdset += 2;
			*fdset++ = h << 32 | l;
			n -= 2;
		}
		if (odd)
			__get_user(*fdset, ufdset);
	} else {
		/* Tricky, must clear full unsigned long in the
		 * kernel fdset at the end, this makes sure that
		 * actually happens.
		 */
		memset(fdset, 0, ((n + 1) & ~1)*sizeof(u32));
	}
	return 0;
}

static inline void
set_fd_set32(unsigned long n, u32 *ufdset, unsigned long *fdset)
{
	unsigned long odd;

	if (!ufdset)
		return;

	odd = n & 1UL;
	n &= ~1UL;
	while (n) {
		unsigned long h, l;
		l = *fdset++;
		h = l >> 32;
		__put_user(l, ufdset);
		__put_user(h, ufdset+1);
		ufdset += 2;
		n -= 2;
	}
	if (odd)
		__put_user(*fdset, ufdset);
}

#define MAX_SELECT_SECONDS \
	((unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)

asmlinkage int sys32_select(int n, u32 *inp, u32 *outp, u32 *exp, u32 tvp_x)
{
	fd_set_bits fds;
	struct timeval32 *tvp = (struct timeval32 *)AA(tvp_x);
	char *bits;
	unsigned long nn;
	long timeout;
	int ret, size;

	timeout = MAX_SCHEDULE_TIMEOUT;
	if (tvp) {
		int sec, usec;

		if ((ret = verify_area(VERIFY_READ, tvp, sizeof(*tvp)))
		    || (ret = __get_user(sec, &tvp->tv_sec))
		    || (ret = __get_user(usec, &tvp->tv_usec)))
			goto out_nofds;

		ret = -EINVAL;
		if(sec < 0 || usec < 0)
			goto out_nofds;

		if ((unsigned long) sec < MAX_SELECT_SECONDS) {
			timeout = (usec + 1000000/HZ - 1) / (1000000/HZ);
			timeout += sec * (unsigned long) HZ;
		}
	}

	ret = -EINVAL;
	if (n < 0)
		goto out_nofds;
	if (n > current->files->max_fdset)
		n = current->files->max_fdset;

	/*
	 * We need 6 bitmaps (in/out/ex for both incoming and outgoing),
	 * since we used fdset we need to allocate memory in units of
	 * long-words. 
	 */
	ret = -ENOMEM;
	size = FDS_BYTES(n);
	bits = kmalloc(6 * size, GFP_KERNEL);
	if (!bits)
		goto out_nofds;
	fds.in      = (unsigned long *)  bits;
	fds.out     = (unsigned long *) (bits +   size);
	fds.ex      = (unsigned long *) (bits + 2*size);
	fds.res_in  = (unsigned long *) (bits + 3*size);
	fds.res_out = (unsigned long *) (bits + 4*size);
	fds.res_ex  = (unsigned long *) (bits + 5*size);

	nn = (n + 8*sizeof(u32) - 1) / (8*sizeof(u32));
	if ((ret = get_fd_set32(nn, fds.in, inp)) ||
	    (ret = get_fd_set32(nn, fds.out, outp)) ||
	    (ret = get_fd_set32(nn, fds.ex, exp)))
		goto out;
	zero_fd_set(n, fds.res_in);
	zero_fd_set(n, fds.res_out);
	zero_fd_set(n, fds.res_ex);

	ret = do_select(n, &fds, &timeout);

	if (tvp && !(current->personality & STICKY_TIMEOUTS)) {
		int sec = 0, usec = 0;
		if (timeout) {
			sec = timeout / HZ;
			usec = timeout % HZ;
			usec *= (1000000/HZ);
		}
		put_user(sec, &tvp->tv_sec);
		put_user(usec, &tvp->tv_usec);
	}

	if (ret < 0)
		goto out;
	if (!ret) {
		ret = -ERESTARTNOHAND;
		if (signal_pending(current))
			goto out;
		ret = 0;
	}

	set_fd_set32(nn, inp, fds.res_in);
	set_fd_set32(nn, outp, fds.res_out);
	set_fd_set32(nn, exp, fds.res_ex);

out:
	kfree(bits);
out_nofds:
	return ret;
}

static int cp_new_stat32(struct inode *inode, struct stat32 *statbuf)
{
	unsigned long ino, blksize, blocks;
	kdev_t dev, rdev;
	umode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	off_t size;
	time_t atime, mtime, ctime;
	int err;

	/* Stream the loads of inode data into the load buffer,
	 * then we push it all into the store buffer below.  This
	 * should give optimal cache performance.
	 */
	ino = inode->i_ino;
	dev = inode->i_dev;
	mode = inode->i_mode;
	nlink = inode->i_nlink;
	uid = inode->i_uid;
	gid = inode->i_gid;
	rdev = inode->i_rdev;
	size = inode->i_size;
	atime = inode->i_atime;
	mtime = inode->i_mtime;
	ctime = inode->i_ctime;
	blksize = inode->i_blksize;
	blocks = inode->i_blocks;

	err  = put_user(kdev_t_to_nr(dev), &statbuf->st_dev);
	err |= put_user(ino, &statbuf->st_ino);
	err |= put_user(mode, &statbuf->st_mode);
	err |= put_user(nlink, &statbuf->st_nlink);
	err |= put_user(high2lowuid(uid), &statbuf->st_uid);
	err |= put_user(high2lowgid(gid), &statbuf->st_gid);
	err |= put_user(kdev_t_to_nr(rdev), &statbuf->st_rdev);
	err |= put_user(size, &statbuf->st_size);
	err |= put_user(atime, &statbuf->st_atime);
	err |= put_user(0, &statbuf->__unused1);
	err |= put_user(mtime, &statbuf->st_mtime);
	err |= put_user(0, &statbuf->__unused2);
	err |= put_user(ctime, &statbuf->st_ctime);
	err |= put_user(0, &statbuf->__unused3);
	if (blksize) {
		err |= put_user(blksize, &statbuf->st_blksize);
		err |= put_user(blocks, &statbuf->st_blocks);
	} else {
		unsigned int tmp_blocks;

#define D_B   7
#define I_B   (BLOCK_SIZE / sizeof(unsigned short))
		tmp_blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
		if (tmp_blocks > D_B) {
			unsigned int indirect;

			indirect = (tmp_blocks - D_B + I_B - 1) / I_B;
			tmp_blocks += indirect;
			if (indirect > 1) {
				indirect = (indirect - 1 + I_B - 1) / I_B;
				tmp_blocks += indirect;
				if (indirect > 1)
					tmp_blocks++;
			}
		}
		err |= put_user(BLOCK_SIZE, &statbuf->st_blksize);
		err |= put_user((BLOCK_SIZE / 512) * tmp_blocks, &statbuf->st_blocks);
#undef D_B
#undef I_B
	}
/* fixme
	err |= put_user(0, &statbuf->__unused4[0]);
	err |= put_user(0, &statbuf->__unused4[1]);
*/

	return err;
}

/* Perhaps this belongs in fs.h or similar. -DaveM */
static __inline__ int
do_revalidate(struct dentry *dentry)
{
	struct inode * inode = dentry->d_inode;
	if (inode->i_op && inode->i_op->revalidate)
		return inode->i_op->revalidate(dentry);
	return 0;
}

asmlinkage int sys32_newstat(char * filename, struct stat32 *statbuf)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_new_stat32(nd.dentry->d_inode, statbuf);
		path_release(&nd);
	}
	return error;
}

asmlinkage int sys32_newlstat(char * filename, struct stat32 *statbuf)
{
	struct nameidata nd;
	int error;

	error = user_path_walk_link(filename, &nd);
	if (!error) {
		error = do_revalidate(nd.dentry);
		if (!error)
			error = cp_new_stat32(nd.dentry->d_inode, statbuf);

		path_release(&nd);
	}
	return error;
}

asmlinkage int sys32_newfstat(unsigned int fd, struct stat32 *statbuf)
{
	struct file *f;
	int err = -EBADF;

	f = fget(fd);
	if (f) {
		struct dentry * dentry = f->f_dentry;

		err = do_revalidate(dentry);
		if (!err)
			err = cp_new_stat32(dentry->d_inode, statbuf);
		fput(f);
	}
	return err;
}

extern asmlinkage int sys_sysfs(int option, unsigned long arg1, unsigned long arg2);

asmlinkage int sys32_sysfs(int option, u32 arg1, u32 arg2)
{
	return sys_sysfs(option, arg1, arg2);
}

struct ncp_mount_data32 {
        int version;
        unsigned int ncp_fd;
        __kernel_uid_t32 mounted_uid;
        __kernel_pid_t32 wdog_pid;
        unsigned char mounted_vol[NCP_VOLNAME_LEN + 1];
        unsigned int time_out;
        unsigned int retry_count;
        unsigned int flags;
        __kernel_uid_t32 uid;
        __kernel_gid_t32 gid;
        __kernel_mode_t32 file_mode;
        __kernel_mode_t32 dir_mode;
};

static void *do_ncp_super_data_conv(void *raw_data)
{
	struct ncp_mount_data *n = (struct ncp_mount_data *)raw_data;
	struct ncp_mount_data32 *n32 = (struct ncp_mount_data32 *)raw_data;

	n->dir_mode = n32->dir_mode;
	n->file_mode = n32->file_mode;
	n->gid = low2highgid(n32->gid);
	n->uid = low2highuid(n32->uid);
	memmove (n->mounted_vol, n32->mounted_vol, (sizeof (n32->mounted_vol) + 3 * sizeof (unsigned int)));
	n->wdog_pid = n32->wdog_pid;
	n->mounted_uid = low2highuid(n32->mounted_uid);
	return raw_data;
}

struct smb_mount_data32 {
        int version;
        __kernel_uid_t32 mounted_uid;
        __kernel_uid_t32 uid;
        __kernel_gid_t32 gid;
        __kernel_mode_t32 file_mode;
        __kernel_mode_t32 dir_mode;
};

static void *do_smb_super_data_conv(void *raw_data)
{
	struct smb_mount_data *s = (struct smb_mount_data *)raw_data;
	struct smb_mount_data32 *s32 = (struct smb_mount_data32 *)raw_data;

	s->version = s32->version;
	s->mounted_uid = low2highuid(s32->mounted_uid);
	s->uid = low2highuid(s32->uid);
	s->gid = low2highgid(s32->gid);
	s->file_mode = s32->file_mode;
	s->dir_mode = s32->dir_mode;
	return raw_data;
}

static int copy_mount_stuff_to_kernel(const void *user, unsigned long *kernel)
{
	int i;
	unsigned long page;
	struct vm_area_struct *vma;

	*kernel = 0;
	if(!user)
		return 0;
	vma = find_vma(current->mm, (unsigned long)user);
	if(!vma || (unsigned long)user < vma->vm_start)
		return -EFAULT;
	if(!(vma->vm_flags & VM_READ))
		return -EFAULT;
	i = vma->vm_end - (unsigned long) user;
	if(PAGE_SIZE <= (unsigned long) i)
		i = PAGE_SIZE - 1;
	if(!(page = __get_free_page(GFP_KERNEL)))
		return -ENOMEM;
	if(copy_from_user((void *) page, user, i)) {
		free_page(page);
		return -EFAULT;
	}
	*kernel = page;
	return 0;
}

#define SMBFS_NAME	"smbfs"
#define NCPFS_NAME	"ncpfs"

asmlinkage int sys32_mount(char *dev_name, char *dir_name, char *type, unsigned long new_flags, u32 data)
{
	unsigned long type_page = 0;
	unsigned long data_page = 0;
	unsigned long dev_page = 0;
	unsigned long dir_page = 0;
	int err, is_smb, is_ncp;

	is_smb = is_ncp = 0;

	err = copy_mount_stuff_to_kernel((const void *)type, &type_page);
	if (err)
		goto out;

	if (!type_page) {
		err = -EINVAL;
		goto out;
	}

	is_smb = !strcmp((char *)type_page, SMBFS_NAME);
	is_ncp = !strcmp((char *)type_page, NCPFS_NAME);

	err = copy_mount_stuff_to_kernel((const void *)AA(data), &data_page);
	if (err)
		goto type_out;

	err = copy_mount_stuff_to_kernel(dev_name, &dev_page);
	if (err)
		goto data_out;

	err = copy_mount_stuff_to_kernel(dir_name, &dir_page);
	if (err)
		goto dev_out;

	if (!is_smb && !is_ncp) {
		lock_kernel();
		err = do_mount((char*)dev_page, (char*)dir_page,
				(char*)type_page, new_flags, (char*)data_page);
		unlock_kernel();
	} else {
		if (is_ncp)
			do_ncp_super_data_conv((void *)data_page);
		else
			do_smb_super_data_conv((void *)data_page);

		lock_kernel();
		err = do_mount((char*)dev_page, (char*)dir_page,
				(char*)type_page, new_flags, (char*)data_page);
		unlock_kernel();
	}
	free_page(dir_page);

dev_out:
	free_page(dev_page);

data_out:
	free_page(data_page);

type_out:
	free_page(type_page);

out:
	return err;
}

struct rusage32 {
        struct timeval32 ru_utime;
        struct timeval32 ru_stime;
        s32    ru_maxrss;
        s32    ru_ixrss;
        s32    ru_idrss;
        s32    ru_isrss;
        s32    ru_minflt;
        s32    ru_majflt;
        s32    ru_nswap;
        s32    ru_inblock;
        s32    ru_oublock;
        s32    ru_msgsnd; 
        s32    ru_msgrcv; 
        s32    ru_nsignals;
        s32    ru_nvcsw;
        s32    ru_nivcsw;
};

static int put_rusage (struct rusage32 *ru, struct rusage *r)
{
	int err;
	
	err = put_user (r->ru_utime.tv_sec, &ru->ru_utime.tv_sec);
	err |= __put_user (r->ru_utime.tv_usec, &ru->ru_utime.tv_usec);
	err |= __put_user (r->ru_stime.tv_sec, &ru->ru_stime.tv_sec);
	err |= __put_user (r->ru_stime.tv_usec, &ru->ru_stime.tv_usec);
	err |= __put_user (r->ru_maxrss, &ru->ru_maxrss);
	err |= __put_user (r->ru_ixrss, &ru->ru_ixrss);
	err |= __put_user (r->ru_idrss, &ru->ru_idrss);
	err |= __put_user (r->ru_isrss, &ru->ru_isrss);
	err |= __put_user (r->ru_minflt, &ru->ru_minflt);
	err |= __put_user (r->ru_majflt, &ru->ru_majflt);
	err |= __put_user (r->ru_nswap, &ru->ru_nswap);
	err |= __put_user (r->ru_inblock, &ru->ru_inblock);
	err |= __put_user (r->ru_oublock, &ru->ru_oublock);
	err |= __put_user (r->ru_msgsnd, &ru->ru_msgsnd);
	err |= __put_user (r->ru_msgrcv, &ru->ru_msgrcv);
	err |= __put_user (r->ru_nsignals, &ru->ru_nsignals);
	err |= __put_user (r->ru_nvcsw, &ru->ru_nvcsw);
	err |= __put_user (r->ru_nivcsw, &ru->ru_nivcsw);
	return err;
}

asmlinkage int sys32_wait4(__kernel_pid_t32 pid, unsigned int *stat_addr, int options, struct rusage32 *ru)
{
	if (!ru)
		return sys_wait4(pid, stat_addr, options, NULL);
	else {
		struct rusage r;
		int ret;
		unsigned int status;
		mm_segment_t old_fs = get_fs();
		
		set_fs (KERNEL_DS);
		ret = sys_wait4(pid, stat_addr ? &status : NULL, options, &r);
		set_fs (old_fs);
		if (put_rusage (ru, &r)) return -EFAULT;
		if (stat_addr && put_user (status, stat_addr))
			return -EFAULT;
		return ret;
	}
}

struct sysinfo32 {
        s32 uptime;
        u32 loads[3];
        u32 totalram;
        u32 freeram;
        u32 sharedram;
        u32 bufferram;
        u32 totalswap;
        u32 freeswap;
        unsigned short procs;
	unsigned short pad;
	u32 totalhigh;
	u32 freehigh;
	unsigned int mem_unit;
        char _f[8];
};

extern asmlinkage int sys_sysinfo(struct sysinfo *info);

asmlinkage int sys32_sysinfo(struct sysinfo32 *info)
{
	struct sysinfo s;
	int ret, err;
	mm_segment_t old_fs = get_fs ();
	
	set_fs (KERNEL_DS);
	ret = sys_sysinfo(&s);
	set_fs (old_fs);
	err = put_user (s.uptime, &info->uptime);
	err |= __put_user (s.loads[0], &info->loads[0]);
	err |= __put_user (s.loads[1], &info->loads[1]);
	err |= __put_user (s.loads[2], &info->loads[2]);
	err |= __put_user (s.totalram, &info->totalram);
	err |= __put_user (s.freeram, &info->freeram);
	err |= __put_user (s.sharedram, &info->sharedram);
	err |= __put_user (s.bufferram, &info->bufferram);
	err |= __put_user (s.totalswap, &info->totalswap);
	err |= __put_user (s.freeswap, &info->freeswap);
	err |= __put_user (s.procs, &info->procs);
	err |= __put_user (s.totalhigh, &info->totalhigh);
	err |= __put_user (s.freehigh, &info->freehigh);
	err |= __put_user (s.mem_unit, &info->mem_unit);
	if (err)
		return -EFAULT;
	return ret;
}

struct timespec32 {
	s32    tv_sec;
	s32    tv_nsec;
};
                
extern asmlinkage int sys_sched_rr_get_interval(pid_t pid, struct timespec *interval);

asmlinkage int sys32_sched_rr_get_interval(__kernel_pid_t32 pid, struct timespec32 *interval)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	set_fs (KERNEL_DS);
	ret = sys_sched_rr_get_interval(pid, &t);
	set_fs (old_fs);
	if (put_user (t.tv_sec, &interval->tv_sec) ||
	    __put_user (t.tv_nsec, &interval->tv_nsec))
		return -EFAULT;
	return ret;
}

extern asmlinkage int sys_nanosleep(struct timespec *rqtp, struct timespec *rmtp);

asmlinkage int sys32_nanosleep(struct timespec32 *rqtp, struct timespec32 *rmtp)
{
	struct timespec t;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	if (get_user (t.tv_sec, &rqtp->tv_sec) ||
	    __get_user (t.tv_nsec, &rqtp->tv_nsec))
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_nanosleep(&t, rmtp ? &t : NULL);
	set_fs (old_fs);
	if (rmtp && ret == -EINTR) {
		if (__put_user (t.tv_sec, &rmtp->tv_sec) ||
	    	    __put_user (t.tv_nsec, &rmtp->tv_nsec))
			return -EFAULT;
	}
	return ret;
}

extern asmlinkage int sys_sigprocmask(int how, old_sigset_t *set, old_sigset_t *oset);

asmlinkage int sys32_sigprocmask(int how, old_sigset_t32 *set, old_sigset_t32 *oset)
{
	old_sigset_t s;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	if (set && get_user (s, set)) return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_sigprocmask(how, set ? &s : NULL, oset ? &s : NULL);
	set_fs (old_fs);
	if (ret) return ret;
	if (oset && put_user (s, oset)) return -EFAULT;
	return 0;
}

extern asmlinkage int sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset, size_t sigsetsize);

asmlinkage int sys32_rt_sigprocmask(int how, sigset_t32 *set, sigset_t32 *oset, __kernel_size_t32 sigsetsize)
{
	sigset_t s;
	sigset_t32 s32;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	if (set) {
		if (copy_from_user (&s32, set, sizeof(sigset_t32)))
			return -EFAULT;
		switch (_NSIG_WORDS) {
		case 4: s.sig[3] = s32.sig[6] | (((long)s32.sig[7]) << 32);
		case 3: s.sig[2] = s32.sig[4] | (((long)s32.sig[5]) << 32);
		case 2: s.sig[1] = s32.sig[2] | (((long)s32.sig[3]) << 32);
		case 1: s.sig[0] = s32.sig[0] | (((long)s32.sig[1]) << 32);
		}
	}
	set_fs (KERNEL_DS);
	ret = sys_rt_sigprocmask(how, set ? &s : NULL, oset ? &s : NULL, sigsetsize);
	set_fs (old_fs);
	if (ret) return ret;
	if (oset) {
		switch (_NSIG_WORDS) {
		case 4: s32.sig[7] = (s.sig[3] >> 32); s32.sig[6] = s.sig[3];
		case 3: s32.sig[5] = (s.sig[2] >> 32); s32.sig[4] = s.sig[2];
		case 2: s32.sig[3] = (s.sig[1] >> 32); s32.sig[2] = s.sig[1];
		case 1: s32.sig[1] = (s.sig[0] >> 32); s32.sig[0] = s.sig[0];
		}
		if (copy_to_user (oset, &s32, sizeof(sigset_t32)))
			return -EFAULT;
	}
	return 0;
}

extern asmlinkage int sys_sigpending(old_sigset_t *set);

asmlinkage int sys32_sigpending(old_sigset_t32 *set)
{
	old_sigset_t s;
	int ret;
	mm_segment_t old_fs = get_fs();
		
	set_fs (KERNEL_DS);
	ret = sys_sigpending(&s);
	set_fs (old_fs);
	if (put_user (s, set)) return -EFAULT;
	return ret;
}

extern asmlinkage int sys_rt_sigpending(sigset_t *set, size_t sigsetsize);

asmlinkage int sys32_rt_sigpending(sigset_t32 *set, __kernel_size_t32 sigsetsize)
{
	sigset_t s;
	sigset_t32 s32;
	int ret;
	mm_segment_t old_fs = get_fs();
		
	set_fs (KERNEL_DS);
	ret = sys_rt_sigpending(&s, sigsetsize);
	set_fs (old_fs);
	if (!ret) {
		switch (_NSIG_WORDS) {
		case 4: s32.sig[7] = (s.sig[3] >> 32); s32.sig[6] = s.sig[3];
		case 3: s32.sig[5] = (s.sig[2] >> 32); s32.sig[4] = s.sig[2];
		case 2: s32.sig[3] = (s.sig[1] >> 32); s32.sig[2] = s.sig[1];
		case 1: s32.sig[1] = (s.sig[0] >> 32); s32.sig[0] = s.sig[0];
		}
		if (copy_to_user (set, &s32, sizeof(sigset_t32)))
			return -EFAULT;
	}
	return ret;
}

extern int
copy_siginfo_to_user32(siginfo_t32 *to, siginfo_t *from);

asmlinkage int
sys32_rt_sigtimedwait(sigset_t32 *uthese, siginfo_t32 *uinfo,
		      struct timespec32 *uts, __kernel_size_t32 sigsetsize)
{
	int ret, sig;
	sigset_t these;
	sigset_t32 these32;
	struct timespec ts;
	siginfo_t info;
	long timeout = 0;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user (&these32, uthese, sizeof(sigset_t32)))
		return -EFAULT;

	switch (_NSIG_WORDS) {
	case 4: these.sig[3] = these32.sig[6] | (((long)these32.sig[7]) << 32);
	case 3: these.sig[2] = these32.sig[4] | (((long)these32.sig[5]) << 32);
	case 2: these.sig[1] = these32.sig[2] | (((long)these32.sig[3]) << 32);
	case 1: these.sig[0] = these32.sig[0] | (((long)these32.sig[1]) << 32);
	}
		
	/*
	 * Invert the set of allowed signals to get those we
	 * want to block.
	 */
	sigdelsetmask(&these, sigmask(SIGKILL)|sigmask(SIGSTOP));
	signotset(&these);

	if (uts) {
		if (get_user (ts.tv_sec, &uts->tv_sec) ||
		    get_user (ts.tv_nsec, &uts->tv_nsec))
			return -EINVAL;
		if (ts.tv_nsec >= 1000000000L || ts.tv_nsec < 0
		    || ts.tv_sec < 0)
			return -EINVAL;
	}

	spin_lock_irq(&current->sigmask_lock);
	sig = dequeue_signal(&these, &info);
	if (!sig) {
		/* None ready -- temporarily unblock those we're interested
		   in so that we'll be awakened when they arrive.  */
		sigset_t oldblocked = current->blocked;
		sigandsets(&current->blocked, &current->blocked, &these);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		timeout = MAX_SCHEDULE_TIMEOUT;
		if (uts)
			timeout = (timespec_to_jiffies(&ts)
				   + (ts.tv_sec || ts.tv_nsec));

		current->state = TASK_INTERRUPTIBLE;
		timeout = schedule_timeout(timeout);

		spin_lock_irq(&current->sigmask_lock);
		sig = dequeue_signal(&these, &info);
		current->blocked = oldblocked;
		recalc_sigpending(current);
	}
	spin_unlock_irq(&current->sigmask_lock);

	if (sig) {
		ret = sig;
		if (uinfo) {
			if (copy_siginfo_to_user32(uinfo, &info))
				ret = -EFAULT;
		}
	} else {
		ret = -EAGAIN;
		if (timeout)
			ret = -EINTR;
	}

	return ret;
}

extern asmlinkage int
sys_rt_sigqueueinfo(int pid, int sig, siginfo_t *uinfo);

asmlinkage int
sys32_rt_sigqueueinfo(int pid, int sig, siginfo_t32 *uinfo)
{
	siginfo_t info;
	int ret;
	mm_segment_t old_fs = get_fs();
	
	if (copy_from_user (&info, uinfo, 3*sizeof(int)) ||
	    copy_from_user (info._sifields._pad, uinfo->_sifields._pad, SI_PAD_SIZE))
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_rt_sigqueueinfo(pid, sig, &info);
	set_fs (old_fs);
	return ret;
}

struct tms32 {
	__kernel_clock_t32 tms_utime;
	__kernel_clock_t32 tms_stime;
	__kernel_clock_t32 tms_cutime;
	__kernel_clock_t32 tms_cstime;
};
                                
extern asmlinkage long sys_times(struct tms * tbuf);

asmlinkage long sys32_times(struct tms32 *tbuf)
{
	struct tms t;
	long ret;
	mm_segment_t old_fs = get_fs ();
	int err;
	
	set_fs (KERNEL_DS);
	ret = sys_times(tbuf ? &t : NULL);
	set_fs (old_fs);
	if (tbuf) {
		err = put_user (t.tms_utime, &tbuf->tms_utime);
		err |= __put_user (t.tms_stime, &tbuf->tms_stime);
		err |= __put_user (t.tms_cutime, &tbuf->tms_cutime);
		err |= __put_user (t.tms_cstime, &tbuf->tms_cstime);
		if (err)
			ret = -EFAULT;
	}
	return ret;
}

#define RLIM_OLD_INFINITY32	0x7fffffff
#define RLIM_INFINITY32		0xffffffff
#define RESOURCE32_OLD(x)	((x > RLIM_OLD_INFINITY32) ? RLIM_OLD_INFINITY32 : x)
#define RESOURCE32(x) 		((x > RLIM_INFINITY32) ? RLIM_INFINITY32 : x)

struct rlimit32 {
	u32	rlim_cur;
	u32	rlim_max;
};

extern asmlinkage long sys_getrlimit(unsigned int resource, struct rlimit *rlim);

asmlinkage int sys32_old_getrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	set_fs (KERNEL_DS);
	ret = sys_getrlimit(resource, &r);
	set_fs (old_fs);
	if (!ret) {
		ret = put_user (RESOURCE32_OLD(r.rlim_cur), &rlim->rlim_cur);
		ret |= __put_user (RESOURCE32_OLD(r.rlim_max), &rlim->rlim_max);
	}
	return ret;
}

asmlinkage int sys32_getrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs ();
	
	set_fs (KERNEL_DS);
	ret = sys_getrlimit(resource, &r);
	set_fs (old_fs);
	if (!ret) {
		ret = put_user (RESOURCE32(r.rlim_cur), &rlim->rlim_cur);
		ret |= __put_user (RESOURCE32(r.rlim_max), &rlim->rlim_max);
	}
	return ret;
}

extern asmlinkage int sys_setrlimit(unsigned int resource, struct rlimit *rlim);

asmlinkage int sys32_setrlimit(unsigned int resource, struct rlimit32 *rlim)
{
	struct rlimit r;
	int ret;
	mm_segment_t old_fs = get_fs ();

	if (resource >= RLIM_NLIMITS) return -EINVAL;	
	if (get_user (r.rlim_cur, &rlim->rlim_cur) ||
	    __get_user (r.rlim_max, &rlim->rlim_max))
		return -EFAULT;
	if (r.rlim_cur == RLIM_INFINITY32)
		r.rlim_cur = RLIM_INFINITY;
	if (r.rlim_max == RLIM_INFINITY32)
		r.rlim_max = RLIM_INFINITY;
	set_fs (KERNEL_DS);
	ret = sys_setrlimit(resource, &r);
	set_fs (old_fs);
	return ret;
}

extern asmlinkage int sys_getrusage(int who, struct rusage *ru);

asmlinkage int sys32_getrusage(int who, struct rusage32 *ru)
{
	struct rusage r;
	int ret;
	mm_segment_t old_fs = get_fs();
		
	set_fs (KERNEL_DS);
	ret = sys_getrusage(who, &r);
	set_fs (old_fs);
	if (put_rusage (ru, &r)) return -EFAULT;
	return ret;
}

/* XXX This really belongs in some header file... -DaveM */
#define MAX_SOCK_ADDR	128		/* 108 for Unix domain - 
					   16 for IP, 16 for IPX,
					   24 for IPv6,
					   about 80 for AX.25 */

extern struct socket *sockfd_lookup(int fd, int *err);

/* XXX This as well... */
extern __inline__ void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

struct msghdr32 {
        u32               msg_name;
        int               msg_namelen;
        u32               msg_iov;
        __kernel_size_t32 msg_iovlen;
        u32               msg_control;
        __kernel_size_t32 msg_controllen;
        unsigned          msg_flags;
};

struct cmsghdr32 {
        __kernel_size_t32 cmsg_len;
        int               cmsg_level;
        int               cmsg_type;
};

/* Bleech... */
#define __CMSG32_NXTHDR(ctl, len, cmsg, cmsglen) __cmsg32_nxthdr((ctl),(len),(cmsg),(cmsglen))
#define CMSG32_NXTHDR(mhdr, cmsg, cmsglen) cmsg32_nxthdr((mhdr), (cmsg), (cmsglen))

#define CMSG32_ALIGN(len) ( ((len)+sizeof(int)-1) & ~(sizeof(int)-1) )

#define CMSG32_DATA(cmsg)	((void *)((char *)(cmsg) + CMSG32_ALIGN(sizeof(struct cmsghdr32))))
#define CMSG32_SPACE(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + CMSG32_ALIGN(len))
#define CMSG32_LEN(len) (CMSG32_ALIGN(sizeof(struct cmsghdr32)) + (len))

#define __CMSG32_FIRSTHDR(ctl,len) ((len) >= sizeof(struct cmsghdr32) ? \
				    (struct cmsghdr32 *)(ctl) : \
				    (struct cmsghdr32 *)NULL)
#define CMSG32_FIRSTHDR(msg)	__CMSG32_FIRSTHDR((msg)->msg_control, (msg)->msg_controllen)

__inline__ struct cmsghdr32 *__cmsg32_nxthdr(void *__ctl, __kernel_size_t __size,
					      struct cmsghdr32 *__cmsg, int __cmsg_len)
{
	struct cmsghdr32 * __ptr;

	__ptr = (struct cmsghdr32 *)(((unsigned char *) __cmsg) +
				     CMSG32_ALIGN(__cmsg_len));
	if ((unsigned long)((char*)(__ptr+1) - (char *) __ctl) > __size)
		return NULL;

	return __ptr;
}

__inline__ struct cmsghdr32 *cmsg32_nxthdr (struct msghdr *__msg,
					    struct cmsghdr32 *__cmsg,
					    int __cmsg_len)
{
	return __cmsg32_nxthdr(__msg->msg_control, __msg->msg_controllen,
			       __cmsg, __cmsg_len);
}

static inline int iov_from_user32_to_kern(struct iovec *kiov,
					  struct iovec32 *uiov32,
					  int niov)
{
	int tot_len = 0;

	while(niov > 0) {
		u32 len, buf;

		if(get_user(len, &uiov32->iov_len) ||
		   get_user(buf, &uiov32->iov_base)) {
			tot_len = -EFAULT;
			break;
		}
		tot_len += len;
		kiov->iov_base = (void *)A(buf);
		kiov->iov_len = (__kernel_size_t) len;
		uiov32++;
		kiov++;
		niov--;
	}
	return tot_len;
}

static inline int msghdr_from_user32_to_kern(struct msghdr *kmsg,
					     struct msghdr32 *umsg)
{
	u32 tmp1, tmp2, tmp3;
	int err;

	err = get_user(tmp1, &umsg->msg_name);
	err |= __get_user(tmp2, &umsg->msg_iov);
	err |= __get_user(tmp3, &umsg->msg_control);
	if (err)
		return -EFAULT;

	kmsg->msg_name = (void *)A(tmp1);
	kmsg->msg_iov = (struct iovec *)A(tmp2);
	kmsg->msg_control = (void *)A(tmp3);

	err = get_user(kmsg->msg_namelen, &umsg->msg_namelen);
	err |= get_user(kmsg->msg_iovlen, &umsg->msg_iovlen);
	err |= get_user(kmsg->msg_controllen, &umsg->msg_controllen);
	err |= get_user(kmsg->msg_flags, &umsg->msg_flags);
	
	return err;
}

/* I've named the args so it is easy to tell whose space the pointers are in. */
static int verify_iovec32(struct msghdr *kern_msg, struct iovec *kern_iov,
			  char *kern_address, int mode)
{
	int tot_len;

	if(kern_msg->msg_namelen) {
		if(mode==VERIFY_READ) {
			int err = move_addr_to_kernel(kern_msg->msg_name,
						      kern_msg->msg_namelen,
						      kern_address);
			if(err < 0)
				return err;
		}
		kern_msg->msg_name = kern_address;
	} else
		kern_msg->msg_name = NULL;

	if(kern_msg->msg_iovlen > UIO_FASTIOV) {
		kern_iov = kmalloc(kern_msg->msg_iovlen * sizeof(struct iovec),
				   GFP_KERNEL);
		if(!kern_iov)
			return -ENOMEM;
	}

	tot_len = iov_from_user32_to_kern(kern_iov,
					  (struct iovec32 *)kern_msg->msg_iov,
					  kern_msg->msg_iovlen);
	if(tot_len >= 0)
		kern_msg->msg_iov = kern_iov;
	else if(kern_msg->msg_iovlen > UIO_FASTIOV)
		kfree(kern_iov);

	return tot_len;
}

/* There is a lot of hair here because the alignment rules (and
 * thus placement) of cmsg headers and length are different for
 * 32-bit apps.  -DaveM
 */
static int cmsghdr_from_user32_to_kern(struct msghdr *kmsg,
				       unsigned char *stackbuf, int stackbuf_size)
{
	struct cmsghdr32 *ucmsg;
	struct cmsghdr *kcmsg, *kcmsg_base;
	__kernel_size_t32 ucmlen;
	__kernel_size_t kcmlen, tmp;

	kcmlen = 0;
	kcmsg_base = kcmsg = (struct cmsghdr *)stackbuf;
	ucmsg = CMSG32_FIRSTHDR(kmsg);
	while(ucmsg != NULL) {
		if(get_user(ucmlen, &ucmsg->cmsg_len))
			return -EFAULT;

		/* Catch bogons. */
		if(CMSG32_ALIGN(ucmlen) <
		   CMSG32_ALIGN(sizeof(struct cmsghdr32)))
			return -EINVAL;
		if((unsigned long)(((char *)ucmsg - (char *)kmsg->msg_control)
				   + ucmlen) > kmsg->msg_controllen)
			return -EINVAL;

		tmp = ((ucmlen - CMSG32_ALIGN(sizeof(*ucmsg))) +
		       CMSG_ALIGN(sizeof(struct cmsghdr)));
		kcmlen += tmp;
		ucmsg = CMSG32_NXTHDR(kmsg, ucmsg, ucmlen);
	}
	if(kcmlen == 0)
		return -EINVAL;

	/* The kcmlen holds the 64-bit version of the control length.
	 * It may not be modified as we do not stick it into the kmsg
	 * until we have successfully copied over all of the data
	 * from the user.
	 */
	if(kcmlen > stackbuf_size)
		kcmsg_base = kcmsg = kmalloc(kcmlen, GFP_KERNEL);
	if(kcmsg == NULL)
		return -ENOBUFS;

	/* Now copy them over neatly. */
	memset(kcmsg, 0, kcmlen);
	ucmsg = CMSG32_FIRSTHDR(kmsg);
	while(ucmsg != NULL) {
		__get_user(ucmlen, &ucmsg->cmsg_len);
		tmp = ((ucmlen - CMSG32_ALIGN(sizeof(*ucmsg))) +
		       CMSG_ALIGN(sizeof(struct cmsghdr)));
		kcmsg->cmsg_len = tmp;
		__get_user(kcmsg->cmsg_level, &ucmsg->cmsg_level);
		__get_user(kcmsg->cmsg_type, &ucmsg->cmsg_type);

		/* Copy over the data. */
		if(copy_from_user(CMSG_DATA(kcmsg),
				  CMSG32_DATA(ucmsg),
				  (ucmlen - CMSG32_ALIGN(sizeof(*ucmsg)))))
			goto out_free_efault;

		/* Advance. */
		kcmsg = (struct cmsghdr *)((char *)kcmsg + CMSG_ALIGN(tmp));
		ucmsg = CMSG32_NXTHDR(kmsg, ucmsg, ucmlen);
	}

	/* Ok, looks like we made it.  Hook it up and return success. */
	kmsg->msg_control = kcmsg_base;
	kmsg->msg_controllen = kcmlen;
	return 0;

out_free_efault:
	if(kcmsg_base != (struct cmsghdr *)stackbuf)
		kfree(kcmsg_base);
	return -EFAULT;
}

static void put_cmsg32(struct msghdr *kmsg, int level, int type,
		       int len, void *data)
{
	struct cmsghdr32 *cm = (struct cmsghdr32 *) kmsg->msg_control;
	struct cmsghdr32 cmhdr;
	int cmlen = CMSG32_LEN(len);

	if(cm == NULL || kmsg->msg_controllen < sizeof(*cm)) {
		kmsg->msg_flags |= MSG_CTRUNC;
		return;
	}

	if(kmsg->msg_controllen < cmlen) {
		kmsg->msg_flags |= MSG_CTRUNC;
		cmlen = kmsg->msg_controllen;
	}
	cmhdr.cmsg_level = level;
	cmhdr.cmsg_type = type;
	cmhdr.cmsg_len = cmlen;

	if(copy_to_user(cm, &cmhdr, sizeof cmhdr))
		return;
	if(copy_to_user(CMSG32_DATA(cm), data, cmlen - sizeof(struct cmsghdr32)))
		return;
	cmlen = CMSG32_SPACE(len);
	kmsg->msg_control += cmlen;
	kmsg->msg_controllen -= cmlen;
}

static void scm_detach_fds32(struct msghdr *kmsg, struct scm_cookie *scm)
{
	struct cmsghdr32 *cm = (struct cmsghdr32 *) kmsg->msg_control;
	int fdmax = (kmsg->msg_controllen - sizeof(struct cmsghdr32)) / sizeof(int);
	int fdnum = scm->fp->count;
	struct file **fp = scm->fp->fp;
	int *cmfptr;
	int err = 0, i;

	if (fdnum < fdmax)
		fdmax = fdnum;

	for (i = 0, cmfptr = (int *) CMSG32_DATA(cm); i < fdmax; i++, cmfptr++) {
		int new_fd;
		err = get_unused_fd();
		if (err < 0)
			break;
		new_fd = err;
		err = put_user(new_fd, cmfptr);
		if (err) {
			put_unused_fd(new_fd);
			break;
		}
		/* Bump the usage count and install the file. */
		get_file(fp[i]);
		fd_install(new_fd, fp[i]);
	}

	if (i > 0) {
		int cmlen = CMSG32_LEN(i * sizeof(int));
		if (!err)
			err = put_user(SOL_SOCKET, &cm->cmsg_level);
		if (!err)
			err = put_user(SCM_RIGHTS, &cm->cmsg_type);
		if (!err)
			err = put_user(cmlen, &cm->cmsg_len);
		if (!err) {
			cmlen = CMSG32_SPACE(i * sizeof(int));
			kmsg->msg_control += cmlen;
			kmsg->msg_controllen -= cmlen;
		}
	}
	if (i < fdnum)
		kmsg->msg_flags |= MSG_CTRUNC;

	/*
	 * All of the files that fit in the message have had their
	 * usage counts incremented, so we just free the list.
	 */
	__scm_destroy(scm);
}

/* In these cases we (currently) can just copy to data over verbatim
 * because all CMSGs created by the kernel have well defined types which
 * have the same layout in both the 32-bit and 64-bit API.  One must add
 * some special cased conversions here if we start sending control messages
 * with incompatible types.
 *
 * SCM_RIGHTS and SCM_CREDENTIALS are done by hand in recvmsg32 right after
 * we do our work.  The remaining cases are:
 *
 * SOL_IP	IP_PKTINFO	struct in_pktinfo	32-bit clean
 *		IP_TTL		int			32-bit clean
 *		IP_TOS		__u8			32-bit clean
 *		IP_RECVOPTS	variable length		32-bit clean
 *		IP_RETOPTS	variable length		32-bit clean
 *		(these last two are clean because the types are defined
 *		 by the IPv4 protocol)
 *		IP_RECVERR	struct sock_extended_err +
 *				struct sockaddr_in	32-bit clean
 * SOL_IPV6	IPV6_RECVERR	struct sock_extended_err +
 *				struct sockaddr_in6	32-bit clean
 *		IPV6_PKTINFO	struct in6_pktinfo	32-bit clean
 *		IPV6_HOPLIMIT	int			32-bit clean
 *		IPV6_FLOWINFO	u32			32-bit clean
 *		IPV6_HOPOPTS	ipv6 hop exthdr		32-bit clean
 *		IPV6_DSTOPTS	ipv6 dst exthdr(s)	32-bit clean
 *		IPV6_RTHDR	ipv6 routing exthdr	32-bit clean
 *		IPV6_AUTHHDR	ipv6 auth exthdr	32-bit clean
 */
static void cmsg32_recvmsg_fixup(struct msghdr *kmsg, unsigned long orig_cmsg_uptr)
{
	unsigned char *workbuf, *wp;
	unsigned long bufsz, space_avail;
	struct cmsghdr *ucmsg;

	bufsz = ((unsigned long)kmsg->msg_control) - orig_cmsg_uptr;
	space_avail = kmsg->msg_controllen + bufsz;
	wp = workbuf = kmalloc(bufsz, GFP_KERNEL);
	if(workbuf == NULL)
		goto fail;

	/* To make this more sane we assume the kernel sends back properly
	 * formatted control messages.  Because of how the kernel will truncate
	 * the cmsg_len for MSG_TRUNC cases, we need not check that case either.
	 */
	ucmsg = (struct cmsghdr *) orig_cmsg_uptr;
	while(((unsigned long)ucmsg) <=
	      (((unsigned long)kmsg->msg_control) - sizeof(struct cmsghdr))) {
		struct cmsghdr32 *kcmsg32 = (struct cmsghdr32 *) wp;
		int clen64, clen32;

		/* UCMSG is the 64-bit format CMSG entry in user-space.
		 * KCMSG32 is within the kernel space temporary buffer
		 * we use to convert into a 32-bit style CMSG.
		 */
		__get_user(kcmsg32->cmsg_len, &ucmsg->cmsg_len);
		__get_user(kcmsg32->cmsg_level, &ucmsg->cmsg_level);
		__get_user(kcmsg32->cmsg_type, &ucmsg->cmsg_type);

		clen64 = kcmsg32->cmsg_len;
		copy_from_user(CMSG32_DATA(kcmsg32), CMSG_DATA(ucmsg),
			       clen64 - CMSG_ALIGN(sizeof(*ucmsg)));
		clen32 = ((clen64 - CMSG_ALIGN(sizeof(*ucmsg))) +
			  CMSG32_ALIGN(sizeof(struct cmsghdr32)));
		kcmsg32->cmsg_len = clen32;

		switch (kcmsg32->cmsg_type) {
			/*
			 * The timestamp type's data needs to be converted
			 * from 64-bit time values to 32-bit time values
			*/
		case SO_TIMESTAMP: {
			__kernel_time_t32* ptr_time32 = CMSG32_DATA(kcmsg32);
			__kernel_time_t*   ptr_time   = CMSG_DATA(ucmsg);
			get_user(*ptr_time32, ptr_time);
			get_user(*(ptr_time32+1), ptr_time+1);
			kcmsg32->cmsg_len -= 2*(sizeof(__kernel_time_t) -
						sizeof(__kernel_time_t32));
		}
		default:;
		}
		ucmsg = (struct cmsghdr *) (((char *)ucmsg) + CMSG_ALIGN(clen64));
		wp = (((char *)kcmsg32) + CMSG32_ALIGN(kcmsg32->cmsg_len));
	}

	/* Copy back fixed up data, and adjust pointers. */
	bufsz = (wp - workbuf);
	copy_to_user((void *)orig_cmsg_uptr, workbuf, bufsz);

	kmsg->msg_control = (struct cmsghdr *)
		(((char *)orig_cmsg_uptr) + bufsz);
	kmsg->msg_controllen = space_avail - bufsz;

	kfree(workbuf);
	return;

fail:
	/* If we leave the 64-bit format CMSG chunks in there,
	 * the application could get confused and crash.  So to
	 * ensure greater recovery, we report no CMSGs.
	 */
	kmsg->msg_controllen += bufsz;
	kmsg->msg_control = (void *) orig_cmsg_uptr;
}

#if 0
asmlinkage int sys32_sendmsg(int fd, struct msghdr32 *user_msg, unsigned user_flags)
{
	struct socket *sock;
	char address[MAX_SOCK_ADDR];
	struct iovec iov[UIO_FASTIOV];
	unsigned char ctl[sizeof(struct cmsghdr) + 20];
	unsigned char *ctl_buf = ctl;
	struct msghdr kern_msg;
	int err, total_len;

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;
	err = verify_iovec32(&kern_msg, iov, address, VERIFY_READ);
	if (err < 0)
		goto out;
	total_len = err;

	if(kern_msg.msg_controllen) {
		err = cmsghdr_from_user32_to_kern(&kern_msg, ctl, sizeof(ctl));
		if(err)
			goto out_freeiov;
		ctl_buf = kern_msg.msg_control;
	}
	kern_msg.msg_flags = user_flags;

	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		if (sock->file->f_flags & O_NONBLOCK)
			kern_msg.msg_flags |= MSG_DONTWAIT;
		err = sock_sendmsg(sock, &kern_msg, total_len);
		sockfd_put(sock);
	}

	/* N.B. Use kfree here, as kern_msg.msg_controllen might change? */
	if(ctl_buf != ctl)
		kfree(ctl_buf);
out_freeiov:
	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	return err;
}

asmlinkage int sys32_recvmsg(int fd, struct msghdr32 *user_msg, unsigned int user_flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct msghdr kern_msg;
	char addr[MAX_SOCK_ADDR];
	struct socket *sock;
	struct iovec *iov = iovstack;
	struct sockaddr *uaddr;
	int *uaddr_len;
	unsigned long cmsg_ptr;
	int err, total_len, len = 0;

	if(msghdr_from_user32_to_kern(&kern_msg, user_msg))
		return -EFAULT;
	if(kern_msg.msg_iovlen > UIO_MAXIOV)
		return -EINVAL;

	uaddr = kern_msg.msg_name;
	uaddr_len = &user_msg->msg_namelen;
	err = verify_iovec32(&kern_msg, iov, addr, VERIFY_WRITE);
	if (err < 0)
		goto out;
	total_len = err;

	cmsg_ptr = (unsigned long) kern_msg.msg_control;
	kern_msg.msg_flags = 0;

	sock = sockfd_lookup(fd, &err);
	if (sock != NULL) {
		struct scm_cookie scm;

		if (sock->file->f_flags & O_NONBLOCK)
			user_flags |= MSG_DONTWAIT;
		memset(&scm, 0, sizeof(scm));
		err = sock->ops->recvmsg(sock, &kern_msg, total_len,
					 user_flags, &scm);
		if(err >= 0) {
			len = err;
			if(!kern_msg.msg_control) {
				if(sock->passcred || scm.fp)
					kern_msg.msg_flags |= MSG_CTRUNC;
				if(scm.fp)
					__scm_destroy(&scm);
			} else {
				/* If recvmsg processing itself placed some
				 * control messages into user space, it's is
				 * using 64-bit CMSG processing, so we need
				 * to fix it up before we tack on more stuff.
				 */
				if((unsigned long) kern_msg.msg_control != cmsg_ptr)
					cmsg32_recvmsg_fixup(&kern_msg, cmsg_ptr);

				/* Wheee... */
				if(sock->passcred)
					put_cmsg32(&kern_msg,
						   SOL_SOCKET, SCM_CREDENTIALS,
						   sizeof(scm.creds), &scm.creds);
				if(scm.fp != NULL)
					scm_detach_fds32(&kern_msg, &scm);
			}
		}
		sockfd_put(sock);
	}

	if(uaddr != NULL && err >= 0 && kern_msg.msg_namelen)
		err = move_addr_to_user(addr, kern_msg.msg_namelen, uaddr, uaddr_len);
	if(cmsg_ptr != 0 && err >= 0) {
		unsigned long ucmsg_ptr = ((unsigned long)kern_msg.msg_control);
		__kernel_size_t32 uclen = (__kernel_size_t32) (ucmsg_ptr - cmsg_ptr);
		err |= __put_user(uclen, &user_msg->msg_controllen);
	}
	if(err >= 0)
		err = __put_user(kern_msg.msg_flags, &user_msg->msg_flags);
	if(kern_msg.msg_iov != iov)
		kfree(kern_msg.msg_iov);
out:
	if(err < 0)
		return err;
	return len;
}
#endif

/*
 *	BSD sendmsg interface
 */

int sys32_sendmsg(int fd, struct msghdr32 *msg, unsigned flags)
{
	struct socket *sock;
	char address[MAX_SOCK_ADDR];
	struct iovec iovstack[UIO_FASTIOV], *iov = iovstack;
	unsigned char ctl[sizeof(struct cmsghdr) + 20];	/* 20 is size of ipv6_pktinfo */
	unsigned char *ctl_buf = ctl;
	struct msghdr msg_sys;
	int err, ctl_len, iov_size, total_len;
	
	err = -EFAULT;
	if (msghdr_from_user32_to_kern(&msg_sys, msg))
		goto out; 

	sock = sockfd_lookup(fd, &err);
	if (!sock) 
		goto out;

	/* do not move before msg_sys is valid */
	err = -EINVAL;
	if (msg_sys.msg_iovlen > UIO_MAXIOV)
		goto out_put;

	/* Check whether to allocate the iovec area*/
	err = -ENOMEM;
	iov_size = msg_sys.msg_iovlen * sizeof(struct iovec32);
	if (msg_sys.msg_iovlen > UIO_FASTIOV) {
		iov = sock_kmalloc(sock->sk, iov_size, GFP_KERNEL);
		if (!iov)
			goto out_put;
	}

	/* This will also move the address data into kernel space */
	err = verify_iovec32(&msg_sys, iov, address, VERIFY_READ);
	if (err < 0) 
		goto out_freeiov;
	total_len = err;

	err = -ENOBUFS;

	if (msg_sys.msg_controllen > INT_MAX)
		goto out_freeiov;
	ctl_len = msg_sys.msg_controllen; 
	if (ctl_len) 
	{
		if (ctl_len > sizeof(ctl))
		{
			ctl_buf = sock_kmalloc(sock->sk, ctl_len, GFP_KERNEL);
			if (ctl_buf == NULL) 
				goto out_freeiov;
		}
		else if (ctl_len < sizeof(struct cmsghdr))
		{
			/* to get same error message as on 31 bit native */
			err = EOPNOTSUPP;
			goto out_freeiov;
		}
		err = -EFAULT;
		if (cmsghdr_from_user32_to_kern(&msg_sys, ctl_buf, ctl_len))
			goto out_freectl;
//		msg_sys.msg_control = ctl_buf;
	}
	msg_sys.msg_flags = flags;

	if (sock->file->f_flags & O_NONBLOCK)
		msg_sys.msg_flags |= MSG_DONTWAIT;
	err = sock_sendmsg(sock, &msg_sys, total_len);

out_freectl:
	if (ctl_buf != ctl)    
		sock_kfree_s(sock->sk, ctl_buf, ctl_len);
out_freeiov:
	if (iov != iovstack)
		sock_kfree_s(sock->sk, iov, iov_size);
out_put:
	sockfd_put(sock);
out:       
	return err;
}

static __inline__ void
scm_recv32(struct socket *sock, struct msghdr *msg,
		struct scm_cookie *scm, int flags, unsigned long cmsg_ptr)
{
	if(!msg->msg_control)
	{
		if(sock->passcred || scm->fp)
			msg->msg_flags |= MSG_CTRUNC;
		scm_destroy(scm);
		return;
	}
	/* If recvmsg processing itself placed some
	 * control messages into user space, it's is
	 * using 64-bit CMSG processing, so we need
	 * to fix it up before we tack on more stuff.
	 */
	if((unsigned long) msg->msg_control != cmsg_ptr)
		cmsg32_recvmsg_fixup(msg, cmsg_ptr);
	/* Wheee... */
	if(sock->passcred)
		put_cmsg32(msg,
			SOL_SOCKET, SCM_CREDENTIALS,
			sizeof(scm->creds), &scm->creds);
	if(!scm->fp)
		return;

	scm_detach_fds32(msg, scm);
}

static int  
sock_recvmsg32(struct socket *sock, struct msghdr *msg, int size, int flags,
               unsigned long cmsg_ptr)
{
	struct scm_cookie scm;

	memset(&scm, 0, sizeof(scm));
	size = sock->ops->recvmsg(sock, msg, size, flags, &scm);
	if (size >= 0)
		scm_recv32(sock, msg, &scm, flags, cmsg_ptr);

	return size;
}

/*
 *	BSD recvmsg interface
 */

int
sys32_recvmsg (int fd, struct msghdr32 *msg, unsigned int flags)
{
	struct socket *sock;
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov=iovstack;
	struct msghdr msg_sys;
	unsigned long cmsg_ptr;
	int err, iov_size, total_len, len;

	/* kernel mode address */
	char addr[MAX_SOCK_ADDR];

	/* user mode address pointers */
	struct sockaddr *uaddr;
	int *uaddr_len;
	
	err=-EFAULT;
	if (msghdr_from_user32_to_kern(&msg_sys, msg))
		goto out;

	sock = sockfd_lookup(fd, &err);
	if (!sock)
		goto out;

	err = -EINVAL;
	if (msg_sys.msg_iovlen > UIO_MAXIOV)
		goto out_put;
	
	/* Check whether to allocate the iovec area*/
	err = -ENOMEM;
	iov_size = msg_sys.msg_iovlen * sizeof(struct iovec);
	if (msg_sys.msg_iovlen > UIO_FASTIOV) {
		iov = sock_kmalloc(sock->sk, iov_size, GFP_KERNEL);
		if (!iov)
			goto out_put;
	}

	/*
	 *	Save the user-mode address (verify_iovec will change the
	 *	kernel msghdr to use the kernel address space)
	 */
	 
	uaddr = msg_sys.msg_name;
	uaddr_len = &msg->msg_namelen;
	err = verify_iovec32(&msg_sys, iov, addr, VERIFY_WRITE);
	if (err < 0)
		goto out_freeiov;
	total_len=err;

	cmsg_ptr = (unsigned long)msg_sys.msg_control;
	msg_sys.msg_flags = 0;
	
	if (sock->file->f_flags & O_NONBLOCK)
		flags |= MSG_DONTWAIT;
	err = sock_recvmsg32(sock, &msg_sys, total_len, flags, cmsg_ptr);
	if (err < 0)
		goto out_freeiov;
	len = err;

	if (uaddr != NULL && 
	/* in order to get same error message as on native 31 bit */
		msg_sys.msg_namelen > 0) {
		err = move_addr_to_user(addr, msg_sys.msg_namelen, uaddr, uaddr_len);
		if (err < 0)
			goto out_freeiov;
	}
	err = __put_user(msg_sys.msg_flags, &msg->msg_flags);
	if (err)
		goto out_freeiov;
	err = __put_user((__kernel_size_t32) ((unsigned long)msg_sys.msg_control - cmsg_ptr), &msg->msg_controllen);
	if (err)
		goto out_freeiov;
	err = len;

out_freeiov:
	if (iov != iovstack)
		sock_kfree_s(sock->sk, iov, iov_size);
out_put:
	sockfd_put(sock);
out:
	return err;
}

extern asmlinkage int sys_setsockopt(int fd, int level, int optname,
				     char *optval, int optlen);

static int do_set_attach_filter(int fd, int level, int optname,
				char *optval, int optlen)
{
	struct sock_fprog32 {
		__u16 len;
		__u32 filter;
	} *fprog32 = (struct sock_fprog32 *)optval;
	struct sock_fprog kfprog;
	struct sock_filter *kfilter;
	unsigned int fsize;
	mm_segment_t old_fs;
	__u32 uptr;
	int ret;

	if (get_user(kfprog.len, &fprog32->len) ||
	    __get_user(uptr, &fprog32->filter))
		return -EFAULT;

	kfprog.filter = (struct sock_filter *)A(uptr);
	fsize = kfprog.len * sizeof(struct sock_filter);

	kfilter = (struct sock_filter *)kmalloc(fsize, GFP_KERNEL);
	if (kfilter == NULL)
		return -ENOMEM;

	if (copy_from_user(kfilter, kfprog.filter, fsize)) {
		kfree(kfilter);
		return -EFAULT;
	}

	kfprog.filter = kfilter;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_setsockopt(fd, level, optname,
			     (char *)&kfprog, sizeof(kfprog));
	set_fs(old_fs);

	kfree(kfilter);

	return ret;
}

static int do_set_icmpv6_filter(int fd, int level, int optname,
				char *optval, int optlen)
{
	struct icmp6_filter kfilter;
	mm_segment_t old_fs;
	int ret, i;

	if (copy_from_user(&kfilter, optval, sizeof(kfilter)))
		return -EFAULT;


	for (i = 0; i < 8; i += 2) {
		u32 tmp = kfilter.data[i];

		kfilter.data[i] = kfilter.data[i + 1];
		kfilter.data[i + 1] = tmp;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = sys_setsockopt(fd, level, optname,
			     (char *) &kfilter, sizeof(kfilter));
	set_fs(old_fs);

	return ret;
}

asmlinkage int sys32_setsockopt(int fd, int level, int optname,
				char *optval, int optlen)
{
	if (optname == SO_ATTACH_FILTER)
		return do_set_attach_filter(fd, level, optname,
					    optval, optlen);
	if (level == SOL_ICMPV6 && optname == ICMPV6_FILTER)
		return do_set_icmpv6_filter(fd, level, optname,
					    optval, optlen);
	if (level == SOL_SOCKET && 
	    (optname == SO_SNDTIMEO || optname == SO_RCVTIMEO)) {
		long ret;
		struct timeval tmp;
		mm_segment_t old_fs;

		if (get_tv32(&tmp, (struct timeval32 *)optval ))
			return -EFAULT;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = sys_setsockopt(fd, level, optname, (char *) &tmp, sizeof(struct timeval));
		set_fs(old_fs);
		return ret; 
	}

	return sys_setsockopt(fd, level, optname, optval, optlen);
}

extern void check_pending(int signum);

/*
 * count32() counts the number of arguments/envelopes
 */
static int count32(u32 * argv)
{
	int i = 0;

	if (argv != NULL) {
		for (;;) {
			u32 p; int error;

			error = get_user(p,argv);
			if (error) return error;
			if (!p) break;
			argv++; i++;
		}
	}
	return i;
}

/*
 * 'copy_string32()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 */
static int copy_strings32(int argc, u32 * argv, struct linux_binprm *bprm)
{
	while (argc-- > 0) {
		u32 str;
		int len;
		unsigned long pos;

		if (get_user(str, argv + argc) ||
		    !str ||
		    !(len = strnlen_user((char *)A(str), bprm->p)))
			return -EFAULT;

		if (bprm->p < len)
			return -E2BIG;

		bprm->p -= len;

		pos = bprm->p;
		while (len) {
			char *kaddr;
			struct page *page;
			int offset, bytes_to_copy, new, err;

			offset = pos % PAGE_SIZE;
			page = bprm->page[pos / PAGE_SIZE];
			new = 0;
			if (!page) {
				page = alloc_page(GFP_USER);
				bprm->page[pos / PAGE_SIZE] = page;
				if (!page)
					return -ENOMEM;
				new = 1;
			}
			kaddr = (char *)kmap(page);

			if (new && offset)
				memset(kaddr, 0, offset);
			bytes_to_copy = PAGE_SIZE - offset;
			if (bytes_to_copy > len) {
				bytes_to_copy = len;
				if (new)
					memset(kaddr+offset+len, 0,
					       PAGE_SIZE-offset-len);
			}

			err = copy_from_user(kaddr + offset, (char *)A(str),
					     bytes_to_copy);
			flush_page_to_ram(page);
			kunmap(page);

			if (err)
				return -EFAULT;

			pos += bytes_to_copy;
			str += bytes_to_copy;
			len -= bytes_to_copy;
		}
	}
	return 0;
}

/*
 * sys32_execve() executes a new program.
 */
static inline int 
do_execve32(char * filename, u32 * argv, u32 * envp, struct pt_regs * regs)
{
	struct linux_binprm bprm;
	struct file * file;
	int retval;
	int i;

	bprm.p = PAGE_SIZE*MAX_ARG_PAGES-sizeof(void *);
	memset(bprm.page, 0, MAX_ARG_PAGES * sizeof(bprm.page[0]));

	file = open_exec(filename);

	retval = PTR_ERR(file);
	if (IS_ERR(file))
		return retval;

	bprm.file = file;
	bprm.filename = filename;
	bprm.sh_bang = 0;
	bprm.loader = 0;
	bprm.exec = 0;
	if ((bprm.argc = count32(argv)) < 0) {
		allow_write_access(file);
		fput(file);
		return bprm.argc;
	}
	if ((bprm.envc = count32(envp)) < 0) {
		allow_write_access(file);
		fput(file);
		return bprm.envc;
	}

	retval = prepare_binprm(&bprm);
	if (retval < 0)
		goto out;
	
	retval = copy_strings_kernel(1, &bprm.filename, &bprm);
	if (retval < 0)
		goto out;

	bprm.exec = bprm.p;
	retval = copy_strings32(bprm.envc, envp, &bprm);
	if (retval < 0)
		goto out;

	retval = copy_strings32(bprm.argc, argv, &bprm);
	if (retval < 0)
		goto out;

	retval = search_binary_handler(&bprm, regs);
	if (retval >= 0)
		/* execve success */
		return retval;

out:
	/* Something went wrong, return the inode and free the argument pages*/
	allow_write_access(bprm.file);
	if (bprm.file)
		fput(bprm.file);

	for (i=0 ; i<MAX_ARG_PAGES ; i++)
		if (bprm.page[i])
			__free_page(bprm.page[i]);

	return retval;
}

/*
 * sys32_execve() executes a new program after the asm stub has set
 * things up for us.  This should basically do what I want it to.
 */
asmlinkage int
sys32_execve(struct pt_regs regs)
{
        int error;
        char * filename;

        filename = getname((char *)A(regs.orig_gpr2));
        error = PTR_ERR(filename);
        if (IS_ERR(filename))
                goto out;
        error = do_execve32(filename, (u32 *)A(regs.gprs[3]), (u32 *)A(regs.gprs[4]), &regs);
	if (error == 0)
	{
		current->ptrace &= ~PT_DTRACE;
		current->thread.fp_regs.fpc=0;
		__asm__ __volatile__
		        ("sr  0,0\n\t"
		         "sfpc 0,0\n\t"
			 : : :"0");
	}
        putname(filename);
out:
        return error;
}


#ifdef CONFIG_MODULES

extern asmlinkage unsigned long sys_create_module(const char *name_user, size_t size);

asmlinkage unsigned long sys32_create_module(const char *name_user, __kernel_size_t32 size)
{
	return sys_create_module(name_user, (size_t)size);
}

extern asmlinkage int sys_init_module(const char *name_user, struct module *mod_user);

/* Hey, when you're trying to init module, take time and prepare us a nice 64bit
 * module structure, even if from 32bit modutils... Why to pollute kernel... :))
 */
asmlinkage int sys32_init_module(const char *name_user, struct module *mod_user)
{
	return sys_init_module(name_user, mod_user);
}

extern asmlinkage int sys_delete_module(const char *name_user);

asmlinkage int sys32_delete_module(const char *name_user)
{
	return sys_delete_module(name_user);
}

struct module_info32 {
	u32 addr;
	u32 size;
	u32 flags;
	s32 usecount;
};

/* Query various bits about modules.  */

static inline long
get_mod_name(const char *user_name, char **buf)
{
	unsigned long page;
	long retval;

	if ((unsigned long)user_name >= TASK_SIZE
	    && !segment_eq(get_fs (), KERNEL_DS))
		return -EFAULT;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	retval = strncpy_from_user((char *)page, user_name, PAGE_SIZE);
	if (retval > 0) {
		if (retval < PAGE_SIZE) {
			*buf = (char *)page;
			return retval;
		}
		retval = -ENAMETOOLONG;
	} else if (!retval)
		retval = -EINVAL;

	free_page(page);
	return retval;
}

static inline void
put_mod_name(char *buf)
{
	free_page((unsigned long)buf);
}

static __inline__ struct module *find_module(const char *name)
{
	struct module *mod;

	for (mod = module_list; mod ; mod = mod->next) {
		if (mod->flags & MOD_DELETED)
			continue;
		if (!strcmp(mod->name, name))
			break;
	}

	return mod;
}

static int
qm_modules(char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	struct module *mod;
	size_t nmod, space, len;

	nmod = space = 0;

	for (mod = module_list; mod->next != NULL; mod = mod->next, ++nmod) {
		len = strlen(mod->name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, mod->name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nmod, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((mod = mod->next)->next != NULL)
		space += strlen(mod->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_deps(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t i, space, len;

	if (mod->next == NULL)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		return put_user(0, ret);

	space = 0;
	for (i = 0; i < mod->ndeps; ++i) {
		const char *dep_name = mod->deps[i].dep->name;

		len = strlen(dep_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, dep_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	return put_user(i, ret);

calc_space_needed:
	space += len;
	while (++i < mod->ndeps)
		space += strlen(mod->deps[i].dep->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static int
qm_refs(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t nrefs, space, len;
	struct module_ref *ref;

	if (mod->next == NULL)
		return -EINVAL;
	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = 0;
	for (nrefs = 0, ref = mod->refs; ref ; ++nrefs, ref = ref->next_ref) {
		const char *ref_name = ref->ref->name;

		len = strlen(ref_name)+1;
		if (len > bufsize)
			goto calc_space_needed;
		if (copy_to_user(buf, ref_name, len))
			return -EFAULT;
		buf += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(nrefs, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	space += len;
	while ((ref = ref->next_ref) != NULL)
		space += strlen(ref->ref->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static inline int
qm_symbols(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	size_t i, space, len;
	struct module_symbol *s;
	char *strings;
	unsigned *vals;

	if (!MOD_CAN_QUERY(mod))
		if (put_user(0, ret))
			return -EFAULT;
		else
			return 0;

	space = mod->nsyms * 2*sizeof(u32);

	i = len = 0;
	s = mod->syms;

	if (space > bufsize)
		goto calc_space_needed;

	if (!access_ok(VERIFY_WRITE, buf, space))
		return -EFAULT;

	bufsize -= space;
	vals = (unsigned *)buf;
	strings = buf+space;

	for (; i < mod->nsyms ; ++i, ++s, vals += 2) {
		len = strlen(s->name)+1;
		if (len > bufsize)
			goto calc_space_needed;

		if (copy_to_user(strings, s->name, len)
		    || __put_user(s->value, vals+0)
		    || __put_user(space, vals+1))
			return -EFAULT;

		strings += len;
		bufsize -= len;
		space += len;
	}

	if (put_user(i, ret))
		return -EFAULT;
	else
		return 0;

calc_space_needed:
	for (; i < mod->nsyms; ++i, ++s)
		space += strlen(s->name)+1;

	if (put_user(space, ret))
		return -EFAULT;
	else
		return -ENOSPC;
}

static inline int
qm_info(struct module *mod, char *buf, size_t bufsize, __kernel_size_t32 *ret)
{
	int error = 0;

	if (mod->next == NULL)
		return -EINVAL;

	if (sizeof(struct module_info32) <= bufsize) {
		struct module_info32 info;
		info.addr = (unsigned long)mod;
		info.size = mod->size;
		info.flags = mod->flags;
		info.usecount =
			((mod_member_present(mod, can_unload)
			  && mod->can_unload)
			 ? -1 : atomic_read(&mod->uc.usecount));

		if (copy_to_user(buf, &info, sizeof(struct module_info32)))
			return -EFAULT;
	} else
		error = -ENOSPC;

	if (put_user(sizeof(struct module_info32), ret))
		return -EFAULT;

	return error;
}

asmlinkage int sys32_query_module(char *name_user, int which, char *buf, __kernel_size_t32 bufsize, u32 ret)
{
	struct module *mod;
	int err;

	lock_kernel();
	if (name_user == 0) {
		/* This finds "kernel_module" which is not exported. */
		for(mod = module_list; mod->next != NULL; mod = mod->next)
			;
	} else {
		long namelen;
		char *name;

		if ((namelen = get_mod_name(name_user, &name)) < 0) {
			err = namelen;
			goto out;
		}
		err = -ENOENT;
		if (namelen == 0) {
			/* This finds "kernel_module" which is not exported. */
			for(mod = module_list; mod->next != NULL; mod = mod->next)
				;
		} else if ((mod = find_module(name)) == NULL) {
			put_mod_name(name);
			goto out;
		}
		put_mod_name(name);
	}

	switch (which)
	{
	case 0:
		err = 0;
		break;
	case QM_MODULES:
		err = qm_modules(buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_DEPS:
		err = qm_deps(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_REFS:
		err = qm_refs(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_SYMBOLS:
		err = qm_symbols(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	case QM_INFO:
		err = qm_info(mod, buf, bufsize, (__kernel_size_t32 *)AA(ret));
		break;
	default:
		err = -EINVAL;
		break;
	}
out:
	unlock_kernel();
	return err;
}

struct kernel_sym32 {
	u32 value;
	char name[60];
};
		 
extern asmlinkage int sys_get_kernel_syms(struct kernel_sym *table);

asmlinkage int sys32_get_kernel_syms(struct kernel_sym32 *table)
{
	int len, i;
	struct kernel_sym *tbl;
	mm_segment_t old_fs;
	
	len = sys_get_kernel_syms(NULL);
	if (!table) return len;
	tbl = kmalloc (len * sizeof (struct kernel_sym), GFP_KERNEL);
	if (!tbl) return -ENOMEM;
	old_fs = get_fs();
	set_fs (KERNEL_DS);
	sys_get_kernel_syms(tbl);
	set_fs (old_fs);
	for (i = 0; i < len; i++, table += sizeof (struct kernel_sym32)) {
		if (put_user (tbl[i].value, &table->value) ||
		    copy_to_user (table->name, tbl[i].name, 60))
			break;
	}
	kfree (tbl);
	return i;
}

#else /* CONFIG_MODULES */

asmlinkage unsigned long
sys32_create_module(const char *name_user, size_t size)
{
	return -ENOSYS;
}

asmlinkage int
sys32_init_module(const char *name_user, struct module *mod_user)
{
	return -ENOSYS;
}

asmlinkage int
sys32_delete_module(const char *name_user)
{
	return -ENOSYS;
}

asmlinkage int
sys32_query_module(const char *name_user, int which, char *buf, size_t bufsize,
		 size_t *ret)
{
	/* Let the program know about the new interface.  Not that
	   it'll do them much good.  */
	if (which == 0)
		return 0;

	return -ENOSYS;
}

asmlinkage int
sys32_get_kernel_syms(struct kernel_sym *table)
{
	return -ENOSYS;
}

#endif  /* CONFIG_MODULES */

/* Stuff for NFS server syscalls... */
struct nfsctl_svc32 {
	u16			svc32_port;
	s32			svc32_nthreads;
};

struct nfsctl_client32 {
	s8			cl32_ident[NFSCLNT_IDMAX+1];
	s32			cl32_naddr;
	struct in_addr		cl32_addrlist[NFSCLNT_ADDRMAX];
	s32			cl32_fhkeytype;
	s32			cl32_fhkeylen;
	u8			cl32_fhkey[NFSCLNT_KEYMAX];
};

struct nfsctl_export32 {
	s8			ex32_client[NFSCLNT_IDMAX+1];
	s8			ex32_path[NFS_MAXPATHLEN+1];
	__kernel_dev_t32	ex32_dev;
	__kernel_ino_t32	ex32_ino;
	s32			ex32_flags;
	__kernel_uid_t32	ex32_anon_uid;
	__kernel_gid_t32	ex32_anon_gid;
};

struct nfsctl_uidmap32 {
	u32			ug32_ident;   /* char * */
	__kernel_uid_t32	ug32_uidbase;
	s32			ug32_uidlen;
	u32			ug32_udimap;  /* uid_t * */
	__kernel_uid_t32	ug32_gidbase;
	s32			ug32_gidlen;
	u32			ug32_gdimap;  /* gid_t * */
};

struct nfsctl_fhparm32 {
	struct sockaddr		gf32_addr;
	__kernel_dev_t32	gf32_dev;
	__kernel_ino_t32	gf32_ino;
	s32			gf32_version;
};

struct nfsctl_fdparm32 {
	struct sockaddr		gd32_addr;
	s8			gd32_path[NFS_MAXPATHLEN+1];
	s32			gd32_version;
};

struct nfsctl_fsparm32 {
	struct sockaddr		gd32_addr;
	s8			gd32_path[NFS_MAXPATHLEN+1];
	s32			gd32_maxlen;
};

struct nfsctl_arg32 {
	s32			ca32_version;	/* safeguard */
	union {
		struct nfsctl_svc32	u32_svc;
		struct nfsctl_client32	u32_client;
		struct nfsctl_export32	u32_export;
		struct nfsctl_uidmap32	u32_umap;
		struct nfsctl_fhparm32	u32_getfh;
		struct nfsctl_fdparm32	u32_getfd;
		struct nfsctl_fsparm32	u32_getfs;
	} u;
#define ca32_svc	u.u32_svc
#define ca32_client	u.u32_client
#define ca32_export	u.u32_export
#define ca32_umap	u.u32_umap
#define ca32_getfh	u.u32_getfh
#define ca32_getfd	u.u32_getfd
#define ca32_getfs	u.u32_getfs
#define ca32_authd	u.u32_authd
};

union nfsctl_res32 {
	__u8			cr32_getfh[NFS_FHSIZE];
	struct knfsd_fh		cr32_getfs;
};

static int nfs_svc32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= __get_user(karg->ca_svc.svc_port, &arg32->ca32_svc.svc32_port);
	err |= __get_user(karg->ca_svc.svc_nthreads, &arg32->ca32_svc.svc32_nthreads);
	return err;
}

static int nfs_clnt32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_client.cl_ident[0],
			  &arg32->ca32_client.cl32_ident[0],
			  NFSCLNT_IDMAX);
	err |= __get_user(karg->ca_client.cl_naddr, &arg32->ca32_client.cl32_naddr);
	err |= copy_from_user(&karg->ca_client.cl_addrlist[0],
			  &arg32->ca32_client.cl32_addrlist[0],
			  (sizeof(struct in_addr) * NFSCLNT_ADDRMAX));
	err |= __get_user(karg->ca_client.cl_fhkeytype,
		      &arg32->ca32_client.cl32_fhkeytype);
	err |= __get_user(karg->ca_client.cl_fhkeylen,
		      &arg32->ca32_client.cl32_fhkeylen);
	err |= copy_from_user(&karg->ca_client.cl_fhkey[0],
			  &arg32->ca32_client.cl32_fhkey[0],
			  NFSCLNT_KEYMAX);
	return err;
}

static int nfs_exp32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_export.ex_client[0],
			  &arg32->ca32_export.ex32_client[0],
			  NFSCLNT_IDMAX);
	err |= copy_from_user(&karg->ca_export.ex_path[0],
			  &arg32->ca32_export.ex32_path[0],
			  NFS_MAXPATHLEN);
	err |= __get_user(karg->ca_export.ex_dev,
		      &arg32->ca32_export.ex32_dev);
	err |= __get_user(karg->ca_export.ex_ino,
		      &arg32->ca32_export.ex32_ino);
	err |= __get_user(karg->ca_export.ex_flags,
		      &arg32->ca32_export.ex32_flags);
	err |= __get_user(karg->ca_export.ex_anon_uid,
		      &arg32->ca32_export.ex32_anon_uid);
	err |= __get_user(karg->ca_export.ex_anon_gid,
		      &arg32->ca32_export.ex32_anon_gid);
	karg->ca_export.ex_anon_uid = high2lowuid(karg->ca_export.ex_anon_uid);
	karg->ca_export.ex_anon_gid = high2lowgid(karg->ca_export.ex_anon_gid);
	return err;
}

static int nfs_uud32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	u32 uaddr;
	int i;
	int err;

	memset(karg, 0, sizeof(*karg));
	if(__get_user(karg->ca_version, &arg32->ca32_version))
		return -EFAULT;
	karg->ca_umap.ug_ident = (char *)get_free_page(GFP_USER);
	if(!karg->ca_umap.ug_ident)
		return -ENOMEM;
	err = __get_user(uaddr, &arg32->ca32_umap.ug32_ident);
	if(strncpy_from_user(karg->ca_umap.ug_ident,
			     (char *)A(uaddr), PAGE_SIZE) <= 0)
		return -EFAULT;
	err |= __get_user(karg->ca_umap.ug_uidbase,
		      &arg32->ca32_umap.ug32_uidbase);
	err |= __get_user(karg->ca_umap.ug_uidlen,
		      &arg32->ca32_umap.ug32_uidlen);
	err |= __get_user(uaddr, &arg32->ca32_umap.ug32_udimap);
	if (err)
		return -EFAULT;
	karg->ca_umap.ug_udimap = kmalloc((sizeof(uid_t) * karg->ca_umap.ug_uidlen),
					  GFP_USER);
	if(!karg->ca_umap.ug_udimap)
		return -ENOMEM;
	for(i = 0; i < karg->ca_umap.ug_uidlen; i++)
		err |= __get_user(karg->ca_umap.ug_udimap[i],
			      &(((__kernel_uid_t32 *)A(uaddr))[i]));
	err |= __get_user(karg->ca_umap.ug_gidbase,
		      &arg32->ca32_umap.ug32_gidbase);
	err |= __get_user(karg->ca_umap.ug_uidlen,
		      &arg32->ca32_umap.ug32_gidlen);
	err |= __get_user(uaddr, &arg32->ca32_umap.ug32_gdimap);
	if (err)
		return -EFAULT;
	karg->ca_umap.ug_gdimap = kmalloc((sizeof(gid_t) * karg->ca_umap.ug_uidlen),
					  GFP_USER);
	if(!karg->ca_umap.ug_gdimap)
		return -ENOMEM;
	for(i = 0; i < karg->ca_umap.ug_gidlen; i++)
		err |= __get_user(karg->ca_umap.ug_gdimap[i],
			      &(((__kernel_gid_t32 *)A(uaddr))[i]));

	return err;
}

static int nfs_getfh32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfh.gf_addr,
			  &arg32->ca32_getfh.gf32_addr,
			  (sizeof(struct sockaddr)));
	err |= __get_user(karg->ca_getfh.gf_dev,
		      &arg32->ca32_getfh.gf32_dev);
	err |= __get_user(karg->ca_getfh.gf_ino,
		      &arg32->ca32_getfh.gf32_ino);
	err |= __get_user(karg->ca_getfh.gf_version,
		      &arg32->ca32_getfh.gf32_version);
	return err;
}

static int nfs_getfd32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfd.gd_addr,
			  &arg32->ca32_getfd.gd32_addr,
			  (sizeof(struct sockaddr)));
	err |= copy_from_user(&karg->ca_getfd.gd_path,
			  &arg32->ca32_getfd.gd32_path,
			  (NFS_MAXPATHLEN+1));
	err |= __get_user(karg->ca_getfd.gd_version,
		      &arg32->ca32_getfd.gd32_version);
	return err;
}

static int nfs_getfs32_trans(struct nfsctl_arg *karg, struct nfsctl_arg32 *arg32)
{
	int err;
	
	err = __get_user(karg->ca_version, &arg32->ca32_version);
	err |= copy_from_user(&karg->ca_getfs.gd_addr,
			  &arg32->ca32_getfs.gd32_addr,
			  (sizeof(struct sockaddr)));
	err |= copy_from_user(&karg->ca_getfs.gd_path,
			  &arg32->ca32_getfs.gd32_path,
			  (NFS_MAXPATHLEN+1));
	err |= __get_user(karg->ca_getfs.gd_maxlen,
		      &arg32->ca32_getfs.gd32_maxlen);
	return err;
}

/* This really doesn't need translations, we are only passing
 * back a union which contains opaque nfs file handle data.
 */
static int nfs_getfh32_res_trans(union nfsctl_res *kres, union nfsctl_res32 *res32)
{
	return copy_to_user(res32, kres, sizeof(*res32)) ? -EFAULT : 0;
}

/*
asmlinkage long sys_ni_syscall(void); 
*/

int asmlinkage sys32_nfsservctl(int cmd, struct nfsctl_arg32 *arg32, union nfsctl_res32 *res32)
{
	struct nfsctl_arg *karg = NULL;
	union nfsctl_res *kres = NULL;
	mm_segment_t oldfs;
	int err;

	karg = kmalloc(sizeof(*karg), GFP_USER);
	if(!karg)
		return -ENOMEM;
	if(res32) {
		kres = kmalloc(sizeof(*kres), GFP_USER);
		if(!kres) {
			kfree(karg);
			return -ENOMEM;
		}
	}
	switch(cmd) {
	case NFSCTL_SVC:
		err = nfs_svc32_trans(karg, arg32);
		break;
	case NFSCTL_ADDCLIENT:
		err = nfs_clnt32_trans(karg, arg32);
		break;
	case NFSCTL_DELCLIENT:
		err = nfs_clnt32_trans(karg, arg32);
		break;
	case NFSCTL_EXPORT:
	case NFSCTL_UNEXPORT:
		err = nfs_exp32_trans(karg, arg32);
		break;
	/* This one is unimplemented, be we're ready for it. */
	case NFSCTL_UGIDUPDATE:
		err = nfs_uud32_trans(karg, arg32);
		break;
	case NFSCTL_GETFH:
		err = nfs_getfh32_trans(karg, arg32);
		break;
	case NFSCTL_GETFD:
		err = nfs_getfd32_trans(karg, arg32);
		break;
	case NFSCTL_GETFS:
		err = nfs_getfs32_trans(karg, arg32);
		break;
	default:
		err = -EINVAL;
		break;
	}
	if(err)
		goto done;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	err = sys_nfsservctl(cmd, karg, kres);
	set_fs(oldfs);

	if (err)
		goto done;

	if((cmd == NFSCTL_GETFH) ||
	   (cmd == NFSCTL_GETFD) ||
	   (cmd == NFSCTL_GETFS))
		err = nfs_getfh32_res_trans(kres, res32);

done:
	if(karg) {
		if(cmd == NFSCTL_UGIDUPDATE) {
			if(karg->ca_umap.ug_ident)
				kfree(karg->ca_umap.ug_ident);
			if(karg->ca_umap.ug_udimap)
				kfree(karg->ca_umap.ug_udimap);
			if(karg->ca_umap.ug_gdimap)
				kfree(karg->ca_umap.ug_gdimap);
		}
		kfree(karg);
	}
	if(kres)
		kfree(kres);
	return err;
}

/* Translations due to time_t size differences.  Which affects all
   sorts of things, like timeval and itimerval.  */

extern struct timezone sys_tz;
extern int do_sys_settimeofday(struct timeval *tv, struct timezone *tz);

asmlinkage int sys32_gettimeofday(struct timeval32 *tv, struct timezone *tz)
{
	if (tv) {
		struct timeval ktv;
		do_gettimeofday(&ktv);
		if (put_tv32(tv, &ktv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_to_user(tz, &sys_tz, sizeof(sys_tz)))
			return -EFAULT;
	}
	return 0;
}

asmlinkage int sys32_settimeofday(struct timeval32 *tv, struct timezone *tz)
{
	struct timeval ktv;
	struct timezone ktz;

 	if (tv) {
		if (get_tv32(&ktv, tv))
			return -EFAULT;
	}
	if (tz) {
		if (copy_from_user(&ktz, tz, sizeof(ktz)))
			return -EFAULT;
	}

	return do_sys_settimeofday(tv ? &ktv : NULL, tz ? &ktz : NULL);
}

extern int do_getitimer(int which, struct itimerval *value);

asmlinkage int sys32_getitimer(int which, struct itimerval32 *it)
{
	struct itimerval kit;
	int error;

	error = do_getitimer(which, &kit);
	if (!error && put_it32(it, &kit))
		error = -EFAULT;

	return error;
}

extern int do_setitimer(int which, struct itimerval *, struct itimerval *);

asmlinkage int sys32_setitimer(int which, struct itimerval32 *in, struct itimerval32 *out)
{
	struct itimerval kin, kout;
	int error;

	if (in) {
		if (get_it32(&kin, in))
			return -EFAULT;
	} else
		memset(&kin, 0, sizeof(kin));

	error = do_setitimer(which, &kin, out ? &kout : NULL);
	if (error || !out)
		return error;
	if (put_it32(out, &kout))
		return -EFAULT;

	return 0;

}

asmlinkage int sys_utimes(char *, struct timeval *);

asmlinkage int sys32_utimes(char *filename, struct timeval32 *tvs)
{
	char *kfilename;
	struct timeval ktvs[2];
	mm_segment_t old_fs;
	int ret;

	kfilename = getname(filename);
	ret = PTR_ERR(kfilename);
	if (!IS_ERR(kfilename)) {
		if (tvs) {
			if (get_tv32(&ktvs[0], tvs) ||
			    get_tv32(&ktvs[1], 1+tvs))
				return -EFAULT;
		}

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		ret = sys_utimes(kfilename, &ktvs[0]);
		set_fs(old_fs);

		putname(kfilename);
	}
	return ret;
}

/* These are here just in case some old sparc32 binary calls it. */
asmlinkage int sys32_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return -ERESTARTNOHAND;
}

extern asmlinkage int sys_prctl(int option, unsigned long arg2, unsigned long arg3,
				unsigned long arg4, unsigned long arg5);

asmlinkage int sys32_prctl(int option, u32 arg2, u32 arg3, u32 arg4, u32 arg5)
{
	return sys_prctl(option,
			 (unsigned long) arg2,
			 (unsigned long) arg3,
			 (unsigned long) arg4,
			 (unsigned long) arg5);
}


extern asmlinkage ssize_t sys_pread(unsigned int fd, char * buf,
				    size_t count, loff_t pos);

extern asmlinkage ssize_t sys_pwrite(unsigned int fd, const char * buf,
				     size_t count, loff_t pos);

typedef __kernel_ssize_t32 ssize_t32;

asmlinkage ssize_t32 sys32_pread(unsigned int fd, char *ubuf,
				 __kernel_size_t32 count, u32 poshi, u32 poslo)
{
	if ((ssize_t32) count < 0)
		return -EINVAL; 
	return sys_pread(fd, ubuf, count, ((loff_t)AA(poshi) << 32) | AA(poslo));
}

asmlinkage ssize_t32 sys32_pwrite(unsigned int fd, char *ubuf,
				  __kernel_size_t32 count, u32 poshi, u32 poslo)
{
	if ((ssize_t32) count < 0)
		return -EINVAL; 
	return sys_pwrite(fd, ubuf, count, ((loff_t)AA(poshi) << 32) | AA(poslo));
}

extern asmlinkage ssize_t sys_readahead(int fd, loff_t offset, size_t count);

asmlinkage ssize_t32 sys32_readahead(int fd, u32 offhi, u32 offlo, s32 count)
{
	return sys_readahead(fd, ((loff_t)AA(offhi) << 32) | AA(offlo), count);
}

extern asmlinkage ssize_t sys_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

asmlinkage int sys32_sendfile(int out_fd, int in_fd, __kernel_off_t32 *offset, s32 count)
{
	mm_segment_t old_fs = get_fs();
	int ret;
	off_t of;
	
	if (offset && get_user(of, offset))
		return -EFAULT;
		
	set_fs(KERNEL_DS);
	ret = sys_sendfile(out_fd, in_fd, offset ? &of : NULL, count);
	set_fs(old_fs);
	
	if (!ret && offset && put_user(of, offset))
		return -EFAULT;
		
	return ret;
}

/* Handle adjtimex compatability. */

struct timex32 {
	u32 modes;
	s32 offset, freq, maxerror, esterror;
	s32 status, constant, precision, tolerance;
	struct timeval32 time;
	s32 tick;
	s32 ppsfreq, jitter, shift, stabil;
	s32 jitcnt, calcnt, errcnt, stbcnt;
	s32  :32; s32  :32; s32  :32; s32  :32;
	s32  :32; s32  :32; s32  :32; s32  :32;
	s32  :32; s32  :32; s32  :32; s32  :32;
};

extern int do_adjtimex(struct timex *);

asmlinkage int sys32_adjtimex(struct timex32 *utp)
{
	struct timex txc;
	int ret;

	memset(&txc, 0, sizeof(struct timex));

	if(get_user(txc.modes, &utp->modes) ||
	   __get_user(txc.offset, &utp->offset) ||
	   __get_user(txc.freq, &utp->freq) ||
	   __get_user(txc.maxerror, &utp->maxerror) ||
	   __get_user(txc.esterror, &utp->esterror) ||
	   __get_user(txc.status, &utp->status) ||
	   __get_user(txc.constant, &utp->constant) ||
	   __get_user(txc.precision, &utp->precision) ||
	   __get_user(txc.tolerance, &utp->tolerance) ||
	   __get_user(txc.time.tv_sec, &utp->time.tv_sec) ||
	   __get_user(txc.time.tv_usec, &utp->time.tv_usec) ||
	   __get_user(txc.tick, &utp->tick) ||
	   __get_user(txc.ppsfreq, &utp->ppsfreq) ||
	   __get_user(txc.jitter, &utp->jitter) ||
	   __get_user(txc.shift, &utp->shift) ||
	   __get_user(txc.stabil, &utp->stabil) ||
	   __get_user(txc.jitcnt, &utp->jitcnt) ||
	   __get_user(txc.calcnt, &utp->calcnt) ||
	   __get_user(txc.errcnt, &utp->errcnt) ||
	   __get_user(txc.stbcnt, &utp->stbcnt))
		return -EFAULT;

	ret = do_adjtimex(&txc);

	if(put_user(txc.modes, &utp->modes) ||
	   __put_user(txc.offset, &utp->offset) ||
	   __put_user(txc.freq, &utp->freq) ||
	   __put_user(txc.maxerror, &utp->maxerror) ||
	   __put_user(txc.esterror, &utp->esterror) ||
	   __put_user(txc.status, &utp->status) ||
	   __put_user(txc.constant, &utp->constant) ||
	   __put_user(txc.precision, &utp->precision) ||
	   __put_user(txc.tolerance, &utp->tolerance) ||
	   __put_user(txc.time.tv_sec, &utp->time.tv_sec) ||
	   __put_user(txc.time.tv_usec, &utp->time.tv_usec) ||
	   __put_user(txc.tick, &utp->tick) ||
	   __put_user(txc.ppsfreq, &utp->ppsfreq) ||
	   __put_user(txc.jitter, &utp->jitter) ||
	   __put_user(txc.shift, &utp->shift) ||
	   __put_user(txc.stabil, &utp->stabil) ||
	   __put_user(txc.jitcnt, &utp->jitcnt) ||
	   __put_user(txc.calcnt, &utp->calcnt) ||
	   __put_user(txc.errcnt, &utp->errcnt) ||
	   __put_user(txc.stbcnt, &utp->stbcnt))
		ret = -EFAULT;

	return ret;
}

extern asmlinkage long sys_setpriority(int which, int who, int niceval);

asmlinkage int sys_setpriority32(u32 which, u32 who, u32 niceval)
{
	return sys_setpriority((int) which,
			       (int) who,
			       (int) niceval);
}

struct __sysctl_args32 {
	u32 name;
	int nlen;
	u32 oldval;
	u32 oldlenp;
	u32 newval;
	u32 newlen;
	u32 __unused[4];
};

extern asmlinkage long sys32_sysctl(struct __sysctl_args32 *args)
{
	struct __sysctl_args32 tmp;
	int error;
	size_t oldlen, *oldlenp = NULL;
	unsigned long addr = (((long)&args->__unused[0]) + 7) & ~7;

	if (copy_from_user(&tmp, args, sizeof(tmp)))
		return -EFAULT;

	if (tmp.oldval && tmp.oldlenp) {
		/* Duh, this is ugly and might not work if sysctl_args
		   is in read-only memory, but do_sysctl does indirectly
		   a lot of uaccess in both directions and we'd have to
		   basically copy the whole sysctl.c here, and
		   glibc's __sysctl uses rw memory for the structure
		   anyway.  */
		if (get_user(oldlen, (u32 *)A(tmp.oldlenp)) ||
		    put_user(oldlen, (size_t *)addr))
			return -EFAULT;
		oldlenp = (size_t *)addr;
	}

	lock_kernel();
	error = do_sysctl((int *)A(tmp.name), tmp.nlen, (void *)A(tmp.oldval),
			  oldlenp, (void *)A(tmp.newval), tmp.newlen);
	unlock_kernel();
	if (oldlenp) {
		if (!error) {
			if (get_user(oldlen, (size_t *)addr) ||
			    put_user(oldlen, (u32 *)A(tmp.oldlenp)))
				error = -EFAULT;
		}
		copy_to_user(args->__unused, tmp.__unused, sizeof(tmp.__unused));
	}
	return error;
}

struct stat64_emu31 {
	unsigned char   __pad0[6];
	unsigned short  st_dev;
	unsigned int    __pad1;
#define STAT64_HAS_BROKEN_ST_INO        1
	u32             __st_ino;
	unsigned int    st_mode;
	unsigned int    st_nlink;
	u32             st_uid;
	u32             st_gid;
	unsigned char   __pad2[6];
	unsigned short  st_rdev;
	unsigned int    __pad3;
	long            st_size;
	u32             st_blksize;
	unsigned char   __pad4[4];
	u32             __pad5;     /* future possible st_blocks high bits */
	u32             st_blocks;  /* Number 512-byte blocks allocated. */
	u32             st_atime;
	u32             __pad6;
	u32             st_mtime;
	u32             __pad7;
	u32             st_ctime;
	u32             __pad8;     /* will be high 32 bits of ctime someday */
	unsigned long   st_ino;
};	

static inline int
putstat64 (struct stat64_emu31 *ubuf, struct stat *kbuf)
{
    struct stat64_emu31 tmp;
   
    memset(&tmp, 0, sizeof(tmp));

    tmp.st_dev = (unsigned short)kbuf->st_dev;
    tmp.st_ino = kbuf->st_ino;
    tmp.__st_ino = (u32)kbuf->st_ino;
    tmp.st_mode = kbuf->st_mode;
    tmp.st_nlink = (unsigned int)kbuf->st_nlink;
    tmp.st_uid = kbuf->st_uid;
    tmp.st_gid = kbuf->st_gid;
    tmp.st_rdev = (unsigned short)kbuf->st_rdev;
    tmp.st_size = kbuf->st_size;
    tmp.st_blksize = (u32)kbuf->st_blksize;
    tmp.st_blocks = (u32)kbuf->st_blocks;
    tmp.st_atime = (u32)kbuf->st_atime;
    tmp.st_mtime = (u32)kbuf->st_mtime;
    tmp.st_ctime = (u32)kbuf->st_ctime;

    return copy_to_user(ubuf,&tmp,sizeof(tmp)) ? -EFAULT : 0; 
}

extern asmlinkage long sys_newstat(char * filename, struct stat * statbuf);

asmlinkage long sys32_stat64(char * filename, struct stat64_emu31 * statbuf, long flags)
{
    int ret;
    struct stat s;
    char * tmp;
    int err;
    mm_segment_t old_fs = get_fs();
    
    tmp = getname(filename);
    err = PTR_ERR(tmp);
    if (IS_ERR(tmp))   
	    return err;

    set_fs (KERNEL_DS);
    ret = sys_newstat(tmp, &s);
    set_fs (old_fs);
    putname(tmp);
    if (putstat64 (statbuf, &s)) 
	    return -EFAULT;
    return ret;
}

extern asmlinkage long sys_newlstat(char * filename, struct stat * statbuf);

asmlinkage long sys32_lstat64(char * filename, struct stat64_emu31 * statbuf, long flags)
{
    int ret;
    struct stat s;
    char * tmp;
    int err;
    mm_segment_t old_fs = get_fs();
    
    tmp = getname(filename);
    err = PTR_ERR(tmp);
    if (IS_ERR(tmp))   
	    return err;

    set_fs (KERNEL_DS);
    ret = sys_newlstat(tmp, &s);
    set_fs (old_fs);
    putname(tmp);
    if (putstat64 (statbuf, &s)) 
	    return -EFAULT;
    return ret;
}

extern asmlinkage long sys_newfstat(unsigned int fd, struct stat * statbuf);

asmlinkage long sys32_fstat64(unsigned long fd, struct stat64_emu31 * statbuf, long flags)
{
    int ret;
    struct stat s;
    mm_segment_t old_fs = get_fs();
    
    set_fs (KERNEL_DS);
    ret = sys_newfstat(fd, &s);
    set_fs (old_fs);
    if (putstat64 (statbuf, &s))
	    return -EFAULT;
    return ret;
}

/*
 * Linux/i386 didn't use to be able to handle more than
 * 4 system call parameters, so these system calls used a memory
 * block for parameter passing..
 */

struct mmap_arg_struct_emu31 {
	u32	addr;
	u32	len;
	u32	prot;
	u32	flags;
	u32	fd;
	u32	offset;
};

/* common code for old and new mmaps */
static inline long do_mmap2(
	unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	struct file * file = NULL;
	unsigned long error = -EBADF;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	if (!IS_ERR((void *) error) && error + len >= 0x80000000ULL) {
		/* Result is out of bounds.  */
		do_munmap(current->mm, addr, len);
		error = -ENOMEM;
	}
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
out:    
	return error;
}


asmlinkage unsigned long
old32_mmap(struct mmap_arg_struct_emu31 *arg)
{
	struct mmap_arg_struct_emu31 a;
	int error = -EFAULT;

	if (copy_from_user(&a, arg, sizeof(a)))
		goto out;

	error = -EINVAL;
	if (a.offset & ~PAGE_MASK)
		goto out;

	error = do_mmap2(a.addr, a.len, a.prot, a.flags, a.fd, a.offset >> PAGE_SHIFT); 
out:
	return error;
}

asmlinkage long 
sys32_mmap2(struct mmap_arg_struct_emu31 *arg)
{
	struct mmap_arg_struct_emu31 a;
	int error = -EFAULT;

	if (copy_from_user(&a, arg, sizeof(a)))
		goto out;
	error = do_mmap2(a.addr, a.len, a.prot, a.flags, a.fd, a.offset);
out:
	return error;
}

extern asmlinkage long sys_socket(int family, int type, int protocol);
extern asmlinkage long sys_bind(int fd, struct sockaddr *umyaddr, int addrlen);
extern asmlinkage long sys_connect(int fd, struct sockaddr *uservaddr, int addrlen);
extern asmlinkage long sys_listen(int fd, int backlog);
extern asmlinkage long sys_accept(int fd, struct sockaddr *upeer_sockaddr, int *upeer_addrlen);
extern asmlinkage long sys_getsockname(int fd, struct sockaddr *usockaddr, int *usockaddr_len);
extern asmlinkage long sys_getpeername(int fd, struct sockaddr *usockaddr, int *usockaddr_len);
extern asmlinkage long sys_socketpair(int family, int type, int protocol, int usockvec[2]);
extern asmlinkage long sys_send(int fd, void * buff, size_t len, unsigned flags);
extern asmlinkage long sys_sendto(int fd, void * buff, size_t len, unsigned flags,
                           struct sockaddr *addr, int addr_len);
extern asmlinkage long sys_recv(int fd, void * ubuf, size_t size, unsigned flags);
extern asmlinkage long sys_recvfrom(int fd, void * ubuf, size_t size, unsigned flags,
                             struct sockaddr *addr, int *addr_len);
extern asmlinkage long sys_shutdown(int fd, int how);
extern asmlinkage long sys_getsockopt(int fd, int level, int optname, char *optval, int * optlen);

/* Argument list sizes for sys_socketcall */
#define AL(x) ((x) * sizeof(u32))
static unsigned char nas[18] = {AL(0),AL(3),AL(3),AL(3),AL(2),AL(3),
                                AL(3),AL(3),AL(4),AL(4),AL(4),AL(6),
                                AL(6),AL(2),AL(5),AL(5),AL(3),AL(3)};
#undef AL

asmlinkage long sys32_socketcall(int call, u32 *args)
{
	int ret;
	u32 a[6];
				 
	if (call < SYS_SOCKET || call > SYS_RECVMSG)
		return -EINVAL;
	if (copy_from_user(a, args, nas[call]))
		return -EFAULT;
	switch(call) {
	case SYS_SOCKET:
		ret = sys_socket(a[0], a[1], a[2]);
		break;
	case SYS_BIND:
		ret = sys_bind(a[0], (struct sockaddr *) A(a[1]), a[2]);
		break;
	case SYS_CONNECT:
		ret = sys_connect(a[0], (struct sockaddr *) A(a[1]), a[2]);
		break;
	case SYS_LISTEN:
		ret = sys_listen(a[0], a[1]);
		break;
	case SYS_ACCEPT:
		ret = sys_accept(a[0], (struct sockaddr *) A(a[1]),
				 (int *) A(a[2]));
		break;
	case SYS_GETSOCKNAME:
		ret = sys_getsockname(a[0], (struct sockaddr *) A(a[1]),
				      (int *) A(a[2]));
		break;
	case SYS_GETPEERNAME:
		ret = sys_getpeername(a[0], (struct sockaddr *) A(a[1]),
				      (int *) A(a[2]));
		break;
	case SYS_SOCKETPAIR:
		ret = sys_socketpair(a[0], a[1], a[2], (int *) A(a[3]));
		break;
	case SYS_SEND:
		ret = sys_send(a[0], (void *) A(a[1]), a[2], a[3]);
		break;
	case SYS_SENDTO:
		ret = sys_sendto(a[0], (void*) A(a[1]), a[2], a[3], (struct sockaddr *) A(a[4]), a[5]);
		break;
	case SYS_RECV:
		ret = sys_recv(a[0], (void *) A(a[1]), a[2], a[3]);
		break;
	case SYS_RECVFROM:
		ret = sys_recvfrom(a[0], (void *) A(a[1]), a[2], a[3], (struct sockaddr *) A(a[4]), (int *) A(a[5]) );
		break;
	case SYS_SHUTDOWN:
		ret = sys_shutdown(a[0], a[1]);
		break;
	case SYS_SETSOCKOPT:
		ret = sys32_setsockopt(a[0], a[1], a[2], (char *) A(a[3]),
				     a[4]);
		break;
	case SYS_GETSOCKOPT:
		ret = sys_getsockopt(a[0], a[1], a[2], (char *) A(a[3]), (int *) A(a[4]) );
		break;
	case SYS_SENDMSG:
		ret = sys32_sendmsg(a[0], (struct msghdr32 *) A(a[1]),
				    a[2]);
		break;
	case SYS_RECVMSG:
		ret = sys32_recvmsg(a[0], (struct msghdr32 *) A(a[1]),
				    a[2]);
		break;
	default:
		ret = EINVAL;
		break;
	}
	return ret;
}

asmlinkage ssize_t sys_read(unsigned int fd, char * buf, size_t count);

asmlinkage ssize_t32 sys32_read(unsigned int fd, char * buf, size_t count)
{
	if ((ssize_t32) count < 0)
		return -EINVAL; 

	return sys_read(fd, buf, count);
}

asmlinkage ssize_t sys_write(unsigned int fd, const char * buf, size_t count);

asmlinkage ssize_t32 sys32_write(unsigned int fd, char * buf, size_t count)
{
	if ((ssize_t32) count < 0)
		return -EINVAL; 

	return sys_write(fd, buf, count);
}
