/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

RCSID("$Id: decomp_ticket.c,v 1.20 1999/12/02 16:58:41 joda Exp $");

/*
 * This routine takes a ticket and pointers to the variables that
 * should be filled in based on the information in the ticket.  It
 * fills in values for its arguments.
 *
 * The routine returns KFAILURE if any of the "pname", "pinstance",
 * or "prealm" fields is too big, otherwise it returns KSUCCESS.
 *
 * The corresponding routine to generate tickets is create_ticket.
 * When changes are made to this routine, the corresponding changes
 * should also be made to that file.
 *
 * See create_ticket.c for the format of the ticket packet.
 */

int
decomp_ticket(KTEXT tkt,	/* The ticket to be decoded */
	      unsigned char *flags, /* Kerberos ticket flags */
	      char *pname,	/* Authentication name */
	      char *pinstance,	/* Principal's instance */
	      char *prealm,	/* Principal's authentication domain */
	      u_int32_t *paddress,/* Net address of entity requesting ticket */
	      unsigned char *session, /* Session key inserted in ticket */
	      int *life,	/* Lifetime of the ticket */
	      u_int32_t *time_sec, /* Issue time and date */
	      char *sname,	/* Service name */
	      char *sinstance,	/* Service instance */
	      des_cblock *key,	/* Service's secret key (to decrypt the ticket) */
	      des_key_schedule schedule) /* The precomputed key schedule */

{
    unsigned char *p = tkt->dat;
    
    int little_endian;

    des_pcbc_encrypt((des_cblock *)tkt->dat, (des_cblock *)tkt->dat,
		     tkt->length, schedule, key, DES_DECRYPT);

    tkt->mbz = 0;

    *flags = *p++;

    little_endian = *flags & 1;

    if(strlen((char*)p) > ANAME_SZ)
	return KFAILURE;
    p += krb_get_string(p, pname, ANAME_SZ);

    if(strlen((char*)p) > INST_SZ)
	return KFAILURE;
    p += krb_get_string(p, pinstance, INST_SZ);

    if(strlen((char*)p) > REALM_SZ)
	return KFAILURE;
    p += krb_get_string(p, prealm, REALM_SZ);

    if (*prealm == '\0')
	krb_get_lrealm (prealm, 1);

    if(tkt->length - (p - tkt->dat) < 8 + 1 + 4)
	return KFAILURE;
    p += krb_get_address(p, paddress);

    memcpy(session, p, 8);
    p += 8;

    *life = *p++;
    
    p += krb_get_int(p, time_sec, 4, little_endian);

    if(strlen((char*)p) > SNAME_SZ)
	return KFAILURE;
    p += krb_get_string(p, sname, SNAME_SZ);

    if(strlen((char*)p) > INST_SZ)
	return KFAILURE;
    p += krb_get_string(p, sinstance, INST_SZ);

    return KSUCCESS;
}
