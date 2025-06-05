/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/des/weak_key.c */
/*
 * Copyright 1989,1990 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * Under U.S. law, this software may not be exported outside the US
 * without license from the U.S. Commerce department.
 *
 * These routines form the library interface to the DES facilities.
 *
 * Originally written 8/85 by Steve Miller, MIT Project Athena.
 */

#include "crypto_int.h"
#include "des_int.h"

#ifdef K5_BUILTIN_DES

/*
 * The following are the weak DES keys:
 */
static const mit_des_cblock weak[16] = {
    /* weak keys */
    {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},
    {0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe,0xfe},
    {0x1f,0x1f,0x1f,0x1f,0x0e,0x0e,0x0e,0x0e},
    {0xe0,0xe0,0xe0,0xe0,0xf1,0xf1,0xf1,0xf1},

    /* semi-weak */
    {0x01,0xfe,0x01,0xfe,0x01,0xfe,0x01,0xfe},
    {0xfe,0x01,0xfe,0x01,0xfe,0x01,0xfe,0x01},

    {0x1f,0xe0,0x1f,0xe0,0x0e,0xf1,0x0e,0xf1},
    {0xe0,0x1f,0xe0,0x1f,0xf1,0x0e,0xf1,0x0e},

    {0x01,0xe0,0x01,0xe0,0x01,0xf1,0x01,0xf1},
    {0xe0,0x01,0xe0,0x01,0xf1,0x01,0xf1,0x01},

    {0x1f,0xfe,0x1f,0xfe,0x0e,0xfe,0x0e,0xfe},
    {0xfe,0x1f,0xfe,0x1f,0xfe,0x0e,0xfe,0x0e},

    {0x01,0x1f,0x01,0x1f,0x01,0x0e,0x01,0x0e},
    {0x1f,0x01,0x1f,0x01,0x0e,0x01,0x0e,0x01},

    {0xe0,0xfe,0xe0,0xfe,0xf1,0xfe,0xf1,0xfe},
    {0xfe,0xe0,0xfe,0xe0,0xfe,0xf1,0xfe,0xf1}
};

/*
 * mit_des_is_weak_key: returns true iff key is a [semi-]weak des key.
 *
 * Requires: key has correct odd parity.
 */
int
mit_des_is_weak_key(mit_des_cblock key)
{
    unsigned int i;
    const mit_des_cblock *weak_p = weak;

    for (i = 0; i < (sizeof(weak)/sizeof(mit_des_cblock)); i++) {
        if (!memcmp(weak_p++,key,sizeof(mit_des_cblock)))
            return 1;
    }

    return 0;
}

#endif /* K5_BUILTIN_DES */
