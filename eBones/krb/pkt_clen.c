/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: pkt_clen.c,v 4.7 88/11/15 16:56:36 jtkohl Exp $
 *	$Id: pkt_clen.c,v 1.3 1995/07/18 16:39:27 mark Exp $
 */

#if 0
#ifndef lint
static char *rcsid =
"$Id: pkt_clen.c,v 1.3 1995/07/18 16:39:27 mark Exp $";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>
#include <string.h>

extern int swap_bytes;

/*
 * Given a pointer to an AUTH_MSG_KDC_REPLY packet, return the length of
 * its ciphertext portion.  The external variable "swap_bytes" is assumed
 * to have been set to indicate whether or not the packet is in local
 * byte order.  pkt_clen() takes this into account when reading the
 * ciphertext length out of the packet.
 */

int pkt_clen(KTEXT pkt)
{
    static unsigned short temp,temp2;
    int clen = 0;

    /* Start of ticket list */
    unsigned char *ptr = pkt_a_realm(pkt) + 10
	+ strlen((char *)pkt_a_realm(pkt));

    /* Finally the length */
    bcopy((char *)(++ptr),(char *)&temp,2); /* alignment */
    if (swap_bytes) {
        /* assume a short is 2 bytes?? */
        swab((char *)&temp,(char *)&temp2,2);
        temp = temp2;
    }

    clen = (int) temp;

    if (krb_debug)
	printf("Clen is %d\n",clen);
    return(clen);
}
