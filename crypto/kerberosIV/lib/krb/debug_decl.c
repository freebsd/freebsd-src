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

RCSID("$Id: debug_decl.c,v 1.10 1999/06/16 15:10:38 joda Exp $");

/* Declare global debugging variables. */

int krb_ap_req_debug = 0;
int krb_debug = 0;
int krb_dns_debug = 0;

int
krb_enable_debug(void)
{
    krb_ap_req_debug = krb_debug = krb_dns_debug = 1;
    return 0;
}

int
krb_disable_debug(void)
{
    krb_ap_req_debug = krb_debug = krb_dns_debug = 0;
    return 0;
}
