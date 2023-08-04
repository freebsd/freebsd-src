/* -*- mode: c; c-file-style: "bsd"; indent-tabs-mode: t -*- */
#include <gssrpc/rpc.h>
#include <kadm5/kadm_rpc.h>
#include <krb5.h>
#include <kadm5/admin.h>
#include <string.h>  /* for memset prototype */

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

enum clnt_stat
create_principal_2(cprinc_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CREATE_PRINCIPAL,
			 (xdrproc_t)xdr_cprinc_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
create_principal3_2(cprinc3_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CREATE_PRINCIPAL3,
			 (xdrproc_t)xdr_cprinc3_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
delete_principal_2(dprinc_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, DELETE_PRINCIPAL,
			 (xdrproc_t)xdr_dprinc_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
modify_principal_2(mprinc_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, MODIFY_PRINCIPAL,
			 (xdrproc_t)xdr_mprinc_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
rename_principal_2(rprinc_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, RENAME_PRINCIPAL,
			 (xdrproc_t)xdr_rprinc_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_principal_2(gprinc_arg *argp, gprinc_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_PRINCIPAL,
			 (xdrproc_t)xdr_gprinc_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_gprinc_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_princs_2(gprincs_arg *argp, gprincs_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_PRINCS,
			 (xdrproc_t)xdr_gprincs_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_gprincs_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
chpass_principal_2(chpass_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CHPASS_PRINCIPAL,
			 (xdrproc_t)xdr_chpass_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
chpass_principal3_2(chpass3_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CHPASS_PRINCIPAL3,
			 (xdrproc_t)xdr_chpass3_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
setkey_principal_2(setkey_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, SETKEY_PRINCIPAL,
			 (xdrproc_t)xdr_setkey_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
setkey_principal3_2(setkey3_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, SETKEY_PRINCIPAL3,
			 (xdrproc_t)xdr_setkey3_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
setkey_principal4_2(setkey4_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, SETKEY_PRINCIPAL4,
			 (xdrproc_t)xdr_setkey4_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
chrand_principal_2(chrand_arg *argp, chrand_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CHRAND_PRINCIPAL,
			 (xdrproc_t)xdr_chrand_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_chrand_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
chrand_principal3_2(chrand3_arg *argp, chrand_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CHRAND_PRINCIPAL3,
			 (xdrproc_t)xdr_chrand3_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_chrand_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
create_policy_2(cpol_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, CREATE_POLICY,
			 (xdrproc_t)xdr_cpol_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
delete_policy_2(dpol_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, DELETE_POLICY,
			 (xdrproc_t)xdr_dpol_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
modify_policy_2(mpol_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, MODIFY_POLICY,
			 (xdrproc_t)xdr_mpol_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_policy_2(gpol_arg *argp, gpol_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_POLICY,
			 (xdrproc_t)xdr_gpol_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_gpol_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_pols_2(gpols_arg *argp, gpols_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_POLS,
			 (xdrproc_t)xdr_gpols_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_gpols_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_privs_2(void *argp, getprivs_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_PRIVS,
			 (xdrproc_t)xdr_u_int32, (caddr_t)argp,
			 (xdrproc_t)xdr_getprivs_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
init_2(void *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, INIT,
			 (xdrproc_t)xdr_u_int32, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
purgekeys_2(purgekeys_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, PURGEKEYS,
			 (xdrproc_t)xdr_purgekeys_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_strings_2(gstrings_arg *argp, gstrings_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, GET_STRINGS,
			 (xdrproc_t)xdr_gstrings_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_gstrings_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
set_string_2(sstring_arg *argp, generic_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, SET_STRING,
			 (xdrproc_t)xdr_sstring_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_generic_ret, (caddr_t)res, TIMEOUT);
}

enum clnt_stat
get_principal_keys_2(getpkeys_arg *argp, getpkeys_ret *res, CLIENT *clnt)
{
	return clnt_call(clnt, EXTRACT_KEYS,
			 (xdrproc_t)xdr_getpkeys_arg, (caddr_t)argp,
			 (xdrproc_t)xdr_getpkeys_ret, (caddr_t)res, TIMEOUT);
}
