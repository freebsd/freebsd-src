/* ccapi/server/ccs_ccache.h */
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

#ifndef CCS_CCACHE_H
#define CCS_CCACHE_H

#include "ccs_types.h"

cc_int32 ccs_ccache_new (ccs_ccache_t      *out_ccache,
                         cc_uint32          in_cred_vers,
                         const char        *in_name,
                         const char        *in_principal,
                         ccs_ccache_list_t  io_ccache_list);

cc_int32 ccs_ccache_reset (ccs_ccache_t            io_ccache,
			   ccs_cache_collection_t  io_cache_collection,
                           cc_uint32               in_cred_vers,
                           const char             *in_principal);

cc_int32 ccs_ccache_swap_contents (ccs_ccache_t           io_source_ccache,
                                   ccs_ccache_t           io_destination_ccache,
				   ccs_cache_collection_t io_cache_collection);

cc_int32 ccs_ccache_release (ccs_ccache_t io_ccache);

cc_int32 ccs_ccache_changed (ccs_ccache_t           io_ccache,
                             ccs_cache_collection_t io_cache_collection);

cc_int32 ccs_ccache_compare_identifier (ccs_ccache_t      in_ccache,
                                        cci_identifier_t  in_identifier,
                                        cc_uint32        *out_equal);

cc_int32 ccs_ccache_compare_name (ccs_ccache_t  in_ccache,
                                  const char   *in_name,
                                  cc_uint32    *out_equal);

cc_int32 ccs_ccache_notify_default_state_changed (ccs_ccache_t           io_ccache,
                                                  ccs_cache_collection_t io_cache_collection,
                                                  cc_uint32              in_new_default_state);

cc_int32 ccs_ccache_find_credentials_iterator (ccs_ccache_t                in_ccache,
                                               cci_identifier_t            in_identifier,
                                               ccs_credentials_iterator_t *out_credentials_iterator);

cc_int32 ccs_ccache_write (ccs_ccache_t in_ccache,
                           k5_ipc_stream io_stream);

cc_int32 ccs_ccache_write_name (ccs_ccache_t in_ccache,
                                k5_ipc_stream io_stream);

cc_int32 ccs_ccache_handle_message (ccs_pipe_t              in_client_pipe,
                                    ccs_pipe_t              in_reply_pipe,
                                    ccs_ccache_t            io_ccache,
                                    ccs_cache_collection_t  io_cache_collection,
                                    enum cci_msg_id_t       in_request_name,
                                    k5_ipc_stream            in_request_data,
                                    cc_uint32              *out_will_block,
                                    k5_ipc_stream           *out_reply_data);

#endif /* CCS_CCACHE_H */
