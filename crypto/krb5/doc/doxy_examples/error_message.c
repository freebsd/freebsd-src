/** @example  error_message.c
 *
 *  Demo for krb5_get/set/free_error_message function family
 */
#include <k5-int.h>

krb5_error_code
func(krb5_context context)
{
    krb5_error_code ret;

    ret = krb5_func(context);
    if (ret) {
        const char *err_str = krb5_get_error_message(context, ret);
        krb5_set_error_message(context, ret,
                               "Failed krb5_func: %s", err_str);
        krb5_free_error_message(context, err_str);
    }
}
                
