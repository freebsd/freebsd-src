/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: get_svc_in_tkt.c,v 4.9 89/07/18 16:33:34 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char rcsid[] =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <prot.h>

#ifndef NULL
#define NULL 0
#endif

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

static int
srvtab_to_key(user, instance, realm, srvtab, key)
    char *user, *instance, *realm, *srvtab;
    C_Block key;
{
    if (!srvtab)
        srvtab = KEYFILE;

    return(read_service_key(user, instance, realm, 0, srvtab,
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
krb_get_svc_in_tkt(user, instance, realm, service, sinstance, life, srvtab)
    char *user, *instance, *realm, *service, *sinstance;
    int life;
    char *srvtab;
{
    return(krb_get_in_tkt(user, instance, realm, service, sinstance,
                          life, srvtab_to_key, NULL, srvtab));
}
