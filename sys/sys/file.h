/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)file.h	8.3 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _SYS_FILE_H_
#define	_SYS_FILE_H_

#ifndef _KERNEL
#include <sys/fcntl.h>
#include <sys/unistd.h>
#endif

#ifdef _KERNEL
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/queue.h>

struct stat;
struct thread;
struct uio;
struct knote;
struct vnode;
struct socket;

/*
 * Kernel descriptor table.
 * One entry for each open kernel vnode and socket.
 *
 * Below is the list of locks that protects members in struct file.
 *
 * (fl)	filelist_lock
 * (f)	f_mtx in struct file
 * none	not locked
 */
struct file {
	LIST_ENTRY(file) f_list;/* (fl) list of active files */
	short	f_gcflag;	/* used by thread doing fd garbage collection */
#define	DTYPE_VNODE	1	/* file */
#define	DTYPE_SOCKET	2	/* communications endpoint */
#define	DTYPE_PIPE	3	/* pipe */
#define	DTYPE_FIFO	4	/* fifo (named pipe) */
#define	DTYPE_KQUEUE	5	/* event queue */
	short	f_type;		/* descriptor type */
	int	f_count;	/* (f) reference count */
	int	f_msgcount;	/* (f) references from message queue */
	struct	ucred *f_cred;	/* credentials associated with descriptor */
	struct fileops {
		int	(*fo_read)	__P((struct file *fp, struct uio *uio,
					    struct ucred *cred, int flags,
					    struct thread *td));
		int	(*fo_write)	__P((struct file *fp, struct uio *uio,
					    struct ucred *cred, int flags,
					    struct thread *td));
#define	FOF_OFFSET	1
		int	(*fo_ioctl)	__P((struct file *fp, u_long com,
					    caddr_t data, struct thread *td));
		int	(*fo_poll)	__P((struct file *fp, int events,
					    struct ucred *cred, struct thread *td));
		int	(*fo_kqfilter)	__P((struct file *fp,
					    struct knote *kn));
		int	(*fo_stat)	__P((struct file *fp, struct stat *sb,
					    struct thread *td));
		int	(*fo_close)	__P((struct file *fp, struct thread *td));
	} *f_ops;
	int	f_seqcount;	/*
				 * count of sequential accesses -- cleared
				 * by most seek operations.
				 */
	off_t	f_nextoff;	/*
				 * offset of next expected read or write
				 */
	off_t	f_offset;
	caddr_t	f_data;		/* vnode or socket */
	u_int	f_flag;		/* see fcntl.h */
	struct mtx	f_mtx;	/* mutex to protect data */
};

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_FILE);
#endif

LIST_HEAD(filelist, file);
extern struct filelist filehead; /* (fl) head of list of open files */
extern struct fileops vnops;
extern struct fileops badfileops;
extern int maxfiles;		/* kernel limit on number of open files */
extern int maxfilesperproc;	/* per process limit on number of open files */
extern int nfiles;		/* (fl) actual number of open files */
extern struct sx filelist_lock; /* sx to protect filelist and nfiles */

static __inline struct file * fhold __P((struct file *fp));
static __inline struct file * fhold_locked __P((struct file *fp));
int fget __P((struct thread *td, int fd, struct file **fpp));
int fget_read __P((struct thread *td, int fd, struct file **fpp));
int fget_write __P((struct thread *td, int fd, struct file **fpp));
int fdrop __P((struct file *fp, struct thread *td));
int fdrop_locked __P((struct file *fp, struct thread *td));

/* Lock a file. */
#define FILE_LOCK(f)	mtx_lock(&(f)->f_mtx)
#define FILE_UNLOCK(f)	mtx_unlock(&(f)->f_mtx)
#define	FILE_LOCKED(f)	mtx_owned(&(f)->f_mtx)
#define	FILE_LOCK_ASSERT(f, type)	mtx_assert(&(f)->f_mtx, (type))

int fgetvp __P((struct thread *td, int fd, struct vnode **vpp));
int fgetvp_read __P((struct thread *td, int fd, struct vnode **vpp));
int fgetvp_write __P((struct thread *td, int fd, struct vnode **vpp));

int fgetsock __P((struct thread *td, int fd, struct socket **spp, u_int *fflagp));
void fputsock __P((struct socket *sp));

static __inline struct file *
fhold_locked(fp)
	struct file *fp;
{

#ifdef INVARIANTS
	FILE_LOCK_ASSERT(fp, MA_OWNED);
#endif
	fp->f_count++;
	return (fp);
}

static __inline struct file *
fhold(fp)
	struct file *fp;
{

	FILE_LOCK(fp);
	fhold_locked(fp);
	FILE_UNLOCK(fp);
	return (fp);
}

static __inline int fo_read __P((struct file *fp, struct uio *uio,
    struct ucred *cred, int flags, struct thread *td));
static __inline int fo_write __P((struct file *fp, struct uio *uio,
    struct ucred *cred, int flags, struct thread *td));
static __inline int fo_ioctl __P((struct file *fp, u_long com, caddr_t data,
    struct thread *td));
static __inline int fo_poll __P((struct file *fp, int events,
    struct ucred *cred, struct thread *td));
static __inline int fo_stat __P((struct file *fp, struct stat *sb,
    struct thread *td));
static __inline int fo_close __P((struct file *fp, struct thread *td));
static __inline int fo_kqfilter __P((struct file *fp, struct knote *kn));
struct proc;
struct file *ffind_hold(struct thread *, int fd);
struct file *ffind_lock(struct thread *, int fd);

static __inline int
fo_read(fp, uio, cred, flags, td)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
	struct thread *td;
	int flags;
{

	return ((*fp->f_ops->fo_read)(fp, uio, cred, flags, td));
}

static __inline int
fo_write(fp, uio, cred, flags, td)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
	struct thread *td;
	int flags;
{
	return ((*fp->f_ops->fo_write)(fp, uio, cred, flags, td));
}

static __inline int
fo_ioctl(fp, com, data, td)
	struct file *fp;
	u_long com;
	caddr_t data;
	struct thread *td;
{
	return ((*fp->f_ops->fo_ioctl)(fp, com, data, td));
}

static __inline int
fo_poll(fp, events, cred, td)
	struct file *fp;
	int events;
	struct ucred *cred;
	struct thread *td;
{
	/* select(2) and poll(2) hold file descriptors. */
	return ((*fp->f_ops->fo_poll)(fp, events, cred, td));
}

static __inline int
fo_stat(fp, sb, td)
	struct file *fp;
	struct stat *sb;
	struct thread *td;
{
	return ((*fp->f_ops->fo_stat)(fp, sb, td));
}

static __inline int
fo_close(fp, td)
	struct file *fp;
	struct thread *td;
{

	return ((*fp->f_ops->fo_close)(fp, td));
}

static __inline int
fo_kqfilter(fp, kn)
	struct file *fp;
	struct knote *kn;
{

	return ((*fp->f_ops->fo_kqfilter)(fp, kn));
}

#endif /* _KERNEL */

#endif /* !SYS_FILE_H */
