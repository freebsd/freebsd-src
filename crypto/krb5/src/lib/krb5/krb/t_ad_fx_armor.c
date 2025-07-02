/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include <memory.h>
#include <stdio.h>
#include <krb5/krb5.h>

#define test(x) do {retval = (x);                                       \
        if(retval != 0) {                                               \
            const char *errmsg = krb5_get_error_message(context, retval); \
            fprintf(stderr, "Error message: %s\n", errmsg);             \
            abort(); }                                                  \
    } while(0);

krb5_authdata ad_fx_armor = {0, KRB5_AUTHDATA_FX_ARMOR, 1, ""};
krb5_authdata *array[] = {&ad_fx_armor, NULL};


int main( int argc, char **argv)
{
    krb5_context context;
    krb5_ccache ccache = NULL;
    krb5_creds creds, *out_creds = NULL;
    krb5_error_code retval = 0;
    test(krb5_init_context(&context));
    memset(&creds, 0, sizeof(creds));
    creds.authdata = array;
    test(krb5_cc_default(context, &ccache));
    test(krb5_cc_get_principal(context, ccache, &creds.client));
    test(krb5_parse_name(context, argv[1], &creds.server));
    test(krb5_get_credentials(context, 0, ccache, &creds, &out_creds));
    test(krb5_cc_destroy(context, ccache));
    test(krb5_cc_default(context, &ccache));
    test(krb5_cc_initialize(context, ccache, out_creds->client));
    test(krb5_cc_store_cred(context, ccache, out_creds));
    test(krb5_cc_close(context,ccache));
    return 0;

}
