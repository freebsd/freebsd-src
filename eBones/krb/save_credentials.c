/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: save_credentials.c,v 4.9 89/05/31 17:45:43 jtkohl Exp $
 *	$Id: save_credentials.c,v 1.3 1995/07/18 16:39:40 mark Exp $
 */

#if 0
#ifndef lint
static char *rcsid =
"$Id: save_credentials.c,v 1.3 1995/07/18 16:39:40 mark Exp $";
#endif /* lint */
#endif

#include <stdio.h>
#include <krb.h>

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

int save_credentials(char *service, char *instance, char *realm,
    des_cblock session, int lifetime, int kvno, KTEXT ticket, long issue_date)
{
    int tf_status;   /* return values of the tf_util calls */

    /* Open and lock the ticket file for writing */
    if ((tf_status = tf_init(TKT_FILE, W_TKT_FIL)) != KSUCCESS)
	return(tf_status);

    /* Save credentials by appending to the ticket file */
    tf_status = tf_save_cred(service, instance, realm, session,
			     lifetime, kvno, ticket, issue_date);
    (void) tf_close();
    return (tf_status);
}
