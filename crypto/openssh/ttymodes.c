/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Encoding and decoding of terminal modes in a portable way.
 * Much of the format is defined in ttymodes.h; it is included multiple times
 * into this file with the appropriate macro definitions to generate the
 * suitable code.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: ttymodes.c,v 1.8 2000/09/07 20:27:55 deraadt Exp $");

#include "packet.h"
#include "ssh.h"

#define TTY_OP_END	0
#define TTY_OP_ISPEED	192	/* int follows */
#define TTY_OP_OSPEED	193	/* int follows */

/*
 * Converts POSIX speed_t to a baud rate.  The values of the
 * constants for speed_t are not themselves portable.
 */
static int
speed_to_baud(speed_t speed)
{
	switch (speed) {
	case B0:
		return 0;
	case B50:
		return 50;
	case B75:
		return 75;
	case B110:
		return 110;
	case B134:
		return 134;
	case B150:
		return 150;
	case B200:
		return 200;
	case B300:
		return 300;
	case B600:
		return 600;
	case B1200:
		return 1200;
	case B1800:
		return 1800;
	case B2400:
		return 2400;
	case B4800:
		return 4800;
	case B9600:
		return 9600;

#ifdef B19200
	case B19200:
		return 19200;
#else /* B19200 */
#ifdef EXTA
	case EXTA:
		return 19200;
#endif /* EXTA */
#endif /* B19200 */

#ifdef B38400
	case B38400:
		return 38400;
#else /* B38400 */
#ifdef EXTB
	case EXTB:
		return 38400;
#endif /* EXTB */
#endif /* B38400 */

#ifdef B7200
	case B7200:
		return 7200;
#endif /* B7200 */
#ifdef B14400
	case B14400:
		return 14400;
#endif /* B14400 */
#ifdef B28800
	case B28800:
		return 28800;
#endif /* B28800 */
#ifdef B57600
	case B57600:
		return 57600;
#endif /* B57600 */
#ifdef B76800
	case B76800:
		return 76800;
#endif /* B76800 */
#ifdef B115200
	case B115200:
		return 115200;
#endif /* B115200 */
#ifdef B230400
	case B230400:
		return 230400;
#endif /* B230400 */
	default:
		return 9600;
	}
}

/*
 * Converts a numeric baud rate to a POSIX speed_t.
 */
static speed_t
baud_to_speed(int baud)
{
	switch (baud) {
		case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;

#ifdef B19200
	case 19200:
		return B19200;
#else /* B19200 */
#ifdef EXTA
	case 19200:
		return EXTA;
#endif /* EXTA */
#endif /* B19200 */

#ifdef B38400
	case 38400:
		return B38400;
#else /* B38400 */
#ifdef EXTB
	case 38400:
		return EXTB;
#endif /* EXTB */
#endif /* B38400 */

#ifdef B7200
	case 7200:
		return B7200;
#endif /* B7200 */
#ifdef B14400
	case 14400:
		return B14400;
#endif /* B14400 */
#ifdef B28800
	case 28800:
		return B28800;
#endif /* B28800 */
#ifdef B57600
	case 57600:
		return B57600;
#endif /* B57600 */
#ifdef B76800
	case 76800:
		return B76800;
#endif /* B76800 */
#ifdef B115200
	case 115200:
		return B115200;
#endif /* B115200 */
#ifdef B230400
	case 230400:
		return B230400;
#endif /* B230400 */
	default:
		return B9600;
	}
}

/*
 * Encodes terminal modes for the terminal referenced by fd
 * in a portable manner, and appends the modes to a packet
 * being constructed.
 */
void
tty_make_modes(int fd)
{
	struct termios tio;
	int baud;

	if (tcgetattr(fd, &tio) < 0) {
		packet_put_char(TTY_OP_END);
		log("tcgetattr: %.100s", strerror(errno));
		return;
	}
	/* Store input and output baud rates. */
	baud = speed_to_baud(cfgetospeed(&tio));
	packet_put_char(TTY_OP_OSPEED);
	packet_put_int(baud);
	baud = speed_to_baud(cfgetispeed(&tio));
	packet_put_char(TTY_OP_ISPEED);
	packet_put_int(baud);

	/* Store values of mode flags. */
#define TTYCHAR(NAME, OP) \
  packet_put_char(OP); packet_put_char(tio.c_cc[NAME]);
#define TTYMODE(NAME, FIELD, OP) \
  packet_put_char(OP); packet_put_char((tio.FIELD & NAME) != 0);
#define SGTTYCHAR(NAME, OP)
#define SGTTYMODE(NAME, FIELD, OP)
#define SGTTYMODEN(NAME, FIELD, OP)

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE
#undef SGTTYCHAR
#undef SGTTYMODE
#undef SGTTYMODEN

	/* Mark end of mode data. */
	packet_put_char(TTY_OP_END);
}

/*
 * Decodes terminal modes for the terminal referenced by fd in a portable
 * manner from a packet being read.
 */
void
tty_parse_modes(int fd, int *n_bytes_ptr)
{
	struct termios tio;
	int opcode, baud;
	int n_bytes = 0;
	int failure = 0;

	/*
	 * Get old attributes for the terminal.  We will modify these
	 * flags. I am hoping that if there are any machine-specific
	 * modes, they will initially have reasonable values.
	 */
	if (tcgetattr(fd, &tio) < 0)
		failure = -1;

	for (;;) {
		n_bytes += 1;
		opcode = packet_get_char();
		switch (opcode) {
		case TTY_OP_END:
			goto set;

		case TTY_OP_ISPEED:
			n_bytes += 4;
			baud = packet_get_int();
			if (failure != -1 && cfsetispeed(&tio, baud_to_speed(baud)) < 0)
				error("cfsetispeed failed for %d", baud);
			break;

		case TTY_OP_OSPEED:
			n_bytes += 4;
			baud = packet_get_int();
			if (failure != -1 && cfsetospeed(&tio, baud_to_speed(baud)) < 0)
				error("cfsetospeed failed for %d", baud);
			break;

#define TTYCHAR(NAME, OP) 				\
	case OP:					\
	  n_bytes += 1;					\
	  tio.c_cc[NAME] = packet_get_char();		\
	  break;
#define TTYMODE(NAME, FIELD, OP)		       	\
	case OP:					\
	  n_bytes += 1;					\
	  if (packet_get_char())			\
	    tio.FIELD |= NAME;				\
	  else						\
	    tio.FIELD &= ~NAME;				\
	  break;
#define SGTTYCHAR(NAME, OP)
#define SGTTYMODE(NAME, FIELD, OP)
#define SGTTYMODEN(NAME, FIELD, OP)

#include "ttymodes.h"

#undef TTYCHAR
#undef TTYMODE
#undef SGTTYCHAR
#undef SGTTYMODE
#undef SGTTYMODEN

		default:
			debug("Ignoring unsupported tty mode opcode %d (0x%x)",
			      opcode, opcode);
			/*
			 * Opcodes 0 to 127 are defined to have
			 * a one-byte argument.
			 */
			if (opcode >= 0 && opcode < 128) {
				n_bytes += 1;
				(void) packet_get_char();
				break;
			} else {
				/*
				 * Opcodes 128 to 159 are defined to have
				 * an integer argument.
				 */
				if (opcode >= 128 && opcode < 160) {
					n_bytes += 4;
					(void) packet_get_int();
					break;
				}
			}
			/*
			 * It is a truly undefined opcode (160 to 255).
			 * We have no idea about its arguments.  So we
			 * must stop parsing.  Note that some data may be
			 * left in the packet; hopefully there is nothing
			 * more coming after the mode data.
			 */
			log("parse_tty_modes: unknown opcode %d", opcode);
			packet_integrity_check(0, 1, SSH_CMSG_REQUEST_PTY);
			goto set;
		}
	}

set:
	if (*n_bytes_ptr != n_bytes) {
		*n_bytes_ptr = n_bytes;
		return;		/* Don't process bytes passed */
	}
	if (failure == -1)
		return;		/* Packet parsed ok but tty stuff failed */

	/* Set the new modes for the terminal. */
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
		log("Setting tty modes failed: %.100s", strerror(errno));
	return;
}
