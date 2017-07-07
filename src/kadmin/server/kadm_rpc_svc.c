/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 */

#include <k5-int.h>
#include <gssrpc/rpc.h>
#include <gssapi/gssapi_krb5.h> /* for gss_nt_krb5_name */
#include <syslog.h>
#include <kadm5/kadm_rpc.h>
#include <krb5.h>
#include <kadm5/admin.h>
#include <adm_proto.h>
#include "misc.h"
#include "kadm5/server_internal.h"

extern void *global_server_handle;

static int check_rpcsec_auth(struct svc_req *);

/*
 * Function: kadm_1
 *
 * Purpose: RPC proccessing procedure.
 *	    originally generated from rpcgen
 *
 * Arguments:
 *	rqstp		    (input) rpc request structure
 *	transp		    (input) rpc transport structure
 *	(input/output)
 *	<return value>
 *
 * Requires:
 * Effects:
 * Modifies:
 */

void kadm_1(rqstp, transp)
   struct svc_req *rqstp;
   register SVCXPRT *transp;
{
     union {
	  cprinc_arg create_principal_2_arg;
	  dprinc_arg delete_principal_2_arg;
	  mprinc_arg modify_principal_2_arg;
	  rprinc_arg rename_principal_2_arg;
	  gprinc_arg get_principal_2_arg;
	  chpass_arg chpass_principal_2_arg;
	  chrand_arg chrand_principal_2_arg;
	  cpol_arg create_policy_2_arg;
	  dpol_arg delete_policy_2_arg;
	  mpol_arg modify_policy_2_arg;
	  gpol_arg get_policy_2_arg;
	  setkey_arg setkey_principal_2_arg;
	  setv4key_arg setv4key_principal_2_arg;
	  cprinc3_arg create_principal3_2_arg;
	  chpass3_arg chpass_principal3_2_arg;
	  chrand3_arg chrand_principal3_2_arg;
	  setkey3_arg setkey_principal3_2_arg;
	  setkey4_arg setkey_principal4_2_arg;
	  getpkeys_arg get_principal_keys_2_arg;
     } argument;
     union {
	  generic_ret gen_ret;
	  gprinc_ret get_principal_2_ret;
	  chrand_ret chrand_principal_2_ret;
	  gpol_ret get_policy_2_ret;
	  getprivs_ret get_privs_2_ret;
	  gprincs_ret get_princs_2_ret;
	  gpols_ret get_pols_2_ret;
	  chrand_ret chrand_principal3_2_ret;
	  gstrings_ret get_string_2_ret;
	  getpkeys_ret get_principal_keys_ret;
     } result;
     bool_t retval;
     bool_t (*xdr_argument)(), (*xdr_result)();
     bool_t (*local)();

     if (rqstp->rq_cred.oa_flavor != AUTH_GSSAPI &&
	 !check_rpcsec_auth(rqstp)) {
	  krb5_klog_syslog(LOG_ERR, "Authentication attempt failed: %s, "
			   "RPC authentication flavor %d",
			   client_addr(rqstp->rq_xprt),
			   rqstp->rq_cred.oa_flavor);
	  svcerr_weakauth(transp);
	  return;
     }

     switch (rqstp->rq_proc) {
     case NULLPROC:
	  (void) svc_sendreply(transp, xdr_void, (char *)NULL);
	  return;

     case CREATE_PRINCIPAL:
	  xdr_argument = xdr_cprinc_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) create_principal_2_svc;
	  break;

     case DELETE_PRINCIPAL:
	  xdr_argument = xdr_dprinc_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) delete_principal_2_svc;
	  break;

     case MODIFY_PRINCIPAL:
	  xdr_argument = xdr_mprinc_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) modify_principal_2_svc;
	  break;

     case RENAME_PRINCIPAL:
	  xdr_argument = xdr_rprinc_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) rename_principal_2_svc;
	  break;

     case GET_PRINCIPAL:
	  xdr_argument = xdr_gprinc_arg;
	  xdr_result = xdr_gprinc_ret;
	  local = (bool_t (*)()) get_principal_2_svc;
	  break;

     case GET_PRINCS:
	  xdr_argument = xdr_gprincs_arg;
	  xdr_result = xdr_gprincs_ret;
	  local = (bool_t (*)()) get_princs_2_svc;
	  break;

     case CHPASS_PRINCIPAL:
	  xdr_argument = xdr_chpass_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) chpass_principal_2_svc;
	  break;

     case SETV4KEY_PRINCIPAL:
	  xdr_argument = xdr_setv4key_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) setv4key_principal_2_svc;
	  break;

     case SETKEY_PRINCIPAL:
	  xdr_argument = xdr_setkey_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) setkey_principal_2_svc;
	  break;

     case CHRAND_PRINCIPAL:
	  xdr_argument = xdr_chrand_arg;
	  xdr_result = xdr_chrand_ret;
	  local = (bool_t (*)()) chrand_principal_2_svc;
	  break;

     case CREATE_POLICY:
	  xdr_argument = xdr_cpol_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) create_policy_2_svc;
	  break;

     case DELETE_POLICY:
	  xdr_argument = xdr_dpol_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) delete_policy_2_svc;
	  break;

     case MODIFY_POLICY:
	  xdr_argument = xdr_mpol_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) modify_policy_2_svc;
	  break;

     case GET_POLICY:
	  xdr_argument = xdr_gpol_arg;
	  xdr_result = xdr_gpol_ret;
	  local = (bool_t (*)()) get_policy_2_svc;
	  break;

     case GET_POLS:
	  xdr_argument = xdr_gpols_arg;
	  xdr_result = xdr_gpols_ret;
	  local = (bool_t (*)()) get_pols_2_svc;
	  break;

     case GET_PRIVS:
	  xdr_argument = xdr_u_int32;
	  xdr_result = xdr_getprivs_ret;
	  local = (bool_t (*)()) get_privs_2_svc;
	  break;

     case INIT:
	  xdr_argument = xdr_u_int32;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) init_2_svc;
	  break;

     case CREATE_PRINCIPAL3:
	  xdr_argument = xdr_cprinc3_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) create_principal3_2_svc;
	  break;

     case CHPASS_PRINCIPAL3:
	  xdr_argument = xdr_chpass3_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) chpass_principal3_2_svc;
	  break;

     case CHRAND_PRINCIPAL3:
	  xdr_argument = xdr_chrand3_arg;
	  xdr_result = xdr_chrand_ret;
	  local = (bool_t (*)()) chrand_principal3_2_svc;
	  break;

     case SETKEY_PRINCIPAL3:
	  xdr_argument = xdr_setkey3_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) setkey_principal3_2_svc;
	  break;

     case PURGEKEYS:
	  xdr_argument = xdr_purgekeys_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) purgekeys_2_svc;
	  break;

     case GET_STRINGS:
	  xdr_argument = xdr_gstrings_arg;
	  xdr_result = xdr_gstrings_ret;
	  local = (bool_t (*)()) get_strings_2_svc;
	  break;

     case SET_STRING:
	  xdr_argument = xdr_sstring_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) set_string_2_svc;
	  break;

     case SETKEY_PRINCIPAL4:
	  xdr_argument = xdr_setkey4_arg;
	  xdr_result = xdr_generic_ret;
	  local = (bool_t (*)()) setkey_principal4_2_svc;
	  break;

     case EXTRACT_KEYS:
	  xdr_argument = xdr_getpkeys_arg;
	  xdr_result = xdr_getpkeys_ret;
	  local = (bool_t (*)()) get_principal_keys_2_svc;
	  break;

     default:
	  krb5_klog_syslog(LOG_ERR, "Invalid KADM5 procedure number: %s, %d",
			   client_addr(rqstp->rq_xprt), rqstp->rq_proc);
	  svcerr_noproc(transp);
	  return;
     }
     memset(&argument, 0, sizeof(argument));
     if (!svc_getargs(transp, xdr_argument, &argument)) {
	  svcerr_decode(transp);
	  return;
     }
     memset(&result, 0, sizeof(result));
     retval = (*local)(&argument, &result, rqstp);
     if (retval && !svc_sendreply(transp, xdr_result, (void *)&result)) {
	  krb5_klog_syslog(LOG_ERR, "WARNING! Unable to send function results, "
		 "continuing.");
	  svcerr_systemerr(transp);
     }
     if (!svc_freeargs(transp, xdr_argument, &argument)) {
	  krb5_klog_syslog(LOG_ERR, "WARNING! Unable to free arguments, "
		 "continuing.");
     }
     if (!svc_freeargs(transp, xdr_result, &result)) {
	  krb5_klog_syslog(LOG_ERR, "WARNING! Unable to free results, "
		 "continuing.");
     }
     return;
}

static int
check_rpcsec_auth(struct svc_req *rqstp)
{
     gss_ctx_id_t ctx;
     krb5_context kctx;
     OM_uint32 maj_stat, min_stat;
     gss_name_t name;
     krb5_principal princ;
     int ret, success;
     krb5_data *c1, *c2, *realm;
     gss_buffer_desc gss_str;
     kadm5_server_handle_t handle;
     size_t slen;
     char *sdots;

     success = 0;
     handle = (kadm5_server_handle_t)global_server_handle;

     if (rqstp->rq_cred.oa_flavor != RPCSEC_GSS)
	  return 0;

     ctx = rqstp->rq_svccred;

     maj_stat = gss_inquire_context(&min_stat, ctx, NULL, &name,
				    NULL, NULL, NULL, NULL, NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	  krb5_klog_syslog(LOG_ERR, _("check_rpcsec_auth: failed "
				      "inquire_context, stat=%u"), maj_stat);
	  log_badauth(maj_stat, min_stat, rqstp->rq_xprt, NULL);
	  goto fail_name;
     }

     kctx = handle->context;
     ret = gss_to_krb5_name_1(rqstp, kctx, name, &princ, &gss_str);
     if (ret == 0)
	  goto fail_name;

     slen = gss_str.length;
     trunc_name(&slen, &sdots);
     /*
      * Since we accept with GSS_C_NO_NAME, the client can authenticate
      * against the entire kdb.  Therefore, ensure that the service
      * name is something reasonable.
      */
     if (krb5_princ_size(kctx, princ) != 2)
	  goto fail_princ;

     c1 = krb5_princ_component(kctx, princ, 0);
     c2 = krb5_princ_component(kctx, princ, 1);
     realm = krb5_princ_realm(kctx, princ);
     success = data_eq_string(*realm, handle->params.realm) &&
	     data_eq_string(*c1, "kadmin") && !data_eq_string(*c2, "history");

fail_princ:
     if (!success) {
	 krb5_klog_syslog(LOG_ERR, _("bad service principal %.*s%s"),
			  (int) slen, (char *) gss_str.value, sdots);
     }
     gss_release_buffer(&min_stat, &gss_str);
     krb5_free_principal(kctx, princ);
fail_name:
     gss_release_name(&min_stat, &name);
     return success;
}

int
gss_to_krb5_name_1(struct svc_req *rqstp, krb5_context ctx, gss_name_t gss_name,
		   krb5_principal *princ, gss_buffer_t gss_str)
{
     OM_uint32 status, minor_stat;
     gss_OID gss_type;
     char *str;
     int success;

     status = gss_display_name(&minor_stat, gss_name, gss_str, &gss_type);
     if ((status != GSS_S_COMPLETE) || (gss_type != gss_nt_krb5_name)) {
	  krb5_klog_syslog(LOG_ERR, _("gss_to_krb5_name: failed display_name "
				      "status %d"), status);
	  log_badauth(status, minor_stat, rqstp->rq_xprt, NULL);
	  return 0;
     }
     str = malloc(gss_str->length +1);
     if (str == NULL)
	  return 0;
     *str = '\0';

     strncat(str, gss_str->value, gss_str->length);
     success = (krb5_parse_name(ctx, str, princ) == 0);
     free(str);
     return success;
}
