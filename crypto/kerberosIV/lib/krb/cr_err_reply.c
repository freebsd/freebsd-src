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

RCSID("$Id: cr_err_reply.c,v 1.11 1999/12/02 16:58:41 joda Exp $");

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

int
cr_err_reply(KTEXT pkt, char *pname, char *pinst, char *prealm, 
	     u_int32_t time_ws, u_int32_t e, char *e_string)
{
    unsigned char *p = pkt->dat;
    int tmp;
    size_t rem = sizeof(pkt->dat);

    tmp = krb_put_int(KRB_PROT_VERSION, p, rem, 1);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(AUTH_MSG_ERR_REPLY, p, rem, 1);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    if (pname == NULL) pname = "";
    if (pinst == NULL) pinst = "";
    if (prealm == NULL) prealm = "";

    tmp = krb_put_nir(pname, pinst, prealm, p, rem);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(time_ws, p, rem, 4);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_int(e, p, rem, 4);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    tmp = krb_put_string(e_string, p, rem);
    if (tmp < 0)
	return -1;
    p += tmp;
    rem -= tmp;

    pkt->length = p - pkt->dat;
    return 0;
}
