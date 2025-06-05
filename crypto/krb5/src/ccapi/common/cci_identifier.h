/* ccapi/common/cci_identifier.h */
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

#ifndef CCI_IDENTIFIER_H
#define CCI_IDENTIFIER_H

#include "cci_types.h"

extern const cci_identifier_t cci_identifier_uninitialized;

cc_int32 cci_identifier_new_uuid (cci_uuid_string_t *out_uuid_string);

cc_int32 cci_identifier_new (cci_identifier_t *out_identifier,
                             cci_uuid_string_t in_server_id);

cc_int32 cci_identifier_copy (cci_identifier_t *out_identifier,
                              cci_identifier_t  in_handle);

cc_int32 cci_identifier_release (cci_identifier_t in_identifier);

cc_int32 cci_identifier_compare (cci_identifier_t  in_identifier,
                                 cci_identifier_t  in_compare_to_identifier,
                                 cc_uint32        *out_equal);

cc_int32 cci_identifier_is_for_server (cci_identifier_t  in_identifier,
                                       cci_uuid_string_t in_server_id,
                                       cc_uint32        *out_is_for_server);

cc_int32 cci_identifier_compare_server_id (cci_identifier_t  in_identifier,
                                           cci_identifier_t  in_compare_to_identifier,
                                           cc_uint32        *out_equal_server_id);

cc_int32 cci_identifier_is_initialized (cci_identifier_t  in_identifier,
                                        cc_uint32        *out_is_initialized);

cc_uint32 cci_identifier_read (cci_identifier_t *out_identifier,
                               k5_ipc_stream      io_stream);

cc_uint32 cci_identifier_write (cci_identifier_t in_identifier,
                                k5_ipc_stream     io_stream);

#endif /* CCI_IDENTIFIER_H */
