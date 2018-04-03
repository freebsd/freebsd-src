/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/krb/enctype_util.c */
/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * krb5int_c_valid_enctype()
 * krb5int_c_weak_enctype()
 * krb5_c_enctype_compare()
 * krb5_string_to_enctype()
 * krb5_enctype_to_string()
 */

#include "crypto_int.h"

krb5_boolean KRB5_CALLCONV
krb5_c_valid_enctype(krb5_enctype etype)
{
    return (find_enctype(etype) != NULL);
}

krb5_boolean KRB5_CALLCONV
krb5int_c_weak_enctype(krb5_enctype etype)
{
    const struct krb5_keytypes *ktp;

    ktp = find_enctype(etype);
    return (ktp != NULL && (ktp->flags & ETYPE_WEAK) != 0);
}

krb5_error_code KRB5_CALLCONV
krb5_c_enctype_compare(krb5_context context, krb5_enctype e1, krb5_enctype e2,
                       krb5_boolean *similar)
{
    const struct krb5_keytypes *ktp1, *ktp2;

    ktp1 = find_enctype(e1);
    ktp2 = find_enctype(e2);
    if (ktp1 == NULL || ktp2 == NULL)
        return KRB5_BAD_ENCTYPE;

    *similar = (ktp1->enc == ktp2->enc && ktp1->str2key == ktp2->str2key);
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_string_to_enctype(char *string, krb5_enctype *enctypep)
{
    int i;
    unsigned int j;
    const char *alias;
    const struct krb5_keytypes *ktp;

    for (i = 0; i < krb5int_enctypes_length; i++) {
        ktp = &krb5int_enctypes_list[i];
        if (strcasecmp(ktp->name, string) == 0) {
            *enctypep = ktp->etype;
            return 0;
        }
        for (j = 0; j < MAX_ETYPE_ALIASES; j++) {
            alias = ktp->aliases[j];
            if (alias == NULL)
                break;
            if (strcasecmp(alias, string) == 0) {
                *enctypep = ktp->etype;
                return 0;
            }
        }
    }

    return EINVAL;
}

krb5_error_code KRB5_CALLCONV
krb5_enctype_to_string(krb5_enctype enctype, char *buffer, size_t buflen)
{
    const struct krb5_keytypes *ktp;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return EINVAL;
    if (strlcpy(buffer, ktp->out_string, buflen) >= buflen)
        return ENOMEM;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_enctype_to_name(krb5_enctype enctype, krb5_boolean shortest,
                     char *buffer, size_t buflen)
{
    const struct krb5_keytypes *ktp;
    const char *name;
    int i;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return EINVAL;
    name = ktp->name;
    if (shortest) {
        for (i = 0; i < MAX_ETYPE_ALIASES; i++) {
            if (ktp->aliases[i] == NULL)
                break;
            if (strlen(ktp->aliases[i]) < strlen(name))
                name = ktp->aliases[i];
        }
    }
    if (strlcpy(buffer, name, buflen) >= buflen)
        return ENOMEM;
    return 0;
}

/* The security of a mechanism cannot be summarized with a simple integer
 * value, but we provide a per-enctype value for Cyrus SASL's SSF. */
krb5_error_code
k5_enctype_to_ssf(krb5_enctype enctype, unsigned int *ssf_out)
{
    const struct krb5_keytypes *ktp;

    *ssf_out = 0;

    ktp = find_enctype(enctype);
    if (ktp == NULL)
        return EINVAL;
    *ssf_out = ktp->ssf;
    return 0;
}
