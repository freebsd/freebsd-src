/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: add_ticket.c,v 4.7 88/10/07 06:06:26 shanzer Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>
#include <strings.h>

/*
 * This routine is now obsolete.  It used to be possible to request
 * more than one ticket at a time from the authentication server, and
 * it looks like this routine was used by the server to package the
 * tickets to be returned to the client.
 */

/*
 * This routine adds a new ticket to the ciphertext to be returned to
 * the client.  The routine takes the ciphertext (which doesn't get
 * encrypted till later), the number of the ticket (i.e. 1st, 2nd,
 * etc) the session key which goes in the ticket and is sent back to
 * the user, the lifetime for the ticket, the service name, the
 * instance, the realm, the key version number, and the ticket itself.
 *
 * This routine returns 0 (KSUCCESS) on success, and 1 (KFAILURE) on
 * failure.  On failure, which occurs when there isn't enough room
 * for the ticket, a 0 length ticket is added.
 *
 * Notes: This routine must be called with successive values of n.
 * i.e. the ticket must be added in order.  The corresponding routine
 * on the client side is extract ticket.
 */

/* XXX they aren't all used; to avoid incompatible changes we will
 * fool lint for the moment */
/*ARGSUSED */
int
add_ticket(cipher,n,session,lifetime,sname,instance,realm,kvno,ticket)
    KTEXT cipher;		/* Ciphertext info for ticket */
    char *sname;		/* Service name */
    char *instance;		/* Instance */
    int n;			/* Relative position of this ticket */
    char *session;		/* Session key for this tkt */
    int lifetime;		/* Lifetime of this ticket */
    char *realm;		/* Realm in which ticket is valid */
    int kvno;			/* Key version number of service key */
    KTEXT ticket;		/* The ticket itself */
{

    /* Note, the 42 is a temporary hack; it will have to be changed. */

    /* Begin check of ticket length */
    if ((cipher->length + ticket->length + 4 + 42 +
	(*(cipher->dat)+1-n)*(11+strlen(realm))) >
       MAX_KTXT_LEN) {
	bcopy(session,(char *)(cipher->dat+cipher->length),8);
	*(cipher->dat+cipher->length+8) = (char) lifetime;
	*(cipher->dat+cipher->length+9) = (char) kvno;
	(void) strcpy((char *)(cipher->dat+cipher->length+10),realm);
	cipher->length += 11 + strlen(realm);
	*(cipher->dat+n) = 0;
	return(KFAILURE);
    }
    /* End check of ticket length */

    /* Add the session key, lifetime, kvno, ticket to the ciphertext */
    bcopy(session,(char *)(cipher->dat+cipher->length),8);
    *(cipher->dat+cipher->length+8) = (char) lifetime;
    *(cipher->dat+cipher->length+9) = (char) kvno;
    (void) strcpy((char *)(cipher->dat+cipher->length+10),realm);
    cipher->length += 11 + strlen(realm);
    bcopy((char *)(ticket->dat),(char *)(cipher->dat+cipher->length),
	  ticket->length);
    cipher->length += ticket->length;

    /* Set the ticket length at beginning of ciphertext */
    *(cipher->dat+n) = ticket->length;
    return(KSUCCESS);
}
