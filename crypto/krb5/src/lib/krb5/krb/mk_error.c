/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/mk_error.c */
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
  formats the error structure *dec_err into an error buffer *enc_err.

  The error buffer storage is allocated, and should be freed by the
  caller when finished.

  returns system errors
*/
krb5_error_code KRB5_CALLCONV
krb5_mk_error(krb5_context context, const krb5_error *dec_err,
              krb5_data *enc_err)
{
    krb5_error_code retval;
    krb5_data *new_enc_err;

    if ((retval = encode_krb5_error(dec_err, &new_enc_err)))
        return(retval);
    *enc_err = *new_enc_err;
    free(new_enc_err);
    return 0;
}
