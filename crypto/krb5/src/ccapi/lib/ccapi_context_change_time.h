/* ccapi/lib/ccapi_context_change_time.h */
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

#ifndef CCAPI_CONTEXT_CHANGE_TIME_H
#define CCAPI_CONTEXT_CHANGE_TIME_H

#include "cci_common.h"

cc_int32 cci_context_change_time_thread_init (void);
void cci_context_change_time_thread_fini (void);

cc_int32 cci_context_change_time_get (cc_time_t *out_change_time);

cc_int32 cci_context_change_time_update (cci_identifier_t in_identifier,
                                         cc_time_t        in_new_change_time);

cc_int32 cci_context_change_time_sync (cci_identifier_t in_new_identifier);

#endif /* CCAPI_CONTEXT_CHANGE_TIME_H */
