/*
 * Copyright (c) 1999 Thomas Nyström and Stacken Computer Club
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

RCSID("$Id: krb_ip_realm.c,v 1.2.2.1 1999/12/06 23:01:12 assar Exp $");

/*
 * Obtain a ticket for ourselves (`user.instance') in REALM and decrypt
 * it using `password' to verify the address that the KDC got our
 * request from.
 * Store in the ticket cache.
 */

int
krb_add_our_ip_for_realm(const char *user, const char *instance,
			 const char *realm, const char *password)
{
    des_cblock newkey;
    des_key_schedule schedule;
    char scrapbuf[1024];
    struct in_addr myAddr;
    KTEXT_ST ticket;
    CREDENTIALS c;
    int err;
    u_int32_t addr;

    if ((err = krb_mk_req(&ticket, (char *)user, (char *)instance,
			  (char *)realm, 0)) != KSUCCESS)
	return err;

    if ((err = krb_get_cred((char *)user, (char *)instance, (char *)realm,
			    &c)) != KSUCCESS)
	return err;

    des_string_to_key((char *)password, &newkey);
    des_set_key(&newkey, schedule);
    err = decomp_ticket(&c.ticket_st,
			(unsigned char *)scrapbuf, /* Flags */
			scrapbuf,	/* Authentication name */
			scrapbuf,	/* Principal's instance */
			scrapbuf,	/* Principal's authentication domain */
			/* The Address Of Me That Servers Sees */
			(u_int32_t *)&addr,
			(unsigned char *)scrapbuf, /* Session key in ticket */
			(int *)scrapbuf, /* Lifetime of ticket */
			(u_int32_t *)scrapbuf, /* Issue time and date */
			scrapbuf,	/* Service name */
			scrapbuf,	/* Service instance */
			&newkey,	/* Secret key */
			schedule	/* Precomp. key schedule */
	);
	
    if (err != KSUCCESS) {
	memset(newkey, 0, sizeof(newkey));
	memset(schedule, 0, sizeof(schedule));
	return err;
    }

    myAddr.s_addr = addr;

    err = tf_store_addr(realm, &myAddr);

    memset(newkey, 0, sizeof(newkey));
    memset(schedule, 0, sizeof(schedule));

    return err;
}

int
krb_get_our_ip_for_realm(const char *realm, struct in_addr *ip_addr)
{
    return tf_get_addr(realm, ip_addr);
}
