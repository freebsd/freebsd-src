/* ccapi/lib/mac/ccapi_vector.h */
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

#include <CredentialsCache2.h>


cc_int32 __cc_initialize_vector (cc_context_t  *out_context,
                                 cc_int32       in_version,
                                 cc_int32      *out_supported_version,
                                 char const   **out_vendor);

cc_int32 __cc_string_release_vector (cc_string_t in_string);

cc_int32 __cc_context_release_vector (cc_context_t io_context);

cc_int32 __cc_context_get_change_time_vector (cc_context_t  in_context,
                                              cc_time_t    *out_change_time);

cc_int32 __cc_context_get_default_ccache_name_vector (cc_context_t  in_context,
                                                      cc_string_t  *out_name);

cc_int32 __cc_context_open_ccache_vector (cc_context_t  in_context,
                                          const char   *in_name,
                                          cc_ccache_t  *out_ccache);

cc_int32 __cc_context_open_default_ccache_vector (cc_context_t  in_context,
                                                  cc_ccache_t  *out_ccache);

cc_int32 __cc_context_create_ccache_vector (cc_context_t  in_context,
                                            const char   *in_name,
                                            cc_uint32     in_cred_vers,
                                            const char   *in_principal,
                                            cc_ccache_t  *out_ccache);

cc_int32 __cc_context_create_default_ccache_vector (cc_context_t  in_context,
                                                    cc_uint32     in_cred_vers,
                                                    const char   *in_principal,
                                                    cc_ccache_t  *out_ccache);

cc_int32 __cc_context_create_new_ccache_vector (cc_context_t in_context,
                                                cc_uint32    in_cred_vers,
                                                const char  *in_principal,
                                                cc_ccache_t *out_ccache);

cc_int32 __cc_context_new_ccache_iterator_vector (cc_context_t          in_context,
                                                  cc_ccache_iterator_t *out_iterator);

cc_int32 __cc_context_lock_vector (cc_context_t in_context,
                                   cc_uint32    in_lock_type,
                                   cc_uint32    in_block);

cc_int32 __cc_context_unlock_vector (cc_context_t in_context);

cc_int32 __cc_context_compare_vector (cc_context_t  in_context,
                                      cc_context_t  in_compare_to_context,
                                      cc_uint32    *out_equal);

cc_int32 __cc_ccache_release_vector (cc_ccache_t io_ccache);

cc_int32 __cc_ccache_destroy_vector (cc_ccache_t io_ccache);

cc_int32 __cc_ccache_set_default_vector (cc_ccache_t io_ccache);

cc_uint32 __cc_ccache_get_credentials_version_vector (cc_ccache_t  in_ccache,
                                                      cc_uint32   *out_credentials_version);

cc_int32 __cc_ccache_get_name_vector (cc_ccache_t  in_ccache,
                                      cc_string_t *out_name);

cc_int32 __cc_ccache_get_principal_vector (cc_ccache_t  in_ccache,
                                           cc_uint32    in_credentials_version,
                                           cc_string_t *out_principal);

cc_int32 __cc_ccache_set_principal_vector (cc_ccache_t  io_ccache,
                                           cc_uint32    in_credentials_version,
                                           const char  *in_principal);

cc_int32 __cc_ccache_store_credentials_vector (cc_ccache_t                 io_ccache,
                                               const cc_credentials_union *in_credentials_union);

cc_int32 __cc_ccache_remove_credentials_vector (cc_ccache_t      io_ccache,
                                                cc_credentials_t in_credentials);

cc_int32 __cc_ccache_new_credentials_iterator_vector (cc_ccache_t                in_ccache,
                                                      cc_credentials_iterator_t *out_credentials_iterator);

cc_int32 __cc_ccache_move_vector (cc_ccache_t io_source_ccache,
                                  cc_ccache_t io_destination_ccache);

cc_int32 __cc_ccache_lock_vector (cc_ccache_t io_ccache,
                                  cc_uint32   in_lock_type,
                                  cc_uint32   in_block);

cc_int32 __cc_ccache_unlock_vector (cc_ccache_t io_ccache);

cc_int32 __cc_ccache_get_last_default_time_vector (cc_ccache_t  in_ccache,
                                                   cc_time_t   *out_last_default_time);

cc_int32 __cc_ccache_get_change_time_vector (cc_ccache_t  in_ccache,
                                             cc_time_t   *out_change_time);

cc_int32 __cc_ccache_compare_vector (cc_ccache_t  in_ccache,
                                     cc_ccache_t  in_compare_to_ccache,
                                     cc_uint32   *out_equal);

cc_int32 __cc_credentials_release_vector (cc_credentials_t io_credentials);

cc_int32 __cc_credentials_compare_vector (cc_credentials_t  in_credentials,
                                          cc_credentials_t  in_compare_to_credentials,
                                          cc_uint32        *out_equal);

cc_int32 __cc_ccache_iterator_release_vector (cc_ccache_iterator_t io_ccache_iterator);

cc_int32 __cc_ccache_iterator_next_vector (cc_ccache_iterator_t  in_ccache_iterator,
                                           cc_ccache_t          *out_ccache);

cc_int32 __cc_credentials_iterator_release_vector (cc_credentials_iterator_t io_credentials_iterator);

cc_int32 __cc_credentials_iterator_next_vector (cc_credentials_iterator_t  in_credentials_iterator,
                                                cc_credentials_t          *out_credentials);

cc_int32 __cc_shutdown_vector (apiCB **io_context);

cc_int32 __cc_get_NC_info_vector (apiCB    *in_context,
                                  infoNC ***out_info);

cc_int32 __cc_get_change_time_vector (apiCB     *in_context,
                                      cc_time_t *out_change_time);

cc_int32 __cc_open_vector (apiCB       *in_context,
                           const char  *in_name,
                           cc_int32     in_version,
                           cc_uint32    in_flags,
                           ccache_p   **out_ccache);

cc_int32 __cc_create_vector (apiCB       *in_context,
                             const char  *in_name,
                             const char  *in_principal,
                             cc_int32     in_version,
                             cc_uint32    in_flags,
                             ccache_p   **out_ccache);

cc_int32 __cc_close_vector (apiCB     *in_context,
                            ccache_p **io_ccache);

cc_int32 __cc_destroy_vector (apiCB     *in_context,
                              ccache_p **io_ccache);

cc_int32 __cc_seq_fetch_NCs_begin_vector (apiCB       *in_context,
                                          ccache_cit **out_iterator);

cc_int32 __cc_seq_fetch_NCs_next_vector (apiCB       *in_context,
                                         ccache_p   **out_ccache,
                                         ccache_cit  *in_iterator);

cc_int32 __cc_seq_fetch_NCs_end_vector (apiCB       *in_context,
                                        ccache_cit **io_iterator);

cc_int32 __cc_get_name_vector (apiCB     *in_context,
                               ccache_p  *in_ccache,
                               char     **out_name);

cc_int32 __cc_get_cred_version_vector (apiCB    *in_context,
                                       ccache_p *in_ccache,
                                       cc_int32 *out_version);

cc_int32 __cc_set_principal_vector (apiCB    *in_context,
                                    ccache_p *io_ccache,
                                    cc_int32  in_version,
                                    char     *in_principal);

cc_int32 __cc_get_principal_vector (apiCB      *in_context,
                                    ccache_p   *in_ccache,
                                    char      **out_principal);

cc_int32 __cc_store_vector (apiCB      *in_context,
                            ccache_p   *io_ccache,
                            cred_union  in_credentials);

cc_int32 __cc_remove_cred_vector (apiCB      *in_context,
                                  ccache_p   *in_ccache,
                                  cred_union  in_credentials);

cc_int32 __cc_seq_fetch_creds_begin_vector (apiCB           *in_context,
                                            const ccache_p  *in_ccache,
                                            ccache_cit     **out_iterator);

cc_int32 __cc_seq_fetch_creds_next_vector (apiCB       *in_context,
                                           cred_union **out_creds,
                                           ccache_cit  *in_iterator);

cc_int32 __cc_seq_fetch_creds_end_vector (apiCB       *in_context,
                                          ccache_cit **io_iterator);

cc_int32 __cc_free_principal_vector (apiCB  *in_context,
                                     char  **io_principal);

cc_int32 __cc_free_name_vector (apiCB  *in_context,
                                char  **io_name);

cc_int32 __cc_free_creds_vector (apiCB       *in_context,
                                 cred_union **io_credentials);

cc_int32 __cc_free_NC_info_vector (apiCB    *in_context,
                                   infoNC ***io_info);
