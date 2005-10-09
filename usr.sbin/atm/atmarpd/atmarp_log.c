/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP-ATMARP server interface: logging routines
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/queue.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h>
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
 
#include <errno.h>
#include <libatm.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>

#include "../scspd/scsp_msg.h"
#include "../scspd/scsp_if.h"
#include "../scspd/scsp_var.h"
#include "atmarp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Write a message to atmarpd's log
 *
 * Arguments:
 *	level	the level (error, info, etc.) of the message
 *	fmt	printf-style format string
 *	...	parameters for printf-style use according to fmt
 *
 * Returns:
 *	none
 *
 */
void
#if __STDC__
atmarp_log(const int level, const char *fmt, ...)
#else
atmarp_log(level, fmt, va_alist)
	int	level;
	char	*fmt;
	va_dcl
#endif
{
	va_list	ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	/*
	 * In debug mode, just write to stdout
	 */
	if (atmarp_debug_mode) {
		vprintf(fmt, ap);
		printf("\n");
		return;
	}

	/*
	 * Check whether we have a log file set up
	 */
	if (!atmarp_log_file) {
		/*
		 * Write to syslog
		 */
		vsyslog(level, fmt, ap);
	} else {
		/*
		 * Write to the log file
		 */
		vfprintf(atmarp_log_file, fmt, ap);
		fprintf(atmarp_log_file, "\n");
	}

	va_end(ap);
}


/*
 * Log a memory error and exit
 *
 * Arguments:
 *	cp	message to log
 *
 * Returns:
 *	exits, does not return
 *
 */
void
atmarp_mem_err(cp)
	char	*cp;
{
	atmarp_log(LOG_CRIT, "out of memory: %s", cp);
	exit(2);
}
