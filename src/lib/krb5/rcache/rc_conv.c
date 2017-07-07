/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_conv.c */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * An implementation for the default replay cache type.
 */

#include "rc_base.h"

/*
  Local stuff:
  krb5_auth_to_replay(context, krb5_tkt_authent *auth,krb5_donot_replay *rep)
  given auth, take important information and make rep; return -1 if failed
*/

krb5_error_code
krb5_auth_to_rep(krb5_context context, krb5_tkt_authent *auth, krb5_donot_replay *rep)
{
    krb5_error_code retval;
    rep->cusec = auth->authenticator->cusec;
    rep->ctime = auth->authenticator->ctime;
    if ((retval = krb5_unparse_name(context, auth->ticket->server, &rep->server)))
        return retval; /* shouldn't happen */
    if ((retval = krb5_unparse_name(context, auth->authenticator->client,
                                    &rep->client))) {
        free(rep->server);
        return retval; /* shouldn't happen. */
    }
    return 0;
}

/*
 * Generate a printable hash value for a message for use in a replay
 * record.  It is not necessary for this hash function to be
 * collision-proof (the only thing you can do with a second preimage
 * is produce a false replay error) but for fine granularity replay detection
 * it is necessary for the function to be consistent across implementations.
 * When two implementations sharing a single replay cache don't agree on hash
 * function, the code falls back to legacy replay detection based on
 * (client, server, timestamp, usec) tuples.  We do an unkeyed
 * SHA256 hash of the message and convert it into uppercase hex
 * representation.
 */
krb5_error_code
krb5_rc_hash_message(krb5_context context, const krb5_data *message,
                     char **out)
{
    krb5_error_code retval;
    uint8_t cksum[K5_SHA256_HASHLEN];
    char *hash, *ptr;
    unsigned int i;

    *out = NULL;

    /* Calculate the binary checksum. */
    retval = k5_sha256(message, cksum);
    if (retval)
        return retval;

    /* Convert the checksum into printable form. */
    hash = malloc(K5_SHA256_HASHLEN * 2 + 1);
    if (!hash) {
        return KRB5_RC_MALLOC;
    }

    for (i = 0, ptr = hash; i < K5_SHA256_HASHLEN; i++, ptr += 2)
        snprintf(ptr, 3, "%02X", cksum[i]);
    *ptr = '\0';
    *out = hash;
    return 0;
}
