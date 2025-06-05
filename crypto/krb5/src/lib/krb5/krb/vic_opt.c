/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"

void KRB5_CALLCONV
krb5_verify_init_creds_opt_init(krb5_verify_init_creds_opt *opt)
{
    opt->flags = 0;
}

void KRB5_CALLCONV
krb5_verify_init_creds_opt_set_ap_req_nofail(krb5_verify_init_creds_opt *opt, int ap_req_nofail)
{
    opt->flags |= KRB5_VERIFY_INIT_CREDS_OPT_AP_REQ_NOFAIL;
    opt->ap_req_nofail = ap_req_nofail;
}
