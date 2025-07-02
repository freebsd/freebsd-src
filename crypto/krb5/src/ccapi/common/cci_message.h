/* ccapi/common/cci_message.h */
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

#ifndef CCI_MESSAGE_H
#define CCI_MESSAGE_H

#include "cci_types.h"

cc_int32 cci_message_invalid_object_err (enum cci_msg_id_t in_request_name);

cc_int32 cci_message_new_request_header (k5_ipc_stream     *out_request,
                                         enum cci_msg_id_t in_request_name,
                                         cci_identifier_t  in_identifier);

cc_int32 cci_message_read_request_header (k5_ipc_stream       in_request,
                                          enum cci_msg_id_t *out_request_name,
                                          cci_identifier_t  *out_identifier);

cc_int32 cci_message_new_reply_header (k5_ipc_stream     *out_reply,
                                       cc_int32          in_error);

cc_int32 cci_message_read_reply_header (k5_ipc_stream in_reply,
                                        cc_int32     *out_reply_error);

uint32_t krb5int_ipc_stream_read_time (k5_ipc_stream  io_stream,
				       cc_time_t     *out_time);
uint32_t krb5int_ipc_stream_write_time (k5_ipc_stream io_stream,
					cc_time_t     in_time);

#endif /* CCI_MESSAGE_H */
