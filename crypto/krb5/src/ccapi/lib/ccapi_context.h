/* ccapi/lib/ccapi_context.h */
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

#ifndef CCAPI_CONTEXT_H
#define CCAPI_CONTEXT_H

#include "cci_common.h"

/* Used for freeing ccapi context in thread fini calls
 * Does not tell the server you are exiting. */
cc_int32 cci_context_destroy (cc_context_t in_context);

cc_int32 ccapi_context_release (cc_context_t in_context);

cc_int32 ccapi_context_get_change_time (cc_context_t  in_context,
                                        cc_time_t    *out_time);

cc_int32 ccapi_context_wait_for_change (cc_context_t  in_context);

cc_int32 ccapi_context_get_default_ccache_name (cc_context_t  in_context,
                                                cc_string_t  *out_name);

cc_int32 ccapi_context_open_ccache (cc_context_t  in_context,
                                    const char   *in_name,
                                    cc_ccache_t  *out_ccache);

cc_int32 ccapi_context_open_default_ccache (cc_context_t  in_context,
                                            cc_ccache_t  *out_ccache);

cc_int32 ccapi_context_create_ccache (cc_context_t  in_context,
                                      const char   *in_name,
                                      cc_uint32     in_cred_vers,
                                      const char   *in_principal,
                                      cc_ccache_t  *out_ccache);

cc_int32 ccapi_context_create_default_ccache (cc_context_t  in_context,
                                              cc_uint32     in_cred_vers,
                                              const char   *in_principal,
                                              cc_ccache_t  *out_ccache);

cc_int32 ccapi_context_create_new_ccache (cc_context_t in_context,
                                          cc_uint32    in_cred_vers,
                                          const char  *in_principal,
                                          cc_ccache_t *out_ccache);

cc_int32 ccapi_context_new_ccache_iterator (cc_context_t          in_context,
                                            cc_ccache_iterator_t *out_iterator);

cc_int32 ccapi_context_lock (cc_context_t in_context,
                             cc_uint32    in_lock_type,
                             cc_uint32    in_block);

cc_int32 ccapi_context_unlock (cc_context_t in_context);

cc_int32 ccapi_context_compare (cc_context_t  in_context,
                                cc_context_t  in_compare_to_context,
                                cc_uint32    *out_equal);

#ifdef WIN32
void cci_thread_init__auxinit();
#endif


#endif /* CCAPI_CONTEXT_H */
