/*
 * Program to control ICOM radios
 *
 * This is a ripoff of the utility routines in the ICOM software
 * distribution. The only function provided is to load the radio
 * frequency. All other parameters must be manually set before use.
 */
#include "icom.h"
#include <unistd.h>
#include <stdio.h>

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif /* HAVE_TERMIOS_H */
#ifdef HAVE_SYS_TERMIOS_H
# include <sys/termios.h>
#endif /* HAVE_SYS_TERMIOS_H */

#include <fcntl.h>
#include <errno.h>

/*
 * Scraps
 */
#define BMAX 50			/* max command length */
#define DICOM /dev/icom/	/* ICOM port link */

/*
 * FSA definitions
 */
#define S_IDLE	0		/* idle */
#define S_HDR	1		/* header */
#define S_TX	2		/* address */
#define S_DATA	3		/* data */
#define S_ERROR	4		/* error */

/*
 * Local function prototypes
 */
static void doublefreq		P((double, u_char *, int));
static int sndpkt		P((int, int, u_char *, u_char *));
static int sndoctet		P((int, int));
static int rcvoctet		P((int));

/*
 * Local variables
 */
static int flags;		/* trace flags */
static int state;		/* fsa state */


/*
 * icom_freq(fd, ident, freq) - load radio frequency
 */
int
icom_freq(			/* returns 0 (ok), EIO (error) */
	int fd,			/* file descriptor */
	int ident,		/* ICOM radio identifier */
	double freq		/* frequency (MHz) */
	)
{
	u_char cmd[BMAX], rsp[BMAX];
	int temp;
	cmd[0] = V_SFREQ;
	if (ident == IC735)
		temp = 4;
	else
		temp = 5;
	doublefreq(freq * 1e6, &cmd[1], temp);
	temp = sndpkt(fd, ident, cmd, rsp);
	if (temp < 1 || rsp[0] != ACK)
		return (EIO);
	return (0);
}


/*
 * doublefreq(freq, y, len) - double to ICOM frequency with padding
 */
static void
doublefreq(			/* returns void */
	double freq,		/* frequency */
	u_char *x,		/* radio frequency */
	int len			/* length (octets) */
	)
{
	int i;
	char s1[11];
	char *y;

	sprintf(s1, " %10.0f", freq);
	y = s1 + 10;
	i = 0;
	while (*y != ' ') {
		x[i] = *y-- & 0x0f;
		x[i] = x[i] | ((*y-- & 0x0f) << 4);
		i++;
	}
	for (; i < len; i++)
		x[i] = 0;
	x[i] = FI;
}


/*
 * Packet routines
 *
 * These routines send a packet and receive the response. If an error
 * (collision) occurs on transmit, the packet is resent. If an error
 * occurs on receive (timeout), all input to the terminating FI is
 * discarded and the packet is resent. If the maximum number of retries
 * is not exceeded, the program returns the number of octets in the user
 * buffer; otherwise, it returns zero.
 *
 * ICOM frame format
 *
 * Frames begin with a two-octet preamble PR-PR followyd by the
 * transceiver address RE, controller address TX, control code CN, zero
 * or more data octets DA (depending on command), and terminator FI.
 * Since the bus is bidirectional, every octet output is echoed on
 * input. Every valid frame sent is answered with a frame in the same
 * format, but with the RE and TX fields interchanged. The CN field is
 * set to NAK if an error has occurred. Otherwise, the data are returned
 * in this and following DA octets. If no data are returned, the CN
 * octet is set to ACK.
 *
 *	+------+------+------+------+------+--//--+------+
 *	|  PR  |  PR  |  RE  |  TX  |  CN  |  DA  |  FI  |
 *	+------+------+------+------+------+--//--+------+
 */
/*
 * icom_open() - open and initialize serial interface
 *
 * This routine opens the serial interface for raw transmission; that
 * is, character-at-a-time, no stripping, checking or monkeying with the
 * bits. For Unix, an input operation ends either with the receipt of a
 * character or a 0.5-s timeout.
 */
int
icom_init(
	char *device,		/* device name/link */
	int speed,		/* line speed */
	int trace		/* trace flags */	)
{
	struct termios ttyb;
	int fd;

	flags = trace;
	fd = open(device, O_RDWR, 0777);
	if (fd < 0)
		return (fd);
	tcgetattr(fd, &ttyb);
	ttyb.c_iflag = 0;	/* input modes */
	ttyb.c_oflag = 0;	/* output modes */
	ttyb.c_cflag = IBAUD|CS8|CREAD|CLOCAL;	/* control modes */
	ttyb.c_lflag = 0;	/* local modes */
	ttyb.c_cc[VMIN] = 0;	/* min chars */
	ttyb.c_cc[VTIME] = 5;	/* receive timeout */
	cfsetispeed(&ttyb, (u_int)speed);
	cfsetospeed(&ttyb, (u_int)speed);
	tcsetattr(fd, TCSANOW, &ttyb);
	return (fd);
}


/*
 * sndpkt(r, x, y) - send packet and receive response
 *
 * This routine sends a command frame, which consists of all except the
 * preamble octets PR-PR. It then listens for the response frame and
 * returns the payload to the caller. The routine checks for correct
 * response header format; that is, the length of the response vector
 * returned to the caller must be at least 2 and the RE and TX octets
 * must be interchanged; otherwise, the operation is retried up to
 * the number of times specified in a global variable.
 *
 * The trace function, which is enabled by the P_TRACE bit of the global
 * flags variable, prints all characters received or echoed on the bus
 * preceded by a T (transmit) or R (receive). The P_ERMSG bit of the
 * flags variable enables printing of bus error messages.
 *
 * Note that the first octet sent is a PAD in order to allow time for
 * the radio to flush its receive buffer after sending the previous
 * response. Even with this precaution, some of the older radios
 * occasionally fail to receive a command and it has to be sent again.
 */
static int
sndpkt(				/* returns octet count */
	int fd,			/* file descriptor */
	int r,			/* radio address */
	u_char *cmd,		/* command vector */
	u_char *rsp		/* response vector */
	)
{
	int i, j, temp;

	(void)tcflush(fd, TCIOFLUSH);
	for (i = 0; i < RETRY; i++) {
		state = S_IDLE;

		/*
		 * Transmit packet.
		 */
		if (flags & P_TRACE)
			printf("icom T:");
		sndoctet(fd, PAD);	/* send header */
		sndoctet(fd, PR);
		sndoctet(fd, PR);
		sndoctet(fd, r);
		sndoctet(fd, TX);
		for (j = 0; j < BMAX; j++) { /* send body */
			if (sndoctet(fd, cmd[j]) == FI)
				break;
		}
		while (rcvoctet(fd) != FI); /* purge echos */
		if (cmd[0] == V_FREQT || cmd[0] == V_MODET)
			return (0);	/* shortcut for broadcast */

		/*
		 * Receive packet. First, delete all characters
		 * preceeding a PR, then discard all PRs. Check that the
		 * RE and TX fields are correctly interchanged, then
		 * copy the remaining data and FI to the user buffer.
		 */
		if (flags & P_TRACE)
			printf("\nicom R:");
		j = 0;
		while ((temp = rcvoctet(fd)) != FI) {
			switch (state) {

			case S_IDLE:
				if (temp != PR)
					continue;
				state = S_HDR;
				break;

			case S_HDR:
				if (temp == PR) {
					continue;
				} else if (temp != TX) {
					if (flags & P_ERMSG)
						printf(
						    "icom: TX error\n");
					state = S_ERROR;
				}
				state = S_TX;
				break;

			case S_TX:
				if (temp != r) {
					if (flags & P_ERMSG)
						printf(
						    "icom: RE error\n");
					state = S_ERROR;
				}
				state = S_DATA;
				break;

			case S_DATA:
				if (j >= BMAX ) {
					if (flags & P_ERMSG)
						printf(
					    "icom: buffer overrun\n");
					state = S_ERROR;
					j = 0;
				}
				rsp[j++] = (u_char)temp;
				break;

			case S_ERROR:
				break;
			}
		}
		if (flags & P_TRACE)
			printf("\n");
		if (j > 0) {
			rsp[j++] = FI;
			return (j);
		}
	}
	if (flags & P_ERMSG)
		printf("icom: retries exceeded\n");
	return (0);
}


/*
 * Interface routines
 *
 * These routines read and write octets on the bus. In case of receive
 * timeout a FI code is returned. In case of output collision (echo
 * does not match octet sent), the remainder of the collision frame
 * (including the trailing FI) is discarded.
 */
/*
 * sndoctet(fd, x) - send octet
 */
static int
sndoctet(			/* returns octet */
	int fd,			/* file descriptor */
	int x			/* octet */
	)
{
	u_char y;

	y = (u_char)x;
	write(fd, &y, 1);
	return (x);
}


/*
 * rcvoctet(fd) - receive octet
 */
static int
rcvoctet(			/* returns octet */
	int fd			/* file descriptor */
	)
{
	u_char y;

	if (read(fd, &y, 1) < 1)
		y = FI;		/* come here if timeout */
	if (flags & P_TRACE && y != PAD)
		printf(" %02x", y);
	return (y);
}

/* end program */
