/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "krb_locl.h"

RCSID("$Id: create_ticket.c,v 1.14 1999/12/02 16:58:41 joda Exp $");

/*
 * Create ticket takes as arguments information that should be in a
 * ticket, and the KTEXT object in which the ticket should be
 * constructed.  It then constructs a ticket and returns, leaving the
 * newly created ticket in tkt.
 * The length of the ticket is a multiple of
 * eight bytes and is in tkt->length.
 *
 * If the ticket is too long, the ticket will contain nulls.
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

int
krb_create_ticket(KTEXT tkt,	/* Gets filled in by the ticket */
		  unsigned char flags, /* Various Kerberos flags */
		  char *pname,	/* Principal's name */
		  char *pinstance, /* Principal's instance */
		  char *prealm, /* Principal's authentication domain */
		  int32_t paddress, /* Net address of requesting entity */
		  void *session, /* Session key inserted in ticket */
		  int16_t life,	/* Lifetime of the ticket */
		  int32_t time_sec, /* Issue time and date */
		  char *sname,	/* Service Name */
		  char *sinstance, /* Instance Name */
		  des_cblock *key) /* Service's secret key */
{
    unsigned char *p = tkt->dat;
    int tmp;
    size_t rem = sizeof(tkt->dat);

    memset(tkt, 0, sizeof(KTEXT_ST));

    tmp = krb_put_int(flags, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_nir(pname, pinstance, prealm, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;
    
    tmp = krb_put_address(paddress, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;
    
    if (rem < 8)
	return KFAILURE;
    memcpy(p, session, 8);
    p += 8;
    rem -= 8;

    tmp = krb_put_int(life, p, rem, 1);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(time_sec, p, rem, 4);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_nir(sname, sinstance, NULL, p, rem);
    if (tmp < 0)
	return KFAILURE;
    p += tmp;
    rem -= tmp;

    /* multiple of eight bytes */
    tkt->length = (p - tkt->dat + 7) & ~7;

    /* Check length of ticket */
    if (tkt->length > (sizeof(KTEXT_ST) - 7)) {
        memset(tkt->dat, 0, tkt->length);
        tkt->length = 0;
        return KFAILURE /* XXX */;
    }

    encrypt_ktext(tkt, key, DES_ENCRYPT);
    return KSUCCESS;
}
