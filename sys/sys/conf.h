/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)conf.h	7.9 (Berkeley) 5/5/91
 *	$Id: conf.h,v 1.6 1994/05/04 08:30:22 rgrimes Exp $
 */

#ifndef _SYS_CONF_H_
#define _SYS_CONF_H_ 1

/*
 * Definitions of device driver entry switches
 */

#ifdef __STDC__
struct tty; struct buf;
struct uio;
#endif

typedef int d_open_t __P((int /*dev_t*/, int, int, struct proc *));
typedef int d_close_t __P((int /*dev_t*/, int, int, struct proc *));
typedef void d_strategy_t __P((struct buf *));
typedef int d_ioctl_t __P((int /*dev_t*/, int, caddr_t, int, struct proc *));
typedef int d_dump_t __P((int /*dev_t*/));
typedef int d_psize_t __P((int /*dev_t*/));

struct bdevsw {
	d_open_t *d_open;
	d_close_t *d_close;
	d_strategy_t *d_strategy;
	d_ioctl_t *d_ioctl;
	d_dump_t *d_dump;
	d_psize_t *d_psize;
	int	d_flags;
};

#ifdef KERNEL
extern struct bdevsw bdevsw[];
#endif

typedef int d_rdwr_t __P((int /*dev_t*/, struct uio *, int));
typedef int d_stop_t __P((struct tty *, int));
typedef int d_reset_t __P((int));
typedef int d_select_t __P((int /*dev_t*/, int, struct proc *));
typedef int d_mmap_t __P((/* XXX */));

struct cdevsw {
	d_open_t *d_open;
	d_close_t *d_close;
	d_rdwr_t *d_read;
	d_rdwr_t *d_write;
	d_ioctl_t *d_ioctl;
	d_stop_t *d_stop;
	d_reset_t *d_reset;
	struct	tty **d_ttys;
	d_select_t *d_select;
	d_mmap_t *d_mmap;
	d_strategy_t *d_strategy;
};

#ifdef KERNEL
extern struct cdevsw cdevsw[];

/* symbolic sleep message strings */
extern const char devopn[], devio[], devwait[], devin[], devout[];
extern const char devioc[], devcls[];
#endif

struct linesw {
	int	(*l_open)(int /*dev_t*/, struct tty *, int);
	void	(*l_close)(struct tty *, int);
	int	(*l_read)(struct tty *, struct uio *, int);
	int	(*l_write)(struct tty *, struct uio *, int);
	int	(*l_ioctl)(struct tty *, int, caddr_t, int);
	void	(*l_rint)(int, struct tty *);
	int	(*l_rend)();	/* XXX - to be deleted */
	int	(*l_meta)();	/* XXX - to be deleted */
	void	(*l_start)(struct tty *);
	int	(*l_modem)(struct tty *, int);
};

#ifdef KERNEL
extern struct linesw linesw[];
#endif

struct swdevt {
	dev_t	sw_dev;
	int	sw_freed;
	int	sw_nblks;
	struct	vnode *sw_vp;
};

#ifdef KERNEL
extern struct swdevt swdevt[];
#endif
#endif /* _SYS_CONF_H_ */
