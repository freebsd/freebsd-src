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

RCSID("$Id: get_in_tkt.c,v 1.15 1997/03/23 03:53:08 joda Exp $");

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
passwd_to_key(char *user, char *instance, char *realm, void *passwd,
	      des_cblock *key)
{
#ifndef NOENCRYPTION
    des_string_to_key((char *)passwd, key);
#endif
    return 0;
}


int
passwd_to_afskey(char *user, char *instance, char *realm, void *passwd,
		  des_cblock *key)
{
#ifndef NOENCRYPTION
    afs_string_to_key((char *)passwd, realm, key);
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

int
krb_get_pw_in_tkt(char *user, char *instance, char *realm, char *service,
		  char *sinstance, int life, char *password)
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

    code = krb_get_in_tkt(user,instance,realm,service,sinstance,life,
                          passwd_to_key, NULL, password);
    if (code == INTK_BADPW)
	 code = krb_get_in_tkt(user,instance,realm,service,sinstance,life,
			       passwd_to_afskey, NULL, password);
    if (password == pword)
        memset(pword, 0, sizeof(pword));
    return(code);
}
