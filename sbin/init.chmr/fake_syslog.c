/*
 * Warning: extremely XXX
 * fake syslog to write to stderr, for standalone test version of init
 */


#ifdef FAKE_SYSLOG


#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/file.h>


#ifdef TESTRUN
#  define FAKE_LOGFILE	"/dev/tty"
#else
#  define FAKE_LOGFILE	"/dev/console"
#endif


static char	*
itoa(i)
	int	i;
{
	static char	buf[12];
	char		*s;
	int		minflg = 0;

	s = &buf[11];
	*s = '\0';
	if (i < 0)
		minflg = 1, i = -i;
	do {
		*(--s) = (i % 10) + '0';
		i /= 10;
	} while (i);
	if (minflg)
		*(--s) = '-';
	return(s);
}


static int		 _log_fd = 2;
static const char	*_log_id = "";


void
openlog(ident, logopt, facility)
const char	*ident;
int		logopt, facility;
{
	if (ident)
		_log_id = ident;
	if (_log_fd >= 0)
		close(_log_fd);
	_log_fd = open(FAKE_LOGFILE, O_WRONLY | O_NONBLOCK, 0);
}


void
vsyslog(int pri, const char *fmt, va_list ap)
{
	const char	*s, *t;
	int		saved_errno = errno;

	if (write(_log_fd, "", 0) < 0)
		openlog(NULL, 0, 0);
	write(_log_fd, _log_id, strlen(_log_id));
	if (*_log_id) write(_log_fd, ": ", 2);
	for (s=fmt; *s; s++)
		if (*s == '%')
			switch (*(++s)) {
				case '\0':
					s--; break;
				case '%':
					write(_log_fd, s, 1); break;
				case 's':
					t = va_arg(ap, char *);
					write(_log_fd, t, strlen(t));
					break;
				case 'm':
#ifdef SMALL
					write(_log_fd, "Error ", 6);
					t = itoa(saved_errno);
#else
					t = strerror(saved_errno);
#endif
					write(_log_fd, t, strlen(t));
					break;
				case 'd':
					t = itoa(va_arg(ap, int));
					write(_log_fd, t, strlen(t));
					break;
				default:
					write(_log_fd, s-1, 2);
			}
		else
			write(_log_fd, s, 1);
	write(_log_fd, "\n", 1);
}


void
syslog(int pri, const char *fmt, ...)
{
va_list		ap;

	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}


void
closelog(void)
{
	close(_log_fd);
	_log_fd = -1;
}


#endif
