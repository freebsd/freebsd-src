/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/valid_times.c */
/*
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "int-proto.h"

/*
 * This is an internal routine which validates the krb5_timestamps
 * field in a krb5_ticket.
 */

krb5_error_code
krb5int_validate_times(krb5_context context, krb5_ticket_times *times)
{
    krb5_timestamp          currenttime, starttime;
    krb5_error_code         retval;

    if ((retval = krb5_timeofday(context, &currenttime)))
        return retval;

    /* if starttime is not in ticket, then treat it as authtime */
    if (times->starttime != 0)
        starttime = times->starttime;
    else
        starttime = times->authtime;

    if (starttime - currenttime > context->clockskew)
        return KRB5KRB_AP_ERR_TKT_NYV;  /* ticket not yet valid */

    if ((currenttime - times->endtime) > context->clockskew)
        return KRB5KRB_AP_ERR_TKT_EXPIRED; /* ticket expired */

    return 0;
}
