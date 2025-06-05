/*
 * Parse PAM options into a struct.
 *
 * Given a struct in which to store options and a specification for what
 * options go where, parse both the PAM configuration options and any options
 * from a Kerberos krb5.conf file and fill out the struct.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2008, 2010-2011, 2013-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/system.h>

#include <errno.h>

#include <pam-util/args.h>
#include <pam-util/logging.h>
#include <pam-util/options.h>
#include <pam-util/vector.h>

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

/*
 * Macros used to resolve a void * pointer to the configuration struct and an
 * offset into a pointer to the appropriate type.  Scary violations of the C
 * type system lurk here.
 */
/* clang-format off */
#define CONF_BOOL(c, o)   (bool *)          (void *)((char *) (c) + (o))
#define CONF_NUMBER(c, o) (long *)          (void *)((char *) (c) + (o))
#define CONF_STRING(c, o) (char **)         (void *)((char *) (c) + (o))
#define CONF_LIST(c, o)   (struct vector **)(void *)((char *) (c) + (o))
/* clang-format on */

/*
 * We can only process times properly if we have Kerberos.  If not, they fall
 * back to longs and we convert them as numbers.
 */
/* clang-format off */
#ifdef HAVE_KRB5
#    define CONF_TIME(c, o) (krb5_deltat *)(void *)((char *) (c) + (o))
#else
#    define CONF_TIME(c, o) (long *)       (void *)((char *) (c) + (o))
#endif
/* clang-format on */


/*
 * Set a vector argument to its default.  This needs to do a deep copy of the
 * vector so that we can safely free it when freeing the configuration.  Takes
 * the PAM argument struct, the pointer in which to store the vector, and the
 * default vector.  Returns true if the default was set correctly and false on
 * memory allocation failure, which is also reported with putil_crit().
 */
static bool
copy_default_list(struct pam_args *args, struct vector **setting,
                  const struct vector *defval)
{
    struct vector *result = NULL;

    *setting = NULL;
    if (defval != NULL && defval->strings != NULL) {
        result = vector_copy(defval);
        if (result == NULL) {
            putil_crit(args, "cannot allocate memory: %s", strerror(errno));
            return false;
        }
        *setting = result;
    }
    return true;
}


/*
 * Set a vector argument to a default based on a string.  Takes the PAM
 * argument struct,t he pointer into which to store the vector, and the
 * default string.  Returns true if the default was set correctly and false on
 * memory allocation failure, which is also reported with putil_crit().
 */
static bool
default_list_string(struct pam_args *args, struct vector **setting,
                    const char *defval)
{
    struct vector *result = NULL;

    *setting = NULL;
    if (defval != NULL) {
        result = vector_split_multi(defval, " \t,", NULL);
        if (result == NULL) {
            putil_crit(args, "cannot allocate memory: %s", strerror(errno));
            return false;
        }
        *setting = result;
    }
    return true;
}


/*
 * Set the defaults for the PAM configuration.  Takes the PAM arguments, an
 * option table defined as above, and the number of entries in the table.  The
 * config member of the args struct must already be allocated.  Returns true
 * on success and false on error (generally out of memory).  Errors will
 * already be reported using putil_crit().
 *
 * This function must be called before either putil_args_krb5() or
 * putil_args_parse(), since neither of those functions set defaults.
 */
bool
putil_args_defaults(struct pam_args *args, const struct option options[],
                    size_t optlen)
{
    size_t opt;

    for (opt = 0; opt < optlen; opt++) {
        bool *bp;
        long *lp;
#ifdef HAVE_KRB5
        krb5_deltat *tp;
#else
        long *tp;
#endif
        char **sp;
        struct vector **vp;

        switch (options[opt].type) {
        case TYPE_BOOLEAN:
            bp = CONF_BOOL(args->config, options[opt].location);
            *bp = options[opt].defaults.boolean;
            break;
        case TYPE_NUMBER:
            lp = CONF_NUMBER(args->config, options[opt].location);
            *lp = options[opt].defaults.number;
            break;
        case TYPE_TIME:
            tp = CONF_TIME(args->config, options[opt].location);
            *tp = (krb5_deltat) options[opt].defaults.number;
            break;
        case TYPE_STRING:
            sp = CONF_STRING(args->config, options[opt].location);
            if (options[opt].defaults.string == NULL)
                *sp = NULL;
            else {
                *sp = strdup(options[opt].defaults.string);
                if (*sp == NULL) {
                    putil_crit(args, "cannot allocate memory: %s",
                               strerror(errno));
                    return false;
                }
            }
            break;
        case TYPE_LIST:
            vp = CONF_LIST(args->config, options[opt].location);
            if (!copy_default_list(args, vp, options[opt].defaults.list))
                return false;
            break;
        case TYPE_STRLIST:
            vp = CONF_LIST(args->config, options[opt].location);
            if (!default_list_string(args, vp, options[opt].defaults.string))
                return false;
            break;
        }
    }
    return true;
}


#ifdef HAVE_KRB5
/*
 * Load a boolean option from Kerberos appdefaults.  Takes the PAM argument
 * struct, the section name, the realm, the option, and the result location.
 *
 * The stupidity of rewriting the realm argument into a krb5_data is required
 * by MIT Kerberos.
 */
static void
default_boolean(struct pam_args *args, const char *section, const char *realm,
                const char *opt, bool *result)
{
    int tmp;
#    ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata = realm;
#    else
    krb5_data realm_struct;
    const krb5_data *rdata;

    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = (unsigned int) strlen(realm);
    }
#    endif

    /*
     * The MIT version of krb5_appdefault_boolean takes an int * and the
     * Heimdal version takes a krb5_boolean *, so hope that Heimdal always
     * defines krb5_boolean to int or this will require more portability work.
     */
    krb5_appdefault_boolean(args->ctx, section, rdata, opt, *result, &tmp);
    *result = tmp;
}


/*
 * Load a number option from Kerberos appdefaults.  Takes the PAM argument
 * struct, the section name, the realm, the option, and the result location.
 * The native interface doesn't support numbers, so we actually read a string
 * and then convert.
 */
static void
default_number(struct pam_args *args, const char *section, const char *realm,
               const char *opt, long *result)
{
    char *tmp = NULL;
    char *end;
    long value;
#    ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata = realm;
#    else
    krb5_data realm_struct;
    const krb5_data *rdata;

    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = (unsigned int) strlen(realm);
    }
#    endif

    krb5_appdefault_string(args->ctx, section, rdata, opt, "", &tmp);
    if (tmp != NULL && tmp[0] != '\0') {
        errno = 0;
        value = strtol(tmp, &end, 10);
        if (errno != 0 || *end != '\0')
            putil_err(args, "invalid number in krb5.conf setting for %s: %s",
                      opt, tmp);
        else
            *result = value;
    }
    free(tmp);
}


/*
 * Load a time option from Kerberos appdefaults.  Takes the PAM argument
 * struct, the section name, the realm, the option, and the result location.
 * The native interface doesn't support numbers, so we actually read a string
 * and then convert using krb5_string_to_deltat.
 */
static void
default_time(struct pam_args *args, const char *section, const char *realm,
             const char *opt, krb5_deltat *result)
{
    char *tmp = NULL;
    krb5_deltat value;
    krb5_error_code retval;
#    ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata = realm;
#    else
    krb5_data realm_struct;
    const krb5_data *rdata;

    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = (unsigned int) strlen(realm);
    }
#    endif

    krb5_appdefault_string(args->ctx, section, rdata, opt, "", &tmp);
    if (tmp != NULL && tmp[0] != '\0') {
        retval = krb5_string_to_deltat(tmp, &value);
        if (retval != 0)
            putil_err(args, "invalid time in krb5.conf setting for %s: %s",
                      opt, tmp);
        else
            *result = value;
    }
    free(tmp);
}


/*
 * Load a string option from Kerberos appdefaults.  Takes the PAM argument
 * struct, the section name, the realm, the option, and the result location.
 *
 * This requires an annoying workaround because one cannot specify a default
 * value of NULL with MIT Kerberos, since MIT Kerberos unconditionally calls
 * strdup on the default value.  There's also no way to determine if memory
 * allocation failed while parsing or while setting the default value, so we
 * don't return an error code.
 */
static void
default_string(struct pam_args *args, const char *section, const char *realm,
               const char *opt, char **result)
{
    char *value = NULL;
#    ifdef HAVE_KRB5_REALM
    krb5_const_realm rdata = realm;
#    else
    krb5_data realm_struct;
    const krb5_data *rdata;

    if (realm == NULL)
        rdata = NULL;
    else {
        rdata = &realm_struct;
        realm_struct.magic = KV5M_DATA;
        realm_struct.data = (void *) realm;
        realm_struct.length = (unsigned int) strlen(realm);
    }
#    endif

    krb5_appdefault_string(args->ctx, section, rdata, opt, "", &value);
    if (value != NULL) {
        if (value[0] == '\0')
            free(value);
        else {
            if (*result != NULL)
                free(*result);
            *result = value;
        }
    }
}


/*
 * Load a list option from Kerberos appdefaults.  Takes the PAM arguments, the
 * context, the section name, the realm, the option, and the result location.
 *
 * We may fail here due to memory allocation problems, in which case we return
 * false to indicate that PAM setup should abort.
 */
static bool
default_list(struct pam_args *args, const char *section, const char *realm,
             const char *opt, struct vector **result)
{
    char *tmp = NULL;
    struct vector *value;

    default_string(args, section, realm, opt, &tmp);
    if (tmp != NULL) {
        value = vector_split_multi(tmp, " \t,", NULL);
        if (value == NULL) {
            free(tmp);
            putil_crit(args, "cannot allocate vector: %s", strerror(errno));
            return false;
        }
        if (*result != NULL)
            vector_free(*result);
        *result = value;
        free(tmp);
    }
    return true;
}


/*
 * The public interface for getting configuration information from krb5.conf.
 * Takes the PAM arguments, the krb5.conf section, the options specification,
 * and the number of options in the options table.  The config member of the
 * args struct must already be allocated.  Iterate through the option list
 * and, for every option where krb5_config is true, see if it's set in the
 * Kerberos configuration.
 *
 * This looks obviously slow, but there haven't been any reports of problems
 * and there's no better interface.  But if you wonder where the cycles in
 * your computer are getting wasted, well, here's one place.
 */
bool
putil_args_krb5(struct pam_args *args, const char *section,
                const struct option options[], size_t optlen)
{
    size_t i;
    char *realm;
    bool free_realm = false;

    /* Having no local realm may be intentional, so don't report an error. */
    if (args->realm != NULL)
        realm = args->realm;
    else {
        if (krb5_get_default_realm(args->ctx, &realm) < 0)
            realm = NULL;
        else
            free_realm = true;
    }
    for (i = 0; i < optlen; i++) {
        const struct option *opt = &options[i];

        if (!opt->krb5_config)
            continue;
        switch (opt->type) {
        case TYPE_BOOLEAN:
            default_boolean(args, section, realm, opt->name,
                            CONF_BOOL(args->config, opt->location));
            break;
        case TYPE_NUMBER:
            default_number(args, section, realm, opt->name,
                           CONF_NUMBER(args->config, opt->location));
            break;
        case TYPE_TIME:
            default_time(args, section, realm, opt->name,
                         CONF_TIME(args->config, opt->location));
            break;
        case TYPE_STRING:
            default_string(args, section, realm, opt->name,
                           CONF_STRING(args->config, opt->location));
            break;
        case TYPE_LIST:
        case TYPE_STRLIST:
            if (!default_list(args, section, realm, opt->name,
                              CONF_LIST(args->config, opt->location)))
                return false;
            break;
        }
    }
    if (free_realm)
        krb5_free_default_realm(args->ctx, realm);
    return true;
}

#else /* !HAVE_KRB5 */

/*
 * Stub function for getting configuration information from krb5.conf used
 * when the PAM module is not built with Kerberos support so that the function
 * can be called unconditionally.
 */
bool
putil_args_krb5(struct pam_args *args UNUSED, const char *section UNUSED,
                const struct option options[] UNUSED, size_t optlen UNUSED)
{
    return true;
}

#endif /* !HAVE_KRB5 */


/*
 * bsearch comparison function for finding PAM arguments in an array of struct
 * options.  We only compare up to the first '=' in the key so that we don't
 * have to munge the string before searching.
 */
static int
option_compare(const void *key, const void *member)
{
    const char *string = key;
    const struct option *option = member;
    const char *p;
    size_t length;
    int result;

    p = strchr(string, '=');
    if (p == NULL)
        return strcmp(string, option->name);
    else {
        length = (size_t)(p - string);
        if (length == 0)
            return -1;
        result = strncmp(string, option->name, length);
        if (result == 0 && strlen(option->name) > length)
            return -1;
        return result;
    }
}


/*
 * Given a PAM argument, convert the value portion of the argument to a
 * boolean and store it in the provided location.  If the value is missing,
 * that's equivalent to a true value.  If the value is invalid, report an
 * error and leave the location unchanged.
 */
static void
convert_boolean(struct pam_args *args, const char *arg, bool *setting)
{
    const char *value;

    value = strchr(arg, '=');
    if (value == NULL)
        *setting = true;
    else {
        value++;
        /* clang-format off */
        if      (   strcasecmp(value, "true") == 0
                 || strcasecmp(value, "yes")  == 0
                 || strcasecmp(value, "on")   == 0
                 || strcmp    (value, "1")    == 0)
            *setting = true;
        else if (   strcasecmp(value, "false") == 0
                 || strcasecmp(value, "no")    == 0
                 || strcasecmp(value, "off")   == 0
                 || strcmp    (value, "0")     == 0)
            *setting = false;
        else
            putil_err(args, "invalid boolean in setting: %s", arg);
        /* clang-format on */
    }
}


/*
 * Given a PAM argument, convert the value portion of the argument to a number
 * and store it in the provided location.  If the value is missing or isn't a
 * number, report an error and leave the location unchanged.
 */
static void
convert_number(struct pam_args *args, const char *arg, long *setting)
{
    const char *value;
    char *end;
    long result;

    value = strchr(arg, '=');
    if (value == NULL || value[1] == '\0') {
        putil_err(args, "value missing for option %s", arg);
        return;
    }
    errno = 0;
    result = strtol(value + 1, &end, 10);
    if (errno != 0 || *end != '\0') {
        putil_err(args, "invalid number in setting: %s", arg);
        return;
    }
    *setting = result;
}


/*
 * Given a PAM argument, convert the value portion of the argument from a
 * Kerberos time string to a krb5_deltat and store it in the provided
 * location.  If the value is missing or isn't a number, report an error and
 * leave the location unchanged.
 */
#ifdef HAVE_KRB5
static void
convert_time(struct pam_args *args, const char *arg, krb5_deltat *setting)
{
    const char *value;
    krb5_deltat result;
    krb5_error_code retval;

    value = strchr(arg, '=');
    if (value == NULL || value[1] == '\0') {
        putil_err(args, "value missing for option %s", arg);
        return;
    }
    retval = krb5_string_to_deltat((char *) value + 1, &result);
    if (retval != 0)
        putil_err(args, "bad time value in setting: %s", arg);
    else
        *setting = result;
}

#else /* HAVE_KRB5 */

static void
convert_time(struct pam_args *args, const char *arg, long *setting)
{
    convert_number(args, arg, setting);
}

#endif /* !HAVE_KRB5 */


/*
 * Given a PAM argument, convert the value portion of the argument to a string
 * and store it in the provided location.  If the value is missing, report an
 * error and leave the location unchanged, returning true since that's a
 * non-fatal error.  If memory allocation fails, return false, since PAM setup
 * should abort.
 */
static bool
convert_string(struct pam_args *args, const char *arg, char **setting)
{
    const char *value;
    char *result;

    value = strchr(arg, '=');
    if (value == NULL) {
        putil_err(args, "value missing for option %s", arg);
        return true;
    }
    result = strdup(value + 1);
    if (result == NULL) {
        putil_crit(args, "cannot allocate memory: %s", strerror(errno));
        return false;
    }
    free(*setting);
    *setting = result;
    return true;
}


/*
 * Given a PAM argument, convert the value portion of the argument to a vector
 * and store it in the provided location.  If the value is missing, report an
 * error and leave the location unchanged, returning true since that's a
 * non-fatal error.  If memory allocation fails, return false, since PAM setup
 * should abort.
 */
static bool
convert_list(struct pam_args *args, const char *arg, struct vector **setting)
{
    const char *value;
    struct vector *result;

    value = strchr(arg, '=');
    if (value == NULL) {
        putil_err(args, "value missing for option %s", arg);
        return true;
    }
    result = vector_split_multi(value + 1, " \t,", NULL);
    if (result == NULL) {
        putil_crit(args, "cannot allocate vector: %s", strerror(errno));
        return false;
    }
    vector_free(*setting);
    *setting = result;
    return true;
}


/*
 * Parse the PAM arguments.  Takes the PAM argument struct, the argument count
 * and vector, the option table, and the number of elements in the option
 * table.  The config member of the args struct must already be allocated.
 * Returns true on success and false on error.  An error return should be
 * considered fatal.  Report errors using putil_crit().  Unknown options will
 * also be diagnosed (to syslog at LOG_ERR using putil_err()), but are not
 * considered fatal errors and will still return true.
 *
 * If options should be retrieved from krb5.conf, call putil_args_krb5()
 * first, before calling this function.
 */
bool
putil_args_parse(struct pam_args *args, int argc, const char *argv[],
                 const struct option options[], size_t optlen)
{
    int i;
    const struct option *option;

    /*
     * Second pass: find each option we were given and set the corresponding
     * configuration parameter.
     */
    for (i = 0; i < argc; i++) {
        option = bsearch(argv[i], options, optlen, sizeof(struct option),
                         option_compare);
        if (option == NULL) {
            putil_err(args, "unknown option %s", argv[i]);
            continue;
        }
        switch (option->type) {
        case TYPE_BOOLEAN:
            convert_boolean(args, argv[i],
                            CONF_BOOL(args->config, option->location));
            break;
        case TYPE_NUMBER:
            convert_number(args, argv[i],
                           CONF_NUMBER(args->config, option->location));
            break;
        case TYPE_TIME:
            convert_time(args, argv[i],
                         CONF_TIME(args->config, option->location));
            break;
        case TYPE_STRING:
            if (!convert_string(args, argv[i],
                                CONF_STRING(args->config, option->location)))
                return false;
            break;
        case TYPE_LIST:
        case TYPE_STRLIST:
            if (!convert_list(args, argv[i],
                              CONF_LIST(args->config, option->location)))
                return false;
            break;
        }
    }
    return true;
}
