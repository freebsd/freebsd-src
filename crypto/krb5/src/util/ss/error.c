/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2007 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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
/*
 * Copyright 1987, 1988, 1989 by MIT Student Information Processing
 * Board
 *
 * For copyright information, see copyright.h.
 */

#include "ss_internal.h"
#include "com_err.h"
#include "copyright.h"

char * ss_name(sci_idx)
    int sci_idx;
{
    ss_data *infop;

    infop = ss_info(sci_idx);
    if (infop->current_request == (char const *)NULL) {
        return strdup(infop->subsystem_name);
    } else {
        char *ret_val;
        if (asprintf(&ret_val, "%s (%s)",
                     infop->subsystem_name, infop->current_request) < 0)
            return NULL;
        return ret_val;
    }
}

void ss_error (int sci_idx, long code, const char * fmt, ...)
{
    char *whoami;
    va_list pvar;
    va_start (pvar, fmt);
    whoami = ss_name (sci_idx);
    com_err_va (whoami, code, fmt, pvar);
    free (whoami);
    va_end(pvar);
}

void ss_perror (sci_idx, code, msg) /* for compatibility */
    int sci_idx;
    long code;
    char const *msg;
{
    ss_error (sci_idx, code, "%s", msg);
}
