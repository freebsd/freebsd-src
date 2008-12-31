/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/compat/svr4/svr4_ttold.h,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_SVR4_TTOLD_H_
#define	_SVR4_TTOLD_H_

struct svr4_tchars {
	char	t_intrc;
	char	t_quitc;
	char	t_startc;
	char	t_stopc;
	char	t_eofc;
	char	t_brkc;
};

struct	svr4_sgttyb {
	u_char	sg_ispeed;
	u_char	sg_ospeed;
	u_char	sg_erase;	
	u_char	sg_kill;
	int	sg_flags;
};

struct svr4_ltchars {
	char	t_suspc;
	char	t_dsuspc;
	char	t_rprntc;
	char	t_flushc;
	char	t_werasc;
	char	t_lnextc;
};

#ifndef SVR4_tIOC
#define	SVR4_tIOC	('t' << 8)
#endif

#define	SVR4_TIOCGETD	(SVR4_tIOC |   0)
#define	SVR4_TIOCSETD	(SVR4_tIOC |   1)
#define	SVR4_TIOCHPCL	(SVR4_tIOC |   2)
#define	SVR4_TIOCGETP	(SVR4_tIOC |   8)
#define	SVR4_TIOCSETP  	(SVR4_tIOC |   9)
#define	SVR4_TIOCSETN	(SVR4_tIOC |  10)
#define	SVR4_TIOCEXCL	(SVR4_tIOC |  13)
#define	SVR4_TIOCNXCL	(SVR4_tIOC |  14)
#define	SVR4_TIOCFLUSH	(SVR4_tIOC |  16)
#define	SVR4_TIOCSETC	(SVR4_tIOC |  17)
#define	SVR4_TIOCGETC	(SVR4_tIOC |  18)
#define	SVR4_TIOCGPGRP	(SVR4_tIOC |  20)
#define	SVR4_TIOCSPGRP	(SVR4_tIOC |  21)
#define	SVR4_TIOCGSID	(SVR4_tIOC |  22)
#define	SVR4_TIOCSTI	(SVR4_tIOC |  23)
#define	SVR4_TIOCSSID	(SVR4_tIOC |  24)
#define	SVR4_TIOCMSET	(SVR4_tIOC |  26)
#define	SVR4_TIOCMBIS	(SVR4_tIOC |  27)
#define	SVR4_TIOCMBIC	(SVR4_tIOC |  28)
#define	SVR4_TIOCMGET	(SVR4_tIOC |  29)
#define	SVR4_TIOCREMOTE	(SVR4_tIOC |  30)
#define SVR4_TIOCSIGNAL	(SVR4_tIOC |  31)

#define	SVR4_TIOCSTART	(SVR4_tIOC | 110)
#define	SVR4_TIOCSTOP	(SVR4_tIOC | 111)
#define	SVR4_TIOCNOTTY	(SVR4_tIOC | 113)
#define	SVR4_TIOCOUTQ	(SVR4_tIOC | 115) 
#define	SVR4_TIOCGLTC	(SVR4_tIOC | 116)
#define	SVR4_TIOCSLTC	(SVR4_tIOC | 117)
#define	SVR4_TIOCCDTR	(SVR4_tIOC | 120)
#define	SVR4_TIOCSDTR	(SVR4_tIOC | 121)
#define	SVR4_TIOCCBRK	(SVR4_tIOC | 122)
#define	SVR4_TIOCSBRK	(SVR4_tIOC | 123)
#define	SVR4_TIOCLGET	(SVR4_tIOC | 124)
#define	SVR4_TIOCLSET	(SVR4_tIOC | 125)
#define	SVR4_TIOCLBIC	(SVR4_tIOC | 126)
#define	SVR4_TIOCLBIS	(SVR4_tIOC | 127)

#define	SVR4_TIOCM_LE	0001
#define	SVR4_TIOCM_DTR	0002
#define	SVR4_TIOCM_RTS	0004
#define	SVR4_TIOCM_ST	0010
#define	SVR4_TIOCM_SR	0020
#define	SVR4_TIOCM_CTS	0040
#define	SVR4_TIOCM_CAR	0100
#define	SVR4_TIOCM_CD	SVR4_TIOCM_CAR
#define	SVR4_TIOCM_RNG	0200
#define	SVR4_TIOCM_RI	SVR4_TIOCM_RNG
#define	SVR4_TIOCM_DSR	0400

#define	SVR4_OTTYDISC	0
#define	SVR4_NETLDISC	1
#define	SVR4_NTTYDISC	2
#define	SVR4_TABLDISC	3
#define	SVR4_NTABLDISC	4
#define	SVR4_MOUSELDISC	5
#define	SVR4_KBDLDISC	6

#endif /* !_SVR4_TTOLD_H_ */
