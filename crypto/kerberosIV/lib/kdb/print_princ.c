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

#include "kdb_locl.h"

RCSID("$Id: print_princ.c,v 1.5 1997/05/07 01:37:13 assar Exp $");

void
krb_print_principal(Principal *a_n)
{
    struct tm *time_p;

    /* run-time database does not contain string versions */
    time_p = k_localtime(&(a_n->exp_date));

    fprintf(stderr,
    "\n%s %s expires %4d-%2d-%2d %2d:%2d, max_life %d*5 = %d min  attr 0x%02x",
    a_n->name, a_n->instance,
    time_p->tm_year + 1900,
    time_p->tm_mon + 1, time_p->tm_mday,
    time_p->tm_hour, time_p->tm_min,
    a_n->max_life, 5 * a_n->max_life, a_n->attributes);

    fprintf(stderr,
    "\n\tkey_ver %d  k_low 0x%08lx  k_high 0x%08lx  akv %d  exists %ld\n",
    a_n->key_version, (long)a_n->key_low, (long)a_n->key_high,
    a_n->kdc_key_ver, (long)a_n->old);

    fflush(stderr);
}
