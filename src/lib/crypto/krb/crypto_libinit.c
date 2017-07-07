/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "crypto_int.h"

MAKE_INIT_FUNCTION(cryptoint_initialize_library);
MAKE_FINI_FUNCTION(cryptoint_cleanup_library);

/*
 * Initialize the crypto library.
 */

int cryptoint_initialize_library (void)
{
    int err;
    err = k5_prng_init();
    if (err)
        return err;
    return krb5int_crypto_impl_init();
}

int krb5int_crypto_init(void)
{
    return CALL_INIT_FUNCTION(cryptoint_initialize_library);
}

/*
 * Clean up the crypto library state
 */

void cryptoint_cleanup_library (void)
{
    if (!INITIALIZER_RAN(cryptoint_initialize_library))
        return;
    k5_prng_cleanup();
    krb5int_crypto_impl_cleanup();
}
