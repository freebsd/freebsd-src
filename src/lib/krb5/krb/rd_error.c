/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/rd_error.c */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"

/*
 * Parse an error message from enc_errbuf and return an allocated structure
 * containing the error message.  Upon return, dec_error will point to
 * allocated storage which the caller should free when finished.
 *
 * Returns system errors.
 */

krb5_error_code KRB5_CALLCONV
krb5_rd_error(krb5_context context, const krb5_data *enc_errbuf,
              krb5_error **dec_error)
{
    if (!krb5_is_krb_error(enc_errbuf))
        return KRB5KRB_AP_ERR_MSG_TYPE;
    return(decode_krb5_error(enc_errbuf, dec_error));
}
