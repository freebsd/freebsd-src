/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/securid_sam2/extern.h */
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
 * Declarations for SecurID SAM2 plugin.
 */

krb5_error_code sam_get_db_entry(krb5_context , krb5_principal,
                                 int *, krb5_db_entry **);

krb5_error_code sam_make_challenge(krb5_context context,
                                   krb5_sam_challenge_2_body *sc2b,
                                   krb5_keyblock *cksum_key,
                                   krb5_sam_challenge_2 *sc2_out);

krb5_error_code get_securid_edata_2(krb5_context context,
                                    krb5_db_entry *client,
                                    krb5_keyblock *client_key,
                                    krb5_sam_challenge_2 *sc2);

krb5_error_code verify_securid_data_2(krb5_context context,
                                      krb5_db_entry *client,
                                      krb5_sam_response_2 *sr2,
                                      krb5_enc_tkt_part *enc_tkt_reply,
                                      krb5_pa_data *pa,
                                      krb5_sam_challenge_2 **sc2_out);

krb5_error_code get_grail_edata(krb5_context context, krb5_db_entry *client,
                                krb5_keyblock *client_key,
                                krb5_sam_challenge_2 *sc2_out);

krb5_error_code verify_grail_data(krb5_context context, krb5_db_entry *client,
                                  krb5_sam_response_2 *sr2,
                                  krb5_enc_tkt_part *enc_tkt_reply,
                                  krb5_pa_data *pa,
                                  krb5_sam_challenge_2 **sc2_out);
