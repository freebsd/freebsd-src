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

RCSID("$Id: verify_user.c,v 1.8 1997/04/01 08:18:46 joda Exp $");

/* Verify user with password. If secure, also verify against local
 * service key, this can (usually) only be done by root.
 *
 * As a side effect, fresh tickets are obtained.
 *
 * Returns zero if ok, a positive kerberos error or -1 for system
 * errors.
 */

int
krb_verify_user(char *name, char *instance, char *realm, char *password, 
		int secure, char *linstance)
{
    int ret;
    ret = krb_get_pw_in_tkt(name, instance, realm,
			    KRB_TICKET_GRANTING_TICKET,
			    realm,
			    DEFAULT_TKT_LIFE, password);
    if(ret != KSUCCESS)
	return ret;

    if(secure){
	struct hostent *hp;
	int32_t addr;
	
	KTEXT_ST ticket;
	AUTH_DAT auth;

	char lrealm[REALM_SZ];
	char hostname[MaxHostNameLen];
	char *phost;

	if (k_gethostname(hostname, sizeof(hostname)) == -1) {
	    dest_tkt();
	    return -1;
	}

	hp = gethostbyname(hostname);
	if(hp == NULL){
	    dest_tkt();
	    return -1;
	}
	memcpy(&addr, hp->h_addr, sizeof(addr));

	ret = krb_get_lrealm(lrealm, 1);
	if(ret != KSUCCESS){
	    dest_tkt();
	    return ret;
	}
	phost = krb_get_phost(hostname);
	
	if (linstance == NULL)
	    linstance = "rcmd";

	ret = krb_mk_req(&ticket, linstance, phost, lrealm, 33);
	if(ret != KSUCCESS){
	    dest_tkt();
	    return ret;
	}
	
	ret = krb_rd_req(&ticket, linstance, phost, addr, &auth, "");
	if(ret != KSUCCESS){
	    dest_tkt();
	    return ret;
	}
    }
    return 0;
}
		
