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

RCSID("$Id: mk_err.c,v 1.6 1997/03/23 03:53:14 joda Exp $");

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

int32_t
krb_mk_err(u_char *p, int32_t e, char *e_string)
{
    unsigned char *start = p;
    p += krb_put_int(KRB_PROT_VERSION, p, 1);
    p += krb_put_int(AUTH_MSG_APPL_ERR, p, 1);
    
    p += krb_put_int(e, p, 4);
    p += krb_put_string(e_string, p);
    return p - start;
}
