/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: create_auth_reply.c,v 4.10 89/01/13 17:47:38 steiner Exp $
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
 * This routine is called by the Kerberos authentication server
 * to create a reply to an authentication request.  The routine
 * takes the user's name, instance, and realm, the client's
 * timestamp, the number of tickets, the user's key version
 * number and the ciphertext containing the tickets themselves.
 * It constructs a packet and returns a pointer to it.
 *
 * Notes: The packet returned by this routine is static.  Thus, if you
 * intend to keep the result beyond the next call to this routine, you
 * must copy it elsewhere.
 *
 * The packet is built in the following format:
 *
 * 			variable
 * type			or constant	   data
 * ----			-----------	   ----
 *
 * unsigned char	KRB_PROT_VERSION   protocol version number
 *
 * unsigned char	AUTH_MSG_KDC_REPLY protocol message type
 *
 * [least significant	HOST_BYTE_ORDER	   sender's (server's) byte
 *  bit of above field]			   order
 *
 * string		pname		   principal's name
 *
 * string		pinst		   principal's instance
 *
 * string		prealm		   principal's realm
 *
 * unsigned long	time_ws		   client's timestamp
 *
 * unsigned char	n		   number of tickets
 *
 * unsigned long	x_date		   expiration date
 *
 * unsigned char	kvno		   master key version
 *
 * short		w_1		   cipher length
 *
 * ---			cipher->dat	   cipher data
 */

KTEXT
create_auth_reply(pname,pinst,prealm,time_ws,n,x_date,kvno,cipher)
    char *pname;                /* Principal's name */
    char *pinst;                /* Principal's instance */
    char *prealm;               /* Principal's authentication domain */
    long time_ws;               /* Workstation time */
    int n;                      /* Number of tickets */
    unsigned long x_date;	/* Principal's expiration date */
    int kvno;                   /* Principal's key version number */
    KTEXT cipher;               /* Cipher text with tickets and
				 * session keys */
{
    static  KTEXT_ST pkt_st;
    KTEXT pkt = &pkt_st;
    unsigned char *v =  pkt->dat; /* Prot vers number */
    unsigned char *t = (pkt->dat+1); /* Prot message type */
    short w_l;			/* Cipher length */

    /* Create fixed part of packet */
    *v = (unsigned char) KRB_PROT_VERSION;
    *t = (unsigned char) AUTH_MSG_KDC_REPLY;
    *t |= HOST_BYTE_ORDER;

    if (n != 0)
	*v = 3;

    /* Add the basic info */
    (void) strcpy((char *) (pkt->dat+2), pname);
    pkt->length = 3 + strlen(pname);
    (void) strcpy((char *) (pkt->dat+pkt->length),pinst);
    pkt->length += 1 + strlen(pinst);
    (void) strcpy((char *) (pkt->dat+pkt->length),prealm);
    pkt->length += 1 + strlen(prealm);
    /* Workstation timestamp */
    bcopy((char *) &time_ws, (char *) (pkt->dat+pkt->length), 4);
    pkt->length += 4;
    *(pkt->dat+(pkt->length)++) = (unsigned char) n;
    /* Expiration date */
    bcopy((char *) &x_date, (char *) (pkt->dat+pkt->length),4);
    pkt->length += 4;

    /* Now send the ciphertext and info to help decode it */
    *(pkt->dat+(pkt->length)++) = (unsigned char) kvno;
    w_l = (short) cipher->length;
    bcopy((char *) &w_l,(char *) (pkt->dat+pkt->length),2);
    pkt->length += 2;
    bcopy((char *) (cipher->dat), (char *) (pkt->dat+pkt->length),
	  cipher->length);
    pkt->length += cipher->length;

    /* And return the packet */
    return pkt;
}
