/*-
 * This program is in the public domain.  I couldn't bring myself to
 * declare Copyright on a variant of Hello World.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/uio.h>
#include <syslog.h>
#include <unistd.h>

#define	MESSAGE	"This account is currently not available.\n"

int
main(int argc, char *argv[])
{
#ifndef NO_NOLOGIN_LOG
	char *user, *tt;

	if ((tt = ttyname(0)) == NULL)
		tt = "UNKNOWN";
	if ((user = getlogin()) == NULL)
		user = "UNKNOWN";

	openlog("nologin", LOG_CONS, LOG_AUTH);
	syslog(LOG_CRIT, "Attempted login by %s on %s", user, tt);
	closelog();
#endif /* NO_NOLOGIN_LOG */

	write(STDOUT_FILENO, MESSAGE, sizeof(MESSAGE) - 1);
	_exit(1);
}
