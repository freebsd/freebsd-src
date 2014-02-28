/****************************************************************************
 * Copyright (c) 1998-2007,2008 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 * Author: Juergen Pfeifer, 2008-on                                         * 
 *                                                                          *
 ****************************************************************************/

/* $Id: ncurses_mingw.h,v 1.1 2008/12/14 19:22:16 juergen Exp $ */

/*
 * This is a placholder up to now and describes what needs to be implemented
 * to support I/O to external terminals with ncurses on the Windows OS.
 */

#if __MINGW32__
#ifndef _NC_MINGWH
#define _NC_MINGWH

#define USE_CONSOLE_DRIVER 1

#undef  TERMIOS
#define TERMIOS 1

#define InvalidHandle ((TERM_HANDLE)-1)
#define InvalidConsoleHandle(s) ((s)==InvalidHandle)

typedef unsigned char cc_t;
typedef unsigned int  speed_t;
typedef unsigned int  tcflag_t;

#define NCCS 32
struct termios
{
  tcflag_t   c_iflag;     /* input mode         */
  tcflag_t   c_oflag;     /* output mode        */
  tcflag_t   c_cflag;     /* control mode       */
  tcflag_t   c_lflag;     /* local mode         */
  cc_t       c_line;      /* line discipline    */
  cc_t       c_cc[NCCS];  /* control characters */
  speed_t    c_ispeed;    /* input speed        */
  speed_t    c_ospeed;    /* c_ospeed           */
};

/* c_cc chars */
#define VINTR     0
#define VQUIT     1
#define VERASE    2
#define VKILL     3
#define VEOF      4
#define VTIME     5
#define VMIN      6

/* c_iflag bits */
#define ISTRIP	0000040
#define INLCR	0000100
#define IGNCR	0000200
#define ICRNL	0000400
#define BRKINT	0000002
#define PARMRK	0000010
#define IXON	0002000
#define IGNBRK	0000001
#define IGNPAR	0000004
#define INPCK	0000020
#define IXOFF	0010000

/* c_oflag bits */
#define OPOST	0000001

/* c_cflag bit meaning */
#define CBAUD	   0010017
#define CSIZE	   0000060
#define CS8	   0000060
#define B0	   0000000
#define B50	   0000001
#define B75	   0000002
#define B110	   0000003
#define B134	   0000004
#define B150	   0000005
#define B200	   0000006
#define B300	   0000007
#define B600	   0000010
#define B1200	   0000011
#define B1800	   0000012
#define B2400	   0000013
#define B4800	   0000014
#define B9600	   0000015
#define CLOCAL	   0004000
#define CREAD	   0000200
#define CSTOPB	   0000100
#define HUPCL	   0002000
#define PARENB	   0000400
#define PARODD	   0001000

/* c_lflag bits */
#define ECHO	0000010
#define ECHONL	0000100
#define ISIG	0000001
#define IEXTEN	0100000
#define ICANON	0000002
#define NOFLSH	0000200
#define ECHOE	0000020
#define ECHOK	0000040

/* tcflush() */
#define	TCIFLUSH	0

/* tcsetattr uses these */
#define	TCSADRAIN	1

/* ioctls */
#define TCGETA		0x5405
#define TCFLSH		0x540B
#define TIOCGWINSZ	0x5413

extern int _nc_mingw_ioctl(int fd, long int request, struct termios* arg);
extern void _nc_set_term_driver(void* term);

#ifndef cfgetospeed
#define cfgetospeed(t) ((t)->c_cflag & CBAUD)
#endif

#ifndef tcsetattr
#define tcsetattr(fd, cmd, arg) _nc_mingw_ioctl(fd, cmd, arg)
#endif

#ifndef tcgetattr
#define tcgetattr(fd, arg) _nc_mingw_ioctl(fd, TCGETA, arg)
#endif

#ifndef tcflush
#define tcflush(fd, arg) _nc_mingw_ioctl(fd, TCFLSH, arg)
#endif

#undef  ttyname
#define ttyname(fd) NULL

#endif
#endif
