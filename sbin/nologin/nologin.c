/*-
 * This program is in the public domain.  I couldn't bring myself to
 * declare Copyright on a variant of Hello World.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sbin/nologin/nologin.c,v 1.1 2003/11/17 06:39:38 das Exp $");

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define	MESSAGE	"This account is currently not available.\n"

int
main(int argc, char *argv[])
{

	write(STDOUT_FILENO, MESSAGE, sizeof(MESSAGE));
	_exit(1);
}
