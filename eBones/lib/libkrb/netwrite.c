/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: netwrite.c,v 4.1 88/11/15 16:48:58 jtkohl Exp $";
 *	$Id: netwrite.c,v 1.2 1994/07/19 19:26:04 g89r4222 Exp $
 */

#ifndef	lint
static char rcsid[] =
"$Id: netwrite.c,v 1.2 1994/07/19 19:26:04 g89r4222 Exp $";
#endif	lint

/*
 * krb_net_write() writes "len" bytes from "buf" to the file
 * descriptor "fd".  It returns the number of bytes written or
 * a write() error.  (The calling interface is identical to
 * write(2).)
 *
 * XXX must not use non-blocking I/O
 */

int
krb_net_write(fd, buf, len)
int fd;
register char *buf;
int len;
{
    int cc;
    register int wrlen = len;
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
