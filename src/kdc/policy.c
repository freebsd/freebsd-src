/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/policy.c - Policy decision routines for KDC */
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

#include "k5-int.h"
#include "kdc_util.h"
#include "extern.h"

int
against_local_policy_as(register krb5_kdc_req *request, krb5_db_entry client,
                        krb5_db_entry server, krb5_timestamp kdc_time,
                        const char **status, krb5_pa_data ***e_data)
{
#if 0
    /* An AS request must include the addresses field */
    if (request->addresses == 0) {
        *status = "NO ADDRESS";
        return KRB5KDC_ERR_POLICY;
    }
#endif

    return 0;                   /* not against policy */
}

/*
 * This is where local policy restrictions for the TGS should placed.
 */
krb5_error_code
against_local_policy_tgs(register krb5_kdc_req *request, krb5_db_entry server,
                         krb5_ticket *ticket, const char **status,
                         krb5_pa_data ***e_data)
{
#if 0
    /*
     * For example, if your site wants to disallow ticket forwarding,
     * you might do something like this:
     */

    if (isflagset(request->kdc_options, KDC_OPT_FORWARDED)) {
        *status = "FORWARD POLICY";
        return KRB5KDC_ERR_POLICY;
    }
#endif

    return 0;                           /* not against policy */
}
