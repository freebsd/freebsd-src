/** @example  verify_init_creds.c
 *
 *  Usage example for krb5_verify_init_creds function family
 */
#include "k5-int.h"

krb5_error_code
func(krb5_context context,  krb5_creds *creds, krb5_principal server_principal)
{
    krb5_error_code ret = KRB5_OK;
    krb5_verify_init_creds_opt options;

    krb5_verify_init_creds_opt_init (&options);
    krb5_verify_init_creds_opt_set_ap_req_nofail (&options, 1);

    ret = krb5_verify_init_creds(context,
                                 creds,
                                 server_principal,
                                 NULL /* use default keytab */,
                                 NULL /* don't store creds in ccache */,
                                 &options);
    if (ret) {
        /* error while verifying credentials for server */
    }

    return ret;
}

