/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_log.c - Logging functions for KDC requests */
/*
 * Copyright 2008,2009 by the Massachusetts Institute of Technology.
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
#include "kdc_util.h"
#include <syslog.h>
#include "adm_proto.h"

/*
 * A note on KDC-status string format.
 *
 * - All letters in the status string should be capitalized;
 * - the words in the status phrase are separated by underscores;
 * - abbreviations should be avoided.  Some acceptable "standard" acronyms
 *   are AS_REQ, TGS_REP etc.
 * - since in almost all cases KDC status string is set on error, no need
 *   to state this fact as part of the status string;
 * - KDC status string should be an imperative phrase.
 *
 * Example: "MAKE_RANDOM_KEY"
 */

/* Main logging routines for ticket requests.

   There are a few simple cases -- unparseable requests mainly --
   where messages are logged otherwise, but once a ticket request can
   be decoded in some basic way, these routines are used for logging
   the details.  */

/* "status" is null to indicate success.  */
/* Someday, pass local address/port as well.  */
/* Currently no info about name canonicalization is logged.  */
void
log_as_req(krb5_context context, const krb5_fulladdr *from,
           krb5_kdc_req *request, krb5_kdc_rep *reply,
           krb5_db_entry *client, const char *cname,
           krb5_db_entry *server, const char *sname,
           krb5_timestamp authtime,
           const char *status, krb5_error_code errcode, const char *emsg)
{
    const char *fromstring = 0;
    char fromstringbuf[70];
    char ktypestr[128];
    const char *cname2 = cname ? cname : "<unknown client>";
    const char *sname2 = sname ? sname : "<unknown server>";

    fromstring = inet_ntop(ADDRTYPE2FAMILY (from->address->addrtype),
                           from->address->contents,
                           fromstringbuf, sizeof(fromstringbuf));
    if (!fromstring)
        fromstring = "<unknown>";
    ktypes2str(ktypestr, sizeof(ktypestr),
               request->nktypes, request->ktype);

    if (status == NULL) {
        /* success */
        char rep_etypestr[128];
        rep_etypes2str(rep_etypestr, sizeof(rep_etypestr), reply);
        krb5_klog_syslog(LOG_INFO, _("AS_REQ (%s) %s: ISSUE: authtime %d, %s, "
                                     "%s for %s"),
                         ktypestr, fromstring, authtime,
                         rep_etypestr, cname2, sname2);
    } else {
        /* fail */
        krb5_klog_syslog(LOG_INFO, _("AS_REQ (%s) %s: %s: %s for %s%s%s"),
                         ktypestr, fromstring, status,
                         cname2, sname2, emsg ? ", " : "", emsg ? emsg : "");
    }
    krb5_db_audit_as_req(context, request, client, server, authtime,
                         errcode);
#if 0
    /* Sun (OpenSolaris) version would probably something like this.
       The client and server names passed can be null, unlike in the
       logging routines used above.  Note that a struct in_addr is
       used, but the real address could be an IPv6 address.  */
    audit_krb5kdc_as_req(some in_addr *, (in_port_t)from->port, 0,
                         cname, sname, errcode);
#endif
}

/*
 * Unparse a principal for logging purposes and limit the string length.
 * Ignore errors because the most likely errors are memory exhaustion, and many
 * other things will fail in the logging functions in that case.
 */
static void
unparse_and_limit(krb5_context ctx, krb5_principal princ, char **str)
{
    /* Ignore errors */
    krb5_unparse_name(ctx, princ, str);
    limit_string(*str);
}

/* Here "status" must be non-null.  Error code
   KRB5KDC_ERR_SERVER_NOMATCH is handled specially.

   Currently no info about name canonicalization is logged.  */
void
log_tgs_req(krb5_context ctx, const krb5_fulladdr *from,
            krb5_kdc_req *request, krb5_kdc_rep *reply,
            krb5_principal cprinc, krb5_principal sprinc,
            krb5_principal altcprinc,
            krb5_timestamp authtime,
            unsigned int c_flags,
            const char *status, krb5_error_code errcode, const char *emsg)
{
    char ktypestr[128];
    const char *fromstring = 0;
    char fromstringbuf[70];
    char rep_etypestr[128];
    char *cname = NULL, *sname = NULL, *altcname = NULL;
    char *logcname = NULL, *logsname = NULL, *logaltcname = NULL;

    fromstring = inet_ntop(ADDRTYPE2FAMILY(from->address->addrtype),
                           from->address->contents,
                           fromstringbuf, sizeof(fromstringbuf));
    if (!fromstring)
        fromstring = "<unknown>";
    ktypes2str(ktypestr, sizeof(ktypestr), request->nktypes, request->ktype);
    if (!errcode)
        rep_etypes2str(rep_etypestr, sizeof(rep_etypestr), reply);
    else
        rep_etypestr[0] = 0;

    unparse_and_limit(ctx, cprinc, &cname);
    logcname = (cname != NULL) ? cname : "<unknown client>";
    unparse_and_limit(ctx, sprinc, &sname);
    logsname = (sname != NULL) ? sname : "<unknown server>";
    unparse_and_limit(ctx, altcprinc, &altcname);
    logaltcname = (altcname != NULL) ? altcname : "<unknown>";

    /* Differences: server-nomatch message logs 2nd ticket's client
       name (useful), and doesn't log ktypestr (probably not
       important).  */
    if (errcode != KRB5KDC_ERR_SERVER_NOMATCH) {
        krb5_klog_syslog(LOG_INFO, _("TGS_REQ (%s) %s: %s: authtime %d, %s%s "
                                     "%s for %s%s%s"),
                         ktypestr, fromstring, status, authtime, rep_etypestr,
                         !errcode ? "," : "", logcname, logsname,
                         errcode ? ", " : "", errcode ? emsg : "");
        if (isflagset(c_flags, KRB5_KDB_FLAG_PROTOCOL_TRANSITION))
            krb5_klog_syslog(LOG_INFO,
                             _("... PROTOCOL-TRANSITION s4u-client=%s"),
                             logaltcname);
        else if (isflagset(c_flags, KRB5_KDB_FLAG_CONSTRAINED_DELEGATION))
            krb5_klog_syslog(LOG_INFO,
                             _("... CONSTRAINED-DELEGATION s4u-client=%s"),
                             logaltcname);

    } else
        krb5_klog_syslog(LOG_INFO, _("TGS_REQ %s: %s: authtime %d, %s for %s, "
                                     "2nd tkt client %s"),
                         fromstring, status, authtime,
                         logcname, logsname, logaltcname);

    /* OpenSolaris: audit_krb5kdc_tgs_req(...)  or
       audit_krb5kdc_tgs_req_2ndtktmm(...) */

    krb5_free_unparsed_name(ctx, cname);
    krb5_free_unparsed_name(ctx, sname);
    krb5_free_unparsed_name(ctx, altcname);
}

void
log_tgs_badtrans(krb5_context ctx, krb5_principal cprinc,
                 krb5_principal sprinc, krb5_data *trcont,
                 krb5_error_code errcode)
{
    unsigned int tlen;
    char *tdots;
    const char *emsg = NULL;
    char *cname = NULL, *sname = NULL;
    char *logcname = NULL, *logsname = NULL;

    unparse_and_limit(ctx, cprinc, &cname);
    logcname = (cname != NULL) ? cname : "<unknown client>";
    unparse_and_limit(ctx, sprinc, &sname);
    logsname = (sname != NULL) ? sname : "<unknown server>";

    tlen = trcont->length;
    tdots = tlen > 125 ? "..." : "";
    tlen = tlen > 125 ? 125 : tlen;

    if (errcode == KRB5KRB_AP_ERR_ILL_CR_TKT)
        krb5_klog_syslog(LOG_INFO, _("bad realm transit path from '%s' "
                                     "to '%s' via '%.*s%s'"),
                         logcname, logsname, tlen,
                         trcont->data, tdots);
    else {
        emsg = krb5_get_error_message(ctx, errcode);
        krb5_klog_syslog(LOG_ERR, _("unexpected error checking transit "
                                    "from '%s' to '%s' via '%.*s%s': %s"),
                         logcname, logsname, tlen,
                         trcont->data, tdots,
                         emsg);
        krb5_free_error_message(ctx, emsg);
        emsg = NULL;
    }
    krb5_free_unparsed_name(ctx, cname);
    krb5_free_unparsed_name(ctx, sname);
}

void
log_tgs_alt_tgt(krb5_context context, krb5_principal p)
{
    char *sname;
    if (krb5_unparse_name(context, p, &sname)) {
        krb5_klog_syslog(LOG_INFO,
                         _("TGS_REQ: issuing alternate <un-unparseable> TGT"));
    } else {
        limit_string(sname);
        krb5_klog_syslog(LOG_INFO, _("TGS_REQ: issuing TGT %s"), sname);
        free(sname);
    }
    /* OpenSolaris: audit_krb5kdc_tgs_req_alt_tgt(...) */
}
