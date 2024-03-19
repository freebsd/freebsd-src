/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_capsicum.h"
#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/sbuf.h>
#include <sys/signalvar.h>
#include <sys/kdb.h>
#include <sys/smr.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>

#include <net/vnet.h>

#include <security/audit/audit.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <ddb/ddb.h>

static MALLOC_DEFINE(M_FILEDESC, "filedesc", "Open file descriptor table");
static MALLOC_DEFINE(M_PWD, "pwd", "Descriptor table vnodes");
static MALLOC_DEFINE(M_PWDDESC, "pwddesc", "Pwd descriptors");
static MALLOC_DEFINE(M_FILEDESC_TO_LEADER, "filedesc_to_leader",
    "file desc to leader structures");
static MALLOC_DEFINE(M_SIGIO, "sigio", "sigio structures");
MALLOC_DEFINE(M_FILECAPS, "filecaps", "descriptor capabilities");

MALLOC_DECLARE(M_FADVISE);

static __read_mostly uma_zone_t file_zone;
static __read_mostly uma_zone_t filedesc0_zone;
__read_mostly uma_zone_t pwd_zone;
VFS_SMR_DECLARE;

static int	closefp(struct filedesc *fdp, int fd, struct file *fp,
		    struct thread *td, bool holdleaders, bool audit);
static void	export_file_to_kinfo(struct file *fp, int fd,
		    cap_rights_t *rightsp, struct kinfo_file *kif,
		    struct filedesc *fdp, int flags);
static int	fd_first_free(struct filedesc *fdp, int low, int size);
static void	fdgrowtable(struct filedesc *fdp, int nfd);
static void	fdgrowtable_exp(struct filedesc *fdp, int nfd);
static void	fdunused(struct filedesc *fdp, int fd);
static void	fdused(struct filedesc *fdp, int fd);
static int	fget_unlocked_seq(struct thread *td, int fd,
		    cap_rights_t *needrightsp, struct file **fpp, seqc_t *seqp);
static int	getmaxfd(struct thread *td);
static u_long	*filecaps_copy_prep(const struct filecaps *src);
static void	filecaps_copy_finish(const struct filecaps *src,
		    struct filecaps *dst, u_long *ioctls);
static u_long 	*filecaps_free_prep(struct filecaps *fcaps);
static void	filecaps_free_finish(u_long *ioctls);

static struct pwd *pwd_alloc(void);

/*
 * Each process has:
 *
 * - An array of open file descriptors (fd_ofiles)
 * - An array of file flags (fd_ofileflags)
 * - A bitmap recording which descriptors are in use (fd_map)
 *
 * A process starts out with NDFILE descriptors.  The value of NDFILE has
 * been selected based the historical limit of 20 open files, and an
 * assumption that the majority of processes, especially short-lived
 * processes like shells, will never need more.
 *
 * If this initial allocation is exhausted, a larger descriptor table and
 * map are allocated dynamically, and the pointers in the process's struct
 * filedesc are updated to point to those.  This is repeated every time
 * the process runs out of file descriptors (provided it hasn't hit its
 * resource limit).
 *
 * Since threads may hold references to individual descriptor table
 * entries, the tables are never freed.  Instead, they are placed on a
 * linked list and freed only when the struct filedesc is released.
 */
#define NDFILE		20
#define NDSLOTSIZE	sizeof(NDSLOTTYPE)
#define	NDENTRIES	(NDSLOTSIZE * __CHAR_BIT)
#define NDSLOT(x)	((x) / NDENTRIES)
#define NDBIT(x)	((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define	NDSLOTS(x)	(((x) + NDENTRIES - 1) / NDENTRIES)

#define	FILEDESC_FOREACH_FDE(fdp, _iterator, _fde)				\
	struct filedesc *_fdp = (fdp);						\
	int _lastfile = fdlastfile_single(_fdp);				\
	for (_iterator = 0; _iterator <= _lastfile; _iterator++)		\
		if ((_fde = &_fdp->fd_ofiles[_iterator])->fde_file != NULL)

#define	FILEDESC_FOREACH_FP(fdp, _iterator, _fp)				\
	struct filedesc *_fdp = (fdp);						\
	int _lastfile = fdlastfile_single(_fdp);				\
	for (_iterator = 0; _iterator <= _lastfile; _iterator++)		\
		if ((_fp = _fdp->fd_ofiles[_iterator].fde_file) != NULL)

/*
 * SLIST entry used to keep track of ofiles which must be reclaimed when
 * the process exits.
 */
struct freetable {
	struct fdescenttbl *ft_table;
	SLIST_ENTRY(freetable) ft_next;
};

/*
 * Initial allocation: a filedesc structure + the head of SLIST used to
 * keep track of old ofiles + enough space for NDFILE descriptors.
 */

struct fdescenttbl0 {
	int	fdt_nfiles;
	struct	filedescent fdt_ofiles[NDFILE];
};

struct filedesc0 {
	struct filedesc fd_fd;
	SLIST_HEAD(, freetable) fd_free;
	struct	fdescenttbl0 fd_dfiles;
	NDSLOTTYPE fd_dmap[NDSLOTS(NDFILE)];
};

/*
 * Descriptor management.
 */
static int __exclusive_cache_line openfiles; /* actual number of open files */
struct mtx sigio_lock;		/* mtx to protect pointers to sigio */
void __read_mostly (*mq_fdclose)(struct thread *td, int fd, struct file *fp);

/*
 * If low >= size, just return low. Otherwise find the first zero bit in the
 * given bitmap, starting at low and not exceeding size - 1. Return size if
 * not found.
 */
static int
fd_first_free(struct filedesc *fdp, int low, int size)
{
	NDSLOTTYPE *map = fdp->fd_map;
	NDSLOTTYPE mask;
	int off, maxoff;

	if (low >= size)
		return (low);

	off = NDSLOT(low);
	if (low % NDENTRIES) {
		mask = ~(~(NDSLOTTYPE)0 >> (NDENTRIES - (low % NDENTRIES)));
		if ((mask &= ~map[off]) != 0UL)
			return (off * NDENTRIES + ffsl(mask) - 1);
		++off;
	}
	for (maxoff = NDSLOTS(size); off < maxoff; ++off)
		if (map[off] != ~0UL)
			return (off * NDENTRIES + ffsl(~map[off]) - 1);
	return (size);
}

/*
 * Find the last used fd.
 *
 * Call this variant if fdp can't be modified by anyone else (e.g, during exec).
 * Otherwise use fdlastfile.
 */
int
fdlastfile_single(struct filedesc *fdp)
{
	NDSLOTTYPE *map = fdp->fd_map;
	int off, minoff;

	off = NDSLOT(fdp->fd_nfiles - 1);
	for (minoff = NDSLOT(0); off >= minoff; --off)
		if (map[off] != 0)
			return (off * NDENTRIES + flsl(map[off]) - 1);
	return (-1);
}

int
fdlastfile(struct filedesc *fdp)
{

	FILEDESC_LOCK_ASSERT(fdp);
	return (fdlastfile_single(fdp));
}

static int
fdisused(struct filedesc *fdp, int fd)
{

	KASSERT(fd >= 0 && fd < fdp->fd_nfiles,
	    ("file descriptor %d out of range (0, %d)", fd, fdp->fd_nfiles));

	return ((fdp->fd_map[NDSLOT(fd)] & NDBIT(fd)) != 0);
}

/*
 * Mark a file descriptor as used.
 */
static void
fdused_init(struct filedesc *fdp, int fd)
{

	KASSERT(!fdisused(fdp, fd), ("fd=%d is already used", fd));

	fdp->fd_map[NDSLOT(fd)] |= NDBIT(fd);
}

static void
fdused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);

	fdused_init(fdp, fd);
	if (fd == fdp->fd_freefile)
		fdp->fd_freefile++;
}

/*
 * Mark a file descriptor as unused.
 */
static void
fdunused(struct filedesc *fdp, int fd)
{

	FILEDESC_XLOCK_ASSERT(fdp);

	KASSERT(fdisused(fdp, fd), ("fd=%d is already unused", fd));
	KASSERT(fdp->fd_ofiles[fd].fde_file == NULL,
	    ("fd=%d is still in use", fd));

	fdp->fd_map[NDSLOT(fd)] &= ~NDBIT(fd);
	if (fd < fdp->fd_freefile)
		fdp->fd_freefile = fd;
}

/*
 * Free a file descriptor.
 *
 * Avoid some work if fdp is about to be destroyed.
 */
static inline void
fdefree_last(struct filedescent *fde)
{

	filecaps_free(&fde->fde_caps);
}

static inline void
fdfree(struct filedesc *fdp, int fd)
{
	struct filedescent *fde;

	FILEDESC_XLOCK_ASSERT(fdp);
	fde = &fdp->fd_ofiles[fd];
#ifdef CAPABILITIES
	seqc_write_begin(&fde->fde_seqc);
#endif
	fde->fde_file = NULL;
#ifdef CAPABILITIES
	seqc_write_end(&fde->fde_seqc);
#endif
	fdefree_last(fde);
	fdunused(fdp, fd);
}

/*
 * System calls on descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct getdtablesize_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
sys_getdtablesize(struct thread *td, struct getdtablesize_args *uap)
{
#ifdef	RACCT
	uint64_t lim;
#endif

	td->td_retval[0] = getmaxfd(td);
#ifdef	RACCT
	PROC_LOCK(td->td_proc);
	lim = racct_get_limit(td->td_proc, RACCT_NOFILE);
	PROC_UNLOCK(td->td_proc);
	if (lim < td->td_retval[0])
		td->td_retval[0] = lim;
#endif
	return (0);
}

/*
 * Duplicate a file descriptor to a particular value.
 *
 * Note: keep in mind that a potential race condition exists when closing
 * descriptors from a shared descriptor table (via rfork).
 */
#ifndef _SYS_SYSPROTO_H_
struct dup2_args {
	u_int	from;
	u_int	to;
};
#endif
/* ARGSUSED */
int
sys_dup2(struct thread *td, struct dup2_args *uap)
{

	return (kern_dup(td, FDDUP_FIXED, 0, (int)uap->from, (int)uap->to));
}

/*
 * Duplicate a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct dup_args {
	u_int	fd;
};
#endif
/* ARGSUSED */
int
sys_dup(struct thread *td, struct dup_args *uap)
{

	return (kern_dup(td, FDDUP_NORMAL, 0, (int)uap->fd, 0));
}

/*
 * The file control system call.
 */
#ifndef _SYS_SYSPROTO_H_
struct fcntl_args {
	int	fd;
	int	cmd;
	long	arg;
};
#endif
/* ARGSUSED */
int
sys_fcntl(struct thread *td, struct fcntl_args *uap)
{

	return (kern_fcntl_freebsd(td, uap->fd, uap->cmd, uap->arg));
}

int
kern_fcntl_freebsd(struct thread *td, int fd, int cmd, intptr_t arg)
{
	struct flock fl;
	struct __oflock ofl;
	intptr_t arg1;
	int error, newcmd;

	error = 0;
	newcmd = cmd;
	switch (cmd) {
	case F_OGETLK:
	case F_OSETLK:
	case F_OSETLKW:
		/*
		 * Convert old flock structure to new.
		 */
		error = copyin((void *)arg, &ofl, sizeof(ofl));
		fl.l_start = ofl.l_start;
		fl.l_len = ofl.l_len;
		fl.l_pid = ofl.l_pid;
		fl.l_type = ofl.l_type;
		fl.l_whence = ofl.l_whence;
		fl.l_sysid = 0;

		switch (cmd) {
		case F_OGETLK:
			newcmd = F_GETLK;
			break;
		case F_OSETLK:
			newcmd = F_SETLK;
			break;
		case F_OSETLKW:
			newcmd = F_SETLKW;
			break;
		}
		arg1 = (intptr_t)&fl;
		break;
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
	case F_SETLK_REMOTE:
		error = copyin((void *)arg, &fl, sizeof(fl));
		arg1 = (intptr_t)&fl;
		break;
	default:
		arg1 = arg;
		break;
	}
	if (error)
		return (error);
	error = kern_fcntl(td, fd, newcmd, arg1);
	if (error)
		return (error);
	if (cmd == F_OGETLK) {
		ofl.l_start = fl.l_start;
		ofl.l_len = fl.l_len;
		ofl.l_pid = fl.l_pid;
		ofl.l_type = fl.l_type;
		ofl.l_whence = fl.l_whence;
		error = copyout(&ofl, (void *)arg, sizeof(ofl));
	} else if (cmd == F_GETLK) {
		error = copyout(&fl, (void *)arg, sizeof(fl));
	}
	return (error);
}

int
kern_fcntl(struct thread *td, int fd, int cmd, intptr_t arg)
{
	struct filedesc *fdp;
	struct flock *flp;
	struct file *fp, *fp2;
	struct filedescent *fde;
	struct proc *p;
	struct vnode *vp;
	struct mount *mp;
	struct kinfo_file *kif;
	int error, flg, kif_sz, seals, tmp, got_set, got_cleared;
	uint64_t bsize;
	off_t foffset;

	error = 0;
	flg = F_POSIX;
	p = td->td_proc;
	fdp = p->p_fd;

	AUDIT_ARG_FD(cmd);
	AUDIT_ARG_CMD(cmd);
	switch (cmd) {
	case F_DUPFD:
		tmp = arg;
		error = kern_dup(td, FDDUP_FCNTL, 0, fd, tmp);
		break;

	case F_DUPFD_CLOEXEC:
		tmp = arg;
		error = kern_dup(td, FDDUP_FCNTL, FDDUP_FLAG_CLOEXEC, fd, tmp);
		break;

	case F_DUP2FD:
		tmp = arg;
		error = kern_dup(td, FDDUP_FIXED, 0, fd, tmp);
		break;

	case F_DUP2FD_CLOEXEC:
		tmp = arg;
		error = kern_dup(td, FDDUP_FIXED, FDDUP_FLAG_CLOEXEC, fd, tmp);
		break;

	case F_GETFD:
		error = EBADF;
		FILEDESC_SLOCK(fdp);
		fde = fdeget_noref(fdp, fd);
		if (fde != NULL) {
			td->td_retval[0] =
			    (fde->fde_flags & UF_EXCLOSE) ? FD_CLOEXEC : 0;
			error = 0;
		}
		FILEDESC_SUNLOCK(fdp);
		break;

	case F_SETFD:
		error = EBADF;
		FILEDESC_XLOCK(fdp);
		fde = fdeget_noref(fdp, fd);
		if (fde != NULL) {
			fde->fde_flags = (fde->fde_flags & ~UF_EXCLOSE) |
			    (arg & FD_CLOEXEC ? UF_EXCLOSE : 0);
			error = 0;
		}
		FILEDESC_XUNLOCK(fdp);
		break;

	case F_GETFL:
		error = fget_fcntl(td, fd, &cap_fcntl_rights, F_GETFL, &fp);
		if (error != 0)
			break;
		td->td_retval[0] = OFLAGS(fp->f_flag);
		fdrop(fp, td);
		break;

	case F_SETFL:
		error = fget_fcntl(td, fd, &cap_fcntl_rights, F_SETFL, &fp);
		if (error != 0)
			break;
		if (fp->f_ops == &path_fileops) {
			fdrop(fp, td);
			error = EBADF;
			break;
		}
		do {
			tmp = flg = fp->f_flag;
			tmp &= ~FCNTLFLAGS;
			tmp |= FFLAGS(arg & ~O_ACCMODE) & FCNTLFLAGS;
		} while (atomic_cmpset_int(&fp->f_flag, flg, tmp) == 0);
		got_set = tmp & ~flg;
		got_cleared = flg & ~tmp;
		tmp = fp->f_flag & FNONBLOCK;
		error = fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
		if (error != 0)
			goto revert_f_setfl;
		tmp = fp->f_flag & FASYNC;
		error = fo_ioctl(fp, FIOASYNC, &tmp, td->td_ucred, td);
		if (error == 0) {
			fdrop(fp, td);
			break;
		}
		atomic_clear_int(&fp->f_flag, FNONBLOCK);
		tmp = 0;
		(void)fo_ioctl(fp, FIONBIO, &tmp, td->td_ucred, td);
revert_f_setfl:
		do {
			tmp = flg = fp->f_flag;
			tmp &= ~FCNTLFLAGS;
			tmp |= got_cleared;
			tmp &= ~got_set;
		} while (atomic_cmpset_int(&fp->f_flag, flg, tmp) == 0);
		fdrop(fp, td);
		break;

	case F_GETOWN:
		error = fget_fcntl(td, fd, &cap_fcntl_rights, F_GETOWN, &fp);
		if (error != 0)
			break;
		error = fo_ioctl(fp, FIOGETOWN, &tmp, td->td_ucred, td);
		if (error == 0)
			td->td_retval[0] = tmp;
		fdrop(fp, td);
		break;

	case F_SETOWN:
		error = fget_fcntl(td, fd, &cap_fcntl_rights, F_SETOWN, &fp);
		if (error != 0)
			break;
		tmp = arg;
		error = fo_ioctl(fp, FIOSETOWN, &tmp, td->td_ucred, td);
		fdrop(fp, td);
		break;

	case F_SETLK_REMOTE:
		error = priv_check(td, PRIV_NFS_LOCKD);
		if (error != 0)
			return (error);
		flg = F_REMOTE;
		goto do_setlk;

	case F_SETLKW:
		flg |= F_WAIT;
		/* FALLTHROUGH F_SETLK */

	case F_SETLK:
	do_setlk:
		flp = (struct flock *)arg;
		if ((flg & F_REMOTE) != 0 && flp->l_sysid == 0) {
			error = EINVAL;
			break;
		}

		error = fget_unlocked(td, fd, &cap_flock_rights, &fp);
		if (error != 0)
			break;
		if (fp->f_type != DTYPE_VNODE || fp->f_ops == &path_fileops) {
			error = EBADF;
			fdrop(fp, td);
			break;
		}

		if (flp->l_whence == SEEK_CUR) {
			foffset = foffset_get(fp);
			if (foffset < 0 ||
			    (flp->l_start > 0 &&
			     foffset > OFF_MAX - flp->l_start)) {
				error = EOVERFLOW;
				fdrop(fp, td);
				break;
			}
			flp->l_start += foffset;
		}

		vp = fp->f_vnode;
		switch (flp->l_type) {
		case F_RDLCK:
			if ((fp->f_flag & FREAD) == 0) {
				error = EBADF;
				break;
			}
			if ((p->p_leader->p_flag & P_ADVLOCK) == 0) {
				PROC_LOCK(p->p_leader);
				p->p_leader->p_flag |= P_ADVLOCK;
				PROC_UNLOCK(p->p_leader);
			}
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    flp, flg);
			break;
		case F_WRLCK:
			if ((fp->f_flag & FWRITE) == 0) {
				error = EBADF;
				break;
			}
			if ((p->p_leader->p_flag & P_ADVLOCK) == 0) {
				PROC_LOCK(p->p_leader);
				p->p_leader->p_flag |= P_ADVLOCK;
				PROC_UNLOCK(p->p_leader);
			}
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_SETLK,
			    flp, flg);
			break;
		case F_UNLCK:
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_UNLCK,
			    flp, flg);
			break;
		case F_UNLCKSYS:
			if (flg != F_REMOTE) {
				error = EINVAL;
				break;
			}
			error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader,
			    F_UNLCKSYS, flp, flg);
			break;
		default:
			error = EINVAL;
			break;
		}
		if (error != 0 || flp->l_type == F_UNLCK ||
		    flp->l_type == F_UNLCKSYS) {
			fdrop(fp, td);
			break;
		}

		/*
		 * Check for a race with close.
		 *
		 * The vnode is now advisory locked (or unlocked, but this case
		 * is not really important) as the caller requested.
		 * We had to drop the filedesc lock, so we need to recheck if
		 * the descriptor is still valid, because if it was closed
		 * in the meantime we need to remove advisory lock from the
		 * vnode - close on any descriptor leading to an advisory
		 * locked vnode, removes that lock.
		 * We will return 0 on purpose in that case, as the result of
		 * successful advisory lock might have been externally visible
		 * already. This is fine - effectively we pretend to the caller
		 * that the closing thread was a bit slower and that the
		 * advisory lock succeeded before the close.
		 */
		error = fget_unlocked(td, fd, &cap_no_rights, &fp2);
		if (error != 0) {
			fdrop(fp, td);
			break;
		}
		if (fp != fp2) {
			flp->l_whence = SEEK_SET;
			flp->l_start = 0;
			flp->l_len = 0;
			flp->l_type = F_UNLCK;
			(void) VOP_ADVLOCK(vp, (caddr_t)p->p_leader,
			    F_UNLCK, flp, F_POSIX);
		}
		fdrop(fp, td);
		fdrop(fp2, td);
		break;

	case F_GETLK:
		error = fget_unlocked(td, fd, &cap_flock_rights, &fp);
		if (error != 0)
			break;
		if (fp->f_type != DTYPE_VNODE || fp->f_ops == &path_fileops) {
			error = EBADF;
			fdrop(fp, td);
			break;
		}
		flp = (struct flock *)arg;
		if (flp->l_type != F_RDLCK && flp->l_type != F_WRLCK &&
		    flp->l_type != F_UNLCK) {
			error = EINVAL;
			fdrop(fp, td);
			break;
		}
		if (flp->l_whence == SEEK_CUR) {
			foffset = foffset_get(fp);
			if ((flp->l_start > 0 &&
			    foffset > OFF_MAX - flp->l_start) ||
			    (flp->l_start < 0 &&
			    foffset < OFF_MIN - flp->l_start)) {
				error = EOVERFLOW;
				fdrop(fp, td);
				break;
			}
			flp->l_start += foffset;
		}
		vp = fp->f_vnode;
		error = VOP_ADVLOCK(vp, (caddr_t)p->p_leader, F_GETLK, flp,
		    F_POSIX);
		fdrop(fp, td);
		break;

	case F_ADD_SEALS:
		error = fget_unlocked(td, fd, &cap_no_rights, &fp);
		if (error != 0)
			break;
		error = fo_add_seals(fp, arg);
		fdrop(fp, td);
		break;

	case F_GET_SEALS:
		error = fget_unlocked(td, fd, &cap_no_rights, &fp);
		if (error != 0)
			break;
		if (fo_get_seals(fp, &seals) == 0)
			td->td_retval[0] = seals;
		else
			error = EINVAL;
		fdrop(fp, td);
		break;

	case F_RDAHEAD:
		arg = arg ? 128 * 1024: 0;
		/* FALLTHROUGH */
	case F_READAHEAD:
		error = fget_unlocked(td, fd, &cap_no_rights, &fp);
		if (error != 0)
			break;
		if (fp->f_type != DTYPE_VNODE || fp->f_ops == &path_fileops) {
			fdrop(fp, td);
			error = EBADF;
			break;
		}
		vp = fp->f_vnode;
		if (vp->v_type != VREG) {
			fdrop(fp, td);
			error = ENOTTY;
			break;
		}

		/*
		 * Exclusive lock synchronizes against f_seqcount reads and
		 * writes in sequential_heuristic().
		 */
		error = vn_lock(vp, LK_EXCLUSIVE);
		if (error != 0) {
			fdrop(fp, td);
			break;
		}
		if (arg >= 0) {
			bsize = fp->f_vnode->v_mount->mnt_stat.f_iosize;
			arg = MIN(arg, INT_MAX - bsize + 1);
			fp->f_seqcount[UIO_READ] = MIN(IO_SEQMAX,
			    (arg + bsize - 1) / bsize);
			atomic_set_int(&fp->f_flag, FRDAHEAD);
		} else {
			atomic_clear_int(&fp->f_flag, FRDAHEAD);
		}
		VOP_UNLOCK(vp);
		fdrop(fp, td);
		break;

	case F_ISUNIONSTACK:
		/*
		 * Check if the vnode is part of a union stack (either the
		 * "union" flag from mount(2) or unionfs).
		 *
		 * Prior to introduction of this op libc's readdir would call
		 * fstatfs(2), in effect unnecessarily copying kilobytes of
		 * data just to check fs name and a mount flag.
		 *
		 * Fixing the code to handle everything in the kernel instead
		 * is a non-trivial endeavor and has low priority, thus this
		 * horrible kludge facilitates the current behavior in a much
		 * cheaper manner until someone(tm) sorts this out.
		 */
		error = fget_unlocked(td, fd, &cap_no_rights, &fp);
		if (error != 0)
			break;
		if (fp->f_type != DTYPE_VNODE) {
			fdrop(fp, td);
			error = EBADF;
			break;
		}
		vp = fp->f_vnode;
		/*
		 * Since we don't prevent dooming the vnode even non-null mp
		 * found can become immediately stale. This is tolerable since
		 * mount points are type-stable (providing safe memory access)
		 * and any vfs op on this vnode going forward will return an
		 * error (meaning return value in this case is meaningless).
		 */
		mp = atomic_load_ptr(&vp->v_mount);
		if (__predict_false(mp == NULL)) {
			fdrop(fp, td);
			error = EBADF;
			break;
		}
		td->td_retval[0] = 0;
		if (mp->mnt_kern_flag & MNTK_UNIONFS ||
		    mp->mnt_flag & MNT_UNION)
			td->td_retval[0] = 1;
		fdrop(fp, td);
		break;

	case F_KINFO:
#ifdef CAPABILITY_MODE
		if (IN_CAPABILITY_MODE(td)) {
			error = ECAPMODE;
			break;
		}
#endif
		error = copyin((void *)arg, &kif_sz, sizeof(kif_sz));
		if (error != 0)
			break;
		if (kif_sz != sizeof(*kif)) {
			error = EINVAL;
			break;
		}
		kif = malloc(sizeof(*kif), M_TEMP, M_WAITOK | M_ZERO);
		FILEDESC_SLOCK(fdp);
		error = fget_cap_noref(fdp, fd, &cap_fcntl_rights, &fp, NULL);
		if (error == 0 && fhold(fp)) {
			export_file_to_kinfo(fp, fd, NULL, kif, fdp, 0);
			FILEDESC_SUNLOCK(fdp);
			fdrop(fp, td);
			if ((kif->kf_status & KF_ATTR_VALID) != 0) {
				kif->kf_structsize = sizeof(*kif);
				error = copyout(kif, (void *)arg, sizeof(*kif));
			} else {
				error = EBADF;
			}
		} else {
			FILEDESC_SUNLOCK(fdp);
			if (error == 0)
				error = EBADF;
		}
		free(kif, M_TEMP);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error);
}

static int
getmaxfd(struct thread *td)
{

	return (min((int)lim_cur(td, RLIMIT_NOFILE), maxfilesperproc));
}

/*
 * Common code for dup, dup2, fcntl(F_DUPFD) and fcntl(F_DUP2FD).
 */
int
kern_dup(struct thread *td, u_int mode, int flags, int old, int new)
{
	struct filedesc *fdp;
	struct filedescent *oldfde, *newfde;
	struct proc *p;
	struct file *delfp, *oldfp;
	u_long *oioctls, *nioctls;
	int error, maxfd;

	p = td->td_proc;
	fdp = p->p_fd;
	oioctls = NULL;

	MPASS((flags & ~(FDDUP_FLAG_CLOEXEC)) == 0);
	MPASS(mode < FDDUP_LASTMODE);

	AUDIT_ARG_FD(old);
	/* XXXRW: if (flags & FDDUP_FIXED) AUDIT_ARG_FD2(new); */

	/*
	 * Verify we have a valid descriptor to dup from and possibly to
	 * dup to. Unlike dup() and dup2(), fcntl()'s F_DUPFD should
	 * return EINVAL when the new descriptor is out of bounds.
	 */
	if (old < 0)
		return (EBADF);
	if (new < 0)
		return (mode == FDDUP_FCNTL ? EINVAL : EBADF);
	maxfd = getmaxfd(td);
	if (new >= maxfd)
		return (mode == FDDUP_FCNTL ? EINVAL : EBADF);

	error = EBADF;
	FILEDESC_XLOCK(fdp);
	if (fget_noref(fdp, old) == NULL)
		goto unlock;
	if (mode == FDDUP_FIXED && old == new) {
		td->td_retval[0] = new;
		if (flags & FDDUP_FLAG_CLOEXEC)
			fdp->fd_ofiles[new].fde_flags |= UF_EXCLOSE;
		error = 0;
		goto unlock;
	}

	oldfde = &fdp->fd_ofiles[old];
	oldfp = oldfde->fde_file;
	if (!fhold(oldfp))
		goto unlock;

	/*
	 * If the caller specified a file descriptor, make sure the file
	 * table is large enough to hold it, and grab it.  Otherwise, just
	 * allocate a new descriptor the usual way.
	 */
	switch (mode) {
	case FDDUP_NORMAL:
	case FDDUP_FCNTL:
		if ((error = fdalloc(td, new, &new)) != 0) {
			fdrop(oldfp, td);
			goto unlock;
		}
		break;
	case FDDUP_FIXED:
		if (new >= fdp->fd_nfiles) {
			/*
			 * The resource limits are here instead of e.g.
			 * fdalloc(), because the file descriptor table may be
			 * shared between processes, so we can't really use
			 * racct_add()/racct_sub().  Instead of counting the
			 * number of actually allocated descriptors, just put
			 * the limit on the size of the file descriptor table.
			 */
#ifdef RACCT
			if (RACCT_ENABLED()) {
				error = racct_set_unlocked(p, RACCT_NOFILE, new + 1);
				if (error != 0) {
					error = EMFILE;
					fdrop(oldfp, td);
					goto unlock;
				}
			}
#endif
			fdgrowtable_exp(fdp, new + 1);
		}
		if (!fdisused(fdp, new))
			fdused(fdp, new);
		break;
	default:
		KASSERT(0, ("%s unsupported mode %d", __func__, mode));
	}

	KASSERT(old != new, ("new fd is same as old"));

	/* Refetch oldfde because the table may have grown and old one freed. */
	oldfde = &fdp->fd_ofiles[old];
	KASSERT(oldfp == oldfde->fde_file,
	    ("fdt_ofiles shift from growth observed at fd %d",
	    old));

	newfde = &fdp->fd_ofiles[new];
	delfp = newfde->fde_file;

	nioctls = filecaps_copy_prep(&oldfde->fde_caps);

	/*
	 * Duplicate the source descriptor.
	 */
#ifdef CAPABILITIES
	seqc_write_begin(&newfde->fde_seqc);
#endif
	oioctls = filecaps_free_prep(&newfde->fde_caps);
	fde_copy(oldfde, newfde);
	filecaps_copy_finish(&oldfde->fde_caps, &newfde->fde_caps,
	    nioctls);
	if ((flags & FDDUP_FLAG_CLOEXEC) != 0)
		newfde->fde_flags = oldfde->fde_flags | UF_EXCLOSE;
	else
		newfde->fde_flags = oldfde->fde_flags & ~UF_EXCLOSE;
#ifdef CAPABILITIES
	seqc_write_end(&newfde->fde_seqc);
#endif
	td->td_retval[0] = new;

	error = 0;

	if (delfp != NULL) {
		(void) closefp(fdp, new, delfp, td, true, false);
		FILEDESC_UNLOCK_ASSERT(fdp);
	} else {
unlock:
		FILEDESC_XUNLOCK(fdp);
	}

	filecaps_free_finish(oioctls);
	return (error);
}

static void
sigiofree(struct sigio *sigio)
{
	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO);
}

static struct sigio *
funsetown_locked(struct sigio *sigio)
{
	struct proc *p;
	struct pgrp *pg;

	SIGIO_ASSERT_LOCKED();

	if (sigio == NULL)
		return (NULL);
	*sigio->sio_myref = NULL;
	if (sigio->sio_pgid < 0) {
		pg = sigio->sio_pgrp;
		PGRP_LOCK(pg);
		SLIST_REMOVE(&pg->pg_sigiolst, sigio, sigio, sio_pgsigio);
		PGRP_UNLOCK(pg);
	} else {
		p = sigio->sio_proc;
		PROC_LOCK(p);
		SLIST_REMOVE(&p->p_sigiolst, sigio, sigio, sio_pgsigio);
		PROC_UNLOCK(p);
	}
	return (sigio);
}

/*
 * If sigio is on the list associated with a process or process group,
 * disable signalling from the device, remove sigio from the list and
 * free sigio.
 */
void
funsetown(struct sigio **sigiop)
{
	struct sigio *sigio;

	/* Racy check, consumers must provide synchronization. */
	if (*sigiop == NULL)
		return;

	SIGIO_LOCK();
	sigio = funsetown_locked(*sigiop);
	SIGIO_UNLOCK();
	if (sigio != NULL)
		sigiofree(sigio);
}

/*
 * Free a list of sigio structures.  The caller must ensure that new sigio
 * structures cannot be added after this point.  For process groups this is
 * guaranteed using the proctree lock; for processes, the P_WEXIT flag serves
 * as an interlock.
 */
void
funsetownlst(struct sigiolst *sigiolst)
{
	struct proc *p;
	struct pgrp *pg;
	struct sigio *sigio, *tmp;

	/* Racy check. */
	sigio = SLIST_FIRST(sigiolst);
	if (sigio == NULL)
		return;

	p = NULL;
	pg = NULL;

	SIGIO_LOCK();
	sigio = SLIST_FIRST(sigiolst);
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}

	/*
	 * Every entry of the list should belong to a single proc or pgrp.
	 */
	if (sigio->sio_pgid < 0) {
		pg = sigio->sio_pgrp;
		sx_assert(&proctree_lock, SX_XLOCKED);
		PGRP_LOCK(pg);
	} else /* if (sigio->sio_pgid > 0) */ {
		p = sigio->sio_proc;
		PROC_LOCK(p);
		KASSERT((p->p_flag & P_WEXIT) != 0,
		    ("%s: process %p is not exiting", __func__, p));
	}

	SLIST_FOREACH(sigio, sigiolst, sio_pgsigio) {
		*sigio->sio_myref = NULL;
		if (pg != NULL) {
			KASSERT(sigio->sio_pgid < 0,
			    ("Proc sigio in pgrp sigio list"));
			KASSERT(sigio->sio_pgrp == pg,
			    ("Bogus pgrp in sigio list"));
		} else /* if (p != NULL) */ {
			KASSERT(sigio->sio_pgid > 0,
			    ("Pgrp sigio in proc sigio list"));
			KASSERT(sigio->sio_proc == p,
			    ("Bogus proc in sigio list"));
		}
	}

	if (pg != NULL)
		PGRP_UNLOCK(pg);
	else
		PROC_UNLOCK(p);
	SIGIO_UNLOCK();

	SLIST_FOREACH_SAFE(sigio, sigiolst, sio_pgsigio, tmp)
		sigiofree(sigio);
}

/*
 * This is common code for FIOSETOWN ioctl called by fcntl(fd, F_SETOWN, arg).
 *
 * After permission checking, add a sigio structure to the sigio list for
 * the process or process group.
 */
int
fsetown(pid_t pgid, struct sigio **sigiop)
{
	struct proc *proc;
	struct pgrp *pgrp;
	struct sigio *osigio, *sigio;
	int ret;

	if (pgid == 0) {
		funsetown(sigiop);
		return (0);
	}

	sigio = malloc(sizeof(struct sigio), M_SIGIO, M_WAITOK);
	sigio->sio_pgid = pgid;
	sigio->sio_ucred = crhold(curthread->td_ucred);
	sigio->sio_myref = sigiop;

	ret = 0;
	if (pgid > 0) {
		ret = pget(pgid, PGET_NOTWEXIT | PGET_NOTID | PGET_HOLD, &proc);
		SIGIO_LOCK();
		osigio = funsetown_locked(*sigiop);
		if (ret == 0) {
			PROC_LOCK(proc);
			_PRELE(proc);
			if ((proc->p_flag & P_WEXIT) != 0) {
				ret = ESRCH;
			} else if (proc->p_session !=
			    curthread->td_proc->p_session) {
				/*
				 * Policy - Don't allow a process to FSETOWN a
				 * process in another session.
				 *
				 * Remove this test to allow maximum flexibility
				 * or restrict FSETOWN to the current process or
				 * process group for maximum safety.
				 */
				ret = EPERM;
			} else {
				sigio->sio_proc = proc;
				SLIST_INSERT_HEAD(&proc->p_sigiolst, sigio,
				    sio_pgsigio);
			}
			PROC_UNLOCK(proc);
		}
	} else /* if (pgid < 0) */ {
		sx_slock(&proctree_lock);
		SIGIO_LOCK();
		osigio = funsetown_locked(*sigiop);
		pgrp = pgfind(-pgid);
		if (pgrp == NULL) {
			ret = ESRCH;
		} else {
			if (pgrp->pg_session != curthread->td_proc->p_session) {
				/*
				 * Policy - Don't allow a process to FSETOWN a
				 * process in another session.
				 *
				 * Remove this test to allow maximum flexibility
				 * or restrict FSETOWN to the current process or
				 * process group for maximum safety.
				 */
				ret = EPERM;
			} else {
				sigio->sio_pgrp = pgrp;
				SLIST_INSERT_HEAD(&pgrp->pg_sigiolst, sigio,
				    sio_pgsigio);
			}
			PGRP_UNLOCK(pgrp);
		}
		sx_sunlock(&proctree_lock);
	}
	if (ret == 0)
		*sigiop = sigio;
	SIGIO_UNLOCK();
	if (osigio != NULL)
		sigiofree(osigio);
	return (ret);
}

/*
 * This is common code for FIOGETOWN ioctl called by fcntl(fd, F_GETOWN, arg).
 */
pid_t
fgetown(struct sigio **sigiop)
{
	pid_t pgid;

	SIGIO_LOCK();
	pgid = (*sigiop != NULL) ? (*sigiop)->sio_pgid : 0;
	SIGIO_UNLOCK();
	return (pgid);
}

static int
closefp_impl(struct filedesc *fdp, int fd, struct file *fp, struct thread *td,
    bool audit)
{
	int error;

	FILEDESC_XLOCK_ASSERT(fdp);

	/*
	 * We now hold the fp reference that used to be owned by the
	 * descriptor array.  We have to unlock the FILEDESC *AFTER*
	 * knote_fdclose to prevent a race of the fd getting opened, a knote
	 * added, and deleteing a knote for the new fd.
	 */
	if (__predict_false(!TAILQ_EMPTY(&fdp->fd_kqlist)))
		knote_fdclose(td, fd);

	/*
	 * We need to notify mqueue if the object is of type mqueue.
	 */
	if (__predict_false(fp->f_type == DTYPE_MQUEUE))
		mq_fdclose(td, fd, fp);
	FILEDESC_XUNLOCK(fdp);

#ifdef AUDIT
	if (AUDITING_TD(td) && audit)
		audit_sysclose(td, fd, fp);
#endif
	error = closef(fp, td);

	/*
	 * All paths leading up to closefp() will have already removed or
	 * replaced the fd in the filedesc table, so a restart would not
	 * operate on the same file.
	 */
	if (error == ERESTART)
		error = EINTR;

	return (error);
}

static int
closefp_hl(struct filedesc *fdp, int fd, struct file *fp, struct thread *td,
    bool holdleaders, bool audit)
{
	int error;

	FILEDESC_XLOCK_ASSERT(fdp);

	if (holdleaders) {
		if (td->td_proc->p_fdtol != NULL) {
			/*
			 * Ask fdfree() to sleep to ensure that all relevant
			 * process leaders can be traversed in closef().
			 */
			fdp->fd_holdleaderscount++;
		} else {
			holdleaders = false;
		}
	}

	error = closefp_impl(fdp, fd, fp, td, audit);
	if (holdleaders) {
		FILEDESC_XLOCK(fdp);
		fdp->fd_holdleaderscount--;
		if (fdp->fd_holdleaderscount == 0 &&
		    fdp->fd_holdleaderswakeup != 0) {
			fdp->fd_holdleaderswakeup = 0;
			wakeup(&fdp->fd_holdleaderscount);
		}
		FILEDESC_XUNLOCK(fdp);
	}
	return (error);
}

static int
closefp(struct filedesc *fdp, int fd, struct file *fp, struct thread *td,
    bool holdleaders, bool audit)
{

	FILEDESC_XLOCK_ASSERT(fdp);

	if (__predict_false(td->td_proc->p_fdtol != NULL)) {
		return (closefp_hl(fdp, fd, fp, td, holdleaders, audit));
	} else {
		return (closefp_impl(fdp, fd, fp, td, audit));
	}
}

/*
 * Close a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct close_args {
	int     fd;
};
#endif
/* ARGSUSED */
int
sys_close(struct thread *td, struct close_args *uap)
{

	return (kern_close(td, uap->fd));
}

int
kern_close(struct thread *td, int fd)
{
	struct filedesc *fdp;
	struct file *fp;

	fdp = td->td_proc->p_fd;

	FILEDESC_XLOCK(fdp);
	if ((fp = fget_noref(fdp, fd)) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	fdfree(fdp, fd);

	/* closefp() drops the FILEDESC lock for us. */
	return (closefp(fdp, fd, fp, td, true, true));
}

static int
close_range_cloexec(struct thread *td, u_int lowfd, u_int highfd)
{
	struct filedesc *fdp;
	struct fdescenttbl *fdt;
	struct filedescent *fde;
	int fd;

	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);
	fdt = atomic_load_ptr(&fdp->fd_files);
	highfd = MIN(highfd, fdt->fdt_nfiles - 1);
	fd = lowfd;
	if (__predict_false(fd > highfd)) {
		goto out_locked;
	}
	for (; fd <= highfd; fd++) {
		fde = &fdt->fdt_ofiles[fd];
		if (fde->fde_file != NULL)
			fde->fde_flags |= UF_EXCLOSE;
	}
out_locked:
	FILEDESC_XUNLOCK(fdp);
	return (0);
}

static int
close_range_impl(struct thread *td, u_int lowfd, u_int highfd)
{
	struct filedesc *fdp;
	const struct fdescenttbl *fdt;
	struct file *fp;
	int fd;

	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);
	fdt = atomic_load_ptr(&fdp->fd_files);
	highfd = MIN(highfd, fdt->fdt_nfiles - 1);
	fd = lowfd;
	if (__predict_false(fd > highfd)) {
		goto out_locked;
	}
	for (;;) {
		fp = fdt->fdt_ofiles[fd].fde_file;
		if (fp == NULL) {
			if (fd == highfd)
				goto out_locked;
		} else {
			fdfree(fdp, fd);
			(void) closefp(fdp, fd, fp, td, true, true);
			if (fd == highfd)
				goto out_unlocked;
			FILEDESC_XLOCK(fdp);
			fdt = atomic_load_ptr(&fdp->fd_files);
		}
		fd++;
	}
out_locked:
	FILEDESC_XUNLOCK(fdp);
out_unlocked:
	return (0);
}

int
kern_close_range(struct thread *td, int flags, u_int lowfd, u_int highfd)
{

	/*
	 * Check this prior to clamping; closefrom(3) with only fd 0, 1, and 2
	 * open should not be a usage error.  From a close_range() perspective,
	 * close_range(3, ~0U, 0) in the same scenario should also likely not
	 * be a usage error as all fd above 3 are in-fact already closed.
	 */
	if (highfd < lowfd) {
		return (EINVAL);
	}

	if ((flags & CLOSE_RANGE_CLOEXEC) != 0)
		return (close_range_cloexec(td, lowfd, highfd));

	return (close_range_impl(td, lowfd, highfd));
}

#ifndef _SYS_SYSPROTO_H_
struct close_range_args {
	u_int	lowfd;
	u_int	highfd;
	int	flags;
};
#endif
int
sys_close_range(struct thread *td, struct close_range_args *uap)
{

	AUDIT_ARG_FD(uap->lowfd);
	AUDIT_ARG_CMD(uap->highfd);
	AUDIT_ARG_FFLAGS(uap->flags);

	if ((uap->flags & ~(CLOSE_RANGE_CLOEXEC)) != 0)
		return (EINVAL);
	return (kern_close_range(td, uap->flags, uap->lowfd, uap->highfd));
}

#ifdef COMPAT_FREEBSD12
/*
 * Close open file descriptors.
 */
#ifndef _SYS_SYSPROTO_H_
struct freebsd12_closefrom_args {
	int	lowfd;
};
#endif
/* ARGSUSED */
int
freebsd12_closefrom(struct thread *td, struct freebsd12_closefrom_args *uap)
{
	u_int lowfd;

	AUDIT_ARG_FD(uap->lowfd);

	/*
	 * Treat negative starting file descriptor values identical to
	 * closefrom(0) which closes all files.
	 */
	lowfd = MAX(0, uap->lowfd);
	return (kern_close_range(td, 0, lowfd, ~0U));
}
#endif	/* COMPAT_FREEBSD12 */

#if defined(COMPAT_43)
/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct ofstat_args {
	int	fd;
	struct	ostat *sb;
};
#endif
/* ARGSUSED */
int
ofstat(struct thread *td, struct ofstat_args *uap)
{
	struct ostat oub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0) {
		cvtstat(&ub, &oub);
		error = copyout(&oub, uap->sb, sizeof(oub));
	}
	return (error);
}
#endif /* COMPAT_43 */

#if defined(COMPAT_FREEBSD11)
int
freebsd11_fstat(struct thread *td, struct freebsd11_fstat_args *uap)
{
	struct stat sb;
	struct freebsd11_stat osb;
	int error;

	error = kern_fstat(td, uap->fd, &sb);
	if (error != 0)
		return (error);
	error = freebsd11_cvtstat(&sb, &osb);
	if (error == 0)
		error = copyout(&osb, uap->sb, sizeof(osb));
	return (error);
}
#endif	/* COMPAT_FREEBSD11 */

/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fstat_args {
	int	fd;
	struct	stat *sb;
};
#endif
/* ARGSUSED */
int
sys_fstat(struct thread *td, struct fstat_args *uap)
{
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error == 0)
		error = copyout(&ub, uap->sb, sizeof(ub));
	return (error);
}

int
kern_fstat(struct thread *td, int fd, struct stat *sbp)
{
	struct file *fp;
	int error;

	AUDIT_ARG_FD(fd);

	error = fget(td, fd, &cap_fstat_rights, &fp);
	if (__predict_false(error != 0))
		return (error);

	AUDIT_ARG_FILE(td->td_proc, fp);

	error = fo_stat(fp, sbp, td->td_ucred);
	fdrop(fp, td);
#ifdef __STAT_TIME_T_EXT
	sbp->st_atim_ext = 0;
	sbp->st_mtim_ext = 0;
	sbp->st_ctim_ext = 0;
	sbp->st_btim_ext = 0;
#endif
#ifdef KTRACE
	if (KTRPOINT(td, KTR_STRUCT))
		ktrstat_error(sbp, error);
#endif
	return (error);
}

#if defined(COMPAT_FREEBSD11)
/*
 * Return status information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct freebsd11_nfstat_args {
	int	fd;
	struct	nstat *sb;
};
#endif
/* ARGSUSED */
int
freebsd11_nfstat(struct thread *td, struct freebsd11_nfstat_args *uap)
{
	struct nstat nub;
	struct stat ub;
	int error;

	error = kern_fstat(td, uap->fd, &ub);
	if (error != 0)
		return (error);
	error = freebsd11_cvtnstat(&ub, &nub);
	if (error != 0)
		error = copyout(&nub, uap->sb, sizeof(nub));
	return (error);
}
#endif /* COMPAT_FREEBSD11 */

/*
 * Return pathconf information about a file descriptor.
 */
#ifndef _SYS_SYSPROTO_H_
struct fpathconf_args {
	int	fd;
	int	name;
};
#endif
/* ARGSUSED */
int
sys_fpathconf(struct thread *td, struct fpathconf_args *uap)
{
	long value;
	int error;

	error = kern_fpathconf(td, uap->fd, uap->name, &value);
	if (error == 0)
		td->td_retval[0] = value;
	return (error);
}

int
kern_fpathconf(struct thread *td, int fd, int name, long *valuep)
{
	struct file *fp;
	struct vnode *vp;
	int error;

	error = fget(td, fd, &cap_fpathconf_rights, &fp);
	if (error != 0)
		return (error);

	if (name == _PC_ASYNC_IO) {
		*valuep = _POSIX_ASYNCHRONOUS_IO;
		goto out;
	}
	vp = fp->f_vnode;
	if (vp != NULL) {
		vn_lock(vp, LK_SHARED | LK_RETRY);
		error = VOP_PATHCONF(vp, name, valuep);
		VOP_UNLOCK(vp);
	} else if (fp->f_type == DTYPE_PIPE || fp->f_type == DTYPE_SOCKET) {
		if (name != _PC_PIPE_BUF) {
			error = EINVAL;
		} else {
			*valuep = PIPE_BUF;
			error = 0;
		}
	} else {
		error = EOPNOTSUPP;
	}
out:
	fdrop(fp, td);
	return (error);
}

/*
 * Copy filecaps structure allocating memory for ioctls array if needed.
 *
 * The last parameter indicates whether the fdtable is locked. If it is not and
 * ioctls are encountered, copying fails and the caller must lock the table.
 *
 * Note that if the table was not locked, the caller has to check the relevant
 * sequence counter to determine whether the operation was successful.
 */
bool
filecaps_copy(const struct filecaps *src, struct filecaps *dst, bool locked)
{
	size_t size;

	if (src->fc_ioctls != NULL && !locked)
		return (false);
	memcpy(dst, src, sizeof(*src));
	if (src->fc_ioctls == NULL)
		return (true);

	KASSERT(src->fc_nioctls > 0,
	    ("fc_ioctls != NULL, but fc_nioctls=%hd", src->fc_nioctls));

	size = sizeof(src->fc_ioctls[0]) * src->fc_nioctls;
	dst->fc_ioctls = malloc(size, M_FILECAPS, M_WAITOK);
	memcpy(dst->fc_ioctls, src->fc_ioctls, size);
	return (true);
}

static u_long *
filecaps_copy_prep(const struct filecaps *src)
{
	u_long *ioctls;
	size_t size;

	if (__predict_true(src->fc_ioctls == NULL))
		return (NULL);

	KASSERT(src->fc_nioctls > 0,
	    ("fc_ioctls != NULL, but fc_nioctls=%hd", src->fc_nioctls));

	size = sizeof(src->fc_ioctls[0]) * src->fc_nioctls;
	ioctls = malloc(size, M_FILECAPS, M_WAITOK);
	return (ioctls);
}

static void
filecaps_copy_finish(const struct filecaps *src, struct filecaps *dst,
    u_long *ioctls)
{
	size_t size;

	*dst = *src;
	if (__predict_true(src->fc_ioctls == NULL)) {
		MPASS(ioctls == NULL);
		return;
	}

	size = sizeof(src->fc_ioctls[0]) * src->fc_nioctls;
	dst->fc_ioctls = ioctls;
	bcopy(src->fc_ioctls, dst->fc_ioctls, size);
}

/*
 * Move filecaps structure to the new place and clear the old place.
 */
void
filecaps_move(struct filecaps *src, struct filecaps *dst)
{

	*dst = *src;
	bzero(src, sizeof(*src));
}

/*
 * Fill the given filecaps structure with full rights.
 */
static void
filecaps_fill(struct filecaps *fcaps)
{

	CAP_ALL(&fcaps->fc_rights);
	fcaps->fc_ioctls = NULL;
	fcaps->fc_nioctls = -1;
	fcaps->fc_fcntls = CAP_FCNTL_ALL;
}

/*
 * Free memory allocated within filecaps structure.
 */
static void
filecaps_free_ioctl(struct filecaps *fcaps)
{

	free(fcaps->fc_ioctls, M_FILECAPS);
	fcaps->fc_ioctls = NULL;
}

void
filecaps_free(struct filecaps *fcaps)
{

	filecaps_free_ioctl(fcaps);
	bzero(fcaps, sizeof(*fcaps));
}

static u_long *
filecaps_free_prep(struct filecaps *fcaps)
{
	u_long *ioctls;

	ioctls = fcaps->fc_ioctls;
	bzero(fcaps, sizeof(*fcaps));
	return (ioctls);
}

static void
filecaps_free_finish(u_long *ioctls)
{

	free(ioctls, M_FILECAPS);
}

/*
 * Validate the given filecaps structure.
 */
static void
filecaps_validate(const struct filecaps *fcaps, const char *func)
{

	KASSERT(cap_rights_is_valid(&fcaps->fc_rights),
	    ("%s: invalid rights", func));
	KASSERT((fcaps->fc_fcntls & ~CAP_FCNTL_ALL) == 0,
	    ("%s: invalid fcntls", func));
	KASSERT(fcaps->fc_fcntls == 0 ||
	    cap_rights_is_set(&fcaps->fc_rights, CAP_FCNTL),
	    ("%s: fcntls without CAP_FCNTL", func));
	/*
	 * open calls without WANTIOCTLCAPS free caps but leave the counter
	 */
#if 0
	KASSERT(fcaps->fc_ioctls != NULL ? fcaps->fc_nioctls > 0 :
	    (fcaps->fc_nioctls == -1 || fcaps->fc_nioctls == 0),
	    ("%s: invalid ioctls", func));
#endif
	KASSERT(fcaps->fc_nioctls == 0 ||
	    cap_rights_is_set(&fcaps->fc_rights, CAP_IOCTL),
	    ("%s: ioctls without CAP_IOCTL", func));
}

static void
fdgrowtable_exp(struct filedesc *fdp, int nfd)
{
	int nfd1;

	FILEDESC_XLOCK_ASSERT(fdp);

	nfd1 = fdp->fd_nfiles * 2;
	if (nfd1 < nfd)
		nfd1 = nfd;
	fdgrowtable(fdp, nfd1);
}

/*
 * Grow the file table to accommodate (at least) nfd descriptors.
 */
static void
fdgrowtable(struct filedesc *fdp, int nfd)
{
	struct filedesc0 *fdp0;
	struct freetable *ft;
	struct fdescenttbl *ntable;
	struct fdescenttbl *otable;
	int nnfiles, onfiles;
	NDSLOTTYPE *nmap, *omap;

	KASSERT(fdp->fd_nfiles > 0, ("zero-length file table"));

	/* save old values */
	onfiles = fdp->fd_nfiles;
	otable = fdp->fd_files;
	omap = fdp->fd_map;

	/* compute the size of the new table */
	nnfiles = NDSLOTS(nfd) * NDENTRIES; /* round up */
	if (nnfiles <= onfiles)
		/* the table is already large enough */
		return;

	/*
	 * Allocate a new table.  We need enough space for the number of
	 * entries, file entries themselves and the struct freetable we will use
	 * when we decommission the table and place it on the freelist.
	 * We place the struct freetable in the middle so we don't have
	 * to worry about padding.
	 */
	ntable = malloc(offsetof(struct fdescenttbl, fdt_ofiles) +
	    nnfiles * sizeof(ntable->fdt_ofiles[0]) +
	    sizeof(struct freetable),
	    M_FILEDESC, M_ZERO | M_WAITOK);
	/* copy the old data */
	ntable->fdt_nfiles = nnfiles;
	memcpy(ntable->fdt_ofiles, otable->fdt_ofiles,
	    onfiles * sizeof(ntable->fdt_ofiles[0]));

	/*
	 * Allocate a new map only if the old is not large enough.  It will
	 * grow at a slower rate than the table as it can map more
	 * entries than the table can hold.
	 */
	if (NDSLOTS(nnfiles) > NDSLOTS(onfiles)) {
		nmap = malloc(NDSLOTS(nnfiles) * NDSLOTSIZE, M_FILEDESC,
		    M_ZERO | M_WAITOK);
		/* copy over the old data and update the pointer */
		memcpy(nmap, omap, NDSLOTS(onfiles) * sizeof(*omap));
		fdp->fd_map = nmap;
	}

	/*
	 * Make sure that ntable is correctly initialized before we replace
	 * fd_files poiner. Otherwise fget_unlocked() may see inconsistent
	 * data.
	 */
	atomic_store_rel_ptr((volatile void *)&fdp->fd_files, (uintptr_t)ntable);

	/*
	 * Free the old file table when not shared by other threads or processes.
	 * The old file table is considered to be shared when either are true:
	 * - The process has more than one thread.
	 * - The file descriptor table has been shared via fdshare().
	 *
	 * When shared, the old file table will be placed on a freelist
	 * which will be processed when the struct filedesc is released.
	 *
	 * Note that if onfiles == NDFILE, we're dealing with the original
	 * static allocation contained within (struct filedesc0 *)fdp,
	 * which must not be freed.
	 */
	if (onfiles > NDFILE) {
		/*
		 * Note we may be called here from fdinit while allocating a
		 * table for a new process in which case ->p_fd points
		 * elsewhere.
		 */
		if (curproc->p_fd != fdp || FILEDESC_IS_ONLY_USER(fdp)) {
			free(otable, M_FILEDESC);
		} else {
			ft = (struct freetable *)&otable->fdt_ofiles[onfiles];
			fdp0 = (struct filedesc0 *)fdp;
			ft->ft_table = otable;
			SLIST_INSERT_HEAD(&fdp0->fd_free, ft, ft_next);
		}
	}
	/*
	 * The map does not have the same possibility of threads still
	 * holding references to it.  So always free it as long as it
	 * does not reference the original static allocation.
	 */
	if (NDSLOTS(onfiles) > NDSLOTS(NDFILE))
		free(omap, M_FILEDESC);
}

/*
 * Allocate a file descriptor for the process.
 */
int
fdalloc(struct thread *td, int minfd, int *result)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	int fd, maxfd, allocfd;
#ifdef RACCT
	int error;
#endif

	FILEDESC_XLOCK_ASSERT(fdp);

	if (fdp->fd_freefile > minfd)
		minfd = fdp->fd_freefile;

	maxfd = getmaxfd(td);

	/*
	 * Search the bitmap for a free descriptor starting at minfd.
	 * If none is found, grow the file table.
	 */
	fd = fd_first_free(fdp, minfd, fdp->fd_nfiles);
	if (__predict_false(fd >= maxfd))
		return (EMFILE);
	if (__predict_false(fd >= fdp->fd_nfiles)) {
		allocfd = min(fd * 2, maxfd);
#ifdef RACCT
		if (RACCT_ENABLED()) {
			error = racct_set_unlocked(p, RACCT_NOFILE, allocfd);
			if (error != 0)
				return (EMFILE);
		}
#endif
		/*
		 * fd is already equal to first free descriptor >= minfd, so
		 * we only need to grow the table and we are done.
		 */
		fdgrowtable_exp(fdp, allocfd);
	}

	/*
	 * Perform some sanity checks, then mark the file descriptor as
	 * used and return it to the caller.
	 */
	KASSERT(fd >= 0 && fd < min(maxfd, fdp->fd_nfiles),
	    ("invalid descriptor %d", fd));
	KASSERT(!fdisused(fdp, fd),
	    ("fd_first_free() returned non-free descriptor"));
	KASSERT(fdp->fd_ofiles[fd].fde_file == NULL,
	    ("file descriptor isn't free"));
	fdused(fdp, fd);
	*result = fd;
	return (0);
}

/*
 * Allocate n file descriptors for the process.
 */
int
fdallocn(struct thread *td, int minfd, int *fds, int n)
{
	struct proc *p = td->td_proc;
	struct filedesc *fdp = p->p_fd;
	int i;

	FILEDESC_XLOCK_ASSERT(fdp);

	for (i = 0; i < n; i++)
		if (fdalloc(td, 0, &fds[i]) != 0)
			break;

	if (i < n) {
		for (i--; i >= 0; i--)
			fdunused(fdp, fds[i]);
		return (EMFILE);
	}

	return (0);
}

/*
 * Create a new open file structure and allocate a file descriptor for the
 * process that refers to it.  We add one reference to the file for the
 * descriptor table and one reference for resultfp. This is to prevent us
 * being preempted and the entry in the descriptor table closed after we
 * release the FILEDESC lock.
 */
int
falloc_caps(struct thread *td, struct file **resultfp, int *resultfd, int flags,
    struct filecaps *fcaps)
{
	struct file *fp;
	int error, fd;

	MPASS(resultfp != NULL);
	MPASS(resultfd != NULL);

	error = _falloc_noinstall(td, &fp, 2);
	if (__predict_false(error != 0)) {
		return (error);
	}

	error = finstall_refed(td, fp, &fd, flags, fcaps);
	if (__predict_false(error != 0)) {
		falloc_abort(td, fp);
		return (error);
	}

	*resultfp = fp;
	*resultfd = fd;

	return (0);
}

/*
 * Create a new open file structure without allocating a file descriptor.
 */
int
_falloc_noinstall(struct thread *td, struct file **resultfp, u_int n)
{
	struct file *fp;
	int maxuserfiles = maxfiles - (maxfiles / 20);
	int openfiles_new;
	static struct timeval lastfail;
	static int curfail;

	KASSERT(resultfp != NULL, ("%s: resultfp == NULL", __func__));
	MPASS(n > 0);

	openfiles_new = atomic_fetchadd_int(&openfiles, 1) + 1;
	if ((openfiles_new >= maxuserfiles &&
	    priv_check(td, PRIV_MAXFILES) != 0) ||
	    openfiles_new >= maxfiles) {
		atomic_subtract_int(&openfiles, 1);
		if (ppsratecheck(&lastfail, &curfail, 1)) {
			printf("kern.maxfiles limit exceeded by uid %i, (%s) "
			    "please see tuning(7).\n", td->td_ucred->cr_ruid, td->td_proc->p_comm);
		}
		return (ENFILE);
	}
	fp = uma_zalloc(file_zone, M_WAITOK);
	bzero(fp, sizeof(*fp));
	refcount_init(&fp->f_count, n);
	fp->f_cred = crhold(td->td_ucred);
	fp->f_ops = &badfileops;
	*resultfp = fp;
	return (0);
}

void
falloc_abort(struct thread *td, struct file *fp)
{

	/*
	 * For assertion purposes.
	 */
	refcount_init(&fp->f_count, 0);
	_fdrop(fp, td);
}

/*
 * Install a file in a file descriptor table.
 */
void
_finstall(struct filedesc *fdp, struct file *fp, int fd, int flags,
    struct filecaps *fcaps)
{
	struct filedescent *fde;

	MPASS(fp != NULL);
	if (fcaps != NULL)
		filecaps_validate(fcaps, __func__);
	FILEDESC_XLOCK_ASSERT(fdp);

	fde = &fdp->fd_ofiles[fd];
#ifdef CAPABILITIES
	seqc_write_begin(&fde->fde_seqc);
#endif
	fde->fde_file = fp;
	fde->fde_flags = (flags & O_CLOEXEC) != 0 ? UF_EXCLOSE : 0;
	if (fcaps != NULL)
		filecaps_move(fcaps, &fde->fde_caps);
	else
		filecaps_fill(&fde->fde_caps);
#ifdef CAPABILITIES
	seqc_write_end(&fde->fde_seqc);
#endif
}

int
finstall_refed(struct thread *td, struct file *fp, int *fd, int flags,
    struct filecaps *fcaps)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	int error;

	MPASS(fd != NULL);

	FILEDESC_XLOCK(fdp);
	error = fdalloc(td, 0, fd);
	if (__predict_true(error == 0)) {
		_finstall(fdp, fp, *fd, flags, fcaps);
	}
	FILEDESC_XUNLOCK(fdp);
	return (error);
}

int
finstall(struct thread *td, struct file *fp, int *fd, int flags,
    struct filecaps *fcaps)
{
	int error;

	MPASS(fd != NULL);

	if (!fhold(fp))
		return (EBADF);
	error = finstall_refed(td, fp, fd, flags, fcaps);
	if (__predict_false(error != 0)) {
		fdrop(fp, td);
	}
	return (error);
}

/*
 * Build a new filedesc structure from another.
 *
 * If fdp is not NULL, return with it shared locked.
 */
struct filedesc *
fdinit(void)
{
	struct filedesc0 *newfdp0;
	struct filedesc *newfdp;

	newfdp0 = uma_zalloc(filedesc0_zone, M_WAITOK | M_ZERO);
	newfdp = &newfdp0->fd_fd;

	/* Create the file descriptor table. */
	FILEDESC_LOCK_INIT(newfdp);
	refcount_init(&newfdp->fd_refcnt, 1);
	refcount_init(&newfdp->fd_holdcnt, 1);
	newfdp->fd_map = newfdp0->fd_dmap;
	newfdp->fd_files = (struct fdescenttbl *)&newfdp0->fd_dfiles;
	newfdp->fd_files->fdt_nfiles = NDFILE;

	return (newfdp);
}

/*
 * Build a pwddesc structure from another.
 * Copy the current, root, and jail root vnode references.
 *
 * If pdp is not NULL and keeplock is true, return with it (exclusively) locked.
 */
struct pwddesc *
pdinit(struct pwddesc *pdp, bool keeplock)
{
	struct pwddesc *newpdp;
	struct pwd *newpwd;

	newpdp = malloc(sizeof(*newpdp), M_PWDDESC, M_WAITOK | M_ZERO);

	PWDDESC_LOCK_INIT(newpdp);
	refcount_init(&newpdp->pd_refcount, 1);
	newpdp->pd_cmask = CMASK;

	if (pdp == NULL) {
		newpwd = pwd_alloc();
		smr_serialized_store(&newpdp->pd_pwd, newpwd, true);
		return (newpdp);
	}

	PWDDESC_XLOCK(pdp);
	newpwd = pwd_hold_pwddesc(pdp);
	smr_serialized_store(&newpdp->pd_pwd, newpwd, true);
	if (!keeplock)
		PWDDESC_XUNLOCK(pdp);
	return (newpdp);
}

/*
 * Hold either filedesc or pwddesc of the passed process.
 *
 * The process lock is used to synchronize against the target exiting and
 * freeing the data.
 *
 * Clearing can be ilustrated in 3 steps:
 * 1. set the pointer to NULL. Either routine can race against it, hence
 *   atomic_load_ptr.
 * 2. observe the process lock as not taken. Until then fdhold/pdhold can
 *   race to either still see the pointer or find NULL. It is still safe to
 *   grab a reference as clearing is stalled.
 * 3. after the lock is observed as not taken, any fdhold/pdhold calls are
 *   guaranteed to see NULL, making it safe to finish clearing
 */
static struct filedesc *
fdhold(struct proc *p)
{
	struct filedesc *fdp;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	fdp = atomic_load_ptr(&p->p_fd);
	if (fdp != NULL)
		refcount_acquire(&fdp->fd_holdcnt);
	return (fdp);
}

static struct pwddesc *
pdhold(struct proc *p)
{
	struct pwddesc *pdp;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	pdp = atomic_load_ptr(&p->p_pd);
	if (pdp != NULL)
		refcount_acquire(&pdp->pd_refcount);
	return (pdp);
}

static void
fddrop(struct filedesc *fdp)
{

	if (refcount_load(&fdp->fd_holdcnt) > 1) {
		if (refcount_release(&fdp->fd_holdcnt) == 0)
			return;
	}

	FILEDESC_LOCK_DESTROY(fdp);
	uma_zfree(filedesc0_zone, fdp);
}

static void
pddrop(struct pwddesc *pdp)
{
	struct pwd *pwd;

	if (refcount_release_if_not_last(&pdp->pd_refcount))
		return;

	PWDDESC_XLOCK(pdp);
	if (refcount_release(&pdp->pd_refcount) == 0) {
		PWDDESC_XUNLOCK(pdp);
		return;
	}
	pwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	pwd_set(pdp, NULL);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(pwd);

	PWDDESC_LOCK_DESTROY(pdp);
	free(pdp, M_PWDDESC);
}

/*
 * Share a filedesc structure.
 */
struct filedesc *
fdshare(struct filedesc *fdp)
{

	refcount_acquire(&fdp->fd_refcnt);
	return (fdp);
}

/*
 * Share a pwddesc structure.
 */
struct pwddesc *
pdshare(struct pwddesc *pdp)
{
	refcount_acquire(&pdp->pd_refcount);
	return (pdp);
}

/*
 * Unshare a filedesc structure, if necessary by making a copy
 */
void
fdunshare(struct thread *td)
{
	struct filedesc *tmp;
	struct proc *p = td->td_proc;

	if (refcount_load(&p->p_fd->fd_refcnt) == 1)
		return;

	tmp = fdcopy(p->p_fd);
	fdescfree(td);
	p->p_fd = tmp;
}

/*
 * Unshare a pwddesc structure.
 */
void
pdunshare(struct thread *td)
{
	struct pwddesc *pdp;
	struct proc *p;

	p = td->td_proc;
	/* Not shared. */
	if (refcount_load(&p->p_pd->pd_refcount) == 1)
		return;

	pdp = pdcopy(p->p_pd);
	pdescfree(td);
	p->p_pd = pdp;
}

/*
 * Copy a filedesc structure.  A NULL pointer in returns a NULL reference,
 * this is to ease callers, not catch errors.
 */
struct filedesc *
fdcopy(struct filedesc *fdp)
{
	struct filedesc *newfdp;
	struct filedescent *nfde, *ofde;
	int i, lastfile;

	MPASS(fdp != NULL);

	newfdp = fdinit();
	FILEDESC_SLOCK(fdp);
	for (;;) {
		lastfile = fdlastfile(fdp);
		if (lastfile < newfdp->fd_nfiles)
			break;
		FILEDESC_SUNLOCK(fdp);
		fdgrowtable(newfdp, lastfile + 1);
		FILEDESC_SLOCK(fdp);
	}
	/* copy all passable descriptors (i.e. not kqueue) */
	newfdp->fd_freefile = fdp->fd_freefile;
	FILEDESC_FOREACH_FDE(fdp, i, ofde) {
		if ((ofde->fde_file->f_ops->fo_flags & DFLAG_PASSABLE) == 0 ||
		    !fhold(ofde->fde_file)) {
			if (newfdp->fd_freefile == fdp->fd_freefile)
				newfdp->fd_freefile = i;
			continue;
		}
		nfde = &newfdp->fd_ofiles[i];
		*nfde = *ofde;
		filecaps_copy(&ofde->fde_caps, &nfde->fde_caps, true);
		fdused_init(newfdp, i);
	}
	MPASS(newfdp->fd_freefile != -1);
	FILEDESC_SUNLOCK(fdp);
	return (newfdp);
}

/*
 * Copy a pwddesc structure.
 */
struct pwddesc *
pdcopy(struct pwddesc *pdp)
{
	struct pwddesc *newpdp;

	MPASS(pdp != NULL);

	newpdp = pdinit(pdp, true);
	newpdp->pd_cmask = pdp->pd_cmask;
	PWDDESC_XUNLOCK(pdp);
	return (newpdp);
}

/*
 * Clear POSIX style locks. This is only used when fdp looses a reference (i.e.
 * one of processes using it exits) and the table used to be shared.
 */
static void
fdclearlocks(struct thread *td)
{
	struct filedesc *fdp;
	struct filedesc_to_leader *fdtol;
	struct flock lf;
	struct file *fp;
	struct proc *p;
	struct vnode *vp;
	int i;

	p = td->td_proc;
	fdp = p->p_fd;
	fdtol = p->p_fdtol;
	MPASS(fdtol != NULL);

	FILEDESC_XLOCK(fdp);
	KASSERT(fdtol->fdl_refcount > 0,
	    ("filedesc_to_refcount botch: fdl_refcount=%d",
	    fdtol->fdl_refcount));
	if (fdtol->fdl_refcount == 1 &&
	    (p->p_leader->p_flag & P_ADVLOCK) != 0) {
		FILEDESC_FOREACH_FP(fdp, i, fp) {
			if (fp->f_type != DTYPE_VNODE ||
			    !fhold(fp))
				continue;
			FILEDESC_XUNLOCK(fdp);
			lf.l_whence = SEEK_SET;
			lf.l_start = 0;
			lf.l_len = 0;
			lf.l_type = F_UNLCK;
			vp = fp->f_vnode;
			(void) VOP_ADVLOCK(vp,
			    (caddr_t)p->p_leader, F_UNLCK,
			    &lf, F_POSIX);
			FILEDESC_XLOCK(fdp);
			fdrop(fp, td);
		}
	}
retry:
	if (fdtol->fdl_refcount == 1) {
		if (fdp->fd_holdleaderscount > 0 &&
		    (p->p_leader->p_flag & P_ADVLOCK) != 0) {
			/*
			 * close() or kern_dup() has cleared a reference
			 * in a shared file descriptor table.
			 */
			fdp->fd_holdleaderswakeup = 1;
			sx_sleep(&fdp->fd_holdleaderscount,
			    FILEDESC_LOCK(fdp), PLOCK, "fdlhold", 0);
			goto retry;
		}
		if (fdtol->fdl_holdcount > 0) {
			/*
			 * Ensure that fdtol->fdl_leader remains
			 * valid in closef().
			 */
			fdtol->fdl_wakeup = 1;
			sx_sleep(fdtol, FILEDESC_LOCK(fdp), PLOCK,
			    "fdlhold", 0);
			goto retry;
		}
	}
	fdtol->fdl_refcount--;
	if (fdtol->fdl_refcount == 0 &&
	    fdtol->fdl_holdcount == 0) {
		fdtol->fdl_next->fdl_prev = fdtol->fdl_prev;
		fdtol->fdl_prev->fdl_next = fdtol->fdl_next;
	} else
		fdtol = NULL;
	p->p_fdtol = NULL;
	FILEDESC_XUNLOCK(fdp);
	if (fdtol != NULL)
		free(fdtol, M_FILEDESC_TO_LEADER);
}

/*
 * Release a filedesc structure.
 */
static void
fdescfree_fds(struct thread *td, struct filedesc *fdp)
{
	struct filedesc0 *fdp0;
	struct freetable *ft, *tft;
	struct filedescent *fde;
	struct file *fp;
	int i;

	KASSERT(refcount_load(&fdp->fd_refcnt) == 0,
	    ("%s: fd table %p carries references", __func__, fdp));

	/*
	 * Serialize with threads iterating over the table, if any.
	 */
	if (refcount_load(&fdp->fd_holdcnt) > 1) {
		FILEDESC_XLOCK(fdp);
		FILEDESC_XUNLOCK(fdp);
	}

	FILEDESC_FOREACH_FDE(fdp, i, fde) {
		fp = fde->fde_file;
		fdefree_last(fde);
		(void) closef(fp, td);
	}

	if (NDSLOTS(fdp->fd_nfiles) > NDSLOTS(NDFILE))
		free(fdp->fd_map, M_FILEDESC);
	if (fdp->fd_nfiles > NDFILE)
		free(fdp->fd_files, M_FILEDESC);

	fdp0 = (struct filedesc0 *)fdp;
	SLIST_FOREACH_SAFE(ft, &fdp0->fd_free, ft_next, tft)
		free(ft->ft_table, M_FILEDESC);

	fddrop(fdp);
}

void
fdescfree(struct thread *td)
{
	struct proc *p;
	struct filedesc *fdp;

	p = td->td_proc;
	fdp = p->p_fd;
	MPASS(fdp != NULL);

#ifdef RACCT
	if (RACCT_ENABLED())
		racct_set_unlocked(p, RACCT_NOFILE, 0);
#endif

	if (p->p_fdtol != NULL)
		fdclearlocks(td);

	/*
	 * Check fdhold for an explanation.
	 */
	atomic_store_ptr(&p->p_fd, NULL);
	atomic_thread_fence_seq_cst();
	PROC_WAIT_UNLOCKED(p);

	if (refcount_release(&fdp->fd_refcnt) == 0)
		return;

	fdescfree_fds(td, fdp);
}

void
pdescfree(struct thread *td)
{
	struct proc *p;
	struct pwddesc *pdp;

	p = td->td_proc;
	pdp = p->p_pd;
	MPASS(pdp != NULL);

	/*
	 * Check pdhold for an explanation.
	 */
	atomic_store_ptr(&p->p_pd, NULL);
	atomic_thread_fence_seq_cst();
	PROC_WAIT_UNLOCKED(p);

	pddrop(pdp);
}

/*
 * For setugid programs, we don't want to people to use that setugidness
 * to generate error messages which write to a file which otherwise would
 * otherwise be off-limits to the process.  We check for filesystems where
 * the vnode can change out from under us after execve (like [lin]procfs).
 *
 * Since fdsetugidsafety calls this only for fd 0, 1 and 2, this check is
 * sufficient.  We also don't check for setugidness since we know we are.
 */
static bool
is_unsafe(struct file *fp)
{
	struct vnode *vp;

	if (fp->f_type != DTYPE_VNODE)
		return (false);

	vp = fp->f_vnode;
	return ((vp->v_vflag & VV_PROCDEP) != 0);
}

/*
 * Make this setguid thing safe, if at all possible.
 */
void
fdsetugidsafety(struct thread *td)
{
	struct filedesc *fdp;
	struct file *fp;
	int i;

	fdp = td->td_proc->p_fd;
	KASSERT(refcount_load(&fdp->fd_refcnt) == 1,
	    ("the fdtable should not be shared"));
	MPASS(fdp->fd_nfiles >= 3);
	for (i = 0; i <= 2; i++) {
		fp = fdp->fd_ofiles[i].fde_file;
		if (fp != NULL && is_unsafe(fp)) {
			FILEDESC_XLOCK(fdp);
			knote_fdclose(td, i);
			/*
			 * NULL-out descriptor prior to close to avoid
			 * a race while close blocks.
			 */
			fdfree(fdp, i);
			FILEDESC_XUNLOCK(fdp);
			(void) closef(fp, td);
		}
	}
}

/*
 * If a specific file object occupies a specific file descriptor, close the
 * file descriptor entry and drop a reference on the file object.  This is a
 * convenience function to handle a subsequent error in a function that calls
 * falloc() that handles the race that another thread might have closed the
 * file descriptor out from under the thread creating the file object.
 */
void
fdclose(struct thread *td, struct file *fp, int idx)
{
	struct filedesc *fdp = td->td_proc->p_fd;

	FILEDESC_XLOCK(fdp);
	if (fdp->fd_ofiles[idx].fde_file == fp) {
		fdfree(fdp, idx);
		FILEDESC_XUNLOCK(fdp);
		fdrop(fp, td);
	} else
		FILEDESC_XUNLOCK(fdp);
}

/*
 * Close any files on exec?
 */
void
fdcloseexec(struct thread *td)
{
	struct filedesc *fdp;
	struct filedescent *fde;
	struct file *fp;
	int i;

	fdp = td->td_proc->p_fd;
	KASSERT(refcount_load(&fdp->fd_refcnt) == 1,
	    ("the fdtable should not be shared"));
	FILEDESC_FOREACH_FDE(fdp, i, fde) {
		fp = fde->fde_file;
		if (fp->f_type == DTYPE_MQUEUE ||
		    (fde->fde_flags & UF_EXCLOSE)) {
			FILEDESC_XLOCK(fdp);
			fdfree(fdp, i);
			(void) closefp(fdp, i, fp, td, false, false);
			FILEDESC_UNLOCK_ASSERT(fdp);
		}
	}
}

/*
 * It is unsafe for set[ug]id processes to be started with file
 * descriptors 0..2 closed, as these descriptors are given implicit
 * significance in the Standard C library.  fdcheckstd() will create a
 * descriptor referencing /dev/null for each of stdin, stdout, and
 * stderr that is not already open.
 */
int
fdcheckstd(struct thread *td)
{
	struct filedesc *fdp;
	register_t save;
	int i, error, devnull;

	fdp = td->td_proc->p_fd;
	KASSERT(refcount_load(&fdp->fd_refcnt) == 1,
	    ("the fdtable should not be shared"));
	MPASS(fdp->fd_nfiles >= 3);
	devnull = -1;
	for (i = 0; i <= 2; i++) {
		if (fdp->fd_ofiles[i].fde_file != NULL)
			continue;

		save = td->td_retval[0];
		if (devnull != -1) {
			error = kern_dup(td, FDDUP_FIXED, 0, devnull, i);
		} else {
			error = kern_openat(td, AT_FDCWD, "/dev/null",
			    UIO_SYSSPACE, O_RDWR, 0);
			if (error == 0) {
				devnull = td->td_retval[0];
				KASSERT(devnull == i, ("we didn't get our fd"));
			}
		}
		td->td_retval[0] = save;
		if (error != 0)
			return (error);
	}
	return (0);
}

/*
 * Internal form of close.  Decrement reference count on file structure.
 * Note: td may be NULL when closing a file that was being passed in a
 * message.
 */
int
closef(struct file *fp, struct thread *td)
{
	struct vnode *vp;
	struct flock lf;
	struct filedesc_to_leader *fdtol;
	struct filedesc *fdp;

	MPASS(td != NULL);

	/*
	 * POSIX record locking dictates that any close releases ALL
	 * locks owned by this process.  This is handled by setting
	 * a flag in the unlock to free ONLY locks obeying POSIX
	 * semantics, and not to free BSD-style file locks.
	 * If the descriptor was in a message, POSIX-style locks
	 * aren't passed with the descriptor, and the thread pointer
	 * will be NULL.  Callers should be careful only to pass a
	 * NULL thread pointer when there really is no owning
	 * context that might have locks, or the locks will be
	 * leaked.
	 */
	if (fp->f_type == DTYPE_VNODE) {
		vp = fp->f_vnode;
		if ((td->td_proc->p_leader->p_flag & P_ADVLOCK) != 0) {
			lf.l_whence = SEEK_SET;
			lf.l_start = 0;
			lf.l_len = 0;
			lf.l_type = F_UNLCK;
			(void) VOP_ADVLOCK(vp, (caddr_t)td->td_proc->p_leader,
			    F_UNLCK, &lf, F_POSIX);
		}
		fdtol = td->td_proc->p_fdtol;
		if (fdtol != NULL) {
			/*
			 * Handle special case where file descriptor table is
			 * shared between multiple process leaders.
			 */
			fdp = td->td_proc->p_fd;
			FILEDESC_XLOCK(fdp);
			for (fdtol = fdtol->fdl_next;
			    fdtol != td->td_proc->p_fdtol;
			    fdtol = fdtol->fdl_next) {
				if ((fdtol->fdl_leader->p_flag &
				    P_ADVLOCK) == 0)
					continue;
				fdtol->fdl_holdcount++;
				FILEDESC_XUNLOCK(fdp);
				lf.l_whence = SEEK_SET;
				lf.l_start = 0;
				lf.l_len = 0;
				lf.l_type = F_UNLCK;
				vp = fp->f_vnode;
				(void) VOP_ADVLOCK(vp,
				    (caddr_t)fdtol->fdl_leader, F_UNLCK, &lf,
				    F_POSIX);
				FILEDESC_XLOCK(fdp);
				fdtol->fdl_holdcount--;
				if (fdtol->fdl_holdcount == 0 &&
				    fdtol->fdl_wakeup != 0) {
					fdtol->fdl_wakeup = 0;
					wakeup(fdtol);
				}
			}
			FILEDESC_XUNLOCK(fdp);
		}
	}
	return (fdrop_close(fp, td));
}

/*
 * Hack for file descriptor passing code.
 */
void
closef_nothread(struct file *fp)
{

	fdrop(fp, NULL);
}

/*
 * Initialize the file pointer with the specified properties.
 *
 * The ops are set with release semantics to be certain that the flags, type,
 * and data are visible when ops is.  This is to prevent ops methods from being
 * called with bad data.
 */
void
finit(struct file *fp, u_int flag, short type, void *data, struct fileops *ops)
{
	fp->f_data = data;
	fp->f_flag = flag;
	fp->f_type = type;
	atomic_store_rel_ptr((volatile uintptr_t *)&fp->f_ops, (uintptr_t)ops);
}

void
finit_vnode(struct file *fp, u_int flag, void *data, struct fileops *ops)
{
	fp->f_seqcount[UIO_READ] = 1;
	fp->f_seqcount[UIO_WRITE] = 1;
	finit(fp, (flag & FMASK) | (fp->f_flag & FHASLOCK), DTYPE_VNODE,
	    data, ops);
}

int
fget_cap_noref(struct filedesc *fdp, int fd, cap_rights_t *needrightsp,
    struct file **fpp, struct filecaps *havecapsp)
{
	struct filedescent *fde;
	int error;

	FILEDESC_LOCK_ASSERT(fdp);

	*fpp = NULL;
	fde = fdeget_noref(fdp, fd);
	if (fde == NULL) {
		error = EBADF;
		goto out;
	}

#ifdef CAPABILITIES
	error = cap_check(cap_rights_fde_inline(fde), needrightsp);
	if (error != 0)
		goto out;
#endif

	if (havecapsp != NULL)
		filecaps_copy(&fde->fde_caps, havecapsp, true);

	*fpp = fde->fde_file;

	error = 0;
out:
	return (error);
}

#ifdef CAPABILITIES
int
fget_cap(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct file **fpp, struct filecaps *havecapsp)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	int error;
	struct file *fp;
	seqc_t seq;

	*fpp = NULL;
	for (;;) {
		error = fget_unlocked_seq(td, fd, needrightsp, &fp, &seq);
		if (error != 0)
			return (error);

		if (havecapsp != NULL) {
			if (!filecaps_copy(&fdp->fd_ofiles[fd].fde_caps,
			    havecapsp, false)) {
				fdrop(fp, td);
				goto get_locked;
			}
		}

		if (!fd_modified(fdp, fd, seq))
			break;
		fdrop(fp, td);
	}

	*fpp = fp;
	return (0);

get_locked:
	FILEDESC_SLOCK(fdp);
	error = fget_cap_noref(fdp, fd, needrightsp, fpp, havecapsp);
	if (error == 0 && !fhold(*fpp))
		error = EBADF;
	FILEDESC_SUNLOCK(fdp);
	return (error);
}
#else
int
fget_cap(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct file **fpp, struct filecaps *havecapsp)
{
	int error;
	error = fget_unlocked(td, fd, needrightsp, fpp);
	if (havecapsp != NULL && error == 0)
		filecaps_fill(havecapsp);

	return (error);
}
#endif

int
fget_remote(struct thread *td, struct proc *p, int fd, struct file **fpp)
{
	struct filedesc *fdp;
	struct file *fp;
	int error;

	if (p == td->td_proc)	/* curproc */
		return (fget_unlocked(td, fd, &cap_no_rights, fpp));

	PROC_LOCK(p);
	fdp = fdhold(p);
	PROC_UNLOCK(p);
	if (fdp == NULL)
		return (ENOENT);
	FILEDESC_SLOCK(fdp);
	if (refcount_load(&fdp->fd_refcnt) != 0) {
		fp = fget_noref(fdp, fd);
		if (fp != NULL && fhold(fp)) {
			*fpp = fp;
			error = 0;
		} else {
			error = EBADF;
		}
	} else {
		error = ENOENT;
	}
	FILEDESC_SUNLOCK(fdp);
	fddrop(fdp);
	return (error);
}

#ifdef CAPABILITIES
int
fgetvp_lookup_smr(struct nameidata *ndp, struct vnode **vpp, bool *fsearch)
{
	const struct filedescent *fde;
	const struct fdescenttbl *fdt;
	struct filedesc *fdp;
	struct file *fp;
	struct vnode *vp;
	const cap_rights_t *haverights;
	cap_rights_t rights;
	seqc_t seq;
	int fd;

	VFS_SMR_ASSERT_ENTERED();

	fd = ndp->ni_dirfd;
	rights = *ndp->ni_rightsneeded;
	cap_rights_set_one(&rights, CAP_LOOKUP);

	fdp = curproc->p_fd;
	fdt = fdp->fd_files;
	if (__predict_false((u_int)fd >= fdt->fdt_nfiles))
		return (EBADF);
	seq = seqc_read_notmodify(fd_seqc(fdt, fd));
	fde = &fdt->fdt_ofiles[fd];
	haverights = cap_rights_fde_inline(fde);
	fp = fde->fde_file;
	if (__predict_false(fp == NULL))
		return (EAGAIN);
	if (__predict_false(cap_check_inline_transient(haverights, &rights)))
		return (EAGAIN);
	*fsearch = ((fp->f_flag & FSEARCH) != 0);
	vp = fp->f_vnode;
	if (__predict_false(vp == NULL)) {
		return (EAGAIN);
	}
	if (!filecaps_copy(&fde->fde_caps, &ndp->ni_filecaps, false)) {
		return (EAGAIN);
	}
	/*
	 * Use an acquire barrier to force re-reading of fdt so it is
	 * refreshed for verification.
	 */
	atomic_thread_fence_acq();
	fdt = fdp->fd_files;
	if (__predict_false(!seqc_consistent_no_fence(fd_seqc(fdt, fd), seq)))
		return (EAGAIN);
	/*
	 * If file descriptor doesn't have all rights,
	 * all lookups relative to it must also be
	 * strictly relative.
	 *
	 * Not yet supported by fast path.
	 */
	CAP_ALL(&rights);
	if (!cap_rights_contains(&ndp->ni_filecaps.fc_rights, &rights) ||
	    ndp->ni_filecaps.fc_fcntls != CAP_FCNTL_ALL ||
	    ndp->ni_filecaps.fc_nioctls != -1) {
#ifdef notyet
		ndp->ni_lcf |= NI_LCF_STRICTRELATIVE;
#else
		return (EAGAIN);
#endif
	}
	*vpp = vp;
	return (0);
}
#else
int
fgetvp_lookup_smr(struct nameidata *ndp, struct vnode **vpp, bool *fsearch)
{
	const struct fdescenttbl *fdt;
	struct filedesc *fdp;
	struct file *fp;
	struct vnode *vp;
	int fd;

	VFS_SMR_ASSERT_ENTERED();

	fd = ndp->ni_dirfd;
	fdp = curproc->p_fd;
	fdt = fdp->fd_files;
	if (__predict_false((u_int)fd >= fdt->fdt_nfiles))
		return (EBADF);
	fp = fdt->fdt_ofiles[fd].fde_file;
	if (__predict_false(fp == NULL))
		return (EAGAIN);
	*fsearch = ((fp->f_flag & FSEARCH) != 0);
	vp = fp->f_vnode;
	if (__predict_false(vp == NULL || vp->v_type != VDIR)) {
		return (EAGAIN);
	}
	/*
	 * Use an acquire barrier to force re-reading of fdt so it is
	 * refreshed for verification.
	 */
	atomic_thread_fence_acq();
	fdt = fdp->fd_files;
	if (__predict_false(fp != fdt->fdt_ofiles[fd].fde_file))
		return (EAGAIN);
	filecaps_fill(&ndp->ni_filecaps);
	*vpp = vp;
	return (0);
}
#endif

int
fgetvp_lookup(struct nameidata *ndp, struct vnode **vpp)
{
	struct thread *td;
	struct file *fp;
	struct vnode *vp;
	struct componentname *cnp;
	cap_rights_t rights;
	int error;

	td = curthread;
	rights = *ndp->ni_rightsneeded;
	cap_rights_set_one(&rights, CAP_LOOKUP);
	cnp = &ndp->ni_cnd;

	error = fget_cap(td, ndp->ni_dirfd, &rights, &fp, &ndp->ni_filecaps);
	if (__predict_false(error != 0))
		return (error);
	if (__predict_false(fp->f_ops == &badfileops)) {
		error = EBADF;
		goto out_free;
	}
	vp = fp->f_vnode;
	if (__predict_false(vp == NULL)) {
		error = ENOTDIR;
		goto out_free;
	}
	vrefact(vp);
	/*
	 * XXX does not check for VDIR, handled by namei_setup
	 */
	if ((fp->f_flag & FSEARCH) != 0)
		cnp->cn_flags |= NOEXECCHECK;
	fdrop(fp, td);

#ifdef CAPABILITIES
	/*
	 * If file descriptor doesn't have all rights,
	 * all lookups relative to it must also be
	 * strictly relative.
	 */
	CAP_ALL(&rights);
	if (!cap_rights_contains(&ndp->ni_filecaps.fc_rights, &rights) ||
	    ndp->ni_filecaps.fc_fcntls != CAP_FCNTL_ALL ||
	    ndp->ni_filecaps.fc_nioctls != -1) {
		ndp->ni_lcf |= NI_LCF_STRICTRELATIVE;
		ndp->ni_resflags |= NIRES_STRICTREL;
	}
#endif

	/*
	 * TODO: avoid copying ioctl caps if it can be helped to begin with
	 */
	if ((cnp->cn_flags & WANTIOCTLCAPS) == 0)
		filecaps_free_ioctl(&ndp->ni_filecaps);

	*vpp = vp;
	return (0);

out_free:
	filecaps_free(&ndp->ni_filecaps);
	fdrop(fp, td);
	return (error);
}

/*
 * Fetch the descriptor locklessly.
 *
 * We avoid fdrop() races by never raising a refcount above 0.  To accomplish
 * this we have to use a cmpset loop rather than an atomic_add.  The descriptor
 * must be re-verified once we acquire a reference to be certain that the
 * identity is still correct and we did not lose a race due to preemption.
 *
 * Force a reload of fdt when looping. Another thread could reallocate
 * the table before this fd was closed, so it is possible that there is
 * a stale fp pointer in cached version.
 */
#ifdef CAPABILITIES
static int
fget_unlocked_seq(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct file **fpp, seqc_t *seqp)
{
	struct filedesc *fdp;
	const struct filedescent *fde;
	const struct fdescenttbl *fdt;
	struct file *fp;
	seqc_t seq;
	cap_rights_t haverights;
	int error;

	fdp = td->td_proc->p_fd;
	fdt = fdp->fd_files;
	if (__predict_false((u_int)fd >= fdt->fdt_nfiles))
		return (EBADF);

	for (;;) {
		seq = seqc_read_notmodify(fd_seqc(fdt, fd));
		fde = &fdt->fdt_ofiles[fd];
		haverights = *cap_rights_fde_inline(fde);
		fp = fde->fde_file;
		if (__predict_false(fp == NULL)) {
			if (seqc_consistent(fd_seqc(fdt, fd), seq))
				return (EBADF);
			fdt = atomic_load_ptr(&fdp->fd_files);
			continue;
		}
		error = cap_check_inline(&haverights, needrightsp);
		if (__predict_false(error != 0)) {
			if (seqc_consistent(fd_seqc(fdt, fd), seq))
				return (error);
			fdt = atomic_load_ptr(&fdp->fd_files);
			continue;
		}
		if (__predict_false(!refcount_acquire_if_not_zero(&fp->f_count))) {
			fdt = atomic_load_ptr(&fdp->fd_files);
			continue;
		}
		/*
		 * Use an acquire barrier to force re-reading of fdt so it is
		 * refreshed for verification.
		 */
		atomic_thread_fence_acq();
		fdt = fdp->fd_files;
		if (seqc_consistent_no_fence(fd_seqc(fdt, fd), seq))
			break;
		fdrop(fp, td);
	}
	*fpp = fp;
	if (seqp != NULL) {
		*seqp = seq;
	}
	return (0);
}
#else
static int
fget_unlocked_seq(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct file **fpp, seqc_t *seqp __unused)
{
	struct filedesc *fdp;
	const struct fdescenttbl *fdt;
	struct file *fp;

	fdp = td->td_proc->p_fd;
	fdt = fdp->fd_files;
	if (__predict_false((u_int)fd >= fdt->fdt_nfiles))
		return (EBADF);

	for (;;) {
		fp = fdt->fdt_ofiles[fd].fde_file;
		if (__predict_false(fp == NULL))
			return (EBADF);
		if (__predict_false(!refcount_acquire_if_not_zero(&fp->f_count))) {
			fdt = atomic_load_ptr(&fdp->fd_files);
			continue;
		}
		/*
		 * Use an acquire barrier to force re-reading of fdt so it is
		 * refreshed for verification.
		 */
		atomic_thread_fence_acq();
		fdt = fdp->fd_files;
		if (__predict_true(fp == fdt->fdt_ofiles[fd].fde_file))
			break;
		fdrop(fp, td);
	}
	*fpp = fp;
	return (0);
}
#endif

/*
 * See the comments in fget_unlocked_seq for an explanation of how this works.
 *
 * This is a simplified variant which bails out to the aforementioned routine
 * if anything goes wrong. In practice this only happens when userspace is
 * racing with itself.
 */
int
fget_unlocked(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct file **fpp)
{
	struct filedesc *fdp;
#ifdef CAPABILITIES
	const struct filedescent *fde;
#endif
	const struct fdescenttbl *fdt;
	struct file *fp;
#ifdef CAPABILITIES
	seqc_t seq;
	const cap_rights_t *haverights;
#endif

	fdp = td->td_proc->p_fd;
	fdt = fdp->fd_files;
	if (__predict_false((u_int)fd >= fdt->fdt_nfiles)) {
		*fpp = NULL;
		return (EBADF);
	}
#ifdef CAPABILITIES
	seq = seqc_read_notmodify(fd_seqc(fdt, fd));
	fde = &fdt->fdt_ofiles[fd];
	haverights = cap_rights_fde_inline(fde);
	fp = fde->fde_file;
#else
	fp = fdt->fdt_ofiles[fd].fde_file;
#endif
	if (__predict_false(fp == NULL))
		goto out_fallback;
#ifdef CAPABILITIES
	if (__predict_false(cap_check_inline_transient(haverights, needrightsp)))
		goto out_fallback;
#endif
	if (__predict_false(!refcount_acquire_if_not_zero(&fp->f_count)))
		goto out_fallback;

	/*
	 * Use an acquire barrier to force re-reading of fdt so it is
	 * refreshed for verification.
	 */
	atomic_thread_fence_acq();
	fdt = fdp->fd_files;
#ifdef	CAPABILITIES
	if (__predict_false(!seqc_consistent_no_fence(fd_seqc(fdt, fd), seq)))
#else
	if (__predict_false(fp != fdt->fdt_ofiles[fd].fde_file))
#endif
		goto out_fdrop;
	*fpp = fp;
	return (0);
out_fdrop:
	fdrop(fp, td);
out_fallback:
	*fpp = NULL;
	return (fget_unlocked_seq(td, fd, needrightsp, fpp, NULL));
}

/*
 * Translate fd -> file when the caller guarantees the file descriptor table
 * can't be changed by others.
 *
 * Note this does not mean the file object itself is only visible to the caller,
 * merely that it wont disappear without having to be referenced.
 *
 * Must be paired with fput_only_user.
 */
#ifdef	CAPABILITIES
int
fget_only_user(struct filedesc *fdp, int fd, cap_rights_t *needrightsp,
    struct file **fpp)
{
	const struct filedescent *fde;
	const struct fdescenttbl *fdt;
	const cap_rights_t *haverights;
	struct file *fp;
	int error;

	MPASS(FILEDESC_IS_ONLY_USER(fdp));

	*fpp = NULL;
	if (__predict_false(fd >= fdp->fd_nfiles))
		return (EBADF);

	fdt = fdp->fd_files;
	fde = &fdt->fdt_ofiles[fd];
	fp = fde->fde_file;
	if (__predict_false(fp == NULL))
		return (EBADF);
	MPASS(refcount_load(&fp->f_count) > 0);
	haverights = cap_rights_fde_inline(fde);
	error = cap_check_inline(haverights, needrightsp);
	if (__predict_false(error != 0))
		return (error);
	*fpp = fp;
	return (0);
}
#else
int
fget_only_user(struct filedesc *fdp, int fd, cap_rights_t *needrightsp,
    struct file **fpp)
{
	struct file *fp;

	MPASS(FILEDESC_IS_ONLY_USER(fdp));

	*fpp = NULL;
	if (__predict_false(fd >= fdp->fd_nfiles))
		return (EBADF);

	fp = fdp->fd_ofiles[fd].fde_file;
	if (__predict_false(fp == NULL))
		return (EBADF);

	MPASS(refcount_load(&fp->f_count) > 0);
	*fpp = fp;
	return (0);
}
#endif

/*
 * Extract the file pointer associated with the specified descriptor for the
 * current user process.
 *
 * If the descriptor doesn't exist or doesn't match 'flags', EBADF is
 * returned.
 *
 * File's rights will be checked against the capability rights mask.
 *
 * If an error occurred the non-zero error is returned and *fpp is set to
 * NULL.  Otherwise *fpp is held and set and zero is returned.  Caller is
 * responsible for fdrop().
 */
static __inline int
_fget(struct thread *td, int fd, struct file **fpp, int flags,
    cap_rights_t *needrightsp)
{
	struct file *fp;
	int error;

	*fpp = NULL;
	error = fget_unlocked(td, fd, needrightsp, &fp);
	if (__predict_false(error != 0))
		return (error);
	if (__predict_false(fp->f_ops == &badfileops)) {
		fdrop(fp, td);
		return (EBADF);
	}

	/*
	 * FREAD and FWRITE failure return EBADF as per POSIX.
	 */
	error = 0;
	switch (flags) {
	case FREAD:
	case FWRITE:
		if ((fp->f_flag & flags) == 0)
			error = EBADF;
		break;
	case FEXEC:
		if (fp->f_ops != &path_fileops &&
		    ((fp->f_flag & (FREAD | FEXEC)) == 0 ||
		    (fp->f_flag & FWRITE) != 0))
			error = EBADF;
		break;
	case 0:
		break;
	default:
		KASSERT(0, ("wrong flags"));
	}

	if (error != 0) {
		fdrop(fp, td);
		return (error);
	}

	*fpp = fp;
	return (0);
}

int
fget(struct thread *td, int fd, cap_rights_t *rightsp, struct file **fpp)
{

	return (_fget(td, fd, fpp, 0, rightsp));
}

int
fget_mmap(struct thread *td, int fd, cap_rights_t *rightsp, vm_prot_t *maxprotp,
    struct file **fpp)
{
	int error;
#ifndef CAPABILITIES
	error = _fget(td, fd, fpp, 0, rightsp);
	if (maxprotp != NULL)
		*maxprotp = VM_PROT_ALL;
	return (error);
#else
	cap_rights_t fdrights;
	struct filedesc *fdp;
	struct file *fp;
	seqc_t seq;

	*fpp = NULL;
	fdp = td->td_proc->p_fd;
	MPASS(cap_rights_is_set(rightsp, CAP_MMAP));
	for (;;) {
		error = fget_unlocked_seq(td, fd, rightsp, &fp, &seq);
		if (__predict_false(error != 0))
			return (error);
		if (__predict_false(fp->f_ops == &badfileops)) {
			fdrop(fp, td);
			return (EBADF);
		}
		if (maxprotp != NULL)
			fdrights = *cap_rights(fdp, fd);
		if (!fd_modified(fdp, fd, seq))
			break;
		fdrop(fp, td);
	}

	/*
	 * If requested, convert capability rights to access flags.
	 */
	if (maxprotp != NULL)
		*maxprotp = cap_rights_to_vmprot(&fdrights);
	*fpp = fp;
	return (0);
#endif
}

int
fget_read(struct thread *td, int fd, cap_rights_t *rightsp, struct file **fpp)
{

	return (_fget(td, fd, fpp, FREAD, rightsp));
}

int
fget_write(struct thread *td, int fd, cap_rights_t *rightsp, struct file **fpp)
{

	return (_fget(td, fd, fpp, FWRITE, rightsp));
}

int
fget_fcntl(struct thread *td, int fd, cap_rights_t *rightsp, int needfcntl,
    struct file **fpp)
{
#ifndef CAPABILITIES
	return (fget_unlocked(td, fd, rightsp, fpp));
#else
	struct filedesc *fdp = td->td_proc->p_fd;
	struct file *fp;
	int error;
	seqc_t seq;

	*fpp = NULL;
	MPASS(cap_rights_is_set(rightsp, CAP_FCNTL));
	for (;;) {
		error = fget_unlocked_seq(td, fd, rightsp, &fp, &seq);
		if (error != 0)
			return (error);
		error = cap_fcntl_check(fdp, fd, needfcntl);
		if (!fd_modified(fdp, fd, seq))
			break;
		fdrop(fp, td);
	}
	if (error != 0) {
		fdrop(fp, td);
		return (error);
	}
	*fpp = fp;
	return (0);
#endif
}

/*
 * Like fget() but loads the underlying vnode, or returns an error if the
 * descriptor does not represent a vnode.  Note that pipes use vnodes but
 * never have VM objects.  The returned vnode will be vref()'d.
 *
 * XXX: what about the unused flags ?
 */
static __inline int
_fgetvp(struct thread *td, int fd, int flags, cap_rights_t *needrightsp,
    struct vnode **vpp)
{
	struct file *fp;
	int error;

	*vpp = NULL;
	error = _fget(td, fd, &fp, flags, needrightsp);
	if (error != 0)
		return (error);
	if (fp->f_vnode == NULL) {
		error = EINVAL;
	} else {
		*vpp = fp->f_vnode;
		vrefact(*vpp);
	}
	fdrop(fp, td);

	return (error);
}

int
fgetvp(struct thread *td, int fd, cap_rights_t *rightsp, struct vnode **vpp)
{

	return (_fgetvp(td, fd, 0, rightsp, vpp));
}

int
fgetvp_rights(struct thread *td, int fd, cap_rights_t *needrightsp,
    struct filecaps *havecaps, struct vnode **vpp)
{
	struct filecaps caps;
	struct file *fp;
	int error;

	error = fget_cap(td, fd, needrightsp, &fp, &caps);
	if (error != 0)
		return (error);
	if (fp->f_ops == &badfileops) {
		error = EBADF;
		goto out;
	}
	if (fp->f_vnode == NULL) {
		error = EINVAL;
		goto out;
	}

	*havecaps = caps;
	*vpp = fp->f_vnode;
	vrefact(*vpp);
	fdrop(fp, td);

	return (0);
out:
	filecaps_free(&caps);
	fdrop(fp, td);
	return (error);
}

int
fgetvp_read(struct thread *td, int fd, cap_rights_t *rightsp, struct vnode **vpp)
{

	return (_fgetvp(td, fd, FREAD, rightsp, vpp));
}

int
fgetvp_exec(struct thread *td, int fd, cap_rights_t *rightsp, struct vnode **vpp)
{

	return (_fgetvp(td, fd, FEXEC, rightsp, vpp));
}

#ifdef notyet
int
fgetvp_write(struct thread *td, int fd, cap_rights_t *rightsp,
    struct vnode **vpp)
{

	return (_fgetvp(td, fd, FWRITE, rightsp, vpp));
}
#endif

/*
 * Handle the last reference to a file being closed.
 *
 * Without the noinline attribute clang keeps inlining the func thorough this
 * file when fdrop is used.
 */
int __noinline
_fdrop(struct file *fp, struct thread *td)
{
	int error;
#ifdef INVARIANTS
	int count;

	count = refcount_load(&fp->f_count);
	if (count != 0)
		panic("fdrop: fp %p count %d", fp, count);
#endif
	error = fo_close(fp, td);
	atomic_subtract_int(&openfiles, 1);
	crfree(fp->f_cred);
	free(fp->f_advice, M_FADVISE);
	uma_zfree(file_zone, fp);

	return (error);
}

/*
 * Apply an advisory lock on a file descriptor.
 *
 * Just attempt to get a record lock of the requested type on the entire file
 * (l_whence = SEEK_SET, l_start = 0, l_len = 0).
 */
#ifndef _SYS_SYSPROTO_H_
struct flock_args {
	int	fd;
	int	how;
};
#endif
/* ARGSUSED */
int
sys_flock(struct thread *td, struct flock_args *uap)
{
	struct file *fp;
	struct vnode *vp;
	struct flock lf;
	int error;

	error = fget(td, uap->fd, &cap_flock_rights, &fp);
	if (error != 0)
		return (error);
	error = EOPNOTSUPP;
	if (fp->f_type != DTYPE_VNODE && fp->f_type != DTYPE_FIFO) {
		goto done;
	}
	if (fp->f_ops == &path_fileops) {
		goto done;
	}

	error = 0;
	vp = fp->f_vnode;
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	if (uap->how & LOCK_UN) {
		lf.l_type = F_UNLCK;
		atomic_clear_int(&fp->f_flag, FHASLOCK);
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_UNLCK, &lf, F_FLOCK);
		goto done;
	}
	if (uap->how & LOCK_EX)
		lf.l_type = F_WRLCK;
	else if (uap->how & LOCK_SH)
		lf.l_type = F_RDLCK;
	else {
		error = EBADF;
		goto done;
	}
	atomic_set_int(&fp->f_flag, FHASLOCK);
	error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf,
	    (uap->how & LOCK_NB) ? F_FLOCK : F_FLOCK | F_WAIT);
done:
	fdrop(fp, td);
	return (error);
}
/*
 * Duplicate the specified descriptor to a free descriptor.
 */
int
dupfdopen(struct thread *td, struct filedesc *fdp, int dfd, int mode,
    int openerror, int *indxp)
{
	struct filedescent *newfde, *oldfde;
	struct file *fp;
	u_long *ioctls;
	int error, indx;

	KASSERT(openerror == ENODEV || openerror == ENXIO,
	    ("unexpected error %d in %s", openerror, __func__));

	/*
	 * If the to-be-dup'd fd number is greater than the allowed number
	 * of file descriptors, or the fd to be dup'd has already been
	 * closed, then reject.
	 */
	FILEDESC_XLOCK(fdp);
	if ((fp = fget_noref(fdp, dfd)) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}

	error = fdalloc(td, 0, &indx);
	if (error != 0) {
		FILEDESC_XUNLOCK(fdp);
		return (error);
	}

	/*
	 * There are two cases of interest here.
	 *
	 * For ENODEV simply dup (dfd) to file descriptor (indx) and return.
	 *
	 * For ENXIO steal away the file structure from (dfd) and store it in
	 * (indx).  (dfd) is effectively closed by this operation.
	 */
	switch (openerror) {
	case ENODEV:
		/*
		 * Check that the mode the file is being opened for is a
		 * subset of the mode of the existing descriptor.
		 */
		if (((mode & (FREAD|FWRITE)) | fp->f_flag) != fp->f_flag) {
			fdunused(fdp, indx);
			FILEDESC_XUNLOCK(fdp);
			return (EACCES);
		}
		if (!fhold(fp)) {
			fdunused(fdp, indx);
			FILEDESC_XUNLOCK(fdp);
			return (EBADF);
		}
		newfde = &fdp->fd_ofiles[indx];
		oldfde = &fdp->fd_ofiles[dfd];
		ioctls = filecaps_copy_prep(&oldfde->fde_caps);
#ifdef CAPABILITIES
		seqc_write_begin(&newfde->fde_seqc);
#endif
		fde_copy(oldfde, newfde);
		filecaps_copy_finish(&oldfde->fde_caps, &newfde->fde_caps,
		    ioctls);
#ifdef CAPABILITIES
		seqc_write_end(&newfde->fde_seqc);
#endif
		break;
	case ENXIO:
		/*
		 * Steal away the file pointer from dfd and stuff it into indx.
		 */
		newfde = &fdp->fd_ofiles[indx];
		oldfde = &fdp->fd_ofiles[dfd];
#ifdef CAPABILITIES
		seqc_write_begin(&oldfde->fde_seqc);
		seqc_write_begin(&newfde->fde_seqc);
#endif
		fde_copy(oldfde, newfde);
		oldfde->fde_file = NULL;
		fdunused(fdp, dfd);
#ifdef CAPABILITIES
		seqc_write_end(&newfde->fde_seqc);
		seqc_write_end(&oldfde->fde_seqc);
#endif
		break;
	}
	FILEDESC_XUNLOCK(fdp);
	*indxp = indx;
	return (0);
}

/*
 * This sysctl determines if we will allow a process to chroot(2) if it
 * has a directory open:
 *	0: disallowed for all processes.
 *	1: allowed for processes that were not already chroot(2)'ed.
 *	2: allowed for all processes.
 */

static int chroot_allow_open_directories = 1;

SYSCTL_INT(_kern, OID_AUTO, chroot_allow_open_directories, CTLFLAG_RW,
    &chroot_allow_open_directories, 0,
    "Allow a process to chroot(2) if it has a directory open");

/*
 * Helper function for raised chroot(2) security function:  Refuse if
 * any filedescriptors are open directories.
 */
static int
chroot_refuse_vdir_fds(struct filedesc *fdp)
{
	struct vnode *vp;
	struct file *fp;
	int i;

	FILEDESC_LOCK_ASSERT(fdp);

	FILEDESC_FOREACH_FP(fdp, i, fp) {
		if (fp->f_type == DTYPE_VNODE) {
			vp = fp->f_vnode;
			if (vp->v_type == VDIR)
				return (EPERM);
		}
	}
	return (0);
}

static void
pwd_fill(struct pwd *oldpwd, struct pwd *newpwd)
{

	if (newpwd->pwd_cdir == NULL && oldpwd->pwd_cdir != NULL) {
		vrefact(oldpwd->pwd_cdir);
		newpwd->pwd_cdir = oldpwd->pwd_cdir;
	}

	if (newpwd->pwd_rdir == NULL && oldpwd->pwd_rdir != NULL) {
		vrefact(oldpwd->pwd_rdir);
		newpwd->pwd_rdir = oldpwd->pwd_rdir;
	}

	if (newpwd->pwd_jdir == NULL && oldpwd->pwd_jdir != NULL) {
		vrefact(oldpwd->pwd_jdir);
		newpwd->pwd_jdir = oldpwd->pwd_jdir;
	}

	if (newpwd->pwd_adir == NULL && oldpwd->pwd_adir != NULL) {
		vrefact(oldpwd->pwd_adir);
		newpwd->pwd_adir = oldpwd->pwd_adir;
	}
}

struct pwd *
pwd_hold_pwddesc(struct pwddesc *pdp)
{
	struct pwd *pwd;

	PWDDESC_ASSERT_XLOCKED(pdp);
	pwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	if (pwd != NULL)
		refcount_acquire(&pwd->pwd_refcount);
	return (pwd);
}

bool
pwd_hold_smr(struct pwd *pwd)
{

	MPASS(pwd != NULL);
	if (__predict_true(refcount_acquire_if_not_zero(&pwd->pwd_refcount))) {
		return (true);
	}
	return (false);
}

struct pwd *
pwd_hold(struct thread *td)
{
	struct pwddesc *pdp;
	struct pwd *pwd;

	pdp = td->td_proc->p_pd;

	vfs_smr_enter();
	pwd = vfs_smr_entered_load(&pdp->pd_pwd);
	if (pwd_hold_smr(pwd)) {
		vfs_smr_exit();
		return (pwd);
	}
	vfs_smr_exit();
	PWDDESC_XLOCK(pdp);
	pwd = pwd_hold_pwddesc(pdp);
	MPASS(pwd != NULL);
	PWDDESC_XUNLOCK(pdp);
	return (pwd);
}

struct pwd *
pwd_hold_proc(struct proc *p)
{
	struct pwddesc *pdp;
	struct pwd *pwd;

	PROC_ASSERT_HELD(p);
	PROC_LOCK(p);
	pdp = pdhold(p);
	MPASS(pdp != NULL);
	PROC_UNLOCK(p);

	PWDDESC_XLOCK(pdp);
	pwd = pwd_hold_pwddesc(pdp);
	MPASS(pwd != NULL);
	PWDDESC_XUNLOCK(pdp);
	pddrop(pdp);
	return (pwd);
}

static struct pwd *
pwd_alloc(void)
{
	struct pwd *pwd;

	pwd = uma_zalloc_smr(pwd_zone, M_WAITOK);
	bzero(pwd, sizeof(*pwd));
	refcount_init(&pwd->pwd_refcount, 1);
	return (pwd);
}

void
pwd_drop(struct pwd *pwd)
{

	if (!refcount_release(&pwd->pwd_refcount))
		return;

	if (pwd->pwd_cdir != NULL)
		vrele(pwd->pwd_cdir);
	if (pwd->pwd_rdir != NULL)
		vrele(pwd->pwd_rdir);
	if (pwd->pwd_jdir != NULL)
		vrele(pwd->pwd_jdir);
	if (pwd->pwd_adir != NULL)
		vrele(pwd->pwd_adir);
	uma_zfree_smr(pwd_zone, pwd);
}

/*
* The caller is responsible for invoking priv_check() and
* mac_vnode_check_chroot() to authorize this operation.
*/
int
pwd_chroot(struct thread *td, struct vnode *vp)
{
	struct pwddesc *pdp;
	struct filedesc *fdp;
	struct pwd *newpwd, *oldpwd;
	int error;

	fdp = td->td_proc->p_fd;
	pdp = td->td_proc->p_pd;
	newpwd = pwd_alloc();
	FILEDESC_SLOCK(fdp);
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	if (chroot_allow_open_directories == 0 ||
	    (chroot_allow_open_directories == 1 &&
	    oldpwd->pwd_rdir != rootvnode)) {
		error = chroot_refuse_vdir_fds(fdp);
		FILEDESC_SUNLOCK(fdp);
		if (error != 0) {
			PWDDESC_XUNLOCK(pdp);
			pwd_drop(newpwd);
			return (error);
		}
	} else {
		FILEDESC_SUNLOCK(fdp);
	}

	vrefact(vp);
	newpwd->pwd_rdir = vp;
	vrefact(vp);
	newpwd->pwd_adir = vp;
	if (oldpwd->pwd_jdir == NULL) {
		vrefact(vp);
		newpwd->pwd_jdir = vp;
	}
	pwd_fill(oldpwd, newpwd);
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
	return (0);
}

void
pwd_chdir(struct thread *td, struct vnode *vp)
{
	struct pwddesc *pdp;
	struct pwd *newpwd, *oldpwd;

	VNPASS(vp->v_usecount > 0, vp);

	newpwd = pwd_alloc();
	pdp = td->td_proc->p_pd;
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	newpwd->pwd_cdir = vp;
	pwd_fill(oldpwd, newpwd);
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
}

/*
 * Process is transitioning to/from a non-native ABI.
 */
void
pwd_altroot(struct thread *td, struct vnode *altroot_vp)
{
	struct pwddesc *pdp;
	struct pwd *newpwd, *oldpwd;

	newpwd = pwd_alloc();
	pdp = td->td_proc->p_pd;
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	if (altroot_vp != NULL) {
		/*
		 * Native process to a non-native ABI.
		 */

		vrefact(altroot_vp);
		newpwd->pwd_adir = altroot_vp;
	} else {
		/*
		 * Non-native process to the native ABI.
		 */

		vrefact(oldpwd->pwd_rdir);
		newpwd->pwd_adir = oldpwd->pwd_rdir;
	}
	pwd_fill(oldpwd, newpwd);
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
}

/*
 * jail_attach(2) changes both root and working directories.
 */
int
pwd_chroot_chdir(struct thread *td, struct vnode *vp)
{
	struct pwddesc *pdp;
	struct filedesc *fdp;
	struct pwd *newpwd, *oldpwd;
	int error;

	fdp = td->td_proc->p_fd;
	pdp = td->td_proc->p_pd;
	newpwd = pwd_alloc();
	FILEDESC_SLOCK(fdp);
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	error = chroot_refuse_vdir_fds(fdp);
	FILEDESC_SUNLOCK(fdp);
	if (error != 0) {
		PWDDESC_XUNLOCK(pdp);
		pwd_drop(newpwd);
		return (error);
	}

	vrefact(vp);
	newpwd->pwd_rdir = vp;
	vrefact(vp);
	newpwd->pwd_cdir = vp;
	if (oldpwd->pwd_jdir == NULL) {
		vrefact(vp);
		newpwd->pwd_jdir = vp;
	}
	vrefact(vp);
	newpwd->pwd_adir = vp;
	pwd_fill(oldpwd, newpwd);
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
	return (0);
}

void
pwd_ensure_dirs(void)
{
	struct pwddesc *pdp;
	struct pwd *oldpwd, *newpwd;

	pdp = curproc->p_pd;
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	if (oldpwd->pwd_cdir != NULL && oldpwd->pwd_rdir != NULL &&
	    oldpwd->pwd_adir != NULL) {
		PWDDESC_XUNLOCK(pdp);
		return;
	}
	PWDDESC_XUNLOCK(pdp);

	newpwd = pwd_alloc();
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	pwd_fill(oldpwd, newpwd);
	if (newpwd->pwd_cdir == NULL) {
		vrefact(rootvnode);
		newpwd->pwd_cdir = rootvnode;
	}
	if (newpwd->pwd_rdir == NULL) {
		vrefact(rootvnode);
		newpwd->pwd_rdir = rootvnode;
	}
	if (newpwd->pwd_adir == NULL) {
		vrefact(rootvnode);
		newpwd->pwd_adir = rootvnode;
	}
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
}

void
pwd_set_rootvnode(void)
{
	struct pwddesc *pdp;
	struct pwd *oldpwd, *newpwd;

	pdp = curproc->p_pd;

	newpwd = pwd_alloc();
	PWDDESC_XLOCK(pdp);
	oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	vrefact(rootvnode);
	newpwd->pwd_cdir = rootvnode;
	vrefact(rootvnode);
	newpwd->pwd_rdir = rootvnode;
	vrefact(rootvnode);
	newpwd->pwd_adir = rootvnode;
	pwd_fill(oldpwd, newpwd);
	pwd_set(pdp, newpwd);
	PWDDESC_XUNLOCK(pdp);
	pwd_drop(oldpwd);
}

/*
 * Scan all active processes and prisons to see if any of them have a current
 * or root directory of `olddp'. If so, replace them with the new mount point.
 */
void
mountcheckdirs(struct vnode *olddp, struct vnode *newdp)
{
	struct pwddesc *pdp;
	struct pwd *newpwd, *oldpwd;
	struct prison *pr;
	struct proc *p;
	int nrele;

	if (vrefcnt(olddp) == 1)
		return;
	nrele = 0;
	newpwd = pwd_alloc();
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		pdp = pdhold(p);
		PROC_UNLOCK(p);
		if (pdp == NULL)
			continue;
		PWDDESC_XLOCK(pdp);
		oldpwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
		if (oldpwd == NULL ||
		    (oldpwd->pwd_cdir != olddp &&
		    oldpwd->pwd_rdir != olddp &&
		    oldpwd->pwd_jdir != olddp &&
		    oldpwd->pwd_adir != olddp)) {
			PWDDESC_XUNLOCK(pdp);
			pddrop(pdp);
			continue;
		}
		if (oldpwd->pwd_cdir == olddp) {
			vrefact(newdp);
			newpwd->pwd_cdir = newdp;
		}
		if (oldpwd->pwd_rdir == olddp) {
			vrefact(newdp);
			newpwd->pwd_rdir = newdp;
		}
		if (oldpwd->pwd_jdir == olddp) {
			vrefact(newdp);
			newpwd->pwd_jdir = newdp;
		}
		if (oldpwd->pwd_adir == olddp) {
			vrefact(newdp);
			newpwd->pwd_adir = newdp;
		}
		pwd_fill(oldpwd, newpwd);
		pwd_set(pdp, newpwd);
		PWDDESC_XUNLOCK(pdp);
		pwd_drop(oldpwd);
		pddrop(pdp);
		newpwd = pwd_alloc();
	}
	sx_sunlock(&allproc_lock);
	pwd_drop(newpwd);
	if (rootvnode == olddp) {
		vrefact(newdp);
		rootvnode = newdp;
		nrele++;
	}
	mtx_lock(&prison0.pr_mtx);
	if (prison0.pr_root == olddp) {
		vrefact(newdp);
		prison0.pr_root = newdp;
		nrele++;
	}
	mtx_unlock(&prison0.pr_mtx);
	sx_slock(&allprison_lock);
	TAILQ_FOREACH(pr, &allprison, pr_list) {
		mtx_lock(&pr->pr_mtx);
		if (pr->pr_root == olddp) {
			vrefact(newdp);
			pr->pr_root = newdp;
			nrele++;
		}
		mtx_unlock(&pr->pr_mtx);
	}
	sx_sunlock(&allprison_lock);
	while (nrele--)
		vrele(olddp);
}

int
descrip_check_write_mp(struct filedesc *fdp, struct mount *mp)
{
	struct file *fp;
	struct vnode *vp;
	int error, i;

	error = 0;
	FILEDESC_SLOCK(fdp);
	FILEDESC_FOREACH_FP(fdp, i, fp) {
		if (fp->f_type != DTYPE_VNODE ||
		    (atomic_load_int(&fp->f_flag) & FWRITE) == 0)
			continue;
		vp = fp->f_vnode;
		if (vp->v_mount == mp) {
			error = EDEADLK;
			break;
		}
	}
	FILEDESC_SUNLOCK(fdp);
	return (error);
}

struct filedesc_to_leader *
filedesc_to_leader_alloc(struct filedesc_to_leader *old, struct filedesc *fdp,
    struct proc *leader)
{
	struct filedesc_to_leader *fdtol;

	fdtol = malloc(sizeof(struct filedesc_to_leader),
	    M_FILEDESC_TO_LEADER, M_WAITOK);
	fdtol->fdl_refcount = 1;
	fdtol->fdl_holdcount = 0;
	fdtol->fdl_wakeup = 0;
	fdtol->fdl_leader = leader;
	if (old != NULL) {
		FILEDESC_XLOCK(fdp);
		fdtol->fdl_next = old->fdl_next;
		fdtol->fdl_prev = old;
		old->fdl_next = fdtol;
		fdtol->fdl_next->fdl_prev = fdtol;
		FILEDESC_XUNLOCK(fdp);
	} else {
		fdtol->fdl_next = fdtol;
		fdtol->fdl_prev = fdtol;
	}
	return (fdtol);
}

struct filedesc_to_leader *
filedesc_to_leader_share(struct filedesc_to_leader *fdtol, struct filedesc *fdp)
{
	FILEDESC_XLOCK(fdp);
	fdtol->fdl_refcount++;
	FILEDESC_XUNLOCK(fdp);
	return (fdtol);
}

static int
sysctl_kern_proc_nfds(SYSCTL_HANDLER_ARGS)
{
	NDSLOTTYPE *map;
	struct filedesc *fdp;
	u_int namelen;
	int count, off, minoff;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	if (*(int *)arg1 != 0)
		return (EINVAL);

	fdp = curproc->p_fd;
	count = 0;
	FILEDESC_SLOCK(fdp);
	map = fdp->fd_map;
	off = NDSLOT(fdp->fd_nfiles - 1);
	for (minoff = NDSLOT(0); off >= minoff; --off)
		count += bitcountl(map[off]);
	FILEDESC_SUNLOCK(fdp);

	return (SYSCTL_OUT(req, &count, sizeof(count)));
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_NFDS, nfds,
    CTLFLAG_RD|CTLFLAG_CAPRD|CTLFLAG_MPSAFE, sysctl_kern_proc_nfds,
    "Number of open file descriptors");

/*
 * Get file structures globally.
 */
static int
sysctl_kern_file(SYSCTL_HANDLER_ARGS)
{
	struct xfile xf;
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	int error, n;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	if (req->oldptr == NULL) {
		n = 0;
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			PROC_LOCK(p);
			if (p->p_state == PRS_NEW) {
				PROC_UNLOCK(p);
				continue;
			}
			fdp = fdhold(p);
			PROC_UNLOCK(p);
			if (fdp == NULL)
				continue;
			/* overestimates sparse tables. */
			n += fdp->fd_nfiles;
			fddrop(fdp);
		}
		sx_sunlock(&allproc_lock);
		return (SYSCTL_OUT(req, 0, n * sizeof(xf)));
	}
	error = 0;
	bzero(&xf, sizeof(xf));
	xf.xf_size = sizeof(xf);
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_state == PRS_NEW) {
			PROC_UNLOCK(p);
			continue;
		}
		if (p_cansee(req->td, p) != 0) {
			PROC_UNLOCK(p);
			continue;
		}
		xf.xf_pid = p->p_pid;
		xf.xf_uid = p->p_ucred->cr_uid;
		fdp = fdhold(p);
		PROC_UNLOCK(p);
		if (fdp == NULL)
			continue;
		FILEDESC_SLOCK(fdp);
		if (refcount_load(&fdp->fd_refcnt) == 0)
			goto nextproc;
		FILEDESC_FOREACH_FP(fdp, n, fp) {
			xf.xf_fd = n;
			xf.xf_file = (uintptr_t)fp;
			xf.xf_data = (uintptr_t)fp->f_data;
			xf.xf_vnode = (uintptr_t)fp->f_vnode;
			xf.xf_type = (uintptr_t)fp->f_type;
			xf.xf_count = refcount_load(&fp->f_count);
			xf.xf_msgcount = 0;
			xf.xf_offset = foffset_get(fp);
			xf.xf_flag = fp->f_flag;
			error = SYSCTL_OUT(req, &xf, sizeof(xf));

			/*
			 * There is no need to re-check the fdtable refcount
			 * here since the filedesc lock is not dropped in the
			 * loop body.
			 */
			if (error != 0)
				break;
		}
nextproc:
		FILEDESC_SUNLOCK(fdp);
		fddrop(fdp);
		if (error)
			break;
	}
	sx_sunlock(&allproc_lock);
	return (error);
}

SYSCTL_PROC(_kern, KERN_FILE, file, CTLTYPE_OPAQUE|CTLFLAG_RD|CTLFLAG_MPSAFE,
    0, 0, sysctl_kern_file, "S,xfile", "Entire file table");

#ifdef KINFO_FILE_SIZE
CTASSERT(sizeof(struct kinfo_file) == KINFO_FILE_SIZE);
#endif

static int
xlate_fflags(int fflags)
{
	static const struct {
		int	fflag;
		int	kf_fflag;
	} fflags_table[] = {
		{ FAPPEND, KF_FLAG_APPEND },
		{ FASYNC, KF_FLAG_ASYNC },
		{ FFSYNC, KF_FLAG_FSYNC },
		{ FHASLOCK, KF_FLAG_HASLOCK },
		{ FNONBLOCK, KF_FLAG_NONBLOCK },
		{ FREAD, KF_FLAG_READ },
		{ FWRITE, KF_FLAG_WRITE },
		{ O_CREAT, KF_FLAG_CREAT },
		{ O_DIRECT, KF_FLAG_DIRECT },
		{ O_EXCL, KF_FLAG_EXCL },
		{ O_EXEC, KF_FLAG_EXEC },
		{ O_EXLOCK, KF_FLAG_EXLOCK },
		{ O_NOFOLLOW, KF_FLAG_NOFOLLOW },
		{ O_SHLOCK, KF_FLAG_SHLOCK },
		{ O_TRUNC, KF_FLAG_TRUNC }
	};
	unsigned int i;
	int kflags;

	kflags = 0;
	for (i = 0; i < nitems(fflags_table); i++)
		if (fflags & fflags_table[i].fflag)
			kflags |=  fflags_table[i].kf_fflag;
	return (kflags);
}

/* Trim unused data from kf_path by truncating the structure size. */
void
pack_kinfo(struct kinfo_file *kif)
{

	kif->kf_structsize = offsetof(struct kinfo_file, kf_path) +
	    strlen(kif->kf_path) + 1;
	kif->kf_structsize = roundup(kif->kf_structsize, sizeof(uint64_t));
}

static void
export_file_to_kinfo(struct file *fp, int fd, cap_rights_t *rightsp,
    struct kinfo_file *kif, struct filedesc *fdp, int flags)
{
	int error;

	bzero(kif, sizeof(*kif));

	/* Set a default type to allow for empty fill_kinfo() methods. */
	kif->kf_type = KF_TYPE_UNKNOWN;
	kif->kf_flags = xlate_fflags(fp->f_flag);
	if (rightsp != NULL)
		kif->kf_cap_rights = *rightsp;
	else
		cap_rights_init_zero(&kif->kf_cap_rights);
	kif->kf_fd = fd;
	kif->kf_ref_count = refcount_load(&fp->f_count);
	kif->kf_offset = foffset_get(fp);

	/*
	 * This may drop the filedesc lock, so the 'fp' cannot be
	 * accessed after this call.
	 */
	error = fo_fill_kinfo(fp, kif, fdp);
	if (error == 0)
		kif->kf_status |= KF_ATTR_VALID;
	if ((flags & KERN_FILEDESC_PACK_KINFO) != 0)
		pack_kinfo(kif);
	else
		kif->kf_structsize = roundup2(sizeof(*kif), sizeof(uint64_t));
}

static void
export_vnode_to_kinfo(struct vnode *vp, int fd, int fflags,
    struct kinfo_file *kif, int flags)
{
	int error;

	bzero(kif, sizeof(*kif));

	kif->kf_type = KF_TYPE_VNODE;
	error = vn_fill_kinfo_vnode(vp, kif);
	if (error == 0)
		kif->kf_status |= KF_ATTR_VALID;
	kif->kf_flags = xlate_fflags(fflags);
	cap_rights_init_zero(&kif->kf_cap_rights);
	kif->kf_fd = fd;
	kif->kf_ref_count = -1;
	kif->kf_offset = -1;
	if ((flags & KERN_FILEDESC_PACK_KINFO) != 0)
		pack_kinfo(kif);
	else
		kif->kf_structsize = roundup2(sizeof(*kif), sizeof(uint64_t));
	vrele(vp);
}

struct export_fd_buf {
	struct filedesc		*fdp;
	struct pwddesc	*pdp;
	struct sbuf 		*sb;
	ssize_t			remainder;
	struct kinfo_file	kif;
	int			flags;
};

static int
export_kinfo_to_sb(struct export_fd_buf *efbuf)
{
	struct kinfo_file *kif;

	kif = &efbuf->kif;
	if (efbuf->remainder != -1) {
		if (efbuf->remainder < kif->kf_structsize)
			return (ENOMEM);
		efbuf->remainder -= kif->kf_structsize;
	}
	if (sbuf_bcat(efbuf->sb, kif, kif->kf_structsize) != 0)
		return (sbuf_error(efbuf->sb));
	return (0);
}

static int
export_file_to_sb(struct file *fp, int fd, cap_rights_t *rightsp,
    struct export_fd_buf *efbuf)
{
	int error;

	if (efbuf->remainder == 0)
		return (ENOMEM);
	export_file_to_kinfo(fp, fd, rightsp, &efbuf->kif, efbuf->fdp,
	    efbuf->flags);
	FILEDESC_SUNLOCK(efbuf->fdp);
	error = export_kinfo_to_sb(efbuf);
	FILEDESC_SLOCK(efbuf->fdp);
	return (error);
}

static int
export_vnode_to_sb(struct vnode *vp, int fd, int fflags,
    struct export_fd_buf *efbuf)
{
	int error;

	if (efbuf->remainder == 0)
		return (ENOMEM);
	if (efbuf->pdp != NULL)
		PWDDESC_XUNLOCK(efbuf->pdp);
	export_vnode_to_kinfo(vp, fd, fflags, &efbuf->kif, efbuf->flags);
	error = export_kinfo_to_sb(efbuf);
	if (efbuf->pdp != NULL)
		PWDDESC_XLOCK(efbuf->pdp);
	return (error);
}

/*
 * Store a process file descriptor information to sbuf.
 *
 * Takes a locked proc as argument, and returns with the proc unlocked.
 */
int
kern_proc_filedesc_out(struct proc *p,  struct sbuf *sb, ssize_t maxlen,
    int flags)
{
	struct file *fp;
	struct filedesc *fdp;
	struct pwddesc *pdp;
	struct export_fd_buf *efbuf;
	struct vnode *cttyvp, *textvp, *tracevp;
	struct pwd *pwd;
	int error, i;
	cap_rights_t rights;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/* ktrace vnode */
	tracevp = ktr_get_tracevp(p, true);
	/* text vnode */
	textvp = p->p_textvp;
	if (textvp != NULL)
		vrefact(textvp);
	/* Controlling tty. */
	cttyvp = NULL;
	if (p->p_pgrp != NULL && p->p_pgrp->pg_session != NULL) {
		cttyvp = p->p_pgrp->pg_session->s_ttyvp;
		if (cttyvp != NULL)
			vrefact(cttyvp);
	}
	fdp = fdhold(p);
	pdp = pdhold(p);
	PROC_UNLOCK(p);

	efbuf = malloc(sizeof(*efbuf), M_TEMP, M_WAITOK);
	efbuf->fdp = NULL;
	efbuf->pdp = NULL;
	efbuf->sb = sb;
	efbuf->remainder = maxlen;
	efbuf->flags = flags;

	error = 0;
	if (tracevp != NULL)
		error = export_vnode_to_sb(tracevp, KF_FD_TYPE_TRACE,
		    FREAD | FWRITE, efbuf);
	if (error == 0 && textvp != NULL)
		error = export_vnode_to_sb(textvp, KF_FD_TYPE_TEXT, FREAD,
		    efbuf);
	if (error == 0 && cttyvp != NULL)
		error = export_vnode_to_sb(cttyvp, KF_FD_TYPE_CTTY,
		    FREAD | FWRITE, efbuf);
	if (error != 0 || pdp == NULL || fdp == NULL)
		goto fail;
	efbuf->fdp = fdp;
	efbuf->pdp = pdp;
	PWDDESC_XLOCK(pdp);
	pwd = pwd_hold_pwddesc(pdp);
	if (pwd != NULL) {
		/* working directory */
		if (pwd->pwd_cdir != NULL) {
			vrefact(pwd->pwd_cdir);
			error = export_vnode_to_sb(pwd->pwd_cdir,
			    KF_FD_TYPE_CWD, FREAD, efbuf);
		}
		/* root directory */
		if (error == 0 && pwd->pwd_rdir != NULL) {
			vrefact(pwd->pwd_rdir);
			error = export_vnode_to_sb(pwd->pwd_rdir,
			    KF_FD_TYPE_ROOT, FREAD, efbuf);
		}
		/* jail directory */
		if (error == 0 && pwd->pwd_jdir != NULL) {
			vrefact(pwd->pwd_jdir);
			error = export_vnode_to_sb(pwd->pwd_jdir,
			    KF_FD_TYPE_JAIL, FREAD, efbuf);
		}
	}
	PWDDESC_XUNLOCK(pdp);
	if (error != 0)
		goto fail;
	if (pwd != NULL)
		pwd_drop(pwd);
	FILEDESC_SLOCK(fdp);
	if (refcount_load(&fdp->fd_refcnt) == 0)
		goto skip;
	FILEDESC_FOREACH_FP(fdp, i, fp) {
#ifdef CAPABILITIES
		rights = *cap_rights(fdp, i);
#else /* !CAPABILITIES */
		rights = cap_no_rights;
#endif
		/*
		 * Create sysctl entry.  It is OK to drop the filedesc
		 * lock inside of export_file_to_sb() as we will
		 * re-validate and re-evaluate its properties when the
		 * loop continues.
		 */
		error = export_file_to_sb(fp, i, &rights, efbuf);
		if (error != 0 || refcount_load(&fdp->fd_refcnt) == 0)
			break;
	}
skip:
	FILEDESC_SUNLOCK(fdp);
fail:
	if (fdp != NULL)
		fddrop(fdp);
	if (pdp != NULL)
		pddrop(pdp);
	free(efbuf, M_TEMP);
	return (error);
}

#define FILEDESC_SBUF_SIZE	(sizeof(struct kinfo_file) * 5)

/*
 * Get per-process file descriptors for use by procstat(1), et al.
 */
static int
sysctl_kern_proc_filedesc(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct proc *p;
	ssize_t maxlen;
	u_int namelen;
	int error, error2, *name;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;

	sbuf_new_for_sysctl(&sb, NULL, FILEDESC_SBUF_SIZE, req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = pget((pid_t)name[0], PGET_CANDEBUG | PGET_NOTWEXIT, &p);
	if (error != 0) {
		sbuf_delete(&sb);
		return (error);
	}
	maxlen = req->oldptr != NULL ? req->oldlen : -1;
	error = kern_proc_filedesc_out(p, &sb, maxlen,
	    KERN_FILEDESC_PACK_KINFO);
	error2 = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

#ifdef COMPAT_FREEBSD7
#ifdef KINFO_OFILE_SIZE
CTASSERT(sizeof(struct kinfo_ofile) == KINFO_OFILE_SIZE);
#endif

static void
kinfo_to_okinfo(struct kinfo_file *kif, struct kinfo_ofile *okif)
{

	okif->kf_structsize = sizeof(*okif);
	okif->kf_type = kif->kf_type;
	okif->kf_fd = kif->kf_fd;
	okif->kf_ref_count = kif->kf_ref_count;
	okif->kf_flags = kif->kf_flags & (KF_FLAG_READ | KF_FLAG_WRITE |
	    KF_FLAG_APPEND | KF_FLAG_ASYNC | KF_FLAG_FSYNC | KF_FLAG_NONBLOCK |
	    KF_FLAG_DIRECT | KF_FLAG_HASLOCK);
	okif->kf_offset = kif->kf_offset;
	if (kif->kf_type == KF_TYPE_VNODE)
		okif->kf_vnode_type = kif->kf_un.kf_file.kf_file_type;
	else
		okif->kf_vnode_type = KF_VTYPE_VNON;
	strlcpy(okif->kf_path, kif->kf_path, sizeof(okif->kf_path));
	if (kif->kf_type == KF_TYPE_SOCKET) {
		okif->kf_sock_domain = kif->kf_un.kf_sock.kf_sock_domain0;
		okif->kf_sock_type = kif->kf_un.kf_sock.kf_sock_type0;
		okif->kf_sock_protocol = kif->kf_un.kf_sock.kf_sock_protocol0;
		okif->kf_sa_local = kif->kf_un.kf_sock.kf_sa_local;
		okif->kf_sa_peer = kif->kf_un.kf_sock.kf_sa_peer;
	} else {
		okif->kf_sa_local.ss_family = AF_UNSPEC;
		okif->kf_sa_peer.ss_family = AF_UNSPEC;
	}
}

static int
export_vnode_for_osysctl(struct vnode *vp, int type, struct kinfo_file *kif,
    struct kinfo_ofile *okif, struct pwddesc *pdp, struct sysctl_req *req)
{
	int error;

	vrefact(vp);
	PWDDESC_XUNLOCK(pdp);
	export_vnode_to_kinfo(vp, type, 0, kif, KERN_FILEDESC_PACK_KINFO);
	kinfo_to_okinfo(kif, okif);
	error = SYSCTL_OUT(req, okif, sizeof(*okif));
	PWDDESC_XLOCK(pdp);
	return (error);
}

/*
 * Get per-process file descriptors for use by procstat(1), et al.
 */
static int
sysctl_kern_proc_ofiledesc(SYSCTL_HANDLER_ARGS)
{
	struct kinfo_ofile *okif;
	struct kinfo_file *kif;
	struct filedesc *fdp;
	struct pwddesc *pdp;
	struct pwd *pwd;
	u_int namelen;
	int error, i, *name;
	struct file *fp;
	struct proc *p;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;
	error = pget((pid_t)name[0], PGET_CANDEBUG | PGET_NOTWEXIT, &p);
	if (error != 0)
		return (error);
	fdp = fdhold(p);
	if (fdp != NULL)
		pdp = pdhold(p);
	PROC_UNLOCK(p);
	if (fdp == NULL || pdp == NULL) {
		if (fdp != NULL)
			fddrop(fdp);
		return (ENOENT);
	}
	kif = malloc(sizeof(*kif), M_TEMP, M_WAITOK);
	okif = malloc(sizeof(*okif), M_TEMP, M_WAITOK);
	PWDDESC_XLOCK(pdp);
	pwd = pwd_hold_pwddesc(pdp);
	if (pwd != NULL) {
		if (pwd->pwd_cdir != NULL)
			export_vnode_for_osysctl(pwd->pwd_cdir, KF_FD_TYPE_CWD, kif,
			    okif, pdp, req);
		if (pwd->pwd_rdir != NULL)
			export_vnode_for_osysctl(pwd->pwd_rdir, KF_FD_TYPE_ROOT, kif,
			    okif, pdp, req);
		if (pwd->pwd_jdir != NULL)
			export_vnode_for_osysctl(pwd->pwd_jdir, KF_FD_TYPE_JAIL, kif,
			    okif, pdp, req);
	}
	PWDDESC_XUNLOCK(pdp);
	if (pwd != NULL)
		pwd_drop(pwd);
	FILEDESC_SLOCK(fdp);
	if (refcount_load(&fdp->fd_refcnt) == 0)
		goto skip;
	FILEDESC_FOREACH_FP(fdp, i, fp) {
		export_file_to_kinfo(fp, i, NULL, kif, fdp,
		    KERN_FILEDESC_PACK_KINFO);
		FILEDESC_SUNLOCK(fdp);
		kinfo_to_okinfo(kif, okif);
		error = SYSCTL_OUT(req, okif, sizeof(*okif));
		FILEDESC_SLOCK(fdp);
		if (error != 0 || refcount_load(&fdp->fd_refcnt) == 0)
			break;
	}
skip:
	FILEDESC_SUNLOCK(fdp);
	fddrop(fdp);
	pddrop(pdp);
	free(kif, M_TEMP);
	free(okif, M_TEMP);
	return (0);
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_OFILEDESC, ofiledesc,
    CTLFLAG_RD|CTLFLAG_MPSAFE, sysctl_kern_proc_ofiledesc,
    "Process ofiledesc entries");
#endif	/* COMPAT_FREEBSD7 */

int
vntype_to_kinfo(int vtype)
{
	struct {
		int	vtype;
		int	kf_vtype;
	} vtypes_table[] = {
		{ VBAD, KF_VTYPE_VBAD },
		{ VBLK, KF_VTYPE_VBLK },
		{ VCHR, KF_VTYPE_VCHR },
		{ VDIR, KF_VTYPE_VDIR },
		{ VFIFO, KF_VTYPE_VFIFO },
		{ VLNK, KF_VTYPE_VLNK },
		{ VNON, KF_VTYPE_VNON },
		{ VREG, KF_VTYPE_VREG },
		{ VSOCK, KF_VTYPE_VSOCK }
	};
	unsigned int i;

	/*
	 * Perform vtype translation.
	 */
	for (i = 0; i < nitems(vtypes_table); i++)
		if (vtypes_table[i].vtype == vtype)
			return (vtypes_table[i].kf_vtype);

	return (KF_VTYPE_UNKNOWN);
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_FILEDESC, filedesc,
    CTLFLAG_RD|CTLFLAG_MPSAFE, sysctl_kern_proc_filedesc,
    "Process filedesc entries");

/*
 * Store a process current working directory information to sbuf.
 *
 * Takes a locked proc as argument, and returns with the proc unlocked.
 */
int
kern_proc_cwd_out(struct proc *p,  struct sbuf *sb, ssize_t maxlen)
{
	struct pwddesc *pdp;
	struct pwd *pwd;
	struct export_fd_buf *efbuf;
	struct vnode *cdir;
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	pdp = pdhold(p);
	PROC_UNLOCK(p);
	if (pdp == NULL)
		return (EINVAL);

	efbuf = malloc(sizeof(*efbuf), M_TEMP, M_WAITOK);
	efbuf->fdp = NULL;
	efbuf->pdp = pdp;
	efbuf->sb = sb;
	efbuf->remainder = maxlen;
	efbuf->flags = 0;

	PWDDESC_XLOCK(pdp);
	pwd = PWDDESC_XLOCKED_LOAD_PWD(pdp);
	cdir = pwd->pwd_cdir;
	if (cdir == NULL) {
		error = EINVAL;
	} else {
		vrefact(cdir);
		error = export_vnode_to_sb(cdir, KF_FD_TYPE_CWD, FREAD, efbuf);
	}
	PWDDESC_XUNLOCK(pdp);
	pddrop(pdp);
	free(efbuf, M_TEMP);
	return (error);
}

/*
 * Get per-process current working directory.
 */
static int
sysctl_kern_proc_cwd(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct proc *p;
	ssize_t maxlen;
	u_int namelen;
	int error, error2, *name;

	namelen = arg2;
	if (namelen != 1)
		return (EINVAL);

	name = (int *)arg1;

	sbuf_new_for_sysctl(&sb, NULL, sizeof(struct kinfo_file), req);
	sbuf_clear_flags(&sb, SBUF_INCLUDENUL);
	error = pget((pid_t)name[0], PGET_CANDEBUG | PGET_NOTWEXIT, &p);
	if (error != 0) {
		sbuf_delete(&sb);
		return (error);
	}
	maxlen = req->oldptr != NULL ? req->oldlen : -1;
	error = kern_proc_cwd_out(p, &sb, maxlen);
	error2 = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error != 0 ? error : error2);
}

static SYSCTL_NODE(_kern_proc, KERN_PROC_CWD, cwd, CTLFLAG_RD|CTLFLAG_MPSAFE,
    sysctl_kern_proc_cwd, "Process current working directory");

#ifdef DDB
/*
 * For the purposes of debugging, generate a human-readable string for the
 * file type.
 */
static const char *
file_type_to_name(short type)
{

	switch (type) {
	case 0:
		return ("zero");
	case DTYPE_VNODE:
		return ("vnode");
	case DTYPE_SOCKET:
		return ("socket");
	case DTYPE_PIPE:
		return ("pipe");
	case DTYPE_FIFO:
		return ("fifo");
	case DTYPE_KQUEUE:
		return ("kqueue");
	case DTYPE_CRYPTO:
		return ("crypto");
	case DTYPE_MQUEUE:
		return ("mqueue");
	case DTYPE_SHM:
		return ("shm");
	case DTYPE_SEM:
		return ("ksem");
	case DTYPE_PTS:
		return ("pts");
	case DTYPE_DEV:
		return ("dev");
	case DTYPE_PROCDESC:
		return ("proc");
	case DTYPE_EVENTFD:
		return ("eventfd");
	case DTYPE_TIMERFD:
		return ("timerfd");
	default:
		return ("unkn");
	}
}

/*
 * For the purposes of debugging, identify a process (if any, perhaps one of
 * many) that references the passed file in its file descriptor array. Return
 * NULL if none.
 */
static struct proc *
file_to_first_proc(struct file *fp)
{
	struct filedesc *fdp;
	struct proc *p;
	int n;

	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_state == PRS_NEW)
			continue;
		fdp = p->p_fd;
		if (fdp == NULL)
			continue;
		for (n = 0; n < fdp->fd_nfiles; n++) {
			if (fp == fdp->fd_ofiles[n].fde_file)
				return (p);
		}
	}
	return (NULL);
}

static void
db_print_file(struct file *fp, int header)
{
#define XPTRWIDTH ((int)howmany(sizeof(void *) * NBBY, 4))
	struct proc *p;

	if (header)
		db_printf("%*s %6s %*s %8s %4s %5s %6s %*s %5s %s\n",
		    XPTRWIDTH, "File", "Type", XPTRWIDTH, "Data", "Flag",
		    "GCFl", "Count", "MCount", XPTRWIDTH, "Vnode", "FPID",
		    "FCmd");
	p = file_to_first_proc(fp);
	db_printf("%*p %6s %*p %08x %04x %5d %6d %*p %5d %s\n", XPTRWIDTH,
	    fp, file_type_to_name(fp->f_type), XPTRWIDTH, fp->f_data,
	    fp->f_flag, 0, refcount_load(&fp->f_count), 0, XPTRWIDTH, fp->f_vnode,
	    p != NULL ? p->p_pid : -1, p != NULL ? p->p_comm : "-");

#undef XPTRWIDTH
}

DB_SHOW_COMMAND(file, db_show_file)
{
	struct file *fp;

	if (!have_addr) {
		db_printf("usage: show file <addr>\n");
		return;
	}
	fp = (struct file *)addr;
	db_print_file(fp, 1);
}

DB_SHOW_COMMAND_FLAGS(files, db_show_files, DB_CMD_MEMSAFE)
{
	struct filedesc *fdp;
	struct file *fp;
	struct proc *p;
	int header;
	int n;

	header = 1;
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_state == PRS_NEW)
			continue;
		if ((fdp = p->p_fd) == NULL)
			continue;
		for (n = 0; n < fdp->fd_nfiles; ++n) {
			if ((fp = fdp->fd_ofiles[n].fde_file) == NULL)
				continue;
			db_print_file(fp, header);
			header = 0;
		}
	}
}
#endif

SYSCTL_INT(_kern, KERN_MAXFILESPERPROC, maxfilesperproc,
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &maxfilesperproc, 0, "Maximum files allowed open per process");

SYSCTL_INT(_kern, KERN_MAXFILES, maxfiles, CTLFLAG_RWTUN | CTLFLAG_NOFETCH,
    &maxfiles, 0, "Maximum number of files");

SYSCTL_INT(_kern, OID_AUTO, openfiles, CTLFLAG_RD,
    &openfiles, 0, "System-wide number of open files");

/* ARGSUSED*/
static void
filelistinit(void *dummy)
{

	file_zone = uma_zcreate("Files", sizeof(struct file), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NOFREE);
	filedesc0_zone = uma_zcreate("filedesc0", sizeof(struct filedesc0),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	pwd_zone = uma_zcreate("PWD", sizeof(struct pwd), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_SMR);
	/*
	 * XXXMJG this is a temporary hack due to boot ordering issues against
	 * the vnode zone.
	 */
	vfs_smr = uma_zone_get_smr(pwd_zone);
	mtx_init(&sigio_lock, "sigio lock", NULL, MTX_DEF);
}
SYSINIT(select, SI_SUB_LOCK, SI_ORDER_FIRST, filelistinit, NULL);

/*-------------------------------------------------------------------*/

static int
badfo_readwrite(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EBADF);
}

static int
badfo_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

static int
badfo_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return (0);
}

static int
badfo_kqfilter(struct file *fp, struct knote *kn)
{

	return (EBADF);
}

static int
badfo_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{

	return (EBADF);
}

static int
badfo_close(struct file *fp, struct thread *td)
{

	return (0);
}

static int
badfo_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_sendfile(struct file *fp, int sockfd, struct uio *hdr_uio,
    struct uio *trl_uio, off_t offset, size_t nbytes, off_t *sent, int flags,
    struct thread *td)
{

	return (EBADF);
}

static int
badfo_fill_kinfo(struct file *fp, struct kinfo_file *kif, struct filedesc *fdp)
{

	return (0);
}

struct fileops badfileops = {
	.fo_read = badfo_readwrite,
	.fo_write = badfo_readwrite,
	.fo_truncate = badfo_truncate,
	.fo_ioctl = badfo_ioctl,
	.fo_poll = badfo_poll,
	.fo_kqfilter = badfo_kqfilter,
	.fo_stat = badfo_stat,
	.fo_close = badfo_close,
	.fo_chmod = badfo_chmod,
	.fo_chown = badfo_chown,
	.fo_sendfile = badfo_sendfile,
	.fo_fill_kinfo = badfo_fill_kinfo,
};

static int
path_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	return (POLLNVAL);
}

static int
path_close(struct file *fp, struct thread *td)
{
	MPASS(fp->f_type == DTYPE_VNODE);
	fp->f_ops = &badfileops;
	vrele(fp->f_vnode);
	return (0);
}

struct fileops path_fileops = {
	.fo_read = badfo_readwrite,
	.fo_write = badfo_readwrite,
	.fo_truncate = badfo_truncate,
	.fo_ioctl = badfo_ioctl,
	.fo_poll = path_poll,
	.fo_kqfilter = vn_kqfilter_opath,
	.fo_stat = vn_statfile,
	.fo_close = path_close,
	.fo_chmod = badfo_chmod,
	.fo_chown = badfo_chown,
	.fo_sendfile = badfo_sendfile,
	.fo_fill_kinfo = vn_fill_kinfo,
	.fo_cmp = vn_cmp,
	.fo_flags = DFLAG_PASSABLE,
};

int
invfo_rdwr(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return (EOPNOTSUPP);
}

int
invfo_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

int
invfo_ioctl(struct file *fp, u_long com, void *data,
    struct ucred *active_cred, struct thread *td)
{

	return (ENOTTY);
}

int
invfo_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return (poll_no_poll(events));
}

int
invfo_kqfilter(struct file *fp, struct knote *kn)
{

	return (EINVAL);
}

int
invfo_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

int
invfo_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{

	return (EINVAL);
}

int
invfo_sendfile(struct file *fp, int sockfd, struct uio *hdr_uio,
    struct uio *trl_uio, off_t offset, size_t nbytes, off_t *sent, int flags,
    struct thread *td)
{

	return (EINVAL);
}

/*-------------------------------------------------------------------*/

/*
 * File Descriptor pseudo-device driver (/dev/fd/).
 *
 * Opening minor device N dup()s the file (if any) connected to file
 * descriptor N belonging to the calling process.  Note that this driver
 * consists of only the ``open()'' routine, because all subsequent
 * references to this file will be direct to the other driver.
 *
 * XXX: we could give this one a cloning event handler if necessary.
 */

/* ARGSUSED */
static int
fdopen(struct cdev *dev, int mode, int type, struct thread *td)
{

	/*
	 * XXX Kludge: set curthread->td_dupfd to contain the value of the
	 * the file descriptor being sought for duplication. The error
	 * return ensures that the vnode for this device will be released
	 * by vn_open. Open will detect this special error and take the
	 * actions in dupfdopen below. Other callers of vn_open or VOP_OPEN
	 * will simply report the error.
	 */
	td->td_dupfd = dev2unit(dev);
	return (ENODEV);
}

static struct cdevsw fildesc_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	fdopen,
	.d_name =	"FD",
};

static void
fildesc_drvinit(void *unused)
{
	struct cdev *dev;

	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 0, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/0");
	make_dev_alias(dev, "stdin");
	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 1, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/1");
	make_dev_alias(dev, "stdout");
	dev = make_dev_credf(MAKEDEV_ETERNAL, &fildesc_cdevsw, 2, NULL,
	    UID_ROOT, GID_WHEEL, 0666, "fd/2");
	make_dev_alias(dev, "stderr");
}

SYSINIT(fildescdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, fildesc_drvinit, NULL);
