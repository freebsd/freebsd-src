/** @example  cc_set_config.c
 *
 *  Usage examples for krb5_cc_set_config and krb5_cc_get_config functions
 */
#include <k5-int.h>

krb5_error_code
func_set(krb5_context context, krb5_ccache id,
         krb5_const_principal principal, const char *key)
{
   krb5_data config_data;

   config_data.data = "yes";
   config_data.length = strlen(config_data.data);
   return  krb5_cc_set_config(context, id, principal, key, &config_data);
}

krb5_error_code
func_get(krb5_context context, krb5_ccache id,
         krb5_const_principal principal, const char *key)
{
   krb5_data config_data;
   krb5_error_code ret;

   config_data.data = NULL;
   ret = krb5_cc_get_config(context, id, principal, key, &config_data);
   if (ret){
        return ret;
   }
   /* do something */
   krb5_free_data_contents(context, &config_data);
   return ret;
}
