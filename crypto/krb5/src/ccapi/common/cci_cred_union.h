/* ccapi/common/cci_cred_union.h */
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

#ifndef CCI_CRED_UNION_H
#define CCI_CRED_UNION_H

#include "cci_types.h"
#include <CredentialsCache2.h>

cc_uint32 cci_credentials_union_release (cc_credentials_union *io_credentials);

cc_uint32 cci_credentials_union_read (cc_credentials_union **out_credentials_union,
                                      k5_ipc_stream           io_stream);

cc_uint32 cci_credentials_union_write (const cc_credentials_union *in_credentials_union,
                                       k5_ipc_stream                io_stream);

cc_uint32 cci_cred_union_release (cred_union *io_cred_union);

cc_uint32 cci_credentials_union_to_cred_union (const cc_credentials_union  *in_credentials_union,
                                               cred_union                 **out_cred_union);

cc_uint32 cci_cred_union_to_credentials_union (const cred_union      *in_cred_union,
                                               cc_credentials_union **out_credentials_union);

cc_uint32 cci_cred_union_compare_to_credentials_union (const cred_union           *in_cred_union_compat,
                                                       const cc_credentials_union *in_credentials_union,
                                                       cc_uint32                  *out_equal);

#endif /* CCI_CRED_UNION_H */
