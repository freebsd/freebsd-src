/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
 

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <machine/stdarg.h>

#include <support/debug.h>

SYSCTL_NODE(_debug, OID_AUTO, xfs, CTLFLAG_RD, 0, "XFS debug options");

static int verbosity = 10;
SYSCTL_INT(_debug_xfs, OID_AUTO, verbosity, CTLFLAG_RW, &verbosity, 0, "");

#ifdef DEBUG

static int doass = 1;
SYSCTL_INT(_debug_xfs, OID_AUTO, assert, CTLFLAG_RW, &doass, 0, "");

void
assfail(char *a, char *f, int l)
{
	if (doass == 0) return;
	panic("XFS assertion failed: %s, file: %s, line: %d\n", a, f, l);
}

int
get_thread_id(void)
{
	return curthread->td_proc->p_pid;
}

#endif

void
cmn_err(register int level, char *fmt, ...)
{
	char    *fp = fmt;
	char    message[256];
	va_list ap;

	if (verbosity < level)
		return;

	va_start(ap, fmt);
	if (*fmt == '!') fp++;
	vsprintf(message, fp, ap);
	printf("%s\n", message);
	va_end(ap);
}


void
icmn_err(register int level, char *fmt, va_list ap)
{ 
	char	message[256];

	if (verbosity < level)
		return;

	vsprintf(message, fmt, ap);
	printf("cmn_err level %d %s\n",level, message);
}

