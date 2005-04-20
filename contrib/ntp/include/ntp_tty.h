/*
 * ntp_tty.h - header file for serial lines handling
 */

#ifndef NTP_TTY_H
#define NTP_TTY_H

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#define TTY	struct sgttyb
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#define TTY	struct termio
#ifndef tcsetattr
#define tcsetattr(fd, cmd, arg) ioctl(fd, cmd, arg)
#endif
#ifndef TCSANOW
#define TCSANOW	TCSETA
#endif
#ifndef TCIFLUSH
#define TCIFLUSH 0
#endif
#ifndef TCOFLUSH
#define TCOFLUSH 1
#endif
#ifndef TCIOFLUSH
#define TCIOFLUSH 2
#endif
#ifndef tcflush
#define tcflush(fd, arg) ioctl(fd, TCFLSH, arg)
#endif
#endif /* HAVE_SYSV_TTYS */

#if defined(HAVE_TERMIOS)
# ifdef TERMIOS_NEEDS__SVID3
#  define _SVID3
# endif
# include <termios.h>
# ifdef TERMIOS_NEEDS__SVID3
#  undef _SVID3
# endif
#define TTY	struct termios
#endif

#if defined(HAVE_SYS_MODEM_H)
#include <sys/modem.h>
#endif

#if !defined(SYSV_TTYS) && !defined(STREAM) & !defined(BSD_TTYS)
#define BSD_TTYS
#endif /* SYSV_TTYS STREAM BSD_TTYS */

/*
 * Line discipline flags. These require line discipline or streams
 * modules to be installed/loaded in the kernel. If specified, but not
 * installed, the code runs as if unspecified.
 */
#define LDISC_STD	0x0	/* standard */
#define LDISC_CLK	0x1	/* tty_clk \n intercept */
#define LDISC_CLKPPS	0x2	/* tty_clk \377 intercept */
#define LDISC_ACTS	0x4	/* tty_clk #* intercept */
#define LDISC_CHU	0x8	/* depredated */
#define LDISC_PPS	0x10	/* ppsclock, ppsapi */
#define LDISC_RAW	0x20	/* raw binary */

#endif /* NTP_TTY_H */
