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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$Id: kdc_reply.c,v 1.9 1997/04/15 21:52:14 assar Exp $");

static int little_endian; /* XXX ugly */

int
kdc_reply_cred(KTEXT cip, CREDENTIALS *cred)
{
    unsigned char *p = cip->dat;
    
    memcpy(cred->session, p, 8);
    p += 8;
    
    if(p + strlen((char*)p) > cip->dat + cip->length)
	return INTK_BADPW;
    p += krb_get_string(p, cred->service);
    
    if(p + strlen((char*)p) > cip->dat + cip->length)
	return INTK_BADPW;
    p += krb_get_string(p, cred->instance);
    
    if(p + strlen((char*)p) > cip->dat + cip->length)
	return INTK_BADPW;
    p += krb_get_string(p, cred->realm);
    
    if(p + 3 > cip->dat + cip->length)
	return INTK_BADPW;
    cred->lifetime = *p++;
    cred->kvno = *p++;
    cred->ticket_st.length = *p++;
    
    if(p + cred->ticket_st.length + 4 > cip->dat + cip->length)
	return INTK_BADPW;
    memcpy(cred->ticket_st.dat, p, cred->ticket_st.length);
    p += cred->ticket_st.length;
    
    p += krb_get_int(p, (u_int32_t *)&cred->issue_date, 4, little_endian);
    
    return KSUCCESS;
}

int
kdc_reply_cipher(KTEXT reply, KTEXT cip)
{
    unsigned char *p;
    unsigned char pvno;
    unsigned char type;

    char aname[ANAME_SZ];
    char inst[INST_SZ];
    char realm[REALM_SZ];
    
    u_int32_t kdc_time;
    u_int32_t exp_date;
    u_int32_t clen;

    p = reply->dat;

    pvno = *p++;

    if (pvno != KRB_PROT_VERSION )
        return INTK_PROT;
    
    type = *p++;
    little_endian = type & 1;
    
    type &= ~1;

    if(type == AUTH_MSG_ERR_REPLY){
	u_int32_t code;
	p += strlen((char*)p) + 1; /* name */
	p += strlen((char*)p) + 1; /* instance */
	p += strlen((char*)p) + 1; /* realm */
	p += 4; /* time */
	p += krb_get_int(p, &code, 4, little_endian);
	return code;
    }
    if(type != AUTH_MSG_KDC_REPLY)
	return INTK_PROT;

    p += krb_get_nir(p, aname, inst, realm);
    p += krb_get_int(p, &kdc_time, 4, little_endian);
    p++; /* number of tickets */
    p += krb_get_int(p, &exp_date, 4, little_endian);
    p++; /* master key version number */
    p += krb_get_int(p, &clen, 2, little_endian);
    cip->length = clen;
    memcpy(cip->dat, p, clen);
    p += clen;
    
    return KSUCCESS;
}
