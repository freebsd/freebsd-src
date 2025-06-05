/* ccapi/server/ccs_list_internal.h */
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

#ifndef CCS_LIST_INTERNAL_H
#define CCS_LIST_INTERNAL_H

#include "ccs_common.h"
#include "cci_array_internal.h"

struct ccs_list_d;
typedef struct ccs_list_d *ccs_list_t;

struct ccs_list_iterator_d;
typedef struct ccs_list_iterator_d *ccs_list_iterator_t;

typedef cci_array_object_t ccs_list_object_t;
typedef cc_int32 (*ccs_object_release_t) (ccs_list_object_t);
typedef cc_int32 (*ccs_object_compare_identifier_t) (ccs_list_object_t, cci_identifier_t, cc_uint32 *);

cc_int32 ccs_list_new (ccs_list_t                      *out_list,
                       cc_int32                         in_object_not_found_err,
                       cc_int32                         in_iterator_not_found_err,
                       ccs_object_compare_identifier_t  in_object_compare_identifier,
                       ccs_object_release_t             in_object_release);

cc_int32 ccs_list_release (ccs_list_t io_list);

cc_int32 ccs_list_new_iterator (ccs_list_t           io_list,
                                ccs_pipe_t           in_client_pipe,
                                ccs_list_iterator_t *out_list_iterator);

cc_int32 ccs_list_release_iterator (ccs_list_t       io_list,
                                    cci_identifier_t in_identifier);

cc_int32 ccs_list_count (ccs_list_t  in_list,
                         cc_uint64  *out_count);

cc_int32 ccs_list_find (ccs_list_t         in_list,
                        cci_identifier_t   in_identifier,
                        ccs_list_object_t *out_object);

cc_int32 ccs_list_find_iterator (ccs_list_t           in_list,
                                 cci_identifier_t     in_identifier,
                                 ccs_list_iterator_t *out_list_iterator);

cc_int32 ccs_list_add (ccs_list_t        io_list,
                       ccs_list_object_t in_object);

cc_int32 ccs_list_remove (ccs_list_t       io_list,
                          cci_identifier_t in_identifier);

cc_int32 ccs_list_push_front (ccs_list_t       io_list,
                              cci_identifier_t in_identifier);


cc_int32 ccs_list_iterator_write (ccs_list_iterator_t in_list_iterator,
                                  k5_ipc_stream        in_stream);

cc_int32 ccs_list_iterator_clone (ccs_list_iterator_t  in_list_iterator,
                                  ccs_list_iterator_t *out_list_iterator);

cc_int32 ccs_list_iterator_current (ccs_list_iterator_t  io_list_iterator,
                                    ccs_list_object_t   *out_object);

cc_int32 ccs_list_iterator_next (ccs_list_iterator_t  io_list_iterator,
                                 ccs_list_object_t   *out_object);

cc_int32 ccs_list_iterator_invalidate (ccs_list_iterator_t io_list_iterator);

cc_int32 ccs_list_iterator_release (ccs_list_iterator_t io_list_iterator);

#endif /* CCS_LIST_INTERNAL_H */
