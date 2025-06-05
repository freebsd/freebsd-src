/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/gen_rname.c */
/*
 * Copyright 1991 by the Massachusetts Institute of Technology.
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
 * Take a port-style address and unique string, and return
 * a replay cache tag string.
 */

#include "k5-int.h"
#include "os-proto.h"

krb5_error_code
krb5_gen_replay_name(krb5_context context, const krb5_address *address, const char *uniq, char **string)
{
    char * tmp;
    unsigned int i;
    unsigned int len;

    len = strlen(uniq) + (address->length * 2) + 1;
    if ((*string = malloc(len)) == NULL)
        return ENOMEM;

    snprintf(*string, len, "%s", uniq);
    tmp = *string + strlen(uniq);
    for (i = 0; i < address->length; i++) {
        snprintf(tmp, len - (tmp-*string), "%.2x", address->contents[i] & 0xff);
        tmp += 2;
    }
    return 0;
}
