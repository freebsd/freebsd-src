/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * These routines check and fix parity of encryption keys for the DES
 * algorithm.
 *
 * They are a replacement for routines in key_parity.c, that don't require
 * the table building that they do.
 *
 * Mark Eichin -- Cygnus Support
 */

#include "crypto_int.h"
#include "des_int.h"

#ifdef K5_BUILTIN_DES_KEY_PARITY

/*
 * des_fixup_key_parity: Forces odd parity per byte; parity is bits
 *                       8,16,...64 in des order, implies 0, 8, 16, ...
 *                       vax order.
 */
#define smask(step) ((1<<step)-1)
#define pstep(x,step) (((x)&smask(step))^(((x)>>step)&smask(step)))
#define parity_char(x) pstep(pstep(pstep((x),4),2),1)

void
mit_des_fixup_key_parity(mit_des_cblock key)
{
    unsigned int i;
    for (i=0; i<sizeof(mit_des_cblock); i++)
    {
        key[i] &= 0xfe;
        key[i] |= 1^parity_char(key[i]);
    }

    return;
}

#endif /* K5_BUILTIN_DES_KEY_PARITY */

#ifdef K5_BUILTIN_DES

/*
 * des_check_key_parity: returns true iff key has the correct des parity.
 *                       See des_fix_key_parity for the definition of
 *                       correct des parity.
 */
int
mit_des_check_key_parity(mit_des_cblock key)
{
    unsigned int i;

    for (i=0; i<sizeof(mit_des_cblock); i++)
    {
        if((key[i] & 1) == parity_char(0xfe&key[i]))
        {
            return 0;
        }
    }

    return(1);
}

#endif /* K5_BUILTIN_DES */
