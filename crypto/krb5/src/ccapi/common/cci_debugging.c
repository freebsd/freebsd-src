/* ccapi/common/cci_debugging.c */
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

#include "cci_common.h"
#include "cci_os_debugging.h"

/* ------------------------------------------------------------------------ */

cc_int32 _cci_check_error (cc_int32    in_error,
                           const char *in_function,
                           const char *in_file,
                           int         in_line)
{
    /* Do not log for flow control errors or when there is no error at all */
    if (in_error != ccNoError && in_error != ccIteratorEnd) {
        cci_debug_printf ("%s() got %d at %s: %d", in_function,
                          in_error, in_file, in_line);
    }

    return in_error;
}

/* ------------------------------------------------------------------------ */

void cci_debug_printf (const char *in_format, ...)
{
    va_list args;

    va_start (args, in_format);
    cci_os_debug_vprintf (in_format, args);
    va_end (args);
}
