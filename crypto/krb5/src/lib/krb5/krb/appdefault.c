/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * appdefault - routines designed to be called from applications to
 *               handle the [appdefaults] profile section
 */

#include "k5-int.h"



/*xxx Duplicating this is annoying; try to work on a better way.*/
static const char *const conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static const char *const conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

static int
conf_boolean(char *s)
{
    const char * const *p;
    for(p=conf_yes; *p; p++) {
        if (!strcasecmp(*p,s))
            return 1;
    }
    for(p=conf_no; *p; p++) {
        if (!strcasecmp(*p,s))
            return 0;
    }
    /* Default to "no" */
    return 0;
}

static krb5_error_code
appdefault_get(krb5_context context, const char *appname, const krb5_data *realm, const char *option, char **ret_value)
{
    profile_t profile;
    const char *names[5];
    char **nameval = NULL;
    krb5_error_code retval;
    const char * realmstr =  realm?realm->data:NULL;

    *ret_value = NULL;

    if (!context || (context->magic != KV5M_CONTEXT))
        return KV5M_CONTEXT;

    profile = context->profile;

    /*
     * Try number one:
     *
     * [appdefaults]
     *      app = {
     *              SOME.REALM = {
     *                      option = <boolean>
     *              }
     *      }
     */

    names[0] = "appdefaults";
    names[1] = appname;

    if (realmstr) {
        names[2] = realmstr;
        names[3] = option;
        names[4] = 0;
        retval = profile_get_values(profile, names, &nameval);
        if (retval == 0 && nameval && nameval[0]) {
            *ret_value = strdup(nameval[0]);
            goto goodbye;
        }
    }

    /*
     * Try number two:
     *
     * [appdefaults]
     *      app = {
     *              option = <boolean>
     *      }
     */

    names[2] = option;
    names[3] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0]) {
        *ret_value = strdup(nameval[0]);
        goto goodbye;
    }

    /*
     * Try number three:
     *
     * [appdefaults]
     *      realm = {
     *              option = <boolean>
     */

    if (realmstr) {
        names[1] = realmstr;
        names[2] = option;
        names[3] = 0;
        retval = profile_get_values(profile, names, &nameval);
        if (retval == 0 && nameval && nameval[0]) {
            *ret_value = strdup(nameval[0]);
            goto goodbye;
        }
    }

    /*
     * Try number four:
     *
     * [appdefaults]
     *      option = <boolean>
     */

    names[1] = option;
    names[2] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0]) {
        *ret_value = strdup(nameval[0]);
    } else {
        return retval;
    }

goodbye:
    if (nameval) {
        char **cpp;
        for (cpp = nameval; *cpp; cpp++)
            free(*cpp);
        free(nameval);
    }
    return 0;
}

void KRB5_CALLCONV
krb5_appdefault_boolean(krb5_context context, const char *appname, const krb5_data *realm, const char *option, int default_value, int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = appdefault_get(context, appname, realm, option, &string);

    if (! retval && string) {
        *ret_value = conf_boolean(string);
        free(string);
    } else
        *ret_value = default_value;
}

void KRB5_CALLCONV
krb5_appdefault_string(krb5_context context, const char *appname, const krb5_data *realm, const char *option, const char *default_value, char **ret_value)
{
    krb5_error_code retval;
    char *string;

    retval = appdefault_get(context, appname, realm, option, &string);

    if (! retval && string) {
        *ret_value = string;
    } else {
        *ret_value = strdup(default_value);
    }
}
