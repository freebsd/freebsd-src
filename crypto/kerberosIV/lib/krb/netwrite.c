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

RCSID("$Id: netwrite.c,v 1.7 1997/03/23 03:53:15 joda Exp $");

/*
 * krb_net_write() writes "len" bytes from "buf" to the file
 * descriptor "fd".  It returns the number of bytes written or
 * a write() error.  (The calling interface is identical to
 * write(2).)
 *
 * XXX must not use non-blocking I/O
 */

int
krb_net_write(int fd, const void *v, size_t len)
{
    int cc;
    int wrlen = len;
    const char *buf = (const char*)v;

    do {
	cc = write(fd, buf, wrlen);
	if (cc < 0)
	    return(cc);
	else {
	    buf += cc;
	    wrlen -= cc;
	}
    } while (wrlen > 0);
    return(len);
}
