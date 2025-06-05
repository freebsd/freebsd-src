/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (C) 2006,2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include "k5-int.h"
#include "pkinit.h"

/*
 * Routines for handling profile [config file] options
 */

/* Forward prototypes */
static int _krb5_conf_boolean(const char *s);

/*
 * XXX
 * The following is duplicated verbatim from src/lib/krb5/krb/get_in_tkt.c,
 * which is duplicated from somewhere else. :-/
 * XXX
 */
static const char *const conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static const char *const conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

static int
_krb5_conf_boolean(const char *s)
{
    const char *const *p;

    for(p=conf_yes; *p; p++) {
        if (strcasecmp(*p,s) == 0)
            return 1;
    }

    for(p=conf_no; *p; p++) {
        if (strcasecmp(*p,s) == 0)
            return 0;
    }

    /* Default to "no" */
    return 0;
}

/*
 * XXX
 * End duplicated code from src/lib/krb5/krb/get_in_tkt.c
 * XXX
 */

/*
 * The following are based on krb5_libdefault_* functions in
 * src/lib/krb5/krb/get_in_tkt.c
 * N.B.  This assumes that context->default_realm has
 * already been established.
 */
krb5_error_code
pkinit_kdcdefault_strings(krb5_context context, const char *realmname,
                          const char *option, char ***ret_value)
{
    profile_t profile = NULL;
    const char *names[5];
    char **values = NULL;
    krb5_error_code retval;

    if (context == NULL)
        return KV5M_CONTEXT;

    profile = context->profile;

    if (realmname != NULL) {
        /*
         * Try number one:
         *
         * [realms]
         *          REALM = {
         *              option = <value>
         *          }
         */

        names[0] = KRB5_CONF_REALMS;
        names[1] = realmname;
        names[2] = option;
        names[3] = 0;
        retval = profile_get_values(profile, names, &values);
        if (retval == 0 && values != NULL)
            goto goodbye;
    }

    /*
     * Try number two:
     *
     * [kdcdefaults]
     *      option = <value>
     */

    names[0] = KRB5_CONF_KDCDEFAULTS;
    names[1] = option;
    names[2] = 0;
    retval = profile_get_values(profile, names, &values);
    if (retval == 0 && values != NULL)
        goto goodbye;

goodbye:
    if (values == NULL)
        retval = ENOENT;

    *ret_value = values;

    return retval;

}

krb5_error_code
pkinit_kdcdefault_string(krb5_context context, const char *realmname,
                         const char *option, char **ret_value)
{
    krb5_error_code retval;
    char **values = NULL;

    retval = pkinit_kdcdefault_strings(context, realmname, option, &values);
    if (retval)
        return retval;

    if (values[0] == NULL) {
        retval = ENOENT;
    } else {
        *ret_value = strdup(values[0]);
        if (*ret_value == NULL)
            retval = ENOMEM;
    }

    profile_free_list(values);
    return retval;
}

krb5_error_code
pkinit_kdcdefault_boolean(krb5_context context, const char *realmname,
                          const char *option, int default_value, int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = pkinit_kdcdefault_string(context, realmname, option, &string);

    if (retval == 0) {
        *ret_value = _krb5_conf_boolean(string);
        free(string);
    } else
        *ret_value = default_value;

    return 0;
}

krb5_error_code
pkinit_kdcdefault_integer(krb5_context context, const char *realmname,
                          const char *option, int default_value, int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = pkinit_kdcdefault_string(context, realmname, option, &string);

    if (retval == 0) {
        char *endptr;
        long l;
        l = strtol(string, &endptr, 0);
        if (endptr == string)
            *ret_value = default_value;
        else
            *ret_value = l;
        free(string);
    } else
        *ret_value = default_value;

    return 0;
}


/*
 * krb5_libdefault_string() is defined as static in
 * src/lib/krb5/krb/get_in_tkt.c.  Create local versions of
 * krb5_libdefault_* functions here.  We need a libdefaults_strings()
 * function which is not currently supported there anyway.  Also,
 * add the ability to supply a default value for the boolean and
 * integer functions.
 */

krb5_error_code
pkinit_libdefault_strings(krb5_context context, const krb5_data *realm,
                          const char *option, char ***ret_value)
{
    profile_t profile;
    const char *names[5];
    char **values = NULL;
    krb5_error_code retval;
    char realmstr[1024];

    if (realm != NULL && realm->length > sizeof(realmstr)-1)
        return EINVAL;

    if (realm != NULL) {
        strncpy(realmstr, realm->data, realm->length);
        realmstr[realm->length] = '\0';
    }

    if (!context || (context->magic != KV5M_CONTEXT))
        return KV5M_CONTEXT;

    profile = context->profile;


    if (realm != NULL) {
        /*
         * Try number one:
         *
         * [libdefaults]
         *        REALM = {
         *                option = <value>
         *        }
         */

        names[0] = KRB5_CONF_LIBDEFAULTS;
        names[1] = realmstr;
        names[2] = option;
        names[3] = 0;
        retval = profile_get_values(profile, names, &values);
        if (retval == 0 && values != NULL && values[0] != NULL)
            goto goodbye;

        /*
         * Try number two:
         *
         * [realms]
         *      REALM = {
         *              option = <value>
         *      }
         */

        names[0] = KRB5_CONF_REALMS;
        names[1] = realmstr;
        names[2] = option;
        names[3] = 0;
        retval = profile_get_values(profile, names, &values);
        if (retval == 0 && values != NULL && values[0] != NULL)
            goto goodbye;
    }

    /*
     * Try number three:
     *
     * [libdefaults]
     *        option = <value>
     */

    names[0] = KRB5_CONF_LIBDEFAULTS;
    names[1] = option;
    names[2] = 0;
    retval = profile_get_values(profile, names, &values);
    if (retval == 0 && values != NULL && values[0] != NULL)
        goto goodbye;

goodbye:
    if (values == NULL)
        return ENOENT;

    *ret_value = values;

    return retval;
}

krb5_error_code
pkinit_libdefault_string(krb5_context context, const krb5_data *realm,
                         const char *option, char **ret_value)
{
    krb5_error_code retval;
    char **values = NULL;

    retval = pkinit_libdefault_strings(context, realm, option, &values);
    if (retval)
        return retval;

    if (values[0] == NULL) {
        retval = ENOENT;
    } else {
        *ret_value = strdup(values[0]);
        if (*ret_value == NULL)
            retval = ENOMEM;
    }

    profile_free_list(values);
    return retval;
}

krb5_error_code
pkinit_libdefault_boolean(krb5_context context, const krb5_data *realm,
                          const char *option, int default_value,
                          int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = pkinit_libdefault_string(context, realm, option, &string);

    if (retval == 0) {
        *ret_value = _krb5_conf_boolean(string);
        free(string);
    } else
        *ret_value = default_value;

    return 0;
}

krb5_error_code
pkinit_libdefault_integer(krb5_context context, const krb5_data *realm,
                          const char *option, int default_value,
                          int *ret_value)
{
    char *string = NULL;
    krb5_error_code retval;

    retval = pkinit_libdefault_string(context, realm, option, &string);

    if (retval == 0) {
        char *endptr;
        long l;
        l = strtol(string, &endptr, 0);
        if (endptr == string)
            *ret_value = default_value;
        else
            *ret_value = l;
        free(string);
    }

    return retval;
}
