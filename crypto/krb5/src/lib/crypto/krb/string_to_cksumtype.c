/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

#include "crypto_int.h"

krb5_error_code KRB5_CALLCONV
krb5_string_to_cksumtype(char *string, krb5_cksumtype *cksumtypep)
{
    unsigned int i, j;
    const char *alias;
    const struct krb5_cksumtypes *ctp;

    for (i=0; i<krb5int_cksumtypes_length; i++) {
        ctp = &krb5int_cksumtypes_list[i];
        if (strcasecmp(ctp->name, string) == 0) {
            *cksumtypep = ctp->ctype;
            return 0;
        }
#define MAX_ALIASES (sizeof(ctp->aliases) / sizeof(ctp->aliases[0]))
        for (j = 0; j < MAX_ALIASES; j++) {
            alias = ctp->aliases[j];
            if (alias == NULL)
                break;
            if (strcasecmp(alias, string) == 0) {
                *cksumtypep = ctp->ctype;
                return 0;
            }
        }
    }

    return EINVAL;
}
