/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2000
 *	Poul-Henning Kamp.  All rights reserved.
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
 *	@(#)conf.h	8.5 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _SYS_CONF_H_
#define	_SYS_CONF_H_

#ifdef _KERNEL
#include <sys/eventhandler.h>
#else
#include <sys/queue.h>
#endif

struct tty;
struct snapdata;
struct devfs_dirent;
struct cdevsw;

struct cdev {
	u_int		si_flags;
#define SI_ALIAS	0x0002	/* carrier of alias name */
#define SI_NAMED	0x0004	/* make_dev{_alias} has been called */
#define SI_CHEAPCLONE	0x0008	/* can be removed_dev'ed when vnode reclaims */
#define SI_CHILD	0x0010	/* child of another struct cdev **/
#define SI_DEVOPEN	0x0020	/* opened by device */
#define SI_CONSOPEN	0x0040	/* opened by console */
#define SI_DUMPDEV	0x0080	/* is kernel dumpdev */
#define SI_CANDELETE	0x0100	/* can do BIO_DELETE */
#define SI_CLONELIST	0x0200	/* on a clone list */
	struct timespec	si_atime;
	struct timespec	si_ctime;
	struct timespec	si_mtime;
	u_int		si_drv0;
	int		si_refcount;
	LIST_ENTRY(cdev)	si_list;
	LIST_ENTRY(cdev)	si_clone;
	LIST_ENTRY(cdev)	si_hash;
	LIST_HEAD(,devfs_dirent)si_alist;
	LIST_HEAD(, cdev)	si_children;
	LIST_ENTRY(cdev)	si_siblings;
	struct cdev *si_parent;
	u_int		si_inode;
	char		*si_name;
	void		*si_drv1, *si_drv2;
	struct cdevsw	*si_devsw;
	int		si_iosize_max;	/* maximum I/O size (for physio &al) */
	u_long		si_usecount;
	u_long		si_threadcount;
	union {
		struct tty *__sit_tty;
		struct snapdata *__sid_snapdata;
	} __si_u;
	char		__si_namebuf[SPECNAMELEN + 1];
};

#define si_tty		__si_u.__sit_tty
#define si_snapdata	__si_u.__sid_snapdata

#ifdef _KERNEL

/*
 * Definitions of device driver entry switches
 */

struct bio;
struct buf;
struct thread;
struct uio;
struct knote;
struct clonedevs;

/*
 * Note: d_thread_t is provided as a transition aid for those drivers
 * that treat struct proc/struct thread as an opaque data type and
 * exist in substantially the same form in both 4.x and 5.x.  Writers
 * of drivers that dips into the d_thread_t structure should use
 * struct thread or struct proc as appropriate for the version of the
 * OS they are using.  It is provided in lieu of each device driver
 * inventing its own way of doing this.  While it does violate style(9)
 * in a number of ways, this violation is deemed to be less
 * important than the benefits that a uniform API between releases
 * gives.
 *
 * Users of struct thread/struct proc that aren't device drivers should
 * not use d_thread_t.
 */

typedef struct thread d_thread_t;

typedef int d_open_t(struct cdev *dev, int oflags, int devtype, struct thread *td);
typedef int d_fdopen_t(struct cdev *dev, int oflags, struct thread *td, int fdidx);
typedef int d_close_t(struct cdev *dev, int fflag, int devtype, struct thread *td);
typedef void d_strategy_t(struct bio *bp);
typedef int d_ioctl_t(struct cdev *dev, u_long cmd, caddr_t data,
		      int fflag, struct thread *td);

typedef int d_read_t(struct cdev *dev, struct uio *uio, int ioflag);
typedef int d_write_t(struct cdev *dev, struct uio *uio, int ioflag);
typedef int d_poll_t(struct cdev *dev, int events, struct thread *td);
typedef int d_kqfilter_t(struct cdev *dev, struct knote *kn);
typedef int d_mmap_t(struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr,
   		     int nprot);
typedef void d_purge_t(struct cdev *dev);

typedef int d_spare2_t(struct cdev *dev);

typedef int dumper_t(
	void *priv,		/* Private to the driver. */
	void *virtual,		/* Virtual (mapped) address. */
	vm_offset_t physical,	/* Physical address of virtual. */
	off_t offset,		/* Byte-offset to write at. */
	size_t length);		/* Number of bytes to dump. */

#endif /* _KERNEL */

/*
 * Types for d_flags.
 */
#define	D_TAPE	0x0001
#define	D_DISK	0x0002
#define	D_TTY	0x0004
#define	D_MEM	0x0008

#ifdef _KERNEL 

#define	D_TYPEMASK	0xffff

/*
 * Flags for d_flags which the drivers can set.
 */
#define	D_MEMDISK	0x00010000	/* memory type disk */
#define	D_TRACKCLOSE	0x00080000	/* track all closes */
#define D_MMAP_ANON	0x00100000	/* special treatment in vm_mmap.c */
#define D_PSEUDO	0x00200000	/* make_dev() can return NULL */
#define D_NEEDGIANT	0x00400000	/* driver want Giant */

/*
 * Version numbers.
 */
#define D_VERSION_00	0x20011966
#define D_VERSION_01	0x17032005	/* Add d_uid,gid,mode & kind */
#define D_VERSION	D_VERSION_01

/*
 * Flags used for internal housekeeping
 */
#define D_INIT		0x80000000	/* cdevsw initialized */
#define D_ALLOCMAJ	0x40000000	/* major# is allocated */

/*
 * Character device switch table
 */
struct cdevsw {
	int			d_version;
	int			d_maj;
	u_int			d_flags;
	const char		*d_name;
	d_open_t		*d_open;
	d_fdopen_t		*d_fdopen;
	d_close_t		*d_close;
	d_read_t		*d_read;
	d_write_t		*d_write;
	d_ioctl_t		*d_ioctl;
	d_poll_t		*d_poll;
	d_mmap_t		*d_mmap;
	d_strategy_t		*d_strategy;
	dumper_t		*d_dump;
	d_kqfilter_t		*d_kqfilter;
	d_purge_t		*d_purge;
	d_spare2_t		*d_spare2;
	uid_t			d_uid;
	gid_t			d_gid;
	mode_t			d_mode;
	const char		*d_kind;

	/* These fields should not be messed with by drivers */
	LIST_ENTRY(cdevsw)	d_list;
	LIST_HEAD(, cdev)	d_devs;
	int			d_spare3;
};

#define NUMCDEVSW 256

#define MAXMINOR	0xffff00ffU

struct module;

struct devsw_module_data {
	int	(*chainevh)(struct module *, int, void *); /* next handler */
	void	*chainarg;	/* arg for next event handler */
	/* Do not initialize fields hereafter */
};

#define DEV_MODULE(name, evh, arg)					\
static moduledata_t name##_mod = {					\
    #name,								\
    evh,								\
    arg									\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)


void clone_setup(struct clonedevs **cdp);
void clone_cleanup(struct clonedevs **);
#define CLONE_UNITMASK 0xfffff
#define CLONE_FLAG0 (CLONE_UNITMASK + 1)
int clone_create(struct clonedevs **, struct cdevsw *, int *unit, struct cdev **dev, u_int extra);

int	count_dev(struct cdev *_dev);
void	destroy_dev(struct cdev *_dev);
struct cdevsw *dev_refthread(struct cdev *_dev);
void	dev_relthread(struct cdev *_dev);
int	dev_named(struct cdev *_pdev, const char *_name);
void	dev_depends(struct cdev *_pdev, struct cdev *_cdev);
void	dev_ref(struct cdev *dev);
void	dev_rel(struct cdev *dev);
void	dev_strategy(struct cdev *dev, struct buf *bp);
struct cdev *make_dev(struct cdevsw *_devsw, int _minor, uid_t _uid, gid_t _gid,
		int _perms, const char *_fmt, ...) __printflike(6, 7);
struct cdev *make_dev_alias(struct cdev *_pdev, const char *_fmt, ...) __printflike(2, 3);
int	dev2unit(struct cdev *_dev);
void	dev_lock(void);
void	dev_unlock(void);
int	unit2minor(int _unit);
u_int	minor2unit(u_int _minor);
void	setconf(void);

void devfs_create(struct cdev *dev);
void devfs_destroy(struct cdev *dev);

#define		UID_ROOT	0
#define		UID_BIN		3
#define		UID_UUCP	66

#define		GID_WHEEL	0
#define		GID_KMEM	2
#define		GID_OPERATOR	5
#define		GID_BIN		7
#define		GID_GAMES	13
#define		GID_DIALER	68

typedef void (*dev_clone_fn)(void *arg, char *name, int namelen, struct cdev **result);

int dev_stdclone(char *_name, char **_namep, const char *_stem, int *_unit);
EVENTHANDLER_DECLARE(dev_clone, dev_clone_fn);

/* Stuff relating to kernel-dump */

struct dumperinfo {
	dumper_t *dumper;	/* Dumping function. */
	void    *priv;		/* Private parts. */
	u_int   blocksize;	/* Size of block in bytes. */
	off_t   mediaoffset;	/* Initial offset in bytes. */
	off_t   mediasize;	/* Space available in bytes. */
};

int set_dumper(struct dumperinfo *);
void dumpsys(struct dumperinfo *);
extern int dumping;		/* system is dumping */

/* D_TTY related functions */
d_close_t	 ttyclose;
d_ioctl_t	 ttyioctl;
d_kqfilter_t	 ttykqfilter;
d_open_t	 ttyopen;
d_poll_t	 ttypoll;
d_read_t	 ttyread;
d_write_t	 ttywrite;

#endif /* _KERNEL */

#endif /* !_SYS_CONF_H_ */
