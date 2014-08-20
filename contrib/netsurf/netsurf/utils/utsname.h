/*
 * Copyright 2010 Vincent Sanders <vince@kyllikki.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _NETSURF_UTILS_UTSNAME_H_
#define _NETSURF_UTILS_UTSNAME_H_

#ifdef HAVE_UTSNAME
#include <sys/utsname.h>
#else
/* from posix spec */
struct utsname {
	char sysname[65];    /* Operating system name (e.g., "Linux") */
	char nodename[65];   /* Name within "some implementation-defined
			      network" */
	char release[65];    /* OS release (e.g., "2.6.28") */
	char version[65];    /* OS version */
	char machine[65];    /* Hardware identifier */
};

int uname(struct utsname *buf);

#endif

#endif
