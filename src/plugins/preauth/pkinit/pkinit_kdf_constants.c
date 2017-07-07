/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/pkinit/pkinit_kdf_constants.c */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 * pkinit_kdf_test.c -- Structures and constants for implementation of
 * pkinit algorithm agility.  Includes definitions of algorithm identifiers
 * for SHA-1, SHA-256 and SHA-512.
 */

#include "pkinit.h"

/* statically declare OID constants for all three algorithms */
const krb5_octet krb5_pkinit_sha1_oid[8] =
{0x2B,0x06,0x01,0x05,0x02,0x03,0x06,0x01};
const size_t krb5_pkinit_sha1_oid_len = 8;
const krb5_octet krb5_pkinit_sha256_oid[8] =
{0x2B,0x06,0x01,0x05,0x02,0x03,0x06,0x02};
const size_t krb5_pkinit_sha256_oid_len = 8;
const krb5_octet krb5_pkinit_sha512_oid [8] =
{0x2B,0x06,0x01,0x05,0x02,0x03,0x06,0x03};
const size_t krb5_pkinit_sha512_oid_len = 8;

#define oid_as_data(var, oid_base)                      \
    const krb5_data var =                               \
    {0, sizeof oid_base, (char *)oid_base}
oid_as_data(sha1_id, krb5_pkinit_sha1_oid);
oid_as_data(sha256_id, krb5_pkinit_sha256_oid);
oid_as_data(sha512_id, krb5_pkinit_sha512_oid);
#undef oid_as_data

krb5_data const * const supported_kdf_alg_ids[] = {
    &sha256_id,
    &sha1_id,
    &sha512_id,
    NULL
};
