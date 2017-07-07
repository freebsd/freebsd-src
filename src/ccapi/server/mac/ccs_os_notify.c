/* ccapi/server/mac/ccs_os_notify.c */
/*
 * Copyright 2006-2008 Massachusetts Institute of Technology.
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
#include "ccs_os_notify.h"
#include <CoreFoundation/CoreFoundation.h>

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_notify_cache_collection_changed (ccs_cache_collection_t io_cache_collection)
{
    cc_int32 err = ccNoError;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter ();

        if (center) {
            CFNotificationCenterPostNotification (center,
                                                  kCCAPICacheCollectionChangedNotification,
                                                  NULL, NULL, TRUE);
        }
    }



    return cci_check_error (err);
}

/* ------------------------------------------------------------------------ */

cc_int32 ccs_os_notify_ccache_changed (ccs_cache_collection_t  io_cache_collection,
                                       const char             *in_ccache_name)
{
    cc_int32 err = ccNoError;

    if (!io_cache_collection) { err = cci_check_error (ccErrBadParam); }
    if (!in_ccache_name     ) { err = cci_check_error (ccErrBadParam); }

    if (!err) {
        CFNotificationCenterRef center = CFNotificationCenterGetDistributedCenter ();
        CFStringRef name = CFStringCreateWithCString (kCFAllocatorDefault,
                                                      in_ccache_name,
                                                      kCFStringEncodingUTF8);

        if (center && name) {
            CFNotificationCenterPostNotification (center,
                                                  kCCAPICCacheChangedNotification,
                                                  name, NULL, TRUE);
        }

        if (name) { CFRelease (name); }
    }

    return cci_check_error (err);
}
