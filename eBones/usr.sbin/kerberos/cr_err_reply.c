/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: cr_err_reply.c,v 4.10 89/01/10 11:34:42 steiner Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

#include <sys/types.h>
#include <krb.h>
#include <prot.h>
#include <strings.h>

extern int req_act_vno;		/* this is defined in the kerberos
				 * server code */

/*
 * This routine is used by the Kerberos authentication server to
 * create an error reply packet to send back to its client.
 *
 * It takes a pointer to the packet to be built, the name, instance,
 * and realm of the principal, the client's timestamp, an error code
 * and an error string as arguments.  Its return value is undefined.
 *
 * The packet is built in the following format:
 *
 * type			variable	   data
 *			or constant
 * ----			-----------	   ----
 *
 * unsigned char	req_ack_vno	   protocol version number
 *
 * unsigned char	AUTH_MSG_ERR_REPLY protocol message type
 *
 * [least significant	HOST_BYTE_ORDER	   sender's (server's) byte
 * bit of above field]			   order
 *
 * string		pname		   principal's name
 *
 * string		pinst		   principal's instance
 *
 * string		prealm		   principal's realm
 *
 * unsigned long	time_ws		   client's timestamp
 *
 * unsigned long	e		   error code
 *
 * string		e_string	   error text
 */

void
cr_err_reply(pkt,pname,pinst,prealm,time_ws,e,e_string)
    KTEXT pkt;
    char *pname;		/* Principal's name */
    char *pinst;		/* Principal's instance */
    char *prealm;		/* Principal's authentication domain */
    u_long time_ws;		/* Workstation time */
    u_long e;			/* Error code */
    char *e_string;		/* Text of error */
{
    u_char *v = (u_char *) pkt->dat; /* Prot vers number */
    u_char *t = (u_char *)(pkt->dat+1); /* Prot message type */

    /* Create fixed part of packet */
    *v = (unsigned char) req_act_vno; /* KRB_PROT_VERSION; */
    *t = (unsigned char) AUTH_MSG_ERR_REPLY;
    *t |= HOST_BYTE_ORDER;

    /* Add the basic info */
    (void) strcpy((char *) (pkt->dat+2),pname);
    pkt->length = 3 + strlen(pname);
    (void) strcpy((char *)(pkt->dat+pkt->length),pinst);
    pkt->length += 1 + strlen(pinst);
    (void) strcpy((char *)(pkt->dat+pkt->length),prealm);
    pkt->length += 1 + strlen(prealm);
    /* ws timestamp */
    bcopy((char *) &time_ws,(char *)(pkt->dat+pkt->length),4);
    pkt->length += 4;
    /* err code */
    bcopy((char *) &e,(char *)(pkt->dat+pkt->length),4);
    pkt->length += 4;
    /* err text */
    (void) strcpy((char *)(pkt->dat+pkt->length),e_string);
    pkt->length += 1 + strlen(e_string);

    /* And return */
    return;
}
