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

#endif /* NTP_TTY_H */
