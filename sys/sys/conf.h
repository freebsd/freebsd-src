/*-
 * Copyright (c) 1990, 1993
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
 * $Id: conf.h,v 1.40 1998/06/07 17:12:57 dfr Exp $
 */

#ifndef _SYS_CONF_H_
#define	_SYS_CONF_H_

/*
 * Definitions of device driver entry switches
 */

struct buf;
struct proc;
struct tty;
struct uio;
struct vnode;

typedef int d_open_t __P((dev_t dev, int oflags, int devtype, struct proc *p));
typedef int d_close_t __P((dev_t dev, int fflag, int devtype, struct proc *p));
typedef void d_strategy_t __P((struct buf *bp));
typedef int d_ioctl_t __P((dev_t dev, u_long cmd, caddr_t data,
			   int fflag, struct proc *p));
typedef int d_dump_t __P((dev_t dev));
typedef int d_psize_t __P((dev_t dev));

typedef int d_read_t __P((dev_t dev, struct uio *uio, int ioflag));
typedef int d_write_t __P((dev_t dev, struct uio *uio, int ioflag));
typedef void d_stop_t __P((struct tty *tp, int rw));
typedef int d_reset_t __P((dev_t dev));
typedef struct tty *d_devtotty_t __P((dev_t dev));
typedef int d_poll_t __P((dev_t dev, int events, struct proc *p));
typedef int d_mmap_t __P((dev_t dev, int offset, int nprot));

typedef int l_open_t __P((dev_t dev, struct tty *tp));
typedef int l_close_t __P((struct tty *tp, int flag));
typedef int l_read_t __P((struct tty *tp, struct uio *uio, int flag));
typedef int l_write_t __P((struct tty *tp, struct uio *uio, int flag));
typedef int l_ioctl_t __P((struct tty *tp, u_long cmd, caddr_t data,
			   int flag, struct proc *p));
typedef int l_rint_t __P((int c, struct tty *tp));
typedef int l_start_t __P((struct tty *tp));
typedef int l_modem_t __P((struct tty *tp, int flag));

/*
 * Types for d_type.
 */
#define	D_TAPE	1
#define	D_DISK	2
#define	D_TTY	3

#define	D_TYPEMASK	0xffff

/*
 * Flags for d_flags.
 */
#define	D_NOCLUSTERR	0x10000		/* disables cluter read */
#define	D_NOCLUSTERW	0x20000		/* disables cluster write */
#define	D_NOCLUSTERRW	(D_NOCLUSTERR | D_NOCLUSTERW)

/*
 * Block device switch table
 */
struct bdevsw {
	d_open_t	*d_open;
	d_close_t	*d_close;
	d_strategy_t	*d_strategy;
	d_ioctl_t	*d_ioctl;
	d_dump_t	*d_dump;
	d_psize_t	*d_psize;
	u_int		d_flags;
	char 		*d_name;	/* name of the driver e.g. audio */
	struct cdevsw	*d_cdev; 	/* cross pointer to the cdev */
	int		d_maj;		/* the major number we were assigned */
	int		d_maxio;
};

#ifdef KERNEL
extern struct bdevsw *bdevsw[];
#endif

/*
 * Character device switch table
 */
struct cdevsw {
	d_open_t	*d_open;
	d_close_t	*d_close;
	d_read_t	*d_read;
	d_write_t	*d_write;
	d_ioctl_t	*d_ioctl;
	d_stop_t	*d_stop;
	d_reset_t	*d_reset;	/* XXX not used */
	d_devtotty_t	*d_devtotty;
	d_poll_t	*d_poll;
	d_mmap_t	*d_mmap;
	d_strategy_t	*d_strategy;
	char		*d_name;	/* see above */
	struct bdevsw	*d_bdev;
	int		d_maj;
};

#ifdef KERNEL
extern struct cdevsw *cdevsw[];
#endif

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

#ifdef KERNEL
extern struct linesw linesw[];
extern int nlinesw;

int ldisc_register __P((int , struct linesw *));
void ldisc_deregister __P((int));
#define LDISC_LOAD 	-1		/* Loadable line discipline */
#endif

/*
 * Swap device table
 */
struct swdevt {
	dev_t	sw_dev;
	int	sw_flags;
	int	sw_nblks;
	struct	vnode *sw_vp;
};
#define	SW_FREED	0x01
#define	SW_SEQUENTIAL	0x02
#define	sw_freed	sw_flags	/* XXX compat */

#ifdef KERNEL
d_open_t	noopen;
d_close_t	noclose;
d_read_t	noread;
d_write_t	nowrite;
d_ioctl_t	noioctl;
d_stop_t	nostop;
d_reset_t	noreset;
d_devtotty_t	nodevtotty;
d_mmap_t	nommap;

/* Bogus defines for compatibility. */
#define	noioc		noioctl
#define	nostrat		nostrategy
#define zerosize	nopsize
/*
 * XXX d_strategy seems to be unused for cdevs that aren't associated with
 * bdevs and called without checking for it being non-NULL for bdevs.
 */
#define	nostrategy	((d_strategy_t *)NULL)

d_dump_t	nodump;

/*
 * nopsize is little used, so not worth having dummy functions for.
 */
#define	nopsize	((d_psize_t *)NULL)

d_open_t	nullopen;
d_close_t	nullclose;
#define	nullstop nostop		/* one void return is as good as another */
#define	nullreset noreset	/* one unused function is as good as another */

d_open_t	nxopen;
d_close_t	nxclose;
d_read_t	nxread;
d_write_t	nxwrite;
d_ioctl_t	nxioctl;
#define	nxstop	nostop		/* one void return is as good as another */
#define	nxreset	noreset		/* one unused function is as good as another */
#define	nxdevtotty nodevtotty	/* one NULL return is as good as another */
#define	nxmmap	nommap		/* one -1 return is as good as another */
#define	nxstrategy nostrategy	/* one NULL value is as good as another */
d_dump_t	nxdump;
#define	nxpsize	nopsize		/* one NULL value is as good as another */

d_read_t	rawread;
d_write_t	rawwrite;

l_read_t	l_noread;
l_write_t	l_nowrite;

/*
 * XXX This is ugly.
 */
#ifdef _SYS_MODULE_H_

struct cdevsw_module_data {
    modeventhand_t	chainevh; /* next event handler in chain */
    void*		chainarg; /* arg for next event handler */
    dev_t		dev;	/* device major to use */
    struct cdevsw*	cdevsw;	/* device functions */
};

struct bdevsw_module_data {
    modeventhand_t	chainevh; /* next event handler in chain */
    void*		chainarg; /* arg for next event handler */
    int			bdev;	/* device major to use */
    int			cdev;	/* device major to use */
    struct bdevsw*	bdevsw;	/* device functions */
};

#define CDEV_MODULE(name, major, devsw, evh, arg)			\
static struct cdevsw_module_data name##_cdevsw_mod = {			\
    evh, arg, makedev(major, 0), &devsw					\
};									\
									\
static moduledata_t name##_mod = {					\
    #name,								\
    cdevsw_module_handler,						\
    &name##_cdevsw_mod							\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+major)

#define BDEV_MODULE(name, bdev, cdev, devsw, evh, arg)			\
static struct bdevsw_module_data name##_bdevsw_mod = {			\
    evh, arg, makedev(bdev, 0), makedev(cdev, 0), &devsw		\
};									\
									\
static moduledata_t name##_mod = {					\
    #name,								\
    bdevsw_module_handler,						\
    &name##_bdevsw_mod							\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+cdev)

int	cdevsw_module_handler __P((module_t mod, modeventtype_t what, void* arg));
int	bdevsw_module_handler __P((module_t mod, modeventtype_t what, void* arg));

#endif /* _SYS_MODULE_H_ */

int	bdevsw_add __P((dev_t *descrip,struct bdevsw *new,struct bdevsw **old));
int	cdevsw_add __P((dev_t *descrip,struct cdevsw *new,struct cdevsw **old));
void	bdevsw_add_generic __P((int bdev, int cdev, struct bdevsw *bdevsw));
dev_t	chrtoblk __P((dev_t dev));
int	iskmemdev __P((dev_t dev));
int	iszerodev __P((dev_t dev));
void	setconf __P((void));
#endif /* KERNEL */

#endif /* !_SYS_CONF_H_ */
