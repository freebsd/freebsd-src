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

RCSID("$Id: netread.c,v 1.6 1997/03/23 03:53:15 joda Exp $");

/*
 * krb_net_read() reads from the file descriptor "fd" to the buffer
 * "buf", until either 1) "len" bytes have been read or 2) cannot
 * read anymore from "fd".  It returns the number of bytes read
 * or a read() error.  (The calling interface is identical to
 * read(2).)
 *
 * XXX must not use non-blocking I/O
 */

int
krb_net_read (int fd, void *v, size_t len)
{
    int cc, len2 = 0;
    char *buf = v;

    do {
	cc = read(fd, buf, len);
	if (cc < 0)
	    return(cc);		 /* errno is already set */
	else if (cc == 0) {
	    return(len2);
	} else {
	    buf += cc;
	    len2 += cc;
	    len -= cc;
	}
    } while (len > 0);
    return(len2);
}
