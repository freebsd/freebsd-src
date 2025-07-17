/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/etype_list.c */
/*
 * Copyright (C) 2010 by the Massachusetts Institute of Technology.
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
 *
 * Helper functions related to zero-terminated lists of enctypes.
 */

#include "k5-int.h"
#include "int-proto.h"

size_t
k5_count_etypes(const krb5_enctype *list)
{
    size_t count;

    for (count = 0; list[count]; count++);
    return count;
}

/* Copy the zero-terminated enctype list old_list into *new_list. */
krb5_error_code
k5_copy_etypes(const krb5_enctype *old_list, krb5_enctype **new_list)
{
    size_t count;
    krb5_enctype *list;

    *new_list = NULL;
    if (old_list == NULL)
        return 0;
    count = k5_count_etypes(old_list);
    list = malloc(sizeof(krb5_enctype) * (count + 1));
    if (list == NULL)
        return ENOMEM;
    memcpy(list, old_list, sizeof(krb5_enctype) * (count + 1));
    *new_list = list;
    return 0;
}

krb5_boolean
k5_etypes_contains(const krb5_enctype *list, krb5_enctype etype)
{
    size_t i;

    for (i = 0; list[i] && list[i] != etype; i++);
    return (list[i] == etype);
}
