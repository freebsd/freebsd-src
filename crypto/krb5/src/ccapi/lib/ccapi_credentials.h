/* ccapi/lib/ccapi_credentials.h */
/*
 * Copyright 2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
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

#ifndef CCAPI_CREDENTIALS_H
#define CCAPI_CREDENTIALS_H

#include "cci_common.h"

cc_int32 cci_credentials_read (cc_credentials_t *out_credentials,
                               k5_ipc_stream      in_stream);

cc_int32 cci_credentials_write (cc_credentials_t in_credentials,
                                k5_ipc_stream     in_stream);

cc_int32 ccapi_credentials_compare (cc_credentials_t  in_credentials,
                                    cc_credentials_t  in_compare_to_credentials,
                                    cc_uint32        *out_equal);

cc_int32 ccapi_credentials_release (cc_credentials_t io_credentials);

#endif /* CCAPI_CREDENTIALS_H */
