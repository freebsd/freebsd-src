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

#include "ftp_locl.h"
RCSID("$Id: kauth.c,v 1.14 1997/05/11 04:08:04 assar Exp $");

void kauth(int argc, char **argv)
{
    int ret;
    char buf[1024];
    des_cblock key;
    des_key_schedule schedule;
    KTEXT_ST tkt, tktcopy;
    char *name;
    char *p;
    int overbose;
    char passwd[100];
    int tmp;
	
    if(argc > 2){
	printf("usage: %s [principal]\n", argv[0]);
	code = -1;
	return;
    }
    if(argc == 2)
	name = argv[1];
    else
	name = username;

    overbose = verbose;
    verbose = 0;

    ret = command("SITE KAUTH %s", name);
    if(ret != CONTINUE){
	verbose = overbose;
	code = -1;
	return;
    }
    verbose = overbose;
    p = strstr(reply_string, "T=");
    if(!p){
	printf("Bad reply from server.\n");
	code = -1;
	return;
    }
    p += 2;
    tmp = base64_decode(p, &tkt.dat);
    if(tmp < 0){
	printf("Failed to decode base64 in reply.\n");
	code = -1;
	return;
    }
    tkt.length = tmp;
    tktcopy.length = tkt.length;
    
    p = strstr(reply_string, "P=");
    if(!p){
	printf("Bad reply from server.\n");
	verbose = overbose;
	code = -1;
	return;
    }
    name = p + 2;
    for(; *p && *p != ' ' && *p != '\r' && *p != '\n'; p++);
    *p = 0;
    
    snprintf(buf, sizeof(buf), "Password for %s:", name);
    if (des_read_pw_string (passwd, sizeof(passwd)-1, buf, 0))
        *passwd = '\0';
    des_string_to_key (passwd, &key);

    des_key_sched(&key, schedule);
    
    des_pcbc_encrypt((des_cblock*)tkt.dat, (des_cblock*)tktcopy.dat,
		     tkt.length,
		     schedule, &key, DES_DECRYPT);
    if (strcmp ((char*)tktcopy.dat + 8,
		KRB_TICKET_GRANTING_TICKET) != 0) {
        afs_string_to_key (passwd, krb_realmofhost(hostname), &key);
	des_key_sched (&key, schedule);
	des_pcbc_encrypt((des_cblock*)tkt.dat, (des_cblock*)tktcopy.dat,
			 tkt.length,
			 schedule, &key, DES_DECRYPT);
    }
    memset(key, 0, sizeof(key));
    memset(schedule, 0, sizeof(schedule));
    memset(passwd, 0, sizeof(passwd));
    base64_encode(tktcopy.dat, tktcopy.length, &p);
    memset (tktcopy.dat, 0, tktcopy.length);
    ret = command("SITE KAUTH %s %s", name, p);
    free(p);
    if(ret != COMPLETE){
	code = -1;
	return;
    }
    code = 0;
}

void klist(int argc, char **argv)
{
    int ret;
    if(argc != 1){
	printf("usage: %s\n", argv[0]);
	code = -1;
	return;
    }
    
    ret = command("SITE KLIST");
    code = (ret == COMPLETE);
}
