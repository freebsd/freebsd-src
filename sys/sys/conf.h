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
 *	@(#)conf.h	8.5 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _SYS_CONF_H_
#define	_SYS_CONF_H_

#ifdef _KERNEL
#include <sys/eventhandler.h>

struct tty;
struct disk;
struct vnode;
struct buf;
TAILQ_HEAD(snaphead, inode);

struct cdev {
	u_int		si_flags;
#define SI_STASHED	0x0001	/* created in stashed storage */
#define SI_ALIAS	0x0002	/* carrier of alias name */
#define SI_NAMED	0x0004	/* make_dev{_alias} has been called */
#define SI_CHEAPCLONE	0x0008	/* can be removed_dev'ed when vnode reclaims */
#define SI_CHILD	0x0010	/* child of another dev_t */
#define SI_DEVOPEN	0x0020	/* opened by device */
#define SI_CONSOPEN	0x0040	/* opened by console */
#define SI_DUMPDEV	0x0080	/* is kernel dumpdev */
#define SI_CANDELETE	0x0100	/* can do BIO_DELETE */
	struct timespec	si_atime;
	struct timespec	si_ctime;
	struct timespec	si_mtime;
	udev_t		si_udev;
	LIST_ENTRY(cdev)	si_hash;
	SLIST_HEAD(, vnode)	si_hlist;
	LIST_HEAD(, cdev)	si_children;
	LIST_ENTRY(cdev)	si_siblings;
	dev_t		si_parent;
	u_int		si_inode;
	char		*si_name;
	void		*si_drv1, *si_drv2;
	struct cdevsw	*si_devsw;
	int		si_iosize_max;	/* maximum I/O size (for physio &al) */
	uid_t		si_uid;
	gid_t		si_gid;
	mode_t		si_mode;
	u_long		si_usecount;
	union {
		struct {
			struct tty *__sit_tty;
		} __si_tty;
		struct {
			struct disk *__sid_disk;
			struct mount *__sid_mountpoint;
			int __sid_bsize_phys; /* min physical block size */
			int __sid_bsize_best; /* optimal block size */
			struct snaphead	__sid_snapshots;
			daddr_t __sid_snaplistsize; /* size of snapblklist. */
			daddr_t	*__sid_snapblklist; /* known snapshot blocks. */
			int (*__sid_copyonwrite)(struct vnode *, struct buf *);
		} __si_disk;
	} __si_u;
	char		__si_namebuf[SPECNAMELEN + 1];
};

#define si_tty		__si_u.__si_tty.__sit_tty
#define si_disk		__si_u.__si_disk.__sid_disk
#define si_mountpoint	__si_u.__si_disk.__sid_mountpoint
#define si_bsize_phys	__si_u.__si_disk.__sid_bsize_phys
#define si_bsize_best	__si_u.__si_disk.__sid_bsize_best
#define si_snapshots	__si_u.__si_disk.__sid_snapshots
#define si_snaplistsize	__si_u.__si_disk.__sid_snaplistsize
#define si_snapblklist	__si_u.__si_disk.__sid_snapblklist
#define si_copyonwrite	__si_u.__si_disk.__sid_copyonwrite

/*
 * Special device management
 */
#define	SPECHSZ	64
#define	SPECHASH(rdev)	(((unsigned)(minor(rdev)))%SPECHSZ)

/*
 * Definitions of device driver entry switches
 */

struct bio;
struct buf;
struct thread;
struct uio;
struct knote;

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

typedef int d_open_t(dev_t dev, int oflags, int devtype, struct thread *td);
typedef int d_close_t(dev_t dev, int fflag, int devtype, struct thread *td);
typedef void d_strategy_t(struct bio *bp);
typedef int d_ioctl_t(dev_t dev, u_long cmd, caddr_t data,
		      int fflag, struct thread *td);
typedef int d_dump_t(dev_t dev,void *virtual, vm_offset_t physical, off_t offset, size_t length);
typedef int d_psize_t(dev_t dev);

typedef int d_read_t(dev_t dev, struct uio *uio, int ioflag);
typedef int d_write_t(dev_t dev, struct uio *uio, int ioflag);
typedef int d_poll_t(dev_t dev, int events, struct thread *td);
typedef int d_kqfilter_t(dev_t dev, struct knote *kn);
typedef int d_mmap_t(dev_t dev, vm_offset_t offset, int nprot);

typedef int l_open_t(dev_t dev, struct tty *tp);
typedef int l_close_t(struct tty *tp, int flag);
typedef int l_read_t(struct tty *tp, struct uio *uio, int flag);
typedef int l_write_t(struct tty *tp, struct uio *uio, int flag);
typedef int l_ioctl_t(struct tty *tp, u_long cmd, caddr_t data,
		      int flag, struct thread *td);
typedef int l_rint_t(int c, struct tty *tp);
typedef int l_start_t(struct tty *tp);
typedef int l_modem_t(struct tty *tp, int flag);

#define BIO_STRATEGY(bp)						\
	do {								\
	if ((!(bp)->bio_cmd) || ((bp)->bio_cmd & ((bp)->bio_cmd - 1)))	\
		Debugger("bio_cmd botch");				\
	(*devsw((bp)->bio_dev)->d_strategy)(bp);			\
	} while (0)

#define DEV_STRATEGY(bp)						\
	do {								\
	if ((bp)->b_flags & B_PHYS)					\
		(bp)->b_io.bio_offset = (bp)->b_offset;			\
	else								\
		(bp)->b_io.bio_offset = dbtob((bp)->b_blkno);		\
	(bp)->b_io.bio_done = bufdonebio;				\
	(bp)->b_io.bio_caller2 = (bp);					\
	BIO_STRATEGY(&(bp)->b_io);					\
	} while (0)

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
 * Flags for d_flags.
 */
#define	D_MEMDISK	0x00010000	/* memory type disk */
#define	D_NAGGED	0x00020000	/* nagged about missing make_dev() */
#define	D_CANFREE	0x00040000	/* can free blocks */
#define	D_TRACKCLOSE	0x00080000	/* track all closes */
#define D_MMAP_ANON	0x00100000	/* special treatment in vm_mmap.c */
#define D_KQFILTER	0x00200000	/* has kqfilter entry */
#define D_NOGIANT	0x00400000	/* Doesn't want Giant */

/*
 * Character device switch table
 */
struct cdevsw {
	d_open_t	*d_open;
	d_close_t	*d_close;
	d_read_t	*d_read;
	d_write_t	*d_write;
	d_ioctl_t	*d_ioctl;
	d_poll_t	*d_poll;
	d_mmap_t	*d_mmap;
	d_strategy_t	*d_strategy;
	const char	*d_name;	/* base device name, e.g. 'vn' */
	int		d_maj;
	d_dump_t	*d_dump;
	d_psize_t	*d_psize;
	u_int		d_flags;
	/* additions below are not binary compatible with 4.2 and below */
	d_kqfilter_t	*d_kqfilter;
};

/*
 * Line discipline switch table
 */
struct linesw {
	l_open_t	*l_open;
	l_close_t	*l_close;
	l_read_t	*l_read;
	l_write_t	*l_write;
	l_ioctl_t	*l_ioctl;
	l_rint_t	*l_rint;
	l_start_t	*l_start;
	l_modem_t	*l_modem;
	u_char		l_hotchar;
};

extern struct linesw linesw[];
extern int nlinesw;

int ldisc_register(int , struct linesw *);
void ldisc_deregister(int);
#define LDISC_LOAD 	-1		/* Loadable line discipline */
#endif /* _KERNEL */

#ifdef _KERNEL
d_open_t	noopen;
d_close_t	noclose;
d_read_t	noread;
d_write_t	nowrite;
d_ioctl_t	noioctl;
d_mmap_t	nommap;
d_kqfilter_t	nokqfilter;
#define	nostrategy	((d_strategy_t *)NULL)
#define	nopoll	seltrue

d_dump_t	nodump;

#define NUMCDEVSW 256

/*
 * nopsize is little used, so not worth having dummy functions for.
 */
#define	nopsize	((d_psize_t *)NULL)

d_open_t	nullopen;
d_close_t	nullclose;

l_ioctl_t	l_nullioctl;
l_read_t	l_noread;
l_write_t	l_nowrite;

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


int	cdevsw_add(struct cdevsw *_new);
int	cdevsw_remove(struct cdevsw *_old);
int	count_dev(dev_t _dev);
void	destroy_dev(dev_t _dev);
void	revoke_and_destroy_dev(dev_t _dev);
struct cdevsw *devsw(dev_t _dev);
const char *devtoname(dev_t _dev);
int	dev_named(dev_t _pdev, const char *_name);
void	dev_depends(dev_t _pdev, dev_t _cdev);
void	freedev(dev_t _dev);
dev_t	makebdev(int _maj, int _min);
dev_t	make_dev(struct cdevsw *_devsw, int _minor, uid_t _uid, gid_t _gid,
		int _perms, const char *_fmt, ...) __printflike(6, 7);
dev_t	make_dev_alias(dev_t _pdev, const char *_fmt, ...) __printflike(2, 3);
int	dev2unit(dev_t _dev);
int	unit2minor(int _unit);
void	setconf(void);
dev_t	getdiskbyname(char *_name);

/* This is type of the function DEVFS uses to hook into the kernel with */
typedef void devfs_create_t(dev_t dev);
typedef void devfs_destroy_t(dev_t dev);

extern devfs_create_t *devfs_create_hook;
extern devfs_destroy_t *devfs_destroy_hook;

#define		UID_ROOT	0
#define		UID_BIN		3
#define		UID_UUCP	66

#define		GID_WHEEL	0
#define		GID_KMEM	2
#define		GID_OPERATOR	5
#define		GID_BIN		7
#define		GID_GAMES	13
#define		GID_DIALER	68

typedef void (*dev_clone_fn)(void *arg, char *name, int namelen, dev_t *result);

int dev_stdclone(char *_name, char **_namep, const char *_stem, int *_unit);
EVENTHANDLER_DECLARE(dev_clone, dev_clone_fn);

/* Stuff relating to kernel-dump */

typedef int dumper_t(
	void *priv,		/* Private to the driver. */
	void *virtual,		/* Virtual (mapped) address. */
	vm_offset_t physical,	/* Physical address of virtual. */
	off_t offset,		/* Byte-offset to write at. */
	size_t length);		/* Number of bytes to dump. */

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

#endif /* _KERNEL */

#endif /* !_SYS_CONF_H_ */
