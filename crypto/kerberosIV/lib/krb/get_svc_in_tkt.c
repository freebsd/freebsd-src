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

RCSID("$Id: get_svc_in_tkt.c,v 1.9 1999/06/29 21:18:04 bg Exp $");

/*
 * This file contains two routines: srvtab_to_key(), which gets
 * a server's key from a srvtab file, and krb_get_svc_in_tkt() which
 * gets an initial ticket for a server.
 */

/*
 * srvtab_to_key(): given a "srvtab" file (where the keys for the
 * service on a host are stored), return the private key of the
 * given service (user.instance@realm).
 *
 * srvtab_to_key() passes its arguments on to read_service_key(),
 * plus one additional argument, the key version number.
 * (Currently, the key version number is always 0; this value
 * is treated as a wildcard by read_service_key().)
 *
 * If the "srvtab" argument is null, KEYFILE (defined in "krb.h")
 * is passed in its place.
 *
 * It returns the return value of the read_service_key() call.
 * The service key is placed in "key".
 */

int 
srvtab_to_key(const char *user,
	      char *instance,
	      const char *realm,
	      const void *srvtab,
	      des_cblock *key)
{
    if (!srvtab)
        srvtab = KEYFILE;

    return(read_service_key(user, instance, realm, 0, (char *)srvtab,
                            (char *)key));
}

/*
 * krb_get_svc_in_tkt() passes its arguments on to krb_get_in_tkt(),
 * plus two additional arguments: a pointer to the srvtab_to_key()
 * function to be used to get the key from the key file and a NULL
 * for the decryption procedure indicating that krb_get_in_tkt should 
 * use the default method of decrypting the response from the KDC.
 *
 * It returns the return value of the krb_get_in_tkt() call.
 */

int
krb_get_svc_in_tkt(char *user, char *instance, char *realm, char *service,
		   char *sinstance, int life, char *srvtab)
{
    return(krb_get_in_tkt(user, instance, realm, service, sinstance,
                          life, srvtab_to_key, NULL, srvtab));
}
