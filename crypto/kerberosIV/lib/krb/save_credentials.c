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

RCSID("$Id: save_credentials.c,v 1.5 1997/03/23 03:53:17 joda Exp $");

/*
 * This routine takes a ticket and associated info and calls
 * tf_save_cred() to store them in the ticket cache.  The peer
 * routine for extracting a ticket and associated info from the
 * ticket cache is krb_get_cred().  When changes are made to
 * this routine, the corresponding changes should be made
 * in krb_get_cred() as well.
 *
 * Returns KSUCCESS if all goes well, otherwise an error returned
 * by the tf_init() or tf_save_cred() routines.
 */

int
save_credentials(char *service,	/* Service name */
		 char *instance, /* Instance */
		 char *realm,	/* Auth domain */
		 unsigned char *session, /* Session key */
		 int lifetime,	/* Lifetime */
		 int kvno,	/* Key version number */
		 KTEXT ticket,	/* The ticket itself */
		 int32_t issue_date) /* The issue time */
{
    int tf_status;   /* return values of the tf_util calls */

    /* Open and lock the ticket file for writing */
    if ((tf_status = tf_init(TKT_FILE, W_TKT_FIL)) != KSUCCESS)
	return(tf_status);

    /* Save credentials by appending to the ticket file */
    tf_status = tf_save_cred(service, instance, realm, session,
			     lifetime, kvno, ticket, issue_date);
    tf_close();
    return (tf_status);
}
