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
 * $Id: conf.h,v 1.59 1999/06/01 18:56:26 phk Exp $
 */

#ifndef _SYS_CONF_H_
#define	_SYS_CONF_H_

/*
 * Definitions of device driver entry switches
 */

struct buf;
struct proc;
struct specinfo;
struct tty;
struct uio;
struct vnode;

typedef int d_open_t __P((dev_t dev, int oflags, int devtype, struct proc *p));
typedef int d_close_t __P((dev_t dev, int fflag, int devtype, struct proc *p));
typedef void d_strategy_t __P((struct buf *bp));
typedef int d_parms_t __P((dev_t dev, struct specinfo *sinfo, int ctl));
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
typedef int d_mmap_t __P((dev_t dev, vm_offset_t offset, int nprot));

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
#define	D_CANFREE	0x40000		/* can free blocks */

/*
 * Control type for d_parms() call.
 */
#define DPARM_GET	0		/* ask device to load parms in  */

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
	d_reset_t	*d_bogoreset;	/* XXX not used */
	d_devtotty_t	*d_devtotty;
	d_poll_t	*d_poll;
	d_mmap_t	*d_mmap;
	d_strategy_t	*d_strategy;
	char		*d_name;	/* base device name, e.g. 'vn' */
	d_parms_t	*d_parms;	/* populate/override specinfo */
	int		d_maj;
	d_dump_t	*d_dump;
	d_psize_t	*d_psize;
	u_int		d_flags;
	int		d_maxio;
	int		d_bmaj;
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
#define	nostrategy	((d_strategy_t *)NULL)
#define	noparms	((d_parms_t *)NULL)
#define	nopoll	seltrue

d_dump_t	nodump;

#define NUMCDEVSW 256

/*
 * nopsize is little used, so not worth having dummy functions for.
 */
#define	nopsize	((d_psize_t *)NULL)

d_open_t	nullopen;
d_close_t	nullclose;

l_read_t	l_noread;
l_write_t	l_nowrite;

struct module;

struct devsw_module_data {
	int	(*chainevh)(struct module *, int, void *); /* next handler */
	void	*chainarg;	/* arg for next event handler */
	int	bmaj;		/* device major to use */
	int	cmaj;		/* device major to use */
	struct	cdevsw *cdevsw;	/* device functions */
	/* Do not initialize fields hereafter */
	dev_t	bdev;
	dev_t	cdev;
};

#define DEV_MODULE(name, cmaj, bmaj, devsw, evh, arg)			\
static struct devsw_module_data name##_devsw_mod = {			\
    evh, arg, bmaj, cmaj, &devsw					\
};									\
									\
static moduledata_t name##_mod = {					\
    #name,								\
    devsw_module_handler,						\
    &name##_devsw_mod							\
};									\
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE+cmaj*256+bmaj)


struct cdevsw *bdevsw __P((dev_t dev));
int	cdevsw_add __P((struct cdevsw *new));
int	cdevsw_remove __P((struct cdevsw *old));
dev_t	chrtoblk __P((dev_t dev));
struct cdevsw *devsw __P((dev_t dev));
int	devsw_module_handler __P((struct module *mod, int what, void *arg));
int	iskmemdev __P((dev_t dev));
int	iszerodev __P((dev_t dev));
dev_t	makebdev __P((int maj, int min));
void	setconf __P((void));
#endif /* KERNEL */

#endif /* !_SYS_CONF_H_ */
