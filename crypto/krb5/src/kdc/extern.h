/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/extern.h */
/*
 * Copyright 1990,2001,2007,2009 by the Massachusetts Institute of Technology.
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

#ifndef __KRB5_KDC_EXTERN__
#define __KRB5_KDC_EXTERN__

/* various externs for KDC */
extern krb5_data        empty_string;   /* an empty string */
extern krb5_timestamp   kdc_infinity;   /* greater than all other timestamps */
extern const int        kdc_modifies_kdb;
extern krb5_int32       max_dgram_reply_size; /* maximum datagram size */

extern const int        vague_errors;
#endif /* __KRB5_KDC_EXTERN__ */
