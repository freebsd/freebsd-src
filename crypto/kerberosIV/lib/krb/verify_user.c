/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$Id: verify_user.c,v 1.14 1999/03/16 17:31:39 assar Exp $");

/*
 * Verify user (name.instance@realm) with `password'.
 *
 * If secure, also verify against local
 * service key (`linstance'.hostname) (or rcmd if linstance == NULL),
 * this can (usually) only be done by root.
 *
 * If secure == KRB_VERIFY_SECURE, fail if there's no key.
 * If secure == KRB_VERIFY_SECURE_FAIL, don't fail if there's no such
 * key in the srvtab.
 *
 * As a side effect, fresh tickets are obtained.
 *
 * srvtab is where the key is found.
 * 
 * Returns zero if ok, a positive kerberos error or -1 for system
 * errors.
 */

static int
krb_verify_user_srvtab_exact(char *name,
			     char *instance,
			     char *realm,
			     char *password, 
			     int secure,
			     char *linstance,
			     char *srvtab)
{
    int ret;

    ret = krb_get_pw_in_tkt(name, instance, realm,
			    KRB_TICKET_GRANTING_TICKET,
			    realm,
			    DEFAULT_TKT_LIFE, password);
    if(ret != KSUCCESS)
	return ret;

    if(secure == KRB_VERIFY_SECURE || secure == KRB_VERIFY_SECURE_FAIL){
	struct hostent *hp;
	int32_t addr;
	
	KTEXT_ST ticket;
	AUTH_DAT auth;

	char lrealm[REALM_SZ];
	char hostname[MaxHostNameLen];
	char *phost;

	if (gethostname(hostname, sizeof(hostname)) == -1) {
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

	if(secure == KRB_VERIFY_SECURE_FAIL) {
	    des_cblock key;
	    ret = read_service_key(linstance, phost, lrealm, 0, srvtab, &key);
	    memset(key, 0, sizeof(key));
	    if(ret == KFAILURE)
		return 0;
	}
	
	ret = krb_mk_req(&ticket, linstance, phost, lrealm, 33);
	if(ret != KSUCCESS){
	    dest_tkt();
	    return ret;
	}
	
	ret = krb_rd_req(&ticket, linstance, phost, addr, &auth, srvtab);
	if(ret != KSUCCESS){
	    dest_tkt();
	    return ret;
	}
    }
    return 0;
}
		
/*
 *
 */

int
krb_verify_user_srvtab(char *name,
		       char *instance,
		       char *realm,
		       char *password, 
		       int secure,
		       char *linstance,
		       char *srvtab)
{
  int n;
  char rlm[256];
#define ERICSSON_COMPAT 1
#ifdef  ERICSSON_COMPAT
  FILE *f;

  f = fopen ("/etc/krb.localrealms", "r");
  if (f != NULL) {
    while (fgets(rlm, sizeof(rlm), f) != NULL) {
      if (rlm[strlen(rlm) - 1] == '\n')
	rlm[strlen(rlm) - 1] = '\0';

      if (krb_verify_user_srvtab_exact(name, instance, rlm, password,
				       secure, linstance, srvtab)
	  == KSUCCESS) {
	fclose(f);
	return KSUCCESS;
      }
    }
    fclose (f);
    return krb_verify_user_srvtab_exact(name, instance, realm, password,
					secure, linstance, srvtab);
  }
#endif
  /* First try to verify against the supplied realm. */
  if (krb_verify_user_srvtab_exact(name, instance, realm, password,
				   secure, linstance, srvtab)
      == KSUCCESS)
    return KSUCCESS;

  /* Verify all local realms, except the supplied realm. */
  for (n = 1; krb_get_lrealm(rlm, n) == KSUCCESS; n++)
    if (strcmp(rlm, realm) != 0)
      if (krb_verify_user_srvtab_exact(name, instance, rlm, password,
				       secure, linstance, srvtab)
	  == KSUCCESS)
	return KSUCCESS;

  return KFAILURE;
}

/*
 * Compat function without srvtab.
 */

int
krb_verify_user(char *name,
		char *instance,
		char *realm,
		char *password, 
		int secure,
		char *linstance)
{
    return krb_verify_user_srvtab (name,
				   instance,
				   realm,
				   password,
				   secure,
				   linstance,
				   KEYFILE);
}
