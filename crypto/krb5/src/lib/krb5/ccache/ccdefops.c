/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/ccdefops.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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

/* Default credentials cache determination.  This is a separate file
 * so that the user can more easily override it. */

#include "k5-int.h"

#if defined(USE_CCAPI)

/*
 * Macs use the shared, memory based credentials cache
 * Windows may also use the ccapi cache, but only if the Krbcc32.dll
 * can be found; otherwise it falls back to using the old
 * file-based ccache.
 */
#include "stdcc.h" /* from ccapi subdir */

const krb5_cc_ops *krb5_cc_dfl_ops = &krb5_cc_stdcc_ops;

#elif defined(NO_FILE_CCACHE)

/* Note that this version isn't likely to work very well for multiple
   processes.  It's mostly a placeholder so we can experiment with
   building the NO_FILE_CCACHE code on UNIX.  */

extern const krb5_cc_ops krb5_mcc_ops;
const krb5_cc_ops *krb5_cc_dfl_ops = &krb5_mcc_ops;

#else

#include "fcc.h"
const krb5_cc_ops *krb5_cc_dfl_ops = &krb5_cc_file_ops;

#endif
