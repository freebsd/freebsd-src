/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	from: @(#)if_slvar.h	7.7 (Berkeley) 5/7/91
 *	$Id: if_slvar.h,v 1.2 1993/10/16 17:43:22 rgrimes Exp $
 */

/*
 * Definitions for SLIP interface data structures
 * 
 * (This exists so programs like slstats can get at the definition
 *  of sl_softc.)
 */
struct sl_softc {
	struct	ifnet sc_if;		/* network-visible interface */
	struct	ifqueue sc_fastq;	/* interactive output queue */
	struct	tty *sc_ttyp;		/* pointer to tty structure */
	u_char	*sc_mp;			/* pointer to next available buf char */
	u_char	*sc_ep;			/* pointer to last available buf char */
	u_char	*sc_buf;		/* input buffer */
	u_int	sc_flags;		/* see below */
	u_int	sc_escape;	/* =1 if last char input was FRAME_ESCAPE */
	u_int	sc_bytessent;
	u_int	sc_bytesrcvd;
	long	sc_lasttime;		/* last time a char arrived */
	long	sc_starttime;		/* last time a char arrived */
	long	sc_abortcount;		/* number of abort esacpe chars */
#ifdef INET				/* XXX */
	struct	slcompress sc_comp;	/* tcp compression data */
#endif
	caddr_t	sc_bpf;
};

/* visible flags */
#define	SC_COMPRESS	0x0002		/* compress TCP traffic */
#define	SC_NOICMP	0x0004		/* supress ICMP traffic */
#define	SC_AUTOCOMP	0x0008		/* auto-enable TCP compression */
/* internal flags (should be separate) */
#define	SC_ERROR	0x08000		/* had an input error - 30 Aug 92*/
#define	SC_ABORT	0x10000		/* have been sent an abort request */

/* this stuff doesn't belong here... */
#define	SLIOCGFLAGS	_IOR('t', 90, int)	/* get configuration flags */
#define	SLIOCSFLAGS	_IOW('t', 89, int)	/* set configuration flags */
#define	SLIOCGUNIT	_IOR('t', 88, int)	/* get slip unit number */
