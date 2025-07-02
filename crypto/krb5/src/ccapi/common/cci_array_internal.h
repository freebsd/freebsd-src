/* ccapi/common/cci_array_internal.h */
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

#ifndef CCI_ARRAY_INTERNAL_H
#define CCI_ARRAY_INTERNAL_H

#include "cci_types.h"

struct cci_array_object_d;
typedef struct cci_array_object_d *cci_array_object_t;

typedef cc_int32 (*cci_array_object_release_t) (cci_array_object_t);

struct cci_array_d;
typedef struct cci_array_d *cci_array_t;

cc_int32 cci_array_new (cci_array_t                *out_array,
                        cci_array_object_release_t  in_array_object_release);

cc_int32 cci_array_release (cci_array_t io_array);

cc_uint64 cci_array_count (cci_array_t in_array);

cci_array_object_t cci_array_object_at_index (cci_array_t io_array,
                                              cc_uint64   in_position);

cc_int32 cci_array_insert (cci_array_t        io_array,
                           cci_array_object_t in_object,
                           cc_uint64          in_position);

cc_int32 cci_array_remove (cci_array_t io_array,
                           cc_uint64   in_position);

cc_int32 cci_array_move (cci_array_t  io_array,
                         cc_uint64    in_position,
                         cc_uint64    in_new_position,
                         cc_uint64   *out_real_new_position);

cc_int32 cci_array_push_front (cci_array_t io_array,
                               cc_uint64   in_position);


#endif /* CCI_ARRAY_INTERNAL_H */
