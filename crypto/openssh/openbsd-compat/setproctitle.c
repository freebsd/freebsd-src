/*
 * Modified for OpenSSH by Kevin Steves
 * October 2000
 */

/*
 * Copyright (c) 1994, 1995 Christopher G. Demetriou
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: setproctitle.c,v 1.8 2001/11/06 19:21:40 art Exp $";
#endif /* LIBC_SCCS and not lint */

#include "includes.h"

#ifndef HAVE_SETPROCTITLE

#define SPT_NONE	0
#define SPT_PSTAT	1

#ifndef SPT_TYPE
#define SPT_TYPE	SPT_NONE
#endif

#if SPT_TYPE == SPT_PSTAT
#include <sys/param.h>
#include <sys/pstat.h>
#endif /* SPT_TYPE == SPT_PSTAT */

#define	MAX_PROCTITLE	2048

extern char *__progname;

/*
 * Set Process Title (SPT) defines.  Modeled after sendmail's
 * SPT type definition strategy.
 *
 * SPT_TYPE:
 *
 * SPT_NONE:	Don't set the process title.  Default.
 * SPT_PSTAT:	Use pstat(PSTAT_SETCMD).  HP-UX specific.
 */

void
setproctitle(const char *fmt, ...)
{
#if SPT_TYPE != SPT_NONE
	va_list ap;
	
	char buf[MAX_PROCTITLE];
	size_t used;

#if SPT_TYPE == SPT_PSTAT
	union pstun pst;
#endif /* SPT_TYPE == SPT_PSTAT */

	va_start(ap, fmt);
	if (fmt != NULL) {
		used = snprintf(buf, MAX_PROCTITLE, "%s: ", __progname);
		if (used >= MAX_PROCTITLE)
			used = MAX_PROCTITLE - 1;
		(void)vsnprintf(buf + used, MAX_PROCTITLE - used, fmt, ap);
	} else
		(void)snprintf(buf, MAX_PROCTITLE, "%s", __progname);
	va_end(ap);
	used = strlen(buf);

#if SPT_TYPE == SPT_PSTAT
	pst.pst_command = buf;
	pstat(PSTAT_SETCMD, pst, used, 0, 0);
#endif /* SPT_TYPE == SPT_PSTAT */

#endif /* SPT_TYPE != SPT_NONE */
}
#endif /* HAVE_SETPROCTITLE */
