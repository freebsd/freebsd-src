/** @example  cc_unique.c
 *
 *  Usage example for krb5_cc_new_unique function
 */
#include "k5-int.h"

krb5_error_code
func(krb5_context context)
{
    krb5_error_code ret;
    krb5_ccache ccache = NULL;

    ret = krb5_cc_new_unique(context, "MEMORY", NULL, &ccache);
    if (ret){
        ccache = NULL; 
        return ret;
    }
    /* do something */
    if (ccache)
        (void)krb5_cc_destroy(context, ccache);
    return 0;
}

