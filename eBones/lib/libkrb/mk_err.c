/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: mk_err.c,v 4.4 88/11/15 16:33:36 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <sys/types.h>
#include <krb.h>
#include <prot.h>
#include <strings.h>

/*
 * This routine creates a general purpose error reply message.  It
 * doesn't use KTEXT because application protocol may have long
 * messages, and may want this part of buffer contiguous to other
 * stuff.
 *
 * The error reply is built in "p", using the error code "e" and
 * error text "e_string" given.  The length of the error reply is
 * returned.
 *
 * The error reply is in the following format:
 *
 * unsigned char	KRB_PROT_VERSION	protocol version no.
 * unsigned char	AUTH_MSG_APPL_ERR	message type
 * (least significant
 * bit of above)	HOST_BYTE_ORDER		local byte order
 * 4 bytes		e			given error code
 * string		e_string		given error text
 */

long krb_mk_err(p,e,e_string)
    u_char *p;			/* Where to build error packet */
    long e;			/* Error code */
    char *e_string;		/* Text of error */
{
    u_char      *start;

    start = p;

    /* Create fixed part of packet */
    *p++ = (unsigned char) KRB_PROT_VERSION;
    *p = (unsigned char) AUTH_MSG_APPL_ERR;
    *p++ |= HOST_BYTE_ORDER;

    /* Add the basic info */
    bcopy((char *)&e,(char *)p,4); /* err code */
    p += sizeof(e);
    (void) strcpy((char *)p,e_string); /* err text */
    p += strlen(e_string);

    /* And return the length */
    return p-start;
}
