/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"
RCSID("$Id: k_gethostname.c,v 1.10 1997/03/23 03:53:12 joda Exp $");

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

/*
 * Return the local host's name in "name", up to "namelen" characters.
 * "name" will be null-terminated if "namelen" is big enough.
 * The return code is 0 on success, -1 on failure.  (The calling
 * interface is identical to gethostname(2).)
 */

int
k_gethostname(char *name, int namelen)
{
#if defined(HAVE_GETHOSTNAME)
    return gethostname(name, namelen);
#elif defined(HAVE_UNAME)
    {
	struct utsname utsname;
	int ret;

	ret = uname (&utsname);
	if (ret < 0)
	    return ret;
	strncpy (name, utsname.nodename, namelen);
	name[namelen-1] = '\0';
	return 0;
    }
#else
    strncpy (name, "some.random.host", namelen);
    name[namelen-1] = '\0';
    return 0;
#endif
}
