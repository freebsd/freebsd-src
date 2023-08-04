/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 *
 */

#include    <kadm5/admin.h>
#include    "kadm_rpc.h"
#include    "server_internal.h"

bool_t      xdr_ui_4(XDR *xdrs, krb5_ui_4 *objp);
bool_t	    xdr_nullstring(XDR *xdrs, char **objp);
bool_t      xdr_nulltype(XDR *xdrs, void **objp, xdrproc_t proc);
bool_t	    xdr_krb5_timestamp(XDR *xdrs, krb5_timestamp *objp);
bool_t	    xdr_krb5_kvno(XDR *xdrs, krb5_kvno *objp);
bool_t	    xdr_krb5_deltat(XDR *xdrs, krb5_deltat *objp);
bool_t	    xdr_krb5_flags(XDR *xdrs, krb5_flags *objp);
bool_t      xdr_krb5_ui_4(XDR *xdrs, krb5_ui_4 *objp);
bool_t      xdr_krb5_int16(XDR *xdrs, krb5_int16 *objp);
bool_t      xdr_krb5_ui_2(XDR *xdrs, krb5_ui_2 *objp);
bool_t      xdr_krb5_key_data_nocontents(XDR *xdrs, krb5_key_data *objp);
bool_t      xdr_krb5_key_salt_tuple(XDR *xdrs, krb5_key_salt_tuple *objp);
bool_t      xdr_krb5_tl_data(XDR *xdrs, krb5_tl_data **tl_data_head);
bool_t	    xdr_kadm5_ret_t(XDR *xdrs, kadm5_ret_t *objp);
bool_t      xdr_kadm5_principal_ent_rec_v1(XDR *xdrs, kadm5_principal_ent_rec *objp);
bool_t	    xdr_kadm5_principal_ent_rec(XDR *xdrs, kadm5_principal_ent_rec *objp);
bool_t	    xdr_kadm5_policy_ent_rec(XDR *xdrs, kadm5_policy_ent_rec *objp);
bool_t	    xdr_kadm5_policy_ent_t(XDR *xdrs, kadm5_policy_ent_t *objp);
bool_t	    xdr_kadm5_principal_ent_t(XDR *xdrs, kadm5_principal_ent_t *objp);
bool_t	    xdr_cprinc_arg(XDR *xdrs, cprinc_arg *objp);
bool_t      xdr_cprinc3_arg(XDR *xdrs, cprinc3_arg *objp);
bool_t      xdr_generic_ret(XDR *xdrs, generic_ret *objp);
bool_t	    xdr_dprinc_arg(XDR *xdrs, dprinc_arg *objp);
bool_t	    xdr_mprinc_arg(XDR *xdrs, mprinc_arg *objp);
bool_t	    xdr_rprinc_arg(XDR *xdrs, rprinc_arg *objp);
bool_t	    xdr_chpass_arg(XDR *xdrs, chpass_arg *objp);
bool_t      xdr_chpass3_arg(XDR *xdrs, chpass3_arg *objp);
bool_t      xdr_setkey_arg(XDR *xdrs, setkey_arg *objp);
bool_t      xdr_setkey3_arg(XDR *xdrs, setkey3_arg *objp);
bool_t      xdr_setkey4_arg(XDR *xdrs, setkey4_arg *objp);
bool_t	    xdr_chrand_arg(XDR *xdrs, chrand_arg *objp);
bool_t      xdr_chrand3_arg(XDR *xdrs, chrand3_arg *objp);
bool_t	    xdr_chrand_ret(XDR *xdrs, chrand_ret *objp);
bool_t	    xdr_gprinc_arg(XDR *xdrs, gprinc_arg *objp);
bool_t      xdr_gprinc_ret(XDR *xdrs, gprinc_ret *objp);
bool_t	    xdr_gprincs_arg(XDR *xdrs, gprincs_arg *objp);
bool_t      xdr_gprincs_ret(XDR *xdrs, gprincs_ret *objp);
bool_t	    xdr_cpol_arg(XDR *xdrs, cpol_arg *objp);
bool_t	    xdr_dpol_arg(XDR *xdrs, dpol_arg *objp);
bool_t	    xdr_mpol_arg(XDR *xdrs, mpol_arg *objp);
bool_t	    xdr_gpol_arg(XDR *xdrs, gpol_arg *objp);
bool_t	    xdr_gpol_ret(XDR *xdrs, gpol_ret *objp);
bool_t      xdr_gpols_arg(XDR *xdrs, gpols_arg *objp);
bool_t      xdr_gpols_ret(XDR *xdrs, gpols_ret *objp);
bool_t      xdr_getprivs_ret(XDR *xdrs, getprivs_ret *objp);
bool_t      xdr_purgekeys_arg(XDR *xdrs, purgekeys_arg *objp);
bool_t      xdr_gstrings_arg(XDR *xdrs, gstrings_arg *objp);
bool_t      xdr_gstrings_ret(XDR *xdrs, gstrings_ret *objp);
bool_t      xdr_sstring_arg(XDR *xdrs, sstring_arg *objp);
bool_t	    xdr_krb5_principal(XDR *xdrs, krb5_principal *objp);
bool_t	    xdr_krb5_octet(XDR *xdrs, krb5_octet *objp);
bool_t	    xdr_krb5_int32(XDR *xdrs, krb5_int32 *objp);
bool_t	    xdr_krb5_enctype(XDR *xdrs, krb5_enctype *objp);
bool_t      xdr_krb5_salttype(XDR *xdrs, krb5_int32 *objp);
bool_t	    xdr_krb5_keyblock(XDR *xdrs, krb5_keyblock *objp);
bool_t      xdr_krb5_key_data(XDR *xdrs, krb5_key_data *objp);
bool_t      xdr_krb5_string_attr(XDR *xdrs, krb5_string_attr *objp);
bool_t      xdr_osa_pw_hist_ent(XDR *xdrs, osa_pw_hist_ent *objp);
bool_t      xdr_kadm5_key_data(XDR *xdrs, kadm5_key_data *objp);
bool_t      xdr_getpkeys_arg(XDR *xdrs, getpkeys_arg *objp);
bool_t      xdr_getpkeys_ret(XDR *xdrs, getpkeys_ret *objp);
