/* ccapi/lib/ccapi_ccache.h */
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

#ifndef CCAPI_CCACHE_H
#define CCAPI_CCACHE_H

#include "cci_common.h"

cc_int32 cci_ccache_new (cc_ccache_t      *out_ccache,
                         cci_identifier_t  in_identifier);

cc_int32 ccapi_ccache_release (cc_ccache_t io_ccache);

cc_int32 cci_ccache_write (cc_ccache_t  in_ccache,
                           k5_ipc_stream in_stream);

cc_int32 ccapi_ccache_destroy (cc_ccache_t io_ccache);

cc_int32 ccapi_ccache_set_default (cc_ccache_t io_ccache);

cc_int32 ccapi_ccache_get_credentials_version (cc_ccache_t  in_ccache,
                                               cc_uint32   *out_credentials_version);

cc_int32 ccapi_ccache_get_name (cc_ccache_t  in_ccache,
                                cc_string_t *out_name);

cc_int32 ccapi_ccache_get_principal (cc_ccache_t  in_ccache,
                                     cc_uint32    in_credentials_version,
                                     cc_string_t *out_principal);

cc_int32 ccapi_ccache_set_principal (cc_ccache_t  io_ccache,
                                     cc_uint32    in_credentials_version,
                                     const char  *in_principal);

cc_int32 ccapi_ccache_store_credentials (cc_ccache_t                 io_ccache,
                                         const cc_credentials_union *in_credentials_union);

cc_int32 ccapi_ccache_remove_credentials (cc_ccache_t      io_ccache,
                                          cc_credentials_t in_credentials);

cc_int32 ccapi_ccache_new_credentials_iterator (cc_ccache_t                in_ccache,
                                                cc_credentials_iterator_t *out_credentials_iterator);

cc_int32 ccapi_ccache_move (cc_ccache_t io_source_ccache,
                            cc_ccache_t io_destination_ccache);

cc_int32 ccapi_ccache_lock (cc_ccache_t io_ccache,
                            cc_uint32   in_lock_type,
                            cc_uint32   in_block);

cc_int32 ccapi_ccache_unlock (cc_ccache_t io_ccache);

cc_int32 ccapi_ccache_get_last_default_time (cc_ccache_t  in_ccache,
                                             cc_time_t   *out_last_default_time);

cc_int32 ccapi_ccache_get_change_time (cc_ccache_t  in_ccache,
                                       cc_time_t   *out_change_time);

cc_int32 ccapi_ccache_wait_for_change (cc_ccache_t  in_ccache);

cc_int32 ccapi_ccache_compare (cc_ccache_t  in_ccache,
                               cc_ccache_t  in_compare_to_ccache,
                               cc_uint32   *out_equal);

cc_int32 ccapi_ccache_get_kdc_time_offset (cc_ccache_t  in_ccache,
                                           cc_uint32    in_credentials_version,
                                           cc_time_t   *out_time_offset);

cc_int32 ccapi_ccache_set_kdc_time_offset (cc_ccache_t io_ccache,
                                           cc_uint32   in_credentials_version,
                                           cc_time_t   in_time_offset);

cc_int32 ccapi_ccache_clear_kdc_time_offset (cc_ccache_t io_ccache,
                                             cc_uint32   in_credentials_version);

cc_int32 cci_ccache_get_compat_version (cc_ccache_t  in_ccache,
                                        cc_uint32   *out_compat_version);

cc_int32 cci_ccache_set_compat_version (cc_ccache_t io_ccache,
                                        cc_uint32   in_compat_version);


#endif /* CCAPI_CCACHE_H */
