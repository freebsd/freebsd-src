/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: extract_ticket.c,v 4.6 88/10/07 06:08:15 shanzer Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>
#include <strings.h>

/*
 * This routine is obsolete.
 *
 * This routine accepts the ciphertext returned by kerberos and
 * extracts the nth ticket.  It also fills in the variables passed as
 * session, liftime and kvno.
 */

void
extract_ticket(cipher,n,session,lifetime,kvno,realm,ticket)
    KTEXT cipher;		/* The ciphertext */
    int n;			/* Which ticket */
    char *session;		/* The session key for this tkt */
    int *lifetime;		/* The life of this ticket */
    int *kvno;			/* The kvno for the service */
    char *realm;		/* Realm in which tkt issued */
    KTEXT ticket;		/* The ticket itself */
{
    char *ptr;
    int i;

    /* Start after the ticket lengths */
    ptr = (char *) cipher->dat;
    ptr = ptr + 1 + (int) *(cipher->dat);

    /* Step through earlier tickets */
    for (i = 1; i < n; i++)
	ptr = ptr + 11 + strlen(ptr+10) + (int) *(cipher->dat+i);
    bcopy(ptr, (char *) session, 8); /* Save the session key */
    ptr += 8;
    *lifetime = *(ptr++);	/* Save the life of the ticket */
    *kvno = *(ptr++);		/* Save the kvno */
    (void) strcpy(realm,ptr);	/* instance */
    ptr += strlen(realm) + 1;

    /* Save the ticket if its length is non zero */
    ticket->length = *(cipher->dat+n);
    if (ticket->length)
	bcopy(ptr, (char *) (ticket->dat), ticket->length);
}
