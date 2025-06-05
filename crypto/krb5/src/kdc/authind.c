/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/authind.c - Functions for manipulating authentication indicator lists */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "kdc_util.h"

/* Return true if ind matches an entry in indicators. */
krb5_boolean
authind_contains(krb5_data *const *indicators, const char *ind)
{
    for (; indicators != NULL && *indicators != NULL; indicators++) {
        if (data_eq_string(**indicators, ind))
            return TRUE;
    }
    return FALSE;
}

/* Add ind to *indicators, reallocating as necessary. */
krb5_error_code
authind_add(krb5_context context, const char *ind, krb5_data ***indicators)
{
    size_t count;
    krb5_data **list = *indicators, *dptr, d;

    /* Count the number of existing indicators and check for duplicates. */
    for (count = 0; list != NULL && list[count] != NULL; count++) {
        if (data_eq_string(*list[count], ind))
            return 0;
    }

    /* Allocate space for a new entry. */
    list = realloc(list, (count + 2) * sizeof(*list));
    if (list == NULL)
        return ENOMEM;
    *indicators = list;

    /* Add a copy of ind (as a krb5_data object) to the list. */
    d = string2data((char *)ind);
    if (krb5_copy_data(context, &d, &dptr) != 0)
        return ENOMEM;
    list[count++] = dptr;
    list[count] = NULL;
    return 0;
}

/* Add all auth indicators from authdata to *indicators, reallocating as
 * necessary.  (Currently does not compress duplicates.) */
krb5_error_code
authind_extract(krb5_context context, krb5_authdata **authdata,
                krb5_data ***indicators)
{
    krb5_error_code ret;
    size_t count, scount;
    krb5_authdata **ind_authdata = NULL, **adp;
    krb5_data der_indicators, **strings = NULL, **list = *indicators;

    for (count = 0; list != NULL && list[count] != NULL; count++);

    ret = krb5_find_authdata(context, authdata, NULL,
                             KRB5_AUTHDATA_AUTH_INDICATOR, &ind_authdata);
    if (ret)
        goto cleanup;

    for (adp = ind_authdata; adp != NULL && *adp != NULL; adp++) {
        /* Decode this authdata element into an auth indicator list. */
        der_indicators = make_data((*adp)->contents, (*adp)->length);
        ret = decode_utf8_strings(&der_indicators, &strings);
        if (ret == ENOMEM)
            goto cleanup;
        if (ret)
            continue;

        /* Count the entries in strings and allocate space in list. */
        for (scount = 0; strings != NULL && strings[scount] != NULL; scount++);
        list = realloc(list, (count + scount + 1) * sizeof(*list));
        if (list == NULL) {
            ret = ENOMEM;
            goto cleanup;
        }
        *indicators = list;

        /* Steal the krb5_data pointers from strings and free the array. */
        memcpy(list + count, strings, scount * sizeof(*strings));
        count += scount;
        list[count] = NULL;
        free(strings);
        strings = NULL;
    }

cleanup:
    krb5_free_authdata(context, ind_authdata);
    k5_free_data_ptr_list(strings);
    return ret;
}
