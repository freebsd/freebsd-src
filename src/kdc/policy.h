/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/policy.h - Declarations for policy.c */
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

#ifndef __KRB5_KDC_POLICY__
#define __KRB5_KDC_POLICY__

krb5_error_code
load_kdcpolicy_plugins(krb5_context context);

void
unload_kdcpolicy_plugins(krb5_context context);

krb5_error_code
check_kdcpolicy_as(krb5_context context, const krb5_kdc_req *request,
                   const krb5_db_entry *client, const krb5_db_entry *server,
                   krb5_data *const *auth_indicators, krb5_timestamp kdc_time,
                   krb5_ticket_times *times, const char **status);

krb5_error_code
check_kdcpolicy_tgs(krb5_context context, const krb5_kdc_req *request,
                    const krb5_db_entry *server, const krb5_ticket *ticket,
                    krb5_data *const *auth_indicators, krb5_timestamp kdc_time,
                    krb5_ticket_times *times, const char **status);

#endif /* __KRB5_KDC_POLICY__ */
