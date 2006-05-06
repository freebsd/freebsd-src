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
 * $FreeBSD: src/sys/compat/svr4/svr4_termios.h,v 1.4 2005/01/05 22:34:37 imp Exp $
 */

#ifndef	_SVR4_TERMIOS_H_
#define	_SVR4_TERMIOS_H_

#define SVR4_POSIX_VDISABLE	0
#define	SVR4_NCC	 	8
#define	SVR4_NCCS		19

typedef u_long	svr4_tcflag_t;
typedef u_char	svr4_cc_t;
typedef u_long	svr4_speed_t;

struct svr4_termios {
	svr4_tcflag_t	c_iflag;
	svr4_tcflag_t	c_oflag;
	svr4_tcflag_t	c_cflag;
	svr4_tcflag_t	c_lflag;
	svr4_cc_t	c_cc[SVR4_NCCS];
};

struct svr4_termio {
	u_short		c_iflag;
	u_short		c_oflag;
	u_short		c_cflag;
	u_short		c_lflag;
	char		c_line;	
	u_char		c_cc[SVR4_NCC];
};

/* control characters */
#define	SVR4_VINTR	0
#define	SVR4_VQUIT	1
#define	SVR4_VERASE	2
#define	SVR4_VKILL	3
#define	SVR4_VEOF	4
#define	SVR4_VEOL	5
#define	SVR4_VEOL2	6
#define	SVR4_VMIN	4
#define	SVR4_VTIME	5
#define	SVR4_VSWTCH	7
#define	SVR4_VSTART	8
#define	SVR4_VSTOP	9
#define	SVR4_VSUSP	10
#define	SVR4_VDSUSP	11
#define	SVR4_VREPRINT	12
#define	SVR4_VDISCARD	13
#define	SVR4_VWERASE	14
#define	SVR4_VLNEXT	15

/* Input modes */
#define	SVR4_IGNBRK	00000001
#define	SVR4_BRKINT	00000002
#define	SVR4_IGNPAR	00000004
#define	SVR4_PARMRK	00000010
#define	SVR4_INPCK	00000020
#define	SVR4_ISTRIP	00000040
#define	SVR4_INLCR	00000100
#define	SVR4_IGNCR	00000200
#define	SVR4_ICRNL	00000400
#define	SVR4_IUCLC	00001000
#define	SVR4_IXON	00002000
#define	SVR4_IXANY	00004000
#define	SVR4_IXOFF	00010000
#define SVR4_IMAXBEL	00020000
#define SVR4_DOSMODE	00100000

/* Output modes */
#define	SVR4_OPOST	00000001
#define	SVR4_OLCUC	00000002
#define	SVR4_ONLCR	00000004
#define	SVR4_OCRNL	00000010
#define	SVR4_ONOCR	00000020
#define	SVR4_ONLRET	00000040
#define	SVR4_OFILL	00000100
#define	SVR4_OFDEL	00000200
#define	SVR4_NLDLY	00000400
#define	SVR4_NL0	00000000
#define	SVR4_NL1	00000400
#define	SVR4_CRDLY	00003000
#define	SVR4_CR0	00000000
#define	SVR4_CR1	00001000
#define	SVR4_CR2	00002000
#define	SVR4_CR3	00003000
#define	SVR4_TABDLY	00014000
#define	SVR4_TAB0	00000000
#define	SVR4_TAB1	00004000
#define	SVR4_TAB2	00010000
#define	SVR4_TAB3	00014000
#define SVR4_XTABS	00014000
#define	SVR4_BSDLY	00020000
#define	SVR4_BS0	00000000
#define	SVR4_BS1	00020000
#define	SVR4_VTDLY	00040000
#define	SVR4_VT0	00000000
#define	SVR4_VT1	00040000
#define	SVR4_FFDLY	00100000
#define	SVR4_FF0	00000000
#define	SVR4_FF1	00100000
#define SVR4_PAGEOUT	00200000
#define SVR4_WRAP	00400000

/* Control modes */
#define	SVR4_CBAUD	00000017
#define	SVR4_CSIZE	00000060
#define	SVR4_CS5	00000000
#define	SVR4_CS6	00000200
#define	SVR4_CS7	00000040
#define	SVR4_CS8	00000006
#define	SVR4_CSTOPB	00000100
#define	SVR4_CREAD	00000200
#define	SVR4_PARENB	00000400
#define	SVR4_PARODD	00001000
#define	SVR4_HUPCL	00002000
#define	SVR4_CLOCAL	00004000
#define SVR4_RCV1EN	00010000
#define	SVR4_XMT1EN	00020000
#define	SVR4_LOBLK	00040000
#define	SVR4_XCLUDE	00100000
#define SVR4_CIBAUD	03600000
#define SVR4_PAREXT	04000000

/* line discipline modes */
#define	SVR4_ISIG	00000001
#define	SVR4_ICANON	00000002
#define	SVR4_XCASE	00000004
#define	SVR4_ECHO	00000010
#define	SVR4_ECHOE	00000020
#define	SVR4_ECHOK	00000040
#define	SVR4_ECHONL	00000100
#define	SVR4_NOFLSH	00000200
#define	SVR4_TOSTOP	00000400
#define	SVR4_ECHOCTL	00001000
#define	SVR4_ECHOPRT	00002000
#define	SVR4_ECHOKE	00004000
#define	SVR4_DEFECHO	00010000
#define	SVR4_FLUSHO	00020000
#define	SVR4_PENDIN	00040000
#define	SVR4_IEXTEN	00100000

#define	SVR4_TIOC	('T' << 8)

#define	SVR4_TCGETA	(SVR4_TIOC| 1)
#define	SVR4_TCSETA	(SVR4_TIOC| 2)
#define	SVR4_TCSETAW	(SVR4_TIOC| 3)
#define	SVR4_TCSETAF	(SVR4_TIOC| 4)
#define	SVR4_TCSBRK	(SVR4_TIOC| 5)
#define	SVR4_TCXONC	(SVR4_TIOC| 6)
#define	SVR4_TCFLSH	(SVR4_TIOC| 7)
#define SVR4_TIOCKBON	(SVR4_TIOC| 8)
#define SVR4_TIOCKBOF 	(SVR4_TIOC| 9)
#define SVR4_KBENABLED 	(SVR4_TIOC|10)
#define SVR4_TCGETS	(SVR4_TIOC|13)
#define SVR4_TCSETS	(SVR4_TIOC|14)
#define SVR4_TCSETSW	(SVR4_TIOC|15)
#define	SVR4_TCSETSF	(SVR4_TIOC|16)
#define	SVR4_TCDSET	(SVR4_TIOC|32)
#define	SVR4_RTS_TOG	(SVR4_TIOC|33)
#define SVR4_TCGETSC	(SVR4_TIOC|34)
#define SVR4_TCSETSC	(SVR4_TIOC|35)
#define	SVR4_TCMOUSE	(SVR4_TIOC|36)
#define SVR4_TIOCGWINSZ	(SVR4_TIOC|104)
#define SVR4_TIOCSWINSZ	(SVR4_TIOC|103)

struct svr4_winsize {
	u_short	ws_row;
	u_short	ws_col;
	u_short	ws_xpixel;
	u_short	ws_ypixel;
};

#define	SVR4_B0		0
#define	SVR4_B50	1
#define	SVR4_B75	2
#define	SVR4_B110	3
#define	SVR4_B134	4
#define	SVR4_B150	5
#define	SVR4_B200	6
#define	SVR4_B300	7
#define	SVR4_B600	8
#define	SVR4_B1200	9
#define	SVR4_B1800	10
#define	SVR4_B2400	11
#define	SVR4_B4800	12
#define	SVR4_B9600	13
#define	SVR4_B19200	14
#define	SVR4_B38400	15
#define	SVR4_B57600	16
#define	SVR4_B76800	17
#define	SVR4_B115200	18
#define	SVR4_B153600	19
#define	SVR4_B230400	20
#define	SVR4_B307200	21
#define	SVR4_B460800	22

#endif /* !_SVR4_TERMIOS_H_ */
