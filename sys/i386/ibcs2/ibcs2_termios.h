/*	$NetBSD: ibcs2_termios.h,v 1.3 1994/10/26 02:53:07 cgd Exp $	*/

/*
 * Copyright (c) 1994 Scott Bartram
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
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#ifndef	_IBCS2_TERMIOS_H
#define	_IBCS2_TERMIOS_H	1

#include <i386/ibcs2/ibcs2_types.h>

#define IBCS2_NCC	8
#define IBCS2_NCCS	13

typedef u_short	ibcs2_tcflag_t;
typedef u_char	ibcs2_cc_t;
typedef u_long	ibcs2_speed_t;

struct ibcs2_termio {
	u_short	c_iflag;
	u_short	c_oflag;
	u_short	c_cflag;
	u_short	c_lflag;
	char	c_line;
	u_char	c_cc[IBCS2_NCC];
};

struct ibcs2_termios {
	ibcs2_tcflag_t	c_iflag;
	ibcs2_tcflag_t	c_oflag;
	ibcs2_tcflag_t	c_cflag;
	ibcs2_tcflag_t	c_lflag;
	char		c_line;
	ibcs2_cc_t	c_cc[IBCS2_NCCS];
	char		c_ispeed;
	char		c_ospeed;
};

#define IBCS2_VINTR		0
#define IBCS2_VQUIT		1
#define IBCS2_VERASE		2
#define IBCS2_VKILL		3
#define IBCS2_VEOF		4
#define IBCS2_VEOL		5
#define IBCS2_VEOL2		6
#define IBCS2_VMIN		4
#define IBCS2_VTIME		5
#define IBCS2_VSWTCH		7
#define IBCS2_VSUSP		10
#define IBCS2_VSTART		11
#define IBCS2_VSTOP		12

#define IBCS2_CNUL		0
#define IBCS2_CDEL		0377
#define IBCS2_CESC		'\\'
#define IBCS2_CINTR		0177
#define IBCS2_CQUIT		034
#define IBCS2_CERASE		'#'
#define IBCS2_CKILL		'@'
#define IBCS2_CSTART		021
#define IBCS2_CSTOP		023
#define IBCS2_CSWTCH		032
#define IBCS2_CNSWTCH		0
#define IBCS2_CSUSP		032

#define IBCS2_IGNBRK		0000001
#define IBCS2_BRKINT		0000002
#define IBCS2_IGNPAR		0000004
#define IBCS2_PARMRK		0000010
#define IBCS2_INPCK		0000020
#define IBCS2_ISTRIP		0000040
#define IBCS2_INLCR		0000100
#define IBCS2_IGNCR		0000200
#define IBCS2_ICRNL		0000400
#define IBCS2_IUCLC		0001000
#define IBCS2_IXON		0002000
#define IBCS2_IXANY		0004000
#define IBCS2_IXOFF		0010000
#define IBCS2_IMAXBEL		0020000
#define IBCS2_DOSMODE		0100000

#define IBCS2_OPOST		0000001
#define IBCS2_OLCUC		0000002
#define IBCS2_ONLCR		0000004
#define IBCS2_OCRNL		0000010
#define IBCS2_ONOCR		0000020
#define IBCS2_ONLRET		0000040
#define IBCS2_OFILL		0000100
#define IBCS2_OFDEL		0000200
#define IBCS2_NLDLY		0000400
#define IBCS2_NL0		0000000
#define IBCS2_NL1		0000400
#define IBCS2_CRDLY		0003000
#define IBCS2_CR0		0000000
#define IBCS2_CR1		0001000
#define IBCS2_CR2		0002000
#define IBCS2_CR3		0003000
#define IBCS2_TABDLY		0014000
#define IBCS2_TAB0		0000000
#define IBCS2_TAB1		0004000
#define IBCS2_TAB2		0010000
#define IBCS2_TAB3		0014000
#define IBCS2_BSDLY		0020000
#define IBCS2_BS0		0000000
#define IBCS2_BS1		0020000
#define IBCS2_VTDLY		0040000
#define IBCS2_VT0		0000000
#define IBCS2_VT1		0040000
#define IBCS2_FFDLY		0100000
#define IBCS2_FF0		0000000
#define IBCS2_FF1		0100000

#define IBCS2_CBAUD		0000017
#define IBCS2_CSIZE		0000060
#define IBCS2_CS5		0000000
#define IBCS2_CS6		0000020
#define IBCS2_CS7		0000040
#define IBCS2_CS8		0000060
#define IBCS2_CSTOPB		0000100
#define IBCS2_CREAD		0000200
#define IBCS2_PARENB		0000400
#define IBCS2_PARODD		0001000
#define IBCS2_HUPCL		0002000
#define IBCS2_CLOCAL		0004000
#define IBCS2_RCV1EN		0010000
#define IBCS2_XMT1EN		0020000
#define IBCS2_LOBLK		0040000
#define IBCS2_XCLUDE		0100000

#define IBCS2_ISIG		0000001
#define IBCS2_ICANON		0000002
#define IBCS2_XCASE		0000004
#define IBCS2_ECHO		0000010
#define IBCS2_ECHOE		0000020
#define IBCS2_ECHOK		0000040
#define IBCS2_ECHONL		0000100
#define IBCS2_NOFLSH		0000200
#define IBCS2_IEXTEN		0000400
#define IBCS2_TOSTOP		0001000

#define IBCS2_XIOC		(('i'<<24)|('X'<<16))
#define IBCS2_XCGETA  		(IBCS2_XIOC|1)
#define IBCS2_XCSETA  		(IBCS2_XIOC|2)
#define IBCS2_XCSETAW 		(IBCS2_XIOC|3)
#define IBCS2_XCSETAF 		(IBCS2_XIOC|4)

#define IBCS2_OXIOC		('x'<<8)
#define IBCS2_OXCGETA  		(IBCS2_OXIOC|1)
#define IBCS2_OXCSETA  		(IBCS2_OXIOC|2)
#define IBCS2_OXCSETAW 		(IBCS2_OXIOC|3)
#define IBCS2_OXCSETAF 		(IBCS2_OXIOC|4)

#define IBCS2_TIOC		('T'<<8)
#define IBCS2_TCGETA  		(IBCS2_TIOC|1)
#define IBCS2_TCSETA  		(IBCS2_TIOC|2)
#define IBCS2_TCSETAW 		(IBCS2_TIOC|3)
#define IBCS2_TCSETAF 		(IBCS2_TIOC|4)
#define IBCS2_TCSBRK  		(IBCS2_TIOC|5)
#define IBCS2_TCXONC  		(IBCS2_TIOC|6)
#define IBCS2_TCFLSH  		(IBCS2_TIOC|7)

#define IBCS2_TCGETSC		(IBCS2_TIOC|34)
#define IBCS2_TCSETSC		(IBCS2_TIOC|35)

#define IBCS2_TIOCSWINSZ	(IBCS2_TIOC|103)
#define IBCS2_TIOCGWINSZ	(IBCS2_TIOC|104)
#define IBCS2_TIOCSPGRP		(IBCS2_TIOC|118)
#define IBCS2_TIOCGPGRP		(IBCS2_TIOC|119)

#define IBCS2_TCSANOW		IBCS2_XCSETA
#define IBCS2_TCSADRAIN		IBCS2_XCSETAW
#define IBCS2_TCSAFLUSH		IBCS2_XCSETAF
#define IBCS2_TCSADFLUSH	IBCS2_XCSETAF

#define IBCS2_TCIFLUSH		0
#define IBCS2_TCOFLUSH		1
#define IBCS2_TCIOFLUSH		2

#define IBCS2_TCOOFF		0
#define IBCS2_TCOON		1
#define IBCS2_TCIOFF		2
#define IBCS2_TCION		3

#define IBCS2_B0		0
#define IBCS2_B50		1
#define IBCS2_B75		2
#define IBCS2_B110		3
#define IBCS2_B134		4
#define IBCS2_B150		5
#define IBCS2_B200		6
#define IBCS2_B300		7
#define IBCS2_B600		8
#define IBCS2_B1200		9
#define IBCS2_B1800		10
#define IBCS2_B2400		11
#define IBCS2_B4800		12
#define IBCS2_B9600		13
#define IBCS2_B19200		14
#define IBCS2_B38400		15

struct ibcs2_winsize {
        u_short ws_row;
        u_short ws_col;
        u_short ws_xpixel;
        u_short ws_ypixel;
};

#endif /* _IBCS2_H_ */

