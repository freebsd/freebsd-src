/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	from: @(#)tty.h	7.10 (Berkeley) 6/26/91
 *	$Id: tty.h,v 1.4 1993/10/16 17:18:06 rgrimes Exp $
 */

#ifndef _SYS_TTY_H_
#define _SYS_TTY_H_

#include <sys/termios.h>

/*
 * Ring buffers provide a contiguous, dense storage for
 * character data used by the tty driver.
 */
#define	RBSZ 1024

struct ringb {
	char	*rb_hd;	  /* head of buffer segment to be read */
	char	*rb_tl;	  /* tail of buffer segment to be written */
	char	rb_buf[RBSZ];	/* segment contents */
};

#define	RB_SUCC(rbp, p) \
		((p) >= (rbp)->rb_buf + RBSZ - 1 ? (rbp)->rb_buf : (p) + 1)

#define	RB_ROLLOVER(rbp, p) \
		((p) > (rbp)->rb_buf + RBSZ - 1 ? (rbp)->rb_buf : (p))

#define	RB_PRED(rbp, p) \
		((p) <= (rbp)->rb_buf ? (rbp)->rb_buf + RBSZ - 1 : (p) - 1)

#define	RB_LEN(rp) \
		((rp)->rb_hd <= (rp)->rb_tl ? (rp)->rb_tl - (rp)->rb_hd : \
		RBSZ - ((rp)->rb_hd - (rp)->rb_tl))

#define	RB_CONTIGPUT(rp) \
		(RB_PRED(rp, (rp)->rb_hd) < (rp)->rb_tl ?  \
			(rp)->rb_buf + RBSZ - (rp)->rb_tl : \
			RB_PRED(rp, (rp)->rb_hd) - (rp)->rb_tl)

#define	RB_CONTIGGET(rp) \
		((rp)->rb_hd <= (rp)->rb_tl ? (rp)->rb_tl - (rp)->rb_hd : \
		(rp)->rb_buf + RBSZ - (rp)->rb_hd)

/*
 * Per-tty structure.
 *
 * Should be split in two, into device and tty drivers.
 * Glue could be masks of what to echo and circular buffer
 * (low, high, timeout).
 */
struct tty {
	int	(*t_oproc)();		/* device */
	int	(*t_param)();		/* device */
	pid_t	t_rsel;			/* tty */
	pid_t	t_wsel;
	caddr_t	T_LINEP; 		/* XXX */
	caddr_t	t_addr;			/* ??? */
	dev_t	t_dev;			/* device */
	int	t_flags;		/* (compat) some of both */
	int	t_state;		/* some of both */
	struct	session *t_session;	/* tty */
	struct	pgrp *t_pgrp;		/* foreground process group */
	char	t_line;			/* glue */
	short	t_col;			/* tty */
	short	t_rocount, t_rocol;	/* tty */
	short	t_hiwat;		/* hi water mark */
	short	t_lowat;		/* low water mark */
	struct	winsize t_winsize;	/* window size */
	struct	termios t_termios;	/* termios state */
#define	t_iflag		t_termios.c_iflag
#define	t_oflag		t_termios.c_oflag
#define	t_cflag		t_termios.c_cflag
#define	t_lflag		t_termios.c_lflag
#define	t_min		t_termios.c_min
#define	t_time		t_termios.c_time
#define	t_cc		t_termios.c_cc
#define t_ispeed	t_termios.c_ispeed
#define t_ospeed	t_termios.c_ospeed
	long	t_cancc;		/* stats */
	long	t_rawcc;
	long	t_outcc;
	short	t_gen;			/* generation number */
	short	t_mask;			/* interrupt mask */
	struct	ringb t_raw;		/* ring buffers */
	struct	ringb t_can;
	struct	ringb t_out;
};

#define	TTIPRI	25			/* sleep priority for tty reads */
#define	TTOPRI	26			/* sleep priority for tty writes */

#define	TTMASK	15
#define	OBUFSIZ	100
#define	TTYHOG	1024

#ifdef KERNEL
#define TTMAXHIWAT	(RBSZ/2)	/* XXX */
#define TTMINHIWAT	128
#define TTMAXLOWAT	256
#define TTMINLOWAT	32
extern	struct ttychars ttydefaults;
#endif /* KERNEL */

/* internal state bits */
#define	TS_TIMEOUT	0x000001	/* delay timeout in progress */
#define	TS_WOPEN	0x000002	/* waiting for open to complete */
#define	TS_ISOPEN	0x000004	/* device is open */
#define	TS_FLUSH	0x000008	/* outq has been flushed during DMA */
#define	TS_CARR_ON	0x000010	/* software copy of carrier-present */
#define	TS_BUSY		0x000020	/* output in progress */
#define	TS_ASLEEP	0x000040	/* wakeup when output done */
#define	TS_XCLUDE	0x000080	/* exclusive-use flag against open */
#define	TS_TTSTOP	0x000100	/* output stopped by ctl-s */
/* was	TS_HUPCLS	0x000200 	 * hang up upon last close */
#define	TS_TBLOCK	0x000400	/* tandem queue blocked */
#define	TS_RCOLL	0x000800	/* collision in read select */
#define	TS_WCOLL	0x001000	/* collision in write select */
#define	TS_ASYNC	0x004000	/* tty in async i/o mode */
/* state for intra-line fancy editing work */
#define	TS_BKSL		0x010000	/* state for lowercase \ work */
#define	TS_ERASE	0x040000	/* within a \.../ for PRTRUB */
#define	TS_LNCH		0x080000	/* next character is literal */
#define	TS_TYPEN	0x100000	/* retyping suspended input (PENDIN) */
#define	TS_CNTTB	0x200000	/* counting tab width, ignore FLUSHO */

#define	TS_LOCAL	(TS_BKSL|TS_ERASE|TS_LNCH|TS_TYPEN|TS_CNTTB)

/* define partab character types */
#define	ORDINARY	0
#define	CONTROL		1
#define	BACKSPACE	2
#define	NEWLINE		3
#define	TAB		4
#define	VTAB		5
#define	RETURN		6

struct speedtab {
        int sp_speed;
        int sp_code;
};
/*
 * Flags on character passed to ttyinput
 */
#define TTY_CHARMASK    0x000000ff      /* Character mask */
#define TTY_QUOTE       0x00000100      /* Character quoted */
#define TTY_ERRORMASK   0xff000000      /* Error mask */
#define TTY_FE          0x01000000      /* Framing error or BREAK condition */
#define TTY_PE          0x02000000      /* Parity error */

/*
 * Is tp controlling terminal for p
 */
#define isctty(p, tp)	((p)->p_session == (tp)->t_session && \
			 (p)->p_flag&SCTTY)
/*
 * Is p in background of tp
 */
#define isbackground(p, tp)	(isctty((p), (tp)) && \
				(p)->p_pgrp != (tp)->t_pgrp)
/*
 * Modem control commands (driver).
 */
#define	DMSET		0
#define	DMBIS		1
#define	DMBIC		2
#define	DMGET		3

#ifdef KERNEL
/* symbolic sleep message strings */
extern	 char ttyin[], ttyout[], ttopen[], ttclos[], ttybg[], ttybuf[];
#endif

#endif	/* _SYS_TTY_H_ */
