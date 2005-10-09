/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)filedesc.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _SYS_FILEDESC_H_
#define	_SYS_FILEDESC_H_

#include <sys/queue.h>
#include <sys/event.h>
#include <sys/priority.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

#include <machine/_limits.h>

/*
 * This structure is used for the management of descriptors.  It may be
 * shared by multiple processes.
 */
#define NDSLOTTYPE	u_long

struct filedesc {
	struct	file **fd_ofiles;	/* file structures for open files */
	char	*fd_ofileflags;		/* per-process open file flags */
	struct	vnode *fd_cdir;		/* current directory */
	struct	vnode *fd_rdir;		/* root directory */
	struct	vnode *fd_jdir;		/* jail root directory */
	int	fd_nfiles;		/* number of open files allocated */
	NDSLOTTYPE *fd_map;		/* bitmap of free fds */
	int	fd_lastfile;		/* high-water mark of fd_ofiles */
	int	fd_freefile;		/* approx. next free file */
	u_short	fd_cmask;		/* mask for file creation */
	u_short	fd_refcnt;		/* thread reference count */
	u_short	fd_holdcnt;		/* hold count on structure + mutex */

	struct	mtx fd_mtx;		/* protects members of this struct */
	int	fd_locked;		/* long lock flag */
	int	fd_wanted;		/* "" */
	struct	kqlist fd_kqlist;	/* list of kqueues on this filedesc */
	int	fd_holdleaderscount;	/* block fdfree() for shared close() */
	int	fd_holdleaderswakeup;	/* fdfree() needs wakeup */
};

/*
 * Structure to keep track of (process leader, struct fildedesc) tuples.
 * Each process has a pointer to such a structure when detailed tracking
 * is needed, e.g., when rfork(RFPROC | RFMEM) causes a file descriptor
 * table to be shared by processes having different "p_leader" pointers
 * and thus distinct POSIX style locks.
 *
 * fdl_refcount and fdl_holdcount are protected by struct filedesc mtx.
 */
struct filedesc_to_leader {
	int		fdl_refcount;	/* references from struct proc */
	int		fdl_holdcount;	/* temporary hold during closef */
	int		fdl_wakeup;	/* fdfree() waits on closef() */
	struct proc	*fdl_leader;	/* owner of POSIX locks */
	/* Circular list: */
	struct filedesc_to_leader *fdl_prev;
	struct filedesc_to_leader *fdl_next;
};

/*
 * Per-process open flags.
 */
#define	UF_EXCLOSE 	0x01		/* auto-close on exec */

#ifdef _KERNEL

/* Lock a file descriptor table. */
#define	FILEDESC_LOCK(fd)								\
	do {										\
		mtx_lock(&(fd)->fd_mtx);						\
		(fd)->fd_wanted++;							\
		while ((fd)->fd_locked)							\
			msleep(&(fd)->fd_locked, &(fd)->fd_mtx, PLOCK, "fdesc", 0);	\
		(fd)->fd_locked = 2;							\
		(fd)->fd_wanted--;							\
		mtx_unlock(&(fd)->fd_mtx);						\
	} while (0)

#define	FILEDESC_UNLOCK(fd)								\
	do {										\
		mtx_lock(&(fd)->fd_mtx);						\
		KASSERT((fd)->fd_locked == 2,						\
		    ("fdesc locking mistake %d should be %d", (fd)->fd_locked, 2));	\
		(fd)->fd_locked = 0;							\
		if ((fd)->fd_wanted)							\
			wakeup(&(fd)->fd_locked);					\
		mtx_unlock(&(fd)->fd_mtx);						\
	} while (0)

#define	FILEDESC_LOCK_FAST(fd)								\
	do {										\
		mtx_lock(&(fd)->fd_mtx);						\
		(fd)->fd_wanted++;							\
		while ((fd)->fd_locked)							\
			msleep(&(fd)->fd_locked, &(fd)->fd_mtx, PLOCK, "fdesc", 0);	\
		(fd)->fd_locked = 1;							\
		(fd)->fd_wanted--;							\
	} while (0)

#define	FILEDESC_UNLOCK_FAST(fd)							\
	do {										\
		KASSERT((fd)->fd_locked == 1,						\
		    ("fdesc locking mistake %d should be %d", (fd)->fd_locked, 1));	\
		(fd)->fd_locked = 0;							\
		if ((fd)->fd_wanted)							\
			wakeup(&(fd)->fd_locked);					\
		mtx_unlock(&(fd)->fd_mtx);						\
	} while (0)

#ifdef INVARIANT_SUPPORT
#define	FILEDESC_LOCK_ASSERT(fd, arg)							\
	do {										\
		if ((arg) == MA_OWNED)							\
			KASSERT((fd)->fd_locked != 0, ("fdesc locking mistake"));	\
		else									\
			KASSERT((fd)->fd_locked == 0, ("fdesc locking mistake"));	\
	} while (0)
#else
#define	FILEDESC_LOCK_ASSERT(fd, arg)
#endif

#define	FILEDESC_LOCK_DESC	"filedesc structure"

struct thread;

int	closef(struct file *fp, struct thread *td);
int	dupfdopen(struct thread *td, struct filedesc *fdp, int indx, int dfd,
	    int mode, int error);
int	falloc(struct thread *td, struct file **resultfp, int *resultfd);
int	fdalloc(struct thread *td, int minfd, int *result);
int	fdavail(struct thread *td, int n);
int	fdcheckstd(struct thread *td);
void	fdclose(struct filedesc *fdp, struct file *fp, int idx, struct thread *td);
void	fdcloseexec(struct thread *td);
struct	filedesc *fdcopy(struct filedesc *fdp);
void	fdunshare(struct proc *p, struct thread *td);
void	fdfree(struct thread *td);
struct	filedesc *fdinit(struct filedesc *fdp);
struct	filedesc *fdshare(struct filedesc *fdp);
struct filedesc_to_leader *
	filedesc_to_leader_alloc(struct filedesc_to_leader *old,
	    struct filedesc *fdp, struct proc *leader);
int	getvnode(struct filedesc *fdp, int fd, struct file **fpp);
void	mountcheckdirs(struct vnode *olddp, struct vnode *newdp);
void	setugidsafety(struct thread *td);

static __inline struct file *
fget_locked(struct filedesc *fdp, int fd)
{

	return (fd < 0 || fd >= fdp->fd_nfiles ? NULL : fdp->fd_ofiles[fd]);
}

#endif /* _KERNEL */

#endif /* !_SYS_FILEDESC_H_ */
