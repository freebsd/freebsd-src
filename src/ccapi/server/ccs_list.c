/* ccapi/server/ccs_list.c */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
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

#include "ccs_common.h"
#include "ccs_list_internal.h"

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_list_object_release (ccs_list_object_t io_object)
{
    return cci_check_error (ccs_cache_collection_release ((ccs_cache_collection_t) io_object));
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_cache_collection_list_object_compare_identifier (ccs_list_object_t in_cache_collection,
                                                                     cci_identifier_t  in_identifier,
                                                                     cc_uint32        *out_equal)
{
    return ccs_cache_collection_compare_identifier ((ccs_cache_collection_t) in_cache_collection,
                                                    in_identifier,
                                                    out_equal);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_new (ccs_cache_collection_list_t *out_list)
{
    return ccs_list_new (out_list,
                         ccErrInvalidContext,
                         ccErrInvalidContext,
                         ccs_cache_collection_list_object_compare_identifier,
                         ccs_cache_collection_list_object_release);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_count (ccs_cache_collection_list_t  in_list,
                                          cc_uint64                   *out_count)
{
    return ccs_list_count (in_list, out_count);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_find (ccs_cache_collection_list_t  in_list,
                                         cci_identifier_t             in_identifier,
                                         ccs_cache_collection_t       *out_cache_collection)
{
    return ccs_list_find (in_list, in_identifier, (ccs_list_object_t *) out_cache_collection);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_add (ccs_cache_collection_list_t io_list,
                                        ccs_cache_collection_t      in_cache_collection)
{
    return ccs_list_add (io_list, (ccs_list_object_t) in_cache_collection);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_remove (ccs_cache_collection_list_t io_list,
                                           cci_identifier_t            in_identifier)
{
    return ccs_list_remove (io_list, in_identifier);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_cache_collection_list_release (ccs_cache_collection_list_t io_list)
{
    return ccs_list_release (io_list);
}

#ifdef TARGET_OS_MAC
#pragma mark -
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_list_object_release (ccs_list_object_t io_ccache)
{
    return cci_check_error (ccs_ccache_release ((ccs_ccache_t) io_ccache));
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_ccache_list_object_compare_identifier (ccs_list_object_t  in_ccache,
                                                           cci_identifier_t   in_identifier,
                                                           cc_uint32         *out_equal)
{
    return ccs_ccache_compare_identifier ((ccs_ccache_t) in_ccache,
                                          in_identifier,
                                          out_equal);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_new (ccs_ccache_list_t *out_list)
{
    return ccs_list_new (out_list,
                         ccErrInvalidCCache,
                         ccErrInvalidCCacheIterator,
                         ccs_ccache_list_object_compare_identifier,
                         ccs_ccache_list_object_release);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_new_iterator (ccs_ccache_list_t           in_list,
                                       ccs_pipe_t                  in_client_pipe,
                                       ccs_ccache_list_iterator_t *out_list_iterator)
{
    return ccs_list_new_iterator (in_list, in_client_pipe, out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_count (ccs_ccache_list_t  in_list,
                                cc_uint64         *out_count)
{
    return ccs_list_count (in_list, out_count);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_find (ccs_ccache_list_t  in_list,
                               cci_identifier_t   in_identifier,
                               ccs_ccache_t      *out_ccache)
{
    return ccs_list_find (in_list, in_identifier, (ccs_list_object_t *) out_ccache);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_find_iterator (ccs_ccache_list_t           in_list,
                                        cci_identifier_t            in_identifier,
                                        ccs_ccache_list_iterator_t *out_list_iterator)
{
    return ccs_list_find_iterator (in_list, in_identifier,
                                   (ccs_list_iterator_t *) out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_add (ccs_ccache_list_t io_list,
                              ccs_ccache_t      in_ccache)
{
    return ccs_list_add (io_list, (ccs_list_object_t) in_ccache);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_remove (ccs_ccache_list_t io_list,
                                 cci_identifier_t  in_identifier)
{
    return ccs_list_remove (io_list, in_identifier);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_push_front (ccs_ccache_list_t io_list,
                                     cci_identifier_t  in_identifier)
{
    return ccs_list_push_front (io_list, in_identifier);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_release (ccs_ccache_list_t io_list)
{
    return ccs_list_release (io_list);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_iterator_write (ccs_ccache_list_iterator_t in_list_iterator,
                                         k5_ipc_stream               in_stream)
{
    return ccs_list_iterator_write (in_list_iterator, in_stream);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_iterator_clone (ccs_ccache_list_iterator_t  in_list_iterator,
                                         ccs_ccache_list_iterator_t *out_list_iterator)
{
    return ccs_list_iterator_clone (in_list_iterator, out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_iterator_next (ccs_ccache_list_iterator_t  io_list_iterator,
                                        ccs_ccache_t               *out_ccache)
{
    return ccs_list_iterator_next (io_list_iterator, (ccs_list_object_t *) out_ccache);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_ccache_list_iterator_release (ccs_ccache_list_iterator_t io_list_iterator)
{
    return ccs_list_iterator_release (io_list_iterator);
}

#ifdef TARGET_OS_MAC
#pragma mark-
#endif

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_credentials_list_object_release (ccs_list_object_t io_object)
{
    return cci_check_error (ccs_credentials_release ((ccs_credentials_t) io_object));
}

/* ------------------------------------------------------------------------ */

static cc_int32 ccs_credentials_list_object_compare_identifier (ccs_list_object_t  in_credentials,
                                                                cci_identifier_t   in_identifier,
                                                                cc_uint32         *out_equal)
{
    return ccs_credentials_compare_identifier ((ccs_credentials_t) in_credentials,
                                               in_identifier,
                                               out_equal);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_new (ccs_credentials_list_t *out_list)
{
    return ccs_list_new (out_list,
                         ccErrInvalidCredentials,
                         ccErrInvalidCredentialsIterator,
                         ccs_credentials_list_object_compare_identifier,
                         ccs_credentials_list_object_release);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_new_iterator (ccs_credentials_list_t              in_list,
                                            ccs_pipe_t                          in_client_pipe,
                                            ccs_credentials_list_iterator_t    *out_list_iterator)
{
    return ccs_list_new_iterator (in_list, in_client_pipe, out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_count (ccs_credentials_list_t  in_list,
                                     cc_uint64              *out_count)
{
    return ccs_list_count (in_list, out_count);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_find (ccs_credentials_list_t  in_list,
                                    cci_identifier_t        in_identifier,
                                    ccs_credentials_t      *out_credentials)
{
    return ccs_list_find (in_list, in_identifier, (ccs_list_object_t *) out_credentials);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_find_iterator (ccs_credentials_list_t           in_list,
                                             cci_identifier_t                 in_identifier,
                                             ccs_credentials_list_iterator_t *out_list_iterator)
{
    return ccs_list_find_iterator (in_list, in_identifier,
                                   (ccs_list_iterator_t *) out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_add (ccs_credentials_list_t io_list,
                                   ccs_credentials_t      in_credential)
{
    return ccs_list_add (io_list, (ccs_list_object_t) in_credential);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_remove (ccs_credentials_list_t io_list,
                                      cci_identifier_t       in_identifier)
{
    return ccs_list_remove (io_list, in_identifier);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_release (ccs_credentials_list_t io_list)
{
    return ccs_list_release (io_list);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_iterator_write (ccs_credentials_list_iterator_t in_list_iterator,
                                              k5_ipc_stream                    in_stream)
{
    return ccs_list_iterator_write (in_list_iterator, in_stream);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_iterator_clone (ccs_credentials_list_iterator_t  in_list_iterator,
                                              ccs_credentials_list_iterator_t *out_list_iterator)
{
    return ccs_list_iterator_clone (in_list_iterator, out_list_iterator);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_iterator_next (ccs_credentials_list_iterator_t  io_list_iterator,
                                             ccs_credentials_t               *out_credential)
{
    return ccs_list_iterator_next (io_list_iterator, (ccs_list_object_t *) out_credential);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_credentials_list_iterator_release (ccs_credentials_list_iterator_t io_list_iterator)
{
    return ccs_list_iterator_release (io_list_iterator);
}

#ifdef TARGET_OS_MAC
#pragma mark-
#endif

/* ------------------------------------------------------------------------ */

cc_int32 ccs_generic_list_iterator_invalidate (ccs_generic_list_iterator_t io_list_iterator)
{
    return ccs_list_iterator_invalidate (io_list_iterator);
}
