/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: create_ticket.c,v 4.11 89/03/22 14:43:23 jtkohl Exp $
 *	$Id: create_ticket.c,v 1.3 1995/07/18 16:38:12 mark Exp $
 */

#if 0
#ifndef lint
static char rcsid[] =
"$Id: create_ticket.c,v 1.3 1995/07/18 16:38:12 mark Exp $";
#endif /* lint */
#endif

#include <stdio.h>
#include <des.h>
#include <krb.h>
#include <prot.h>
#include <strings.h>

/*
 * Create ticket takes as arguments information that should be in a
 * ticket, and the KTEXT object in which the ticket should be
 * constructed.  It then constructs a ticket and returns, leaving the
 * newly created ticket in tkt.
 * The length of the ticket is a multiple of
 * eight bytes and is in tkt->length.
 *
 * If the ticket is too long, the ticket will contain nulls.
 * The return value of the routine is undefined.
 *
 * The corresponding routine to extract information from a ticket it
 * decomp_ticket.  When changes are made to this routine, the
 * corresponding changes should also be made to that file.
 *
 * The packet is built in the following format:
 *
 * 			variable
 * type			or constant	   data
 * ----			-----------	   ----
 *
 * tkt->length		length of ticket (multiple of 8 bytes)
 *
 * tkt->dat:
 *
 * unsigned char	flags		   namely, HOST_BYTE_ORDER
 *
 * string		pname		   client's name
 *
 * string		pinstance	   client's instance
 *
 * string		prealm		   client's realm
 *
 * 4 bytes		paddress	   client's address
 *
 * 8 bytes		session		   session key
 *
 * 1 byte		life		   ticket lifetime
 *
 * 4 bytes		time_sec	   KDC timestamp
 *
 * string		sname		   service's name
 *
 * string		sinstance	   service's instance
 *
 * <=7 bytes		null		   null pad to 8 byte multiple
 *
 */

int krb_create_ticket(KTEXT tkt, unsigned char flags, char *pname,
    char *pinstance, char *prealm, long paddress, char *session, short life,
    long time_sec, char *sname, char *sinstance, des_cblock key)
{
    Key_schedule key_s;
    register char *data;        /* running index into ticket */

    tkt->length = 0;            /* Clear previous data  */
    flags |= HOST_BYTE_ORDER;   /* ticket byte order   */
    bcopy((char *) &flags,(char *) (tkt->dat),sizeof(flags));
    data = ((char *)tkt->dat) + sizeof(flags);
    (void) strcpy(data, pname);
    data += 1 + strlen(pname);
    (void) strcpy(data, pinstance);
    data += 1 + strlen(pinstance);
    (void) strcpy(data, prealm);
    data += 1 + strlen(prealm);
    bcopy((char *) &paddress, data, 4);
    data += 4;

    bcopy((char *) session, data, 8);
    data += 8;
    *(data++) = (char) life;
    /* issue time */
    bcopy((char *) &time_sec, data, 4);
    data += 4;
    (void) strcpy(data, sname);
    data += 1 + strlen(sname);
    (void) strcpy(data, sinstance);
    data += 1 + strlen(sinstance);

    /* guarantee null padded ticket to multiple of 8 bytes */
    bzero(data, 7);
    tkt->length = ((data - ((char *)tkt->dat) + 7)/8)*8;

    /* Check length of ticket */
    if (tkt->length > (sizeof(KTEXT_ST) - 7)) {
        bzero(tkt->dat, tkt->length);
        tkt->length = 0;
        return KFAILURE /* XXX */;
    }

#ifndef NOENCRYPTION
    key_sched((des_cblock *)key,key_s);
    pcbc_encrypt((des_cblock *)tkt->dat,(des_cblock *)tkt->dat,
	(long)tkt->length,key_s,(des_cblock *)key,ENCRYPT);
#endif
    return 0;
}
