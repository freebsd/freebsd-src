/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: get_in_tkt.c,v 1.24 1999/11/25 05:22:43 assar Exp $");

/*
 * This file contains three routines: passwd_to_key() and
 * passwd_to_afskey() converts a password into a DES key, using the
 * normal strinttokey and the AFS one, respectively, and
 * krb_get_pw_in_tkt() gets an initial ticket for a user.  
 */

/*
 * passwd_to_key() and passwd_to_afskey: given a password, return a DES key.
 */

int
passwd_to_key(const char *user,
	      const char *instance,
	      const char *realm,
	      const void *passwd,
	      des_cblock *key)
{
#ifndef NOENCRYPTION
    des_string_to_key((char *)passwd, key);
#endif
    return 0;
}

int
passwd_to_5key(const char *user,
	       const char *instance,
	       const char *realm,
	       const void *passwd, 
	       des_cblock *key)
{
    char *p;
    size_t len;
    len = roken_mconcat (&p, 512, passwd, realm, user, instance, NULL);
    if(len == 0)
	return  -1;
    des_string_to_key(p, key);
    memset(p, 0, len);
    free(p);
    return 0;
}


int
passwd_to_afskey(const char *user,
		 const char *instance,
		 const char *realm,
		 const void *passwd,
		 des_cblock *key)
{
#ifndef NOENCRYPTION
    afs_string_to_key(passwd, realm, key);
#endif
    return (0);
}

/*
 * krb_get_pw_in_tkt() takes the name of the server for which the initial
 * ticket is to be obtained, the name of the principal the ticket is
 * for, the desired lifetime of the ticket, and the user's password.
 * It passes its arguments on to krb_get_in_tkt(), which contacts
 * Kerberos to get the ticket, decrypts it using the password provided,
 * and stores it away for future use.
 *
 * krb_get_pw_in_tkt() passes two additional arguments to krb_get_in_tkt():
 * the name of a routine (passwd_to_key()) to be used to get the
 * password in case the "password" argument is null and NULL for the
 * decryption procedure indicating that krb_get_in_tkt should use the 
 * default method of decrypting the response from the KDC.
 *
 * The result of the call to krb_get_in_tkt() is returned.
 */

typedef int (*const_key_proc_t) __P((const char *name,
				     const char *instance, /* IN parameter */
				     const char *realm,
				     const void *password,
				     des_cblock *key));

int
krb_get_pw_in_tkt2(const char *user,
		   const char *instance,
		   const char *realm,
		   const char *service,
		   const char *sinstance,
		   int life,
		   const char *password,
		   des_cblock *key)
{
    char pword[100];		/* storage for the password */
    int code;

    /* Only request password once! */
    if (!password) {
        if (des_read_pw_string(pword, sizeof(pword)-1, "Password: ", 0)){
	    memset(pword, 0, sizeof(pword));
	    return INTK_BADPW;
	}
        password = pword;
    }

    {
	KTEXT_ST as_rep;
	CREDENTIALS cred;
	int ret = 0;
	const_key_proc_t key_procs[] = { passwd_to_key,
					 passwd_to_afskey, 
					 passwd_to_5key,
					 NULL };
	const_key_proc_t *kp;
	
	code = krb_mk_as_req(user, instance, realm,
			     service, sinstance, life, &as_rep);
	if(code)
	    return code;
	for(kp = key_procs; *kp; kp++){
	    KTEXT_ST tmp;
	    memcpy(&tmp, &as_rep, sizeof(as_rep));
	    code = krb_decode_as_rep(user,
				     (char *)instance, /* const_key_proc_t */
				     realm,
				     service,
				     sinstance, 
				     (key_proc_t)*kp, /* const_key_proc_t */
				     NULL,
				     password,
				     &tmp,
				     &cred);
	    if(code == 0){
		if(key)
		    (**kp)(user, instance, realm, password, key);
		break;
	    }
	    if(code != INTK_BADPW)
		ret = code; /* this is probably a better code than
			       what code gets after this loop */
	}
	if(code)
	    return ret ? ret : code;

	code = tf_setup(&cred, user, instance);
	if (code == KSUCCESS) {
	  if (krb_get_config_bool("nat_in_use"))
	    krb_add_our_ip_for_realm(user, instance, realm, password);
	}
    }
    if (password == pword)
        memset(pword, 0, sizeof(pword));
    return(code);
}

int
krb_get_pw_in_tkt(const char *user,
		  const char *instance,
		  const char *realm,
		  const char *service,
		  const char *sinstance,
		  int life,
		  const char *password)
{
    return krb_get_pw_in_tkt2(user, instance, realm, 
			      service, sinstance, life, password, NULL);
}
