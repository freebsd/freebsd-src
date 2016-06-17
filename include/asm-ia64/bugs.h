/*
 * This is included by init/main.c to check for architecture-dependent bugs.
 *
 * Needs:
 *	void check_bugs(void);
 *
 * Based on <asm-alpha/bugs.h>.
 *
 * Modified 1998, 1999
 *	David Mosberger-Tang <davidm@hpl.hp.com>,  Hewlett-Packard Co.
 */

#include <asm/processor.h>

/*
 * I don't know of any ia-64 bugs yet..
 */
static void
check_bugs (void)
{
}
