/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/extern.c */
/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
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
 *
 * allocations of extern stuff
 */

#include "k5-int.h"
#include "kdb.h"
#include "extern.h"
#include "realm_data.h"

/* real declarations of KDC's externs */
kdc_realm_t     **kdc_realmlist = (kdc_realm_t **) NULL;
int             kdc_numrealms = 0;
krb5_data empty_string = {0, 0, ""};
krb5_int32      max_dgram_reply_size = MAX_DGRAM_SIZE;

/* With ts_after(), this is the largest timestamp value. */
krb5_timestamp kdc_infinity = -1;
