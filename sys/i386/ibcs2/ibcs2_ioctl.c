/*-
 * Copyright (c) 1994 Søren Schmidt
 * Copyright (c) 1994 Sean Eric Fagan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *	$Id: ibcs2_ioctl.c,v 1.3 1994/10/23 19:19:42 sos Exp $
 */

#include <i386/ibcs2/ibcs2.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/termios.h>
#include <machine/console.h>

struct ibcs2_termio {
	unsigned short	c_iflag;
	unsigned short	c_oflag;
	unsigned short	c_cflag;
	unsigned short	c_lflag;
	char		c_line;
	unsigned char	c_cc[IBCS2_NCC];
};

struct ibcs2_termios {
	unsigned short	c_iflag;
	unsigned short	c_oflag;
	unsigned short	c_cflag;
	unsigned short	c_lflag;
	char		c_line;
	unsigned char	c_cc[IBCS2_NCCS];
	char		c_ispeed;
	char		c_ospeed;
};

struct ibcs2_winsize {
	char bytex, bytey;
	short bitx, bity;
};

static struct speedtab sptab[] = {
	{ 0, 0 }, { 50, 1 }, { 75, 2 }, { 110, 3 },
	{ 134, 4 }, { 135, 4 }, { 150, 5 }, { 200, 6 },
	{ 300, 7 }, { 600, 8 }, { 1200, 9 }, { 1800, 10 },
	{ 2400, 11 }, { 4800, 12 }, { 9600, 13 },
	{ 19200, 14 }, { 38400, 15 },
	{ 57600, 15 }, { 115200, 15 }, {-1, -1 }
};

static int
ibcs2_to_bsd_speed(int code, struct speedtab *table)
{
	for ( ; table->sp_code != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);
	return -1;
}

static int
bsd_to_ibcs2_speed(int speed, struct speedtab *table)
{
	for ( ; table->sp_speed != -1; table++)
		if (table->sp_speed == speed)
			return (table->sp_code);
	return -1;
}

static void
bsd_termios_to_ibcs2_termio(struct termios *bsd_termios,
			    struct ibcs2_termio *ibcs2_termio)
{
	int speed;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: BSD termios structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
			bsd_termios->c_iflag, bsd_termios->c_oflag,
			bsd_termios->c_cflag, bsd_termios->c_lflag,
			bsd_termios->c_ispeed, bsd_termios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++)
			printf("%02x ", bsd_termios->c_cc[i]);
		printf("\n");
	}

	ibcs2_termio->c_iflag = bsd_termios->c_iflag &
		(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK
		 |ISTRIP|INLCR|IGNCR|ICRNL|IXANY);
	if (bsd_termios->c_iflag & IXON)
		ibcs2_termio->c_iflag |= IBCS2_IXON;
	if (bsd_termios->c_iflag & IXOFF)
		ibcs2_termio->c_iflag |= IBCS2_IXOFF;

	ibcs2_termio->c_oflag = 0;
	if (bsd_termios->c_oflag & OPOST)
		ibcs2_termio->c_oflag |= IBCS2_OPOST;
	if (bsd_termios->c_oflag & ONLCR)
		ibcs2_termio->c_oflag |= IBCS2_ONLCR;
	if (bsd_termios->c_oflag & OXTABS)
		ibcs2_termio->c_oflag |= (IBCS2_TAB1|IBCS2_TAB2);

	speed = bsd_to_ibcs2_speed(bsd_termios->c_ospeed, sptab);

	ibcs2_termio->c_cflag = speed >= 0 ? speed : 0;
	ibcs2_termio->c_cflag |= (bsd_termios->c_cflag & CSIZE) >> 4; /* XXX */
	if (bsd_termios->c_cflag & CSTOPB)
		ibcs2_termio->c_cflag |= IBCS2_CSTOPB;
	if (bsd_termios->c_cflag & PARENB)
		ibcs2_termio->c_cflag |= IBCS2_PARENB;
	if (bsd_termios->c_cflag & PARODD)
		ibcs2_termio->c_cflag |= IBCS2_PARODD;
	if (bsd_termios->c_cflag & HUPCL)
		ibcs2_termio->c_cflag |= IBCS2_HUPCL;
	if (bsd_termios->c_cflag & CLOCAL)
		ibcs2_termio->c_cflag |= IBCS2_CLOCAL;

	ibcs2_termio->c_lflag = 0;
	if (bsd_termios->c_lflag & ISIG)
		ibcs2_termio->c_lflag |= IBCS2_ISIG;
	if (bsd_termios->c_lflag & ICANON)
		ibcs2_termio->c_lflag |= IBCS2_ICANON;
	if (bsd_termios->c_lflag & ECHO)
		ibcs2_termio->c_lflag |= IBCS2_ECHO;
	if (bsd_termios->c_lflag & ECHOE)
		ibcs2_termio->c_lflag |= IBCS2_ECHOE;
	if (bsd_termios->c_lflag & ECHOK)
		ibcs2_termio->c_lflag |= IBCS2_ECHOK;
	if (bsd_termios->c_lflag & ECHONL)
		ibcs2_termio->c_lflag |= IBCS2_ECHONL;
	if (bsd_termios->c_lflag & NOFLSH)
		ibcs2_termio->c_lflag |= IBCS2_NOFLSH;
	if (bsd_termios->c_lflag & ECHOCTL)
		ibcs2_termio->c_lflag |= 0x0200; /* XXX */
	if (bsd_termios->c_lflag & ECHOPRT)
		ibcs2_termio->c_lflag |= 0x0400; /* XXX */
	if (bsd_termios->c_lflag & ECHOKE)
		ibcs2_termio->c_lflag |= 0x0800; /* XXX */
	if (bsd_termios->c_lflag & IEXTEN)
		ibcs2_termio->c_lflag |= 0x8000; /* XXX */

	ibcs2_termio->c_cc[IBCS2_VINTR] = bsd_termios->c_cc[VINTR];
	ibcs2_termio->c_cc[IBCS2_VQUIT] = bsd_termios->c_cc[VQUIT];
	ibcs2_termio->c_cc[IBCS2_VERASE] = bsd_termios->c_cc[VERASE];
	ibcs2_termio->c_cc[IBCS2_VKILL] = bsd_termios->c_cc[VKILL];
	if (bsd_termios->c_lflag & ICANON) {
		ibcs2_termio->c_cc[IBCS2_VEOF] = bsd_termios->c_cc[VEOF];
		ibcs2_termio->c_cc[IBCS2_VEOL] = bsd_termios->c_cc[VEOL];
	} else {
		ibcs2_termio->c_cc[IBCS2_VMIN] = bsd_termios->c_cc[VMIN];
		ibcs2_termio->c_cc[IBCS2_VTIME] = bsd_termios->c_cc[VTIME];
	}
	ibcs2_termio->c_cc[IBCS2_VEOL2] = bsd_termios->c_cc[VEOL2];
	ibcs2_termio->c_cc[IBCS2_VSWTCH] = 0xff;
	ibcs2_termio->c_line = 0;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: IBCS2 termio structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x speed=%d line=%d\n",
			ibcs2_termio->c_iflag, ibcs2_termio->c_oflag,
			ibcs2_termio->c_cflag, ibcs2_termio->c_lflag,
			ibcs2_to_bsd_speed(
				ibcs2_termio->c_cflag & IBCS2_CBAUD, sptab),
			ibcs2_termio->c_line);
		printf("c_cc ");
		for (i=0; i<IBCS2_NCC; i++)
			printf("%02x ", ibcs2_termio->c_cc[i]);
		printf("\n");
	}
}

static void
ibcs2_termio_to_bsd_termios(struct ibcs2_termio *ibcs2_termio,
			    struct termios *bsd_termios)
{
	int i, speed;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: IBCS2 termio structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x speed=%d line=%d\n",
			ibcs2_termio->c_iflag, ibcs2_termio->c_oflag,
			ibcs2_termio->c_cflag, ibcs2_termio->c_lflag,
			ibcs2_to_bsd_speed(
				ibcs2_termio->c_cflag & IBCS2_CBAUD, sptab),
			ibcs2_termio->c_line);
		printf("c_cc ");
		for (i=0; i<IBCS2_NCC; i++)
			printf("%02x ", ibcs2_termio->c_cc[i]);
		printf("\n");
	}

	bsd_termios->c_iflag = ibcs2_termio->c_iflag &
		(IBCS2_IGNBRK|IBCS2_BRKINT|IBCS2_IGNPAR|IBCS2_PARMRK|IBCS2_INPCK
	 	|IBCS2_ISTRIP|IBCS2_INLCR|IBCS2_IGNCR|IBCS2_ICRNL|IBCS2_IXANY);
	if (ibcs2_termio->c_iflag & IBCS2_IXON)
		bsd_termios->c_iflag |= IXON;
	if (ibcs2_termio->c_iflag & IBCS2_IXOFF)
		bsd_termios->c_iflag |= IXOFF;

	bsd_termios->c_oflag = 0;
	if (ibcs2_termio->c_oflag & IBCS2_OPOST)
		bsd_termios->c_oflag |= OPOST;
	if (ibcs2_termio->c_oflag & IBCS2_ONLCR)
		bsd_termios->c_oflag |= ONLCR;
	if (ibcs2_termio->c_oflag & (IBCS2_TAB1|IBCS2_TAB2))
		bsd_termios->c_oflag |= OXTABS;

	speed = ibcs2_to_bsd_speed(ibcs2_termio->c_cflag & IBCS2_CBAUD, sptab);
	bsd_termios->c_ospeed = bsd_termios->c_ispeed = speed >= 0 ? speed : 0;

	bsd_termios->c_cflag = (ibcs2_termio->c_cflag & IBCS2_CSIZE) << 4;
	if (ibcs2_termio->c_cflag & IBCS2_CSTOPB)
		bsd_termios->c_cflag |= CSTOPB;
	if (ibcs2_termio->c_cflag & IBCS2_PARENB)
		bsd_termios->c_cflag |= PARENB;
	if (ibcs2_termio->c_cflag & IBCS2_PARODD)
		bsd_termios->c_cflag |= PARODD;
	if (ibcs2_termio->c_cflag & IBCS2_HUPCL)
		bsd_termios->c_cflag |= HUPCL;
	if (ibcs2_termio->c_cflag & IBCS2_CLOCAL)
		bsd_termios->c_cflag |= CLOCAL;

	bsd_termios->c_lflag = 0;
	if (ibcs2_termio->c_lflag & IBCS2_ISIG)
		bsd_termios->c_lflag |= ISIG;
	if (ibcs2_termio->c_lflag & IBCS2_ICANON)
		bsd_termios->c_lflag |= ICANON;
	if (ibcs2_termio->c_lflag & IBCS2_ECHO)
		bsd_termios->c_lflag |= ECHO;
	if (ibcs2_termio->c_lflag & IBCS2_ECHOE)
		bsd_termios->c_lflag |= ECHOE;
	if (ibcs2_termio->c_lflag & IBCS2_ECHOK)
		bsd_termios->c_lflag |= ECHOK;
	if (ibcs2_termio->c_lflag & IBCS2_ECHONL)
		bsd_termios->c_lflag |= ECHONL;
	if (ibcs2_termio->c_lflag & IBCS2_NOFLSH)
		bsd_termios->c_lflag |= NOFLSH;
	if (ibcs2_termio->c_lflag & 0x0200)	/* XXX */
		bsd_termios->c_lflag |= ECHOCTL;
	if (ibcs2_termio->c_lflag & 0x0400)	/* XXX */
		bsd_termios->c_lflag |= ECHOPRT;
	if (ibcs2_termio->c_lflag & 0x0800)	/* XXX */
		bsd_termios->c_lflag |= ECHOKE;
	if (ibcs2_termio->c_lflag & 0x8000)	/* XXX */
		bsd_termios->c_lflag |= IEXTEN;

	for (i=0; i<NCCS; bsd_termios->c_cc[i++] = 0) ;
	bsd_termios->c_cc[VINTR] = ibcs2_termio->c_cc[IBCS2_VINTR];
	bsd_termios->c_cc[VQUIT] = ibcs2_termio->c_cc[IBCS2_VQUIT];
	bsd_termios->c_cc[VERASE] = ibcs2_termio->c_cc[IBCS2_VERASE];
	bsd_termios->c_cc[VKILL] = ibcs2_termio->c_cc[IBCS2_VKILL];
	if (ibcs2_termio->c_lflag & IBCS2_ICANON) {
		bsd_termios->c_cc[VEOF] = ibcs2_termio->c_cc[IBCS2_VEOF];
		bsd_termios->c_cc[VEOL] = ibcs2_termio->c_cc[IBCS2_VEOL];
	} else {
		bsd_termios->c_cc[VMIN] = ibcs2_termio->c_cc[IBCS2_VMIN];
		bsd_termios->c_cc[VTIME] = ibcs2_termio->c_cc[IBCS2_VTIME];
	}
	bsd_termios->c_cc[VEOL2] = ibcs2_termio->c_cc[IBCS2_VEOL2];

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: BSD termios structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
			bsd_termios->c_iflag, bsd_termios->c_oflag,
			bsd_termios->c_cflag, bsd_termios->c_lflag,
			bsd_termios->c_ispeed, bsd_termios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++)
			printf("%02x ", bsd_termios->c_cc[i]);
		printf("\n");
	}
}

static void
bsd_to_ibcs2_termios(struct termios *bsd_termios,
			    struct ibcs2_termios *ibcs2_termios)
{
	int speed;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: BSD termios structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
			bsd_termios->c_iflag, bsd_termios->c_oflag,
			bsd_termios->c_cflag, bsd_termios->c_lflag,
			bsd_termios->c_ispeed, bsd_termios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++)
			printf("%02x ", bsd_termios->c_cc[i]);
		printf("\n");
	}

	ibcs2_termios->c_iflag = bsd_termios->c_iflag &
		(IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK
		 |ISTRIP|INLCR|IGNCR|ICRNL|IXANY);
	if (bsd_termios->c_iflag & IXON)
		ibcs2_termios->c_iflag |= IBCS2_IXON;
	if (bsd_termios->c_iflag & IXOFF)
		ibcs2_termios->c_iflag |= IBCS2_IXOFF;

	ibcs2_termios->c_oflag = 0;
	if (bsd_termios->c_oflag & OPOST)
		ibcs2_termios->c_oflag |= IBCS2_OPOST;
	if (bsd_termios->c_oflag & ONLCR)
		ibcs2_termios->c_oflag |= IBCS2_ONLCR;
	if (bsd_termios->c_oflag & OXTABS)
		ibcs2_termios->c_oflag |= (IBCS2_TAB1|IBCS2_TAB2);

	ibcs2_termios->c_cflag = (bsd_termios->c_cflag & CSIZE) >> 4; /* XXX */
	if (bsd_termios->c_cflag & CSTOPB)
		ibcs2_termios->c_cflag |= IBCS2_CSTOPB;
	if (bsd_termios->c_cflag & PARENB)
		ibcs2_termios->c_cflag |= IBCS2_PARENB;
	if (bsd_termios->c_cflag & PARODD)
		ibcs2_termios->c_cflag |= IBCS2_PARODD;
	if (bsd_termios->c_cflag & HUPCL)
		ibcs2_termios->c_cflag |= IBCS2_HUPCL;
	if (bsd_termios->c_cflag & CLOCAL)
		ibcs2_termios->c_cflag |= IBCS2_CLOCAL;
	if (bsd_termios->c_cflag & CRTSCTS)
		ibcs2_termios->c_cflag |= 0x8000;	/* XXX */

	ibcs2_termios->c_lflag = 0;
	if (bsd_termios->c_lflag & ISIG)
		ibcs2_termios->c_lflag |= IBCS2_ISIG;
	if (bsd_termios->c_lflag & ICANON)
		ibcs2_termios->c_lflag |= IBCS2_ICANON;
	if (bsd_termios->c_lflag & ECHO)
		ibcs2_termios->c_lflag |= IBCS2_ECHO;
	if (bsd_termios->c_lflag & ECHOE)
		ibcs2_termios->c_lflag |= IBCS2_ECHOE;
	if (bsd_termios->c_lflag & ECHOK)
		ibcs2_termios->c_lflag |= IBCS2_ECHOK;
	if (bsd_termios->c_lflag & ECHONL)
		ibcs2_termios->c_lflag |= IBCS2_ECHONL;
	if (bsd_termios->c_lflag & NOFLSH)
		ibcs2_termios->c_lflag |= IBCS2_NOFLSH;
	if (bsd_termios->c_lflag & ECHOCTL)
		ibcs2_termios->c_lflag |= 0x0200; /* XXX */
	if (bsd_termios->c_lflag & ECHOPRT)
		ibcs2_termios->c_lflag |= 0x0400; /* XXX */
	if (bsd_termios->c_lflag & ECHOKE)
		ibcs2_termios->c_lflag |= 0x0800; /* XXX */
	if (bsd_termios->c_lflag & IEXTEN)
		ibcs2_termios->c_lflag |= 0x8000; /* XXX */

	ibcs2_termios->c_cc[IBCS2_VINTR] = bsd_termios->c_cc[VINTR];
	ibcs2_termios->c_cc[IBCS2_VQUIT] = bsd_termios->c_cc[VQUIT];
	ibcs2_termios->c_cc[IBCS2_VERASE] = bsd_termios->c_cc[VERASE];
	ibcs2_termios->c_cc[IBCS2_VKILL] = bsd_termios->c_cc[VKILL];
	if (bsd_termios->c_lflag & ICANON) {
		ibcs2_termios->c_cc[IBCS2_VEOF] = bsd_termios->c_cc[VEOF];
		ibcs2_termios->c_cc[IBCS2_VEOL] = bsd_termios->c_cc[VEOL];
	} else {
		ibcs2_termios->c_cc[IBCS2_VMIN] = bsd_termios->c_cc[VMIN];
		ibcs2_termios->c_cc[IBCS2_VTIME] = bsd_termios->c_cc[VTIME];
	}
	ibcs2_termios->c_cc[IBCS2_VEOL2] = bsd_termios->c_cc[VEOL2];
	ibcs2_termios->c_cc[IBCS2_VSWTCH] = 0xff;
	ibcs2_termios->c_cc[IBCS2_VSUSP] = bsd_termios->c_cc[VSUSP];
	ibcs2_termios->c_cc[IBCS2_VSTART] = bsd_termios->c_cc[VSTART];
	ibcs2_termios->c_cc[IBCS2_VSTOP] = bsd_termios->c_cc[VSTOP];

	ibcs2_termios->c_ispeed =
		bsd_to_ibcs2_speed(bsd_termios->c_ispeed, sptab);
	ibcs2_termios->c_ospeed =
		bsd_to_ibcs2_speed(bsd_termios->c_ospeed, sptab);
	ibcs2_termios->c_line = 0;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: IBCS2 termios structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d "
			"line=%d\n",
			ibcs2_termios->c_iflag, ibcs2_termios->c_oflag,
			ibcs2_termios->c_cflag, ibcs2_termios->c_lflag,
			ibcs2_to_bsd_speed(ibcs2_termios->c_ispeed, sptab),
			ibcs2_to_bsd_speed(ibcs2_termios->c_ospeed, sptab),
			ibcs2_termios->c_line);
		printf("c_cc ");
		for (i=0; i<IBCS2_NCCS; i++)
			printf("%02x ", ibcs2_termios->c_cc[i]);
		printf("\n");
	}
}

static void
ibcs2_to_bsd_termios(struct ibcs2_termios *ibcs2_termios,
			    struct termios *bsd_termios)
{
	int i, speed;

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: IBCS2 termios structure (input):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d "
			"line=%d\n",
			ibcs2_termios->c_iflag, ibcs2_termios->c_oflag,
			ibcs2_termios->c_cflag, ibcs2_termios->c_lflag,
			ibcs2_to_bsd_speed(ibcs2_termios->c_ispeed, sptab),
			ibcs2_to_bsd_speed(ibcs2_termios->c_ospeed, sptab),
			ibcs2_termios->c_line);
		printf("c_cc ");
		for (i=0; i<IBCS2_NCCS; i++)
			printf("%02x ", ibcs2_termios->c_cc[i]);
		printf("\n");
	}

	bsd_termios->c_iflag = ibcs2_termios->c_iflag &
		(IBCS2_IGNBRK|IBCS2_BRKINT|IBCS2_IGNPAR|IBCS2_PARMRK|IBCS2_INPCK
	 	|IBCS2_ISTRIP|IBCS2_INLCR|IBCS2_IGNCR|IBCS2_ICRNL|IBCS2_IXANY);
	if (ibcs2_termios->c_iflag & IBCS2_IXON)
		bsd_termios->c_iflag |= IXON;
	if (ibcs2_termios->c_iflag & IBCS2_IXOFF)
		bsd_termios->c_iflag |= IXOFF;

	bsd_termios->c_oflag = 0;
	if (ibcs2_termios->c_oflag & IBCS2_OPOST)
		bsd_termios->c_oflag |= OPOST;
	if (ibcs2_termios->c_oflag & IBCS2_ONLCR)
		bsd_termios->c_oflag |= ONLCR;
	if (ibcs2_termios->c_oflag & (IBCS2_TAB1|IBCS2_TAB2))
		bsd_termios->c_oflag |= OXTABS;

	bsd_termios->c_cflag = (ibcs2_termios->c_cflag & IBCS2_CSIZE) << 4;
	if (ibcs2_termios->c_cflag & IBCS2_CSTOPB)
		bsd_termios->c_cflag |= CSTOPB;
	if (ibcs2_termios->c_cflag & IBCS2_PARENB)
		bsd_termios->c_cflag |= PARENB;
	if (ibcs2_termios->c_cflag & IBCS2_PARODD)
		bsd_termios->c_cflag |= PARODD;
	if (ibcs2_termios->c_cflag & IBCS2_HUPCL)
		bsd_termios->c_cflag |= HUPCL;
	if (ibcs2_termios->c_cflag & IBCS2_CLOCAL)
		bsd_termios->c_cflag |= CLOCAL;
	if (ibcs2_termios->c_cflag & 0x8000)
		bsd_termios->c_cflag |= CRTSCTS;	/* XXX */

	bsd_termios->c_lflag = 0;
	if (ibcs2_termios->c_lflag & IBCS2_ISIG)
		bsd_termios->c_lflag |= ISIG;
	if (ibcs2_termios->c_lflag & IBCS2_ICANON)
		bsd_termios->c_lflag |= ICANON;
	if (ibcs2_termios->c_lflag & IBCS2_ECHO)
		bsd_termios->c_lflag |= ECHO;
	if (ibcs2_termios->c_lflag & IBCS2_ECHOE)
		bsd_termios->c_lflag |= ECHOE;
	if (ibcs2_termios->c_lflag & IBCS2_ECHOK)
		bsd_termios->c_lflag |= ECHOK;
	if (ibcs2_termios->c_lflag & IBCS2_ECHONL)
		bsd_termios->c_lflag |= ECHONL;
	if (ibcs2_termios->c_lflag & IBCS2_NOFLSH)
		bsd_termios->c_lflag |= NOFLSH;
	if (ibcs2_termios->c_lflag & 0x0200)	/* XXX */
		bsd_termios->c_lflag |= ECHOCTL;
	if (ibcs2_termios->c_lflag & 0x0400)	/* XXX */
		bsd_termios->c_lflag |= ECHOPRT;
	if (ibcs2_termios->c_lflag & 0x0800)	/* XXX */
		bsd_termios->c_lflag |= ECHOKE;
	if (ibcs2_termios->c_lflag & 0x8000)	/* XXX */
		bsd_termios->c_lflag |= IEXTEN;

	for (i=0; i<NCCS; bsd_termios->c_cc[i++] = 0) ;
	bsd_termios->c_cc[VINTR] = ibcs2_termios->c_cc[IBCS2_VINTR];
	bsd_termios->c_cc[VQUIT] = ibcs2_termios->c_cc[IBCS2_VQUIT];
	bsd_termios->c_cc[VERASE] = ibcs2_termios->c_cc[IBCS2_VERASE];
	bsd_termios->c_cc[VKILL] = ibcs2_termios->c_cc[IBCS2_VKILL];
	if (ibcs2_termios->c_lflag & IBCS2_ICANON) {
		bsd_termios->c_cc[VEOF] = ibcs2_termios->c_cc[IBCS2_VEOF];
		bsd_termios->c_cc[VEOL] = ibcs2_termios->c_cc[IBCS2_VEOL];
	} else {
		bsd_termios->c_cc[VMIN] = ibcs2_termios->c_cc[IBCS2_VMIN];
		bsd_termios->c_cc[VTIME] = ibcs2_termios->c_cc[IBCS2_VTIME];
	}
	bsd_termios->c_cc[VEOL2] = ibcs2_termios->c_cc[IBCS2_VEOL2];
	bsd_termios->c_cc[VSUSP] = ibcs2_termios->c_cc[IBCS2_VSUSP];
	bsd_termios->c_cc[VSTART] = ibcs2_termios->c_cc[IBCS2_VSTART];
	bsd_termios->c_cc[VSTOP] = ibcs2_termios->c_cc[IBCS2_VSTOP];

	bsd_termios->c_ispeed =
		ibcs2_to_bsd_speed(ibcs2_termios->c_ispeed, sptab);
	bsd_termios->c_ospeed =
		ibcs2_to_bsd_speed(ibcs2_termios->c_ospeed, sptab);

	if (ibcs2_trace & IBCS2_TRACE_IOCTLCNV) {
		int i;
		printf("IBCS2: BSD termios structure (output):\n");
		printf("i=%08x o=%08x c=%08x l=%08x ispeed=%d ospeed=%d\n",
			bsd_termios->c_iflag, bsd_termios->c_oflag,
			bsd_termios->c_cflag, bsd_termios->c_lflag,
			bsd_termios->c_ispeed, bsd_termios->c_ospeed);
		printf("c_cc ");
		for (i=0; i<NCCS; i++)
			printf("%02x ", bsd_termios->c_cc[i]);
		printf("\n");
	}
}


struct ibcs2_ioctl_args {
	int	fd;
	int	cmd;
	int	arg;
};

int
ibcs2_ioctl(struct proc *p, struct ibcs2_ioctl_args *args, int *retval)
{
	struct termios bsd_termios;
	struct winsize bsd_winsize;
	struct ibcs2_termio ibcs2_termio;
	struct ibcs2_termios ibcs2_termios;
	struct ibcs2_winsize ibcs2_winsize;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	int (*func)();
	int type = (args->cmd&0xffff00)>>8;
	int num = args->cmd&0xff;
	int error;

	if (ibcs2_trace & IBCS2_TRACE_IOCTL)
		printf("IBCS2: 'ioctl' fd=%d, typ=%d(%c), num=%d\n",
			args->fd, type, type, num);

	if ((unsigned)args->fd >= fdp->fd_nfiles
	    || (fp = fdp->fd_ofiles[args->fd]) == 0)
		return EBADF;

	if (!fp || (fp->f_flag & (FREAD | FWRITE)) == 0) {
		return EBADF;
	}

	func = fp->f_ops->fo_ioctl;

	switch (type) {
	case 'f':
		switch (num) {
		case 1:
			args->cmd = FIOCLEX;
			return ioctl(p, args, retval);
		case 2:
			args->cmd = FIONCLEX;
			return ioctl(p, args, retval);
		case 3:
			args->cmd = FIONREAD;
			return ioctl(p, args, retval);
		}
		break;
#if 0
	case 'j':
		switch (num) {
		case 5: /* jerq winsize ?? */
			ibcs2_winsize.bytex = 80;
			/* p->p_session->s_ttyp->t_winsize.ws_col; XXX */
			ibcs2_winsize.bytey = 25;
			/* p->p_session->s_ttyp->t_winsize.ws_row; XXX */
			ibcs2_winsize.bitx =
				p->p_session->s_ttyp->t_winsize.ws_xpixel;
			ibcs2_winsize.bity =
				p->p_session->s_ttyp->t_winsize.ws_ypixel;
			return copyout((caddr_t)&ibcs2_winsize,
					(caddr_t)args->arg,
					sizeof(ibcs2_winsize));
		}
#endif
	case 't':
		switch (num) {
		case 0:
			args->cmd = TIOCGETD;
			return ioctl(p, args, retval);
		case 1:
			args->cmd = TIOCSETD;
			return ioctl(p, args, retval);
		case 2:
			args->cmd = TIOCHPCL;
			return ioctl(p, args, retval);
		case 8:
			args->cmd = TIOCGETP;
			return ioctl(p, args, retval);
		case 9:
			args->cmd = TIOCSETP;
			return ioctl(p, args, retval);
		case 10:
			args->cmd = TIOCSETN;
			return ioctl(p, args, retval);
		case 13:
			args->cmd = TIOCEXCL;
			return ioctl(p, args, retval);
		case 14:
			args->cmd = TIOCNXCL;
			return ioctl(p, args, retval);
		case 16:
			args->cmd = TIOCFLUSH;
			return ioctl(p, args, retval);
		case 17:
			args->cmd = TIOCSETC;
			return ioctl(p, args, retval);
		case 18:
			args->cmd = TIOCGETC;
			return ioctl(p, args, retval);
		}
		break;

	case 'T':
		switch (num) {
		case 1:		/* TCGETA */
			if ((error = (*func)(fp, TIOCGETA,
					     (caddr_t)&bsd_termios, p)) != 0)
				return error;
			bsd_termios_to_ibcs2_termio(&bsd_termios,&ibcs2_termio);
			return copyout((caddr_t)&ibcs2_termio,
				       (caddr_t)args->arg,
				       sizeof(ibcs2_termio));

		case 2:		/* TCSETA */
			ibcs2_termio_to_bsd_termios(
				(struct ibcs2_termio *)args->arg, &bsd_termios);
			return (*func)(fp, TIOCSETA, (caddr_t)&bsd_termios, p);

		case 3:		/* TCSETAW */
			ibcs2_termio_to_bsd_termios(
				(struct ibcs2_termio *)args->arg, &bsd_termios);
			return (*func)(fp, TIOCSETAW, (caddr_t)&bsd_termios, p);

		case 4:		/* TCSETAF */
			ibcs2_termio_to_bsd_termios(
				(struct ibcs2_termio *)args->arg, &bsd_termios);
			return (*func)(fp, TIOCSETAF, (caddr_t)&bsd_termios, p);

		case 5:		/* TCSBRK */
			args->cmd = TIOCDRAIN;
			if (error = ioctl(p, args, retval))
				return error;
			args->cmd = TIOCSBRK;
			ioctl(p, args, retval);
			args->cmd = TIOCCBRK;
			error = ioctl(p, args, retval);
			return error;

		case 6:		/* TCONC */
			if (args->arg == 0) args->cmd = TIOCSTOP;
			else args->cmd = TIOCSTART;
			return ioctl(p, args, retval);

		case 7:		/* TCFLSH */
			args->cmd = TIOCFLUSH;
			if ((int)args->arg == 0) (int)args->arg = FREAD;
			if ((int)args->arg == 1) (int)args->arg = FWRITE;
			if ((int)args->arg == 2) (int)args->arg = FREAD|FWRITE;
			return ioctl(p, args, retval);

		case 103:	/* TIOCSWINSZ */
			bsd_winsize.ws_row =
				((struct ibcs2_winsize *)(args->arg))->bytex;
			bsd_winsize.ws_col =
				((struct ibcs2_winsize *)(args->arg))->bytey;
			bsd_winsize.ws_xpixel =
				((struct ibcs2_winsize *)(args->arg))->bitx;
			bsd_winsize.ws_ypixel =
				((struct ibcs2_winsize *)(args->arg))->bity;
			return (*func)(fp, TIOCSWINSZ,
				       (caddr_t)&bsd_winsize, p);

		case 104:	/* TIOCGWINSZ */
			if ((error = (*func)(fp, TIOCGWINSZ,
					     (caddr_t)&bsd_winsize, p)) != 0)
				return error;
			ibcs2_winsize.bytex = bsd_winsize.ws_col;
			ibcs2_winsize.bytey = bsd_winsize.ws_row;
			ibcs2_winsize.bitx = bsd_winsize.ws_xpixel;
			ibcs2_winsize.bity = bsd_winsize.ws_ypixel;
			return copyout((caddr_t)&ibcs2_winsize,
					(caddr_t)args->arg,
					sizeof(ibcs2_winsize));

		case  20:	/* TCSETPGRP */
		case 118:	/* TIOCSPGRP */
			args->cmd = TIOCSPGRP;
			return ioctl(p, args, retval);

		case  21:	/* TCGETPGRP */
		case 119:	/* TIOCGPGRP */
			args->cmd = TIOCGPGRP;
			return ioctl(p, args, retval);
		}
		break;

	case ('x'):
		switch (num) {
		case 1:
			if ((error = (*func)(fp, TIOCGETA,
					     (caddr_t)&bsd_termios, p)) != 0)
				return error;
			bsd_to_ibcs2_termios(&bsd_termios, &ibcs2_termios);
			return copyout((caddr_t)&ibcs2_termios,
					(caddr_t)args->arg,
					sizeof(ibcs2_termios));
		case 2:
			ibcs2_to_bsd_termios((struct ibcs2_termios *)args->arg,
					     &bsd_termios);
			return (*func)(fp, TIOCSETA, (caddr_t)&bsd_termios, p);
		case 3:
			ibcs2_to_bsd_termios((struct ibcs2_termios *)args->arg,
					     &bsd_termios);
			return (*func)(fp, TIOCSETAW, (caddr_t)&bsd_termios, p);
		case 4:
			ibcs2_to_bsd_termios((struct ibcs2_termios *)args->arg,
					     &bsd_termios);
			return (*func)(fp, TIOCSETAF, (caddr_t)&bsd_termios, p);

		}
		break;

	/* below is console ioctl's, we have syscons so no problem here */
	case 'a':
		switch (num) {
		case 0:
			args->cmd = GIO_ATTR;
			error = ioctl(p, args, retval);
			*retval = (int)args->arg;
			return error;
		}
		break;

	case 'c':
		switch (num) {
		case 0:
			args->cmd = GIO_COLOR;
			ioctl(p, args, retval);
			*retval = (int)args->arg;
			return error;
		case 1:
			args->cmd = CONS_CURRENT;
			ioctl(p, args, retval);
			*retval = (int)args->arg;
			return error;
		case 2:
			args->cmd = CONS_GET;
			ioctl(p, args, retval);
			*retval = (int)args->arg;
			return error;
		case 4:
			args->cmd = CONS_BLANKTIME;
			return ioctl(p, args, retval);
		case 64:
			args->cmd = PIO_FONT8x8;
			return ioctl(p, args, retval);
		case 65:
			args->cmd = GIO_FONT8x8;
			return ioctl(p, args, retval);
		case 66:
			args->cmd = PIO_FONT8x14;
			return ioctl(p, args, retval);
		case 67:
			args->cmd = GIO_FONT8x14;
			return ioctl(p, args, retval);
		case 68:
			args->cmd = PIO_FONT8x16;
			return ioctl(p, args, retval);
		case 69:
			args->cmd = GIO_FONT8x16;
			return ioctl(p, args, retval);
		case 73:
			args->cmd = CONS_GETINFO;
			return ioctl(p, args, retval);
		}
		break;

	case 'k':
		switch (num) {
		case 0:
			args->cmd = GETFKEY;
			return ioctl(p, args, retval);
		case 1:
			args->cmd = SETFKEY;
			return ioctl(p, args, retval);
		case 2:
			args->cmd = GIO_SCRNMAP;
			return ioctl(p, args, retval);
		case 3:
			args->cmd = PIO_SCRNMAP;
			return ioctl(p, args, retval);
		case 6:
			args->cmd = GIO_KEYMAP;
			return ioctl(p, args, retval);
		case 7:
			args->cmd = PIO_KEYMAP;
			return ioctl(p, args, retval);
		}
		break;

	case 'K':
		switch (num) {
		case 6:
			args->cmd = KDGKBMODE;
			return ioctl(p, args, retval);
		case 7:
			args->cmd = KDSKBMODE;
			return ioctl(p, args, retval);
		case 8:
			args->cmd = KDMKTONE;
			return ioctl(p, args, retval);
		case 9:
			args->cmd = KDGETMODE;
			return ioctl(p, args, retval);
		case 10:
			args->cmd = KDSETMODE;
			return ioctl(p, args, retval);
		case 13:
			args->cmd = KDSBORDER;
			return ioctl(p, args, retval);
		case 19:
			args->cmd = KDGKBSTATE;
			return ioctl(p, args, retval);
		case 20:
			args->cmd = KDSETRAD;
			return ioctl(p, args, retval);
		case 60:
			args->cmd = KDENABIO;
			return ioctl(p, args, retval);
		case 61:
			args->cmd = KDDISABIO;
			return ioctl(p, args, retval);
		case 63:
			args->cmd = KIOCSOUND;
			return ioctl(p, args, retval);
		case 64:
			args->cmd = KDGKBTYPE;
			return ioctl(p, args, retval);
		case 65:
			args->cmd = KDGETLED;
			return ioctl(p, args, retval);
		case 66:
			args->cmd = KDSETLED;
			return ioctl(p, args, retval);
		}
		break;

	case 'S':
		args->cmd = _IO('S', num);
		return ioctl(p, args, retval);

	case 'v':
		switch (num) {
		case 1:
			args->cmd = VT_OPENQRY;
			return ioctl(p, args, retval);
		case 2:
			args->cmd = VT_SETMODE;
			return ioctl(p, args, retval);
		case 3:
			args->cmd = VT_GETMODE;
			return ioctl(p, args, retval);
		case 4:
			args->cmd = VT_RELDISP;
			return ioctl(p, args, retval);
		case 5:
			args->cmd = VT_ACTIVATE;
			return ioctl(p, args, retval);
		case 6:
			args->cmd = VT_WAITACTIVE;
			return ioctl(p, args, retval);
		}
		break;
	}

	switch (type & 0xff) {
	case 'I':	/* socksys 'I' type calls */
		return ioctl(p, args, retval);
	case 'R':	/* socksys 'R' type calls */
		return ioctl(p, args, retval);
	case 'S':	/* socksys 'S' type calls */
		return ioctl(p, args, retval);
	}
	uprintf("IBCS2: 'ioctl' fd=%d, typ=%d(%c), num=%d not implemented\n",
			args->fd, type, type, num);
	return EINVAL;
}

struct ibcs2_sgtty_args {
	int fd;
	struct sgttyb *buf;
};

struct ioctl_args {
	int	fd;
	int	cmd;
	caddr_t	arg;
};

int
ibcs2_gtty(struct proc *p, struct ibcs2_sgtty_args *args, int *retval)
{
	struct ioctl_args ioctl_arg;

	if (ibcs2_trace & IBCS2_TRACE_IOCTL)
		printf("IBCS2: 'gtty' fd=%d\n", args->fd);
	ioctl_arg.fd = args->fd;
	ioctl_arg.cmd = TIOCGETC;
	ioctl_arg.arg = (caddr_t)args->buf;

	return ioctl(p, &ioctl_arg, retval);
}

int
ibcs2_stty(struct proc *p, struct ibcs2_sgtty_args *args, int *retval)
{
	struct ioctl_args ioctl_arg;

	if (ibcs2_trace & IBCS2_TRACE_IOCTL)
		printf("IBCS2: 'stty' fd=%d\n", args->fd);
	ioctl_arg.fd = args->fd;
	ioctl_arg.cmd = TIOCSETC;
	ioctl_arg.arg = (caddr_t)args->buf;
	return ioctl(p, &ioctl_arg, retval);
}
