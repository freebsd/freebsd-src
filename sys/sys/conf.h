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
 *	@(#)conf.h	8.3 (Berkeley) 1/21/94
 * $Id: conf.h,v 1.17 1995/09/10 21:36:12 bde Exp $
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

typedef void d_strategy_t __P((struct buf *));
typedef int d_open_t __P((dev_t, int, int, struct proc *));
typedef int d_close_t __P((dev_t, int, int, struct proc *));
typedef int d_ioctl_t __P((dev_t, int, caddr_t, int, struct proc *));
typedef int d_dump_t __P((dev_t));
typedef int d_psize_t __P((dev_t));

typedef int d_read_t __P((dev_t, struct uio *, int));
typedef int d_write_t __P((dev_t, struct uio *, int));
typedef int d_rdwr_t __P((dev_t, struct uio *, int));
typedef void d_stop_t __P((struct tty *, int));
typedef int d_reset_t __P((int));
typedef int d_select_t __P((dev_t, int, struct proc *));
typedef int d_mmap_t __P((dev_t, int, int));
typedef	struct tty * d_ttycv_t __P((dev_t));

struct bdevsw {
	d_open_t	*d_open;
	d_close_t	*d_close;
	d_strategy_t	*d_strategy;
	d_ioctl_t	*d_ioctl;
	d_dump_t	*d_dump;
	d_psize_t	*d_psize;
	int		d_flags;
};

#ifdef KERNEL
extern struct bdevsw bdevsw[];
#endif

struct cdevsw {
	d_open_t	*d_open;
	d_close_t	*d_close;
	d_rdwr_t	*d_read;
	d_rdwr_t	*d_write;
	d_ioctl_t	*d_ioctl;
	d_stop_t	*d_stop;
	d_reset_t	*d_reset;
	d_ttycv_t	*d_devtotty;
	d_select_t	*d_select;
	d_mmap_t	*d_mmap;
	d_strategy_t	*d_strategy;
};

#ifdef KERNEL
extern struct cdevsw cdevsw[];

/* symbolic sleep message strings */
extern char devopn[], devio[], devwait[], devin[], devout[];
extern char devioc[], devcls[];
#endif

struct linesw {
	int	(*l_open)	__P((dev_t dev, struct tty *tp));
	int	(*l_close)	__P((struct tty *tp, int flag));
	int	(*l_read)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_write)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_ioctl)	__P((struct tty *tp, int cmd, caddr_t data,
				     int flag, struct proc *p));
	int	(*l_rint)	__P((int c, struct tty *tp));
	int	(*l_start)	__P((struct tty *tp));
	int	(*l_modem)	__P((struct tty *tp, int flag));
};

#ifdef KERNEL
extern struct linesw linesw[];
extern int nlinesw;

int ldisc_register __P((int , struct linesw *));
void ldisc_deregister __P((int));
#define LDISC_LOAD 	-1		/* Loadable line discipline */
#endif

struct swdevt {
	dev_t	sw_dev;
	int	sw_flags;
	int	sw_nblks;
	struct	vnode *sw_vp;
};
#define	SW_FREED	0x01
#define	SW_SEQUENTIAL	0x02
#define sw_freed	sw_flags	/* XXX compat */

#ifdef KERNEL
d_reset_t	noreset;
d_mmap_t	nommap;
d_strategy_t	nostrategy;

d_open_t	nullopen;
d_close_t	nullclose;
d_stop_t	nullstop;
d_reset_t	nullreset;
/*
 * XXX d_strategy seems to be unused for cdevs and called without checking
 * for it being non-NULL for bdevs.
 */
#define	nullstrategy	((d_strategy *)NULL)

dev_t	chrtoblk __P((dev_t dev));
int	getmajorbyname __P((const char *name));
int	isdisk __P((dev_t dev, int type));
int	iskmemdev __P((dev_t dev));
int	iszerodev __P((dev_t dev));
int	register_cdev __P((const char *name, const struct cdevsw *cdp));
int	setdumpdev __P((dev_t));
int	unregister_cdev __P((const char *name, const struct cdevsw *cdp));
#ifdef JREMOD
int	cdevsw_add __P((dev_t *descrip,struct cdevsw *new,struct cdevsw *old));
int	bdevsw_add __P((dev_t *descrip,struct bdevsw *new,struct bdevsw *old));
#endif
#endif

#endif /* !_SYS_CONF_H_ */
