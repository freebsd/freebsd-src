#ifndef NTP_WIN_TERMIOS_H
#define NTP_WIN_TERMIOS_H

/*  Flag definitions for compatibility
 *  ==================================
*/

#include <fcntl.h>

#define NCCS	18	/* refclock_arc.c uses VTIME (17) */
#define VEOL	3
typedef unsigned char	cc_t;
typedef unsigned int	speed_t;
typedef unsigned int	tcflag_t;

struct termios
{
	tcflag_t	c_iflag;	/* input mode flags */
	tcflag_t	c_oflag;	/* output mode flags */
	tcflag_t	c_cflag;	/* control mode flags */
	tcflag_t	c_lflag;	/* local mode flags */
	cc_t		c_line;		/* line discipline */
	cc_t		c_cc[NCCS];	/* control characters */ 
	speed_t		c_ispeed;	/* input speed */
	speed_t		c_ospeed;	/* output speed */
};

/* c_cc characters 
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16
*/

/* c_iflag bits */
#define IGNBRK	0000001
#define BRKINT	0000002
#define IGNPAR	0000004
#define PARMRK	0000010
#define INPCK	0000020
#define ISTRIP	0000040
#define INLCR	0000100
#define IGNCR	0000200
#define ICRNL	0000400
#define IUCLC	0001000
#define IXON	0002000
#define IXANY	0004000
#define IXOFF	0010000
#define IMAXBEL	0020000

/* c_oflag bits */
#define OPOST	0000001
#define OLCUC	0000002
#define ONLCR	0000004
#define OCRNL	0000010
#define ONOCR	0000020
#define ONLRET	0000040
#define OFILL	0000100
#define OFDEL	0000200

#define NLDLY	0000400
#define NL0	0000000
#define NL1	0000400

#define CRDLY	0003000
#define CR0	0000000
#define CR1	0001000
#define CR2	0002000
#define CR3	0003000

#define TABDLY	0014000
#define TAB0	0000000
#define TAB1	0004000
#define TAB2	0010000
#define TAB3	0014000
#define XTABS	0014000

#define BSDLY	0020000
#define BS0	0000000
#define BS1	0020000

#define VTDLY	0040000
#define VT0	0000000
#define VT1	0040000

#define FFDLY	0100000
#define FF0	0000000
#define FF1	0100000

/* c_cflag bit meaning */
#define CBAUD	0010017
#define B0	0000000		/* hang up */
#define B50	0000001
#define B75	0000002
#define B110	0000003
#define B134	0000004
#define B150	0000005
#define B200	0000006
#define B300	0000007
#define B600	0000010
#define B1200	0000011
#define B1800	0000012
#define B2400	0000013
#define B4800	0000014
#define B9600	0000015
#define B19200	0000016
#define B38400	0000017

#define EXTA	B19200
#define EXTB	B38400

#define CSIZE	0000060
#define CS5	0000000
#define CS6	0000020
#define CS7	0000040
#define CS8	0000060

#define CSTOPB	0000100
#define CREAD	0000200
#define PARENB	0000400
#define PARODD	0001000
#define HUPCL	0002000
#define CLOCAL	0004000

#define CBAUDEX 0010000
#define B57600  0010001
#define B115200 0010002
#define B230400 0010003
#define B460800 0010004

#define CIBAUD	002003600000	/* input baud rate (not used) */
#define CRTSCTS	020000000000	/* flow control */

/* c_lflag bits */
#define ISIG	0000001
#define ICANON	0000002
#define XCASE	0000004
#define ECHO	0000010
#define ECHOE	0000020
#define ECHOK	0000040
#define ECHONL	0000100
#define NOFLSH	0000200
#define TOSTOP	0000400
#define ECHOCTL	0001000
#define ECHOPRT	0002000
#define ECHOKE	0004000
#define FLUSHO	0010000
#define PENDIN	0040000
#define IEXTEN	0100000

/* tcflow() and TCXONC use these */
#define	TCOOFF		0
#define	TCOON		1
#define	TCIOFF		2
#define	TCION		3

/* tcflush() and TCFLSH use these */
#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2

/* tcsetattr uses these */
#define	TCSANOW		0
#define	TCSADRAIN	1
#define	TCSAFLUSH	2
#define	VMIN		16
#define VTIME		17

/* modem lines */
#define TIOCM_LE	0x001
#define TIOCM_DTR	0x002
#define TIOCM_RTS	0x004
#define TIOCM_ST	0x008
#define TIOCM_SR	0x010
#define TIOCM_CTS	0x020
#define TIOCM_CAR	0x040
#define TIOCM_RNG	0x080
#define TIOCM_DSR	0x100
#define TIOCM_CD	TIOCM_CAR
#define TIOCM_RI	TIOCM_RNG
#define TIOCM_OUT1	0x2000
#define TIOCM_OUT2	0x4000

/* ioctl */
#define TIOCMGET	1
#define TIOCMSET	2
#define TIOCMBIC	3
#define TIOCMBIS	4

/* NOP cfsetospeed() and cfsetispeed() for now */
#define cfsetospeed(dcb, spd)	(0)
#define cfsetispeed(dcb, spd)	(0)

extern	int	closeserial	(int);
extern	int	ioctl		(int, int, void *);
extern	int	tcsetattr	(int, int, const struct termios *);
extern	int	tcgetattr	(int, struct termios *);
extern	int	tcflush		(int, int);
extern	int	isserialhandle	(HANDLE);

typedef struct DeviceContext DevCtx_t;
extern	DevCtx_t*	serial_devctx(HANDLE);

#endif	/* NTP_WIN_TERMIOS_H */
