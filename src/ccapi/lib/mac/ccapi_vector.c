/* ccapi/lib/mac/ccapi_vector.c */
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

#include "ccapi_vector.h"

#include "ccapi_context.h"
#include "ccapi_string.h"
#include "ccapi_ccache.h"
#include "ccapi_credentials.h"
#include "ccapi_ccache_iterator.h"
#include "ccapi_credentials_iterator.h"

/* ------------------------------------------------------------------------ */

static void cci_swap_string_functions (cc_string_t io_string)
{
    cc_string_f temp = *(io_string->functions);
    *((cc_string_f *)io_string->functions) = *(io_string->vector_functions);
    *((cc_string_f *)io_string->vector_functions) = temp;
}

/* ------------------------------------------------------------------------ */

static void cci_swap_context_functions (cc_context_t io_context)
{
    cc_context_f temp = *(io_context->functions);
    *((cc_context_f *)io_context->functions) = *(io_context->vector_functions);
    *((cc_context_f *)io_context->vector_functions) = temp;
}

/* ------------------------------------------------------------------------ */

static void cci_swap_ccache_functions (cc_ccache_t io_ccache)
{
    cc_ccache_f temp = *(io_ccache->functions);
    *((cc_ccache_f *)io_ccache->functions) = *(io_ccache->vector_functions);
    *((cc_ccache_f *)io_ccache->vector_functions) = temp;
}

/* ------------------------------------------------------------------------ */

static void cci_swap_credentials_functions (cc_credentials_t io_credentials)
{
    cc_credentials_f temp = *(io_credentials->functions);
    *((cc_credentials_f *)io_credentials->functions) = *(io_credentials->otherFunctions);
    *((cc_credentials_f *)io_credentials->otherFunctions) = temp;
}

/* ------------------------------------------------------------------------ */

static void cci_swap_ccache_iterator_functions (cc_ccache_iterator_t io_ccache_iterator)
{
    cc_ccache_iterator_f temp = *(io_ccache_iterator->functions);
    *((cc_ccache_iterator_f *)io_ccache_iterator->functions) = *(io_ccache_iterator->vector_functions);
    *((cc_ccache_iterator_f *)io_ccache_iterator->vector_functions) = temp;
}

/* ------------------------------------------------------------------------ */

static void cci_swap_credentials_iterator_functions (cc_credentials_iterator_t io_credentials_iterator)
{
    cc_credentials_iterator_f temp = *(io_credentials_iterator->functions);
    *((cc_credentials_iterator_f *)io_credentials_iterator->functions) = *(io_credentials_iterator->vector_functions);
    *((cc_credentials_iterator_f *)io_credentials_iterator->vector_functions) = temp;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_initialize_vector (cc_context_t  *out_context,
                                 cc_int32       in_version,
                                 cc_int32      *out_supported_version,
                                 char const   **out_vendor)
{
    return cc_initialize (out_context, in_version, out_supported_version, out_vendor);
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_string_release_vector (cc_string_t in_string)
{
    cci_swap_string_functions (in_string);
    return ccapi_string_release (in_string);
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_release_vector (cc_context_t io_context)
{
    cci_swap_context_functions (io_context);
    return ccapi_context_release (io_context);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_get_change_time_vector (cc_context_t  in_context,
                                              cc_time_t    *out_change_time)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_get_change_time (in_context, out_change_time);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_get_default_ccache_name_vector (cc_context_t  in_context,
                                                      cc_string_t  *out_name)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_get_default_ccache_name (in_context, out_name);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_open_ccache_vector (cc_context_t  in_context,
                                          const char   *in_name,
                                          cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_open_ccache (in_context, in_name, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_open_default_ccache_vector (cc_context_t  in_context,
                                                  cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_open_default_ccache (in_context, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_create_ccache_vector (cc_context_t  in_context,
                                            const char   *in_name,
                                            cc_uint32     in_cred_vers,
                                            const char   *in_principal,
                                            cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_create_ccache (in_context, in_name, in_cred_vers, in_principal, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}


/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_create_default_ccache_vector (cc_context_t  in_context,
                                                    cc_uint32     in_cred_vers,
                                                    const char   *in_principal,
                                                    cc_ccache_t  *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_create_default_ccache (in_context, in_cred_vers, in_principal, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_create_new_ccache_vector (cc_context_t in_context,
                                                cc_uint32    in_cred_vers,
                                                const char  *in_principal,
                                                cc_ccache_t *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_create_new_ccache (in_context, in_cred_vers, in_principal, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_new_ccache_iterator_vector (cc_context_t          in_context,
                                                  cc_ccache_iterator_t *out_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_new_ccache_iterator (in_context, out_iterator);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_lock_vector (cc_context_t in_context,
                                   cc_uint32    in_lock_type,
                                   cc_uint32    in_block)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_lock (in_context, in_lock_type, in_block);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_unlock_vector (cc_context_t in_context)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_unlock (in_context);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_context_compare_vector (cc_context_t  in_context,
                                      cc_context_t  in_compare_to_context,
                                      cc_uint32    *out_equal)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = ccapi_context_compare (in_context, in_compare_to_context, out_equal);
    cci_swap_context_functions (in_context);
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_release_vector (cc_ccache_t io_ccache)
{
    cci_swap_ccache_functions (io_ccache);
    return ccapi_ccache_release (io_ccache);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_destroy_vector (cc_ccache_t io_ccache)
{
    cci_swap_ccache_functions (io_ccache);
    return ccapi_ccache_destroy (io_ccache);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_set_default_vector (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    err = ccapi_ccache_set_default (io_ccache);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_uint32 __cc_ccache_get_credentials_version_vector (cc_ccache_t  in_ccache,
                                                      cc_uint32   *out_credentials_version)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_get_credentials_version (in_ccache, out_credentials_version);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_get_name_vector (cc_ccache_t  in_ccache,
                                      cc_string_t *out_name)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_get_name (in_ccache, out_name);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_get_principal_vector (cc_ccache_t  in_ccache,
                                           cc_uint32    in_credentials_version,
                                           cc_string_t *out_principal)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_get_principal (in_ccache, in_credentials_version, out_principal);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_set_principal_vector (cc_ccache_t  io_ccache,
                                           cc_uint32    in_credentials_version,
                                           const char  *in_principal)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    err = ccapi_ccache_set_principal (io_ccache, in_credentials_version, in_principal);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_store_credentials_vector (cc_ccache_t                 io_ccache,
                                               const cc_credentials_union *in_credentials_union)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    err = ccapi_ccache_store_credentials (io_ccache, in_credentials_union);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_remove_credentials_vector (cc_ccache_t      io_ccache,
                                                cc_credentials_t in_credentials)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    cci_swap_credentials_functions (in_credentials);
    err = ccapi_ccache_remove_credentials (io_ccache, in_credentials);
    cci_swap_ccache_functions (io_ccache);
    cci_swap_credentials_functions (in_credentials);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_new_credentials_iterator_vector (cc_ccache_t                in_ccache,
                                                      cc_credentials_iterator_t *out_credentials_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_new_credentials_iterator (in_ccache, out_credentials_iterator);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_move_vector (cc_ccache_t io_source_ccache,
                                  cc_ccache_t io_destination_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_source_ccache);
    cci_swap_ccache_functions (io_destination_ccache);
    err = ccapi_ccache_move (io_source_ccache, io_destination_ccache);
    cci_swap_ccache_functions (io_source_ccache);
    cci_swap_ccache_functions (io_destination_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_lock_vector (cc_ccache_t io_ccache,
                                  cc_uint32   in_lock_type,
                                  cc_uint32   in_block)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    err = ccapi_ccache_lock (io_ccache, in_lock_type, in_block);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_unlock_vector (cc_ccache_t io_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (io_ccache);
    err = ccapi_ccache_unlock (io_ccache);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_get_last_default_time_vector (cc_ccache_t  in_ccache,
                                                   cc_time_t   *out_last_default_time)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_get_last_default_time (in_ccache, out_last_default_time);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_get_change_time_vector (cc_ccache_t  in_ccache,
                                             cc_time_t   *out_change_time)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    err = ccapi_ccache_get_change_time (in_ccache, out_change_time);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_compare_vector (cc_ccache_t  in_ccache,
                                     cc_ccache_t  in_compare_to_ccache,
                                     cc_uint32   *out_equal)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_functions (in_ccache);
    cci_swap_ccache_functions (in_compare_to_ccache);
    err = ccapi_ccache_compare (in_ccache, in_compare_to_ccache, out_equal);
    cci_swap_ccache_functions (in_ccache);
    cci_swap_ccache_functions (in_compare_to_ccache);
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_credentials_release_vector (cc_credentials_t io_credentials)
{
    cci_swap_credentials_functions (io_credentials);
    return ccapi_credentials_release (io_credentials);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_credentials_compare_vector (cc_credentials_t  in_credentials,
                                          cc_credentials_t  in_compare_to_credentials,
                                          cc_uint32        *out_equal)
{
    cc_int32 err = ccNoError;
    cci_swap_credentials_functions (in_credentials);
    cci_swap_credentials_functions (in_compare_to_credentials);
    err = ccapi_credentials_compare (in_credentials, in_compare_to_credentials, out_equal);
    cci_swap_credentials_functions (in_credentials);
    cci_swap_credentials_functions (in_compare_to_credentials);
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_iterator_release_vector (cc_ccache_iterator_t io_ccache_iterator)
{
    cci_swap_ccache_iterator_functions (io_ccache_iterator);
    return ccapi_ccache_iterator_release (io_ccache_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_ccache_iterator_next_vector (cc_ccache_iterator_t  in_ccache_iterator,
                                           cc_ccache_t          *out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_ccache_iterator_functions (in_ccache_iterator);
    err = ccapi_ccache_iterator_next (in_ccache_iterator, out_ccache);
    cci_swap_ccache_iterator_functions (in_ccache_iterator);
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_credentials_iterator_release_vector (cc_credentials_iterator_t io_credentials_iterator)
{
    cci_swap_credentials_iterator_functions (io_credentials_iterator);
    return ccapi_credentials_iterator_release (io_credentials_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_credentials_iterator_next_vector (cc_credentials_iterator_t  in_credentials_iterator,
                                                cc_credentials_t          *out_credentials)
{
    cc_int32 err = ccNoError;
    cci_swap_credentials_iterator_functions (in_credentials_iterator);
    err = ccapi_credentials_iterator_next (in_credentials_iterator, out_credentials);
    cci_swap_credentials_iterator_functions (in_credentials_iterator);
    return err;
}

#pragma mark -

/* ------------------------------------------------------------------------ */

cc_int32 __cc_shutdown_vector (apiCB **io_context)
{
    cci_swap_context_functions (*io_context);
    return cc_shutdown (io_context);
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_get_NC_info_vector (apiCB    *in_context,
                                  infoNC ***out_info)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_get_NC_info (in_context, out_info);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_get_change_time_vector (apiCB     *in_context,
                                      cc_time_t *out_change_time)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_get_change_time (in_context, out_change_time);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_open_vector (apiCB       *in_context,
                           const char  *in_name,
                           cc_int32     in_version,
                           cc_uint32    in_flags,
                           ccache_p   **out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_open (in_context, in_name, in_version, in_flags, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_create_vector (apiCB       *in_context,
                             const char  *in_name,
                             const char  *in_principal,
                             cc_int32     in_version,
                             cc_uint32    in_flags,
                             ccache_p   **out_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_create (in_context, in_name, in_principal, in_version, in_flags, out_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_close_vector (apiCB     *in_context,
                            ccache_p **io_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (*io_ccache);
    err = cc_close (in_context, io_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_destroy_vector (apiCB     *in_context,
                              ccache_p **io_ccache)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (*io_ccache);
    err = cc_destroy (in_context, io_ccache);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_NCs_begin_vector (apiCB       *in_context,
                                          ccache_cit **out_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_seq_fetch_NCs_begin (in_context, out_iterator);
    cci_swap_context_functions (in_context);
    return err;
}


/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_NCs_next_vector (apiCB       *in_context,
                                         ccache_p   **out_ccache,
                                         ccache_cit  *in_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_iterator_functions ((ccache_cit_ccache *)in_iterator);
    err = cc_seq_fetch_NCs_next (in_context, out_ccache, in_iterator);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_iterator_functions ((ccache_cit_ccache *)in_iterator);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_NCs_end_vector (apiCB       *in_context,
                                        ccache_cit **io_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_iterator_functions ((ccache_cit_ccache *) *io_iterator);
    err = cc_seq_fetch_NCs_end (in_context, io_iterator);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_get_name_vector (apiCB     *in_context,
                               ccache_p  *in_ccache,
                               char     **out_name)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    err = cc_get_name (in_context, in_ccache, out_name);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_get_cred_version_vector (apiCB    *in_context,
                                       ccache_p *in_ccache,
                                       cc_int32 *out_version)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    err = cc_get_cred_version (in_context, in_ccache, out_version);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_set_principal_vector (apiCB    *in_context,
                                    ccache_p *io_ccache,
                                    cc_int32  in_version,
                                    char     *in_principal)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (io_ccache);
    err = cc_set_principal (in_context, io_ccache, in_version, in_principal);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_get_principal_vector (apiCB      *in_context,
                                    ccache_p   *in_ccache,
                                    char      **out_principal)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    err = cc_get_principal (in_context, in_ccache, out_principal);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_store_vector (apiCB      *in_context,
                            ccache_p   *io_ccache,
                            cred_union  in_credentials)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (io_ccache);
    err = cc_store (in_context, io_ccache, in_credentials);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (io_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_remove_cred_vector (apiCB      *in_context,
                                  ccache_p   *in_ccache,
                                  cred_union  in_credentials)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    err = cc_remove_cred (in_context, in_ccache, in_credentials);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions (in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_creds_begin_vector (apiCB           *in_context,
                                            const ccache_p  *in_ccache,
                                            ccache_cit     **out_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions ((ccache_p *)in_ccache);
    err = cc_seq_fetch_creds_begin (in_context, in_ccache, out_iterator);
    cci_swap_context_functions (in_context);
    cci_swap_ccache_functions ((ccache_p *)in_ccache);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_creds_next_vector (apiCB       *in_context,
                                           cred_union **out_creds,
                                           ccache_cit  *in_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_credentials_iterator_functions ((ccache_cit_creds *)in_iterator);
    err = cc_seq_fetch_creds_next (in_context, out_creds, in_iterator);
    cci_swap_context_functions (in_context);
    cci_swap_credentials_iterator_functions ((ccache_cit_creds *)in_iterator);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_seq_fetch_creds_end_vector (apiCB       *in_context,
                                          ccache_cit **io_iterator)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    cci_swap_credentials_iterator_functions ((ccache_cit_creds *) *io_iterator);
    err = cc_seq_fetch_creds_end (in_context, io_iterator);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_free_principal_vector (apiCB  *in_context,
                                     char  **io_principal)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_free_principal (in_context, io_principal);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_free_name_vector (apiCB  *in_context,
                                char  **io_name)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_free_name (in_context, io_name);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_free_creds_vector (apiCB       *in_context,
                                 cred_union **io_credentials)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_free_creds (in_context, io_credentials);
    cci_swap_context_functions (in_context);
    return err;
}

/* ------------------------------------------------------------------------ */

cc_int32 __cc_free_NC_info_vector (apiCB    *in_context,
                                   infoNC ***io_info)
{
    cc_int32 err = ccNoError;
    cci_swap_context_functions (in_context);
    err = cc_free_NC_info (in_context, io_info);
    cci_swap_context_functions (in_context);
    return err;
}
