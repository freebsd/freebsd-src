/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/authdata.h */
/*
 * Copyright (C) 2009 by the Massachusetts Institute of Technology.
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

#ifndef KRB_AUTHDATA_H

#define KRB_AUTHDATA_H

#include <k5-int.h>
#include "k5-utf8.h"


/* authdata.c */
krb5_error_code
krb5int_authdata_verify(krb5_context context,
                        krb5_authdata_context,
                        krb5_flags usage,
                        const krb5_auth_context *auth_context,
                        const krb5_keyblock *key,
                        const krb5_ap_req *ap_req);

/* PAC */
/*
 * A PAC consists of a sequence of PAC_INFO_BUFFERs, preceeded by
 * a PACTYPE header. Decoding the contents of the buffers is left
 * to the application (notwithstanding signature verification).
 */

typedef struct _PAC_INFO_BUFFER {
    krb5_ui_4 ulType;
    krb5_ui_4 cbBufferSize;
    uint64_t Offset;
} PAC_INFO_BUFFER;

typedef struct _PACTYPE {
    krb5_ui_4 cBuffers;
    krb5_ui_4 Version;
    PAC_INFO_BUFFER Buffers[1];
} PACTYPE;

struct krb5_pac_data {
    PACTYPE *pac;       /* PAC header + info buffer array */
    krb5_data data;     /* PAC data (including uninitialised header) */
    krb5_boolean verified;
};



#define PAC_ALIGNMENT               8
#define PACTYPE_LENGTH              8U
#define PAC_SIGNATURE_DATA_LENGTH   4U
#define PAC_CLIENT_INFO_LENGTH      10U
#define PAC_INFO_BUFFER_LENGTH  16

#define NT_TIME_EPOCH               11644473600LL

extern krb5plugin_authdata_client_ftable_v0 k5_mspac_ad_client_ftable;
extern krb5plugin_authdata_client_ftable_v0 k5_s4u2proxy_ad_client_ftable;
extern krb5plugin_authdata_client_ftable_v0 k5_authind_ad_client_ftable;

krb5_error_code
k5_pac_locate_buffer(krb5_context context,
                     const krb5_pac pac,
                     krb5_ui_4 type,
                     krb5_data *data);

krb5_error_code
k5_pac_validate_client(krb5_context context,
                       const krb5_pac pac,
                       krb5_timestamp authtime,
                       krb5_const_principal principal);

krb5_error_code
k5_pac_add_buffer(krb5_context context,
                  krb5_pac pac,
                  krb5_ui_4 type,
                  const krb5_data *data,
                  krb5_boolean zerofill,
                  krb5_data *out_data);

krb5_error_code
k5_seconds_since_1970_to_time(krb5_timestamp elapsedSeconds, uint64_t *ntTime);


#endif /* !KRB_AUTHDATA_H */
