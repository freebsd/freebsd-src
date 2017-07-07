/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/alt_prof.c */
/*
 * Copyright 1995,2001,2008,2009 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/* Implement alternate profile file handling. */
#include "k5-int.h"
#include "fake-addrinfo.h"
#include <kadm5/admin.h>
#include "adm_proto.h"
#include <stdio.h>
#include <ctype.h>
#include <kdb_log.h>

static krb5_key_salt_tuple *
copy_key_salt_tuple(krb5_key_salt_tuple *ksalt, krb5_int32 len)
{
    krb5_key_salt_tuple *knew;

    knew = calloc(len, sizeof(krb5_key_salt_tuple));
    if (knew == NULL)
        return NULL;
    memcpy(knew, ksalt, len * sizeof(krb5_key_salt_tuple));
    return knew;
}

/*
 * krb5_aprof_init()        - Initialize alternate profile context.
 *
 * Parameters:
 *        fname             - default file name of the profile.
 *        envname           - environment variable which can override fname
 *        acontextp         - Pointer to opaque context for alternate profile
 *
 * Returns:
 *        error codes from profile_init()
 */
krb5_error_code
krb5_aprof_init(char *fname, char *envname, krb5_pointer *acontextp)
{
    krb5_error_code ret;
    profile_t profile;
    const char *kdc_config;
    char **filenames;
    int i;
    struct k5buf buf;

    ret = krb5_get_default_config_files(&filenames);
    if (ret)
        return ret;
    if (envname == NULL || (kdc_config = getenv(envname)) == NULL)
        kdc_config = fname;
    k5_buf_init_dynamic(&buf);
    if (kdc_config)
        k5_buf_add(&buf, kdc_config);
    for (i = 0; filenames[i] != NULL; i++) {
        if (buf.len > 0)
            k5_buf_add(&buf, ":");
        k5_buf_add(&buf, filenames[i]);
    }
    krb5_free_config_files(filenames);
    if (k5_buf_status(&buf) != 0)
        return ENOMEM;
    profile = (profile_t) NULL;
    ret = profile_init_path(buf.data, &profile);
    k5_buf_free(&buf);
    if (ret)
        return ret;
    *acontextp = profile;
    return 0;
}

/*
 * krb5_aprof_getvals()     - Get values from alternate profile.
 *
 * Parameters:
 *        acontext          - opaque context for alternate profile.
 *        hierarchy         - hierarchy of value to retrieve.
 *        retdata           - Returned data values.
 *
 * Returns:
 *         error codes from profile_get_values()
 */
krb5_error_code
krb5_aprof_getvals(krb5_pointer acontext, const char **hierarchy,
                   char ***retdata)
{
    return profile_get_values(acontext, hierarchy, retdata);
}

/*
 * krb5_aprof_get_boolean()
 *
 * Parameters:
 *        acontext          - opaque context for alternate profile
 *        hierarchy         - hierarchy of value to retrieve
 *        retdata           - Returned data value
 * Returns:
 *        error codes
 */

static krb5_error_code
string_to_boolean(const char *string, krb5_boolean *out)
{
    static const char *const yes[] = { "y", "yes", "true", "t", "1", "on" };
    static const char *const no[] = { "n", "no", "false", "f", "nil", "0",
                                      "off" };
    unsigned int i;

    for (i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
        if (!strcasecmp(string, yes[i])) {
            *out = TRUE;
            return 0;
        }
    }
    for (i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
        if (!strcasecmp(string, no[i])) {
            *out = FALSE;
            return 0;
        }
    }
    return PROF_BAD_BOOLEAN;
}

krb5_error_code
krb5_aprof_get_boolean(krb5_pointer acontext, const char **hierarchy,
                       int uselast, krb5_boolean *retdata)
{
    krb5_error_code ret;
    char **values, *valp;
    int idx;
    krb5_boolean val;

    ret = krb5_aprof_getvals(acontext, hierarchy, &values);
    if (ret)
        return ret;
    idx = 0;
    if (uselast) {
        while (values[idx] != NULL)
            idx++;
        idx--;
    }
    valp = values[idx];
    ret = string_to_boolean(valp, &val);
    profile_free_list(values);
    if (ret)
        return ret;
    *retdata = val;
    return 0;
}

/*
 * krb5_aprof_get_deltat()  - Get a delta time value from the alternate
 *                            profile.
 *
 * Parameters:
 *        acontext          - opaque context for alternate profile.
 *        hierarchy         - hierarchy of value to retrieve.
 *        uselast           - if true, use last value, otherwise use first
 *                            value found.
 *        deltatp           - returned delta time value.
 *
 * Returns:
 *        error codes from profile_get_values()
 *        error codes from krb5_string_to_deltat()
 */
krb5_error_code
krb5_aprof_get_deltat(krb5_pointer acontext, const char **hierarchy,
                      krb5_boolean uselast, krb5_deltat *deltatp)
{
    krb5_error_code ret;
    char **values, *valp;
    int idx;

    ret = krb5_aprof_getvals(acontext, hierarchy, &values);
    if (ret)
        return ret;

    idx = 0;
    if (uselast) {
        for (idx = 0; values[idx] != NULL; idx++);
        idx--;
    }
    valp = values[idx];

    ret = krb5_string_to_deltat(valp, deltatp);
    profile_free_list(values);
    return ret;
}

/*
 * krb5_aprof_get_string()  - Get a string value from the alternate profile.
 *
 * Parameters:
 *        acontext          - opaque context for alternate profile.
 *        hierarchy         - hierarchy of value to retrieve.
 *        uselast           - if true, use last value, otherwise use first
 *                            value found.
 *        stringp           - returned string value.
 *
 * Returns:
 *         error codes from profile_get_values()
 */
krb5_error_code
krb5_aprof_get_string(krb5_pointer acontext, const char **hierarchy,
                      krb5_boolean uselast, char **stringp)
{
    krb5_error_code ret;
    char **values;
    int lastidx;

    ret = krb5_aprof_getvals(acontext, hierarchy, &values);
    if (ret)
        return ret;

    for (lastidx = 0; values[lastidx] != NULL; lastidx++);
    lastidx--;

    /* Excise the entry we want from the null-terminated list,
     * and free up the rest. */
    if (uselast) {
        *stringp = values[lastidx];
        values[lastidx] = NULL;
    } else {
        *stringp = values[0];
        values[0] = values[lastidx];
        values[lastidx] = NULL;
    }

    profile_free_list(values);
    return 0;
}

/*
 * krb5_aprof_get_string_all() - When the attr identified by "hierarchy" is
 *                               specified multiple times, concatenate all of
 *                               its string values from the alternate profile,
 *                               separated with spaces.
 *
 * Parameters:
 *        acontext             - opaque context for alternate profile.
 *        hierarchy            - hierarchy of value to retrieve.
 *        stringp              - Returned string value.
 *
 * Returns:
 *        error codes from profile_get_values() or ENOMEM
 *        Caller is responsible for deallocating stringp buffer
 */
krb5_error_code
krb5_aprof_get_string_all(krb5_pointer acontext, const char **hierarchy,
                          char **stringp)
{
    krb5_error_code ret;
    char **values;
    int idx = 0;
    size_t buf_size = 0;

    ret = krb5_aprof_getvals(acontext, hierarchy, &values);
    if (ret)
        return ret;

    buf_size = strlen(values[0]) + 3;
    for (idx = 1; values[idx] != NULL; idx++)
        buf_size += strlen(values[idx]) + 3;

    *stringp = calloc(1, buf_size);
    if (*stringp == NULL) {
        profile_free_list(values);
        return ENOMEM;
    }
    strlcpy(*stringp, values[0], buf_size);
    for (idx = 1; values[idx] != NULL; idx++) {
        strlcat(*stringp, " ", buf_size);
        strlcat(*stringp, values[idx], buf_size);
    }

    profile_free_list(values);
    return 0;
}


/*
 * krb5_aprof_get_int32()   - Get a 32-bit integer value from the alternate
 *                            profile.
 *
 * Parameters:
 *        acontext          - opaque context for alternate profile.
 *        hierarchy         - hierarchy of value to retrieve.
 *        uselast           - if true, use last value, otherwise use first
 *                            value found.
 *        intp              - returned 32-bit integer value.
 *
 * Returns:
 *        error codes from profile_get_values()
 *        EINVAL            - value is not an integer
 */
krb5_error_code
krb5_aprof_get_int32(krb5_pointer acontext, const char **hierarchy,
                     krb5_boolean uselast, krb5_int32 *intp)
{
    krb5_error_code ret;
    char **values;
    int idx;

    ret = krb5_aprof_getvals(acontext, hierarchy, &values);
    if (ret)
        return ret;

    idx = 0;
    if (uselast) {
        for (idx = 0; values[idx] != NULL; idx++);
        idx--;
    }

    if (sscanf(values[idx], "%d", intp) != 1)
        ret = EINVAL;

    profile_free_list(values);
    return ret;
}

/*
 * krb5_aprof_finish()      - Finish alternate profile context.
 *
 * Parameter:
 *        acontext          - opaque context for alternate profile.
 *
 * Returns:
 *        0 on success, something else on failure.
 */
krb5_error_code
krb5_aprof_finish(krb5_pointer acontext)
{
    profile_release(acontext);
    return 0;
}

/*
 * Returns nonzero if it found something to copy; the caller may still need to
 * check the output field or mask to see if the copy (allocation) was
 * successful.  Returns zero if nothing was found to copy, and thus the caller
 * may want to apply some default heuristic.  If the default action is just to
 * use a fixed, compiled-in string, supply it as the default value here and
 * ignore the return value.
 */
static int
get_string_param(char **param_out, char *param_in, long *mask_out,
                 long mask_in, long mask_bit, krb5_pointer aprofile,
                 const char **hierarchy, const char *config_name,
                 const char *default_value)
{
    char *svalue;

    hierarchy[2] = config_name;
    if (mask_in & mask_bit) {
        *param_out = strdup(param_in);
        if (*param_out)
            *mask_out |= mask_bit;
        return 1;
    } else if (aprofile != NULL &&
               !krb5_aprof_get_string(aprofile, hierarchy, TRUE, &svalue)) {
        *param_out = svalue;
        *mask_out |= mask_bit;
        return 1;
    } else if (default_value) {
        *param_out = strdup(default_value);
        if (*param_out)
            *mask_out |= mask_bit;
        return 1;
    } else {
        return 0;
    }
}
/*
 * Similar, for (host-order) port number, if not already set in the output
 * field; default_value == 0 means no default.
 */
static void
get_port_param(int *param_out, int param_in, long *mask_out, long mask_in,
               long mask_bit, krb5_pointer aprofile, const char **hierarchy,
               const char *config_name, int default_value)
{
    krb5_int32 ivalue;

    if (*mask_out & mask_bit)
        return;
    hierarchy[2] = config_name;
    if (mask_in & mask_bit) {
        *mask_out |= mask_bit;
        *param_out = param_in;
    } else if (aprofile != NULL &&
               !krb5_aprof_get_int32(aprofile, hierarchy, TRUE, &ivalue)) {
        *param_out = ivalue;
        *mask_out |= mask_bit;
    } else if (default_value) {
        *param_out = default_value;
        *mask_out |= mask_bit;
    }
}

/*
 * Similar, for delta_t; default is required.
 */
static void
get_deltat_param(krb5_deltat *param_out, krb5_deltat param_in, long *mask_out,
                 long mask_in, long mask_bit, krb5_pointer aprofile,
                 const char **hierarchy, const char *config_name,
                 krb5_deltat default_value)
{
    krb5_deltat dtvalue;

    hierarchy[2] = config_name;
    if (mask_in & mask_bit) {
        *mask_out |= mask_bit;
        *param_out = param_in;
    } else if (aprofile &&
               !krb5_aprof_get_deltat(aprofile, hierarchy, TRUE, &dtvalue)) {
        *param_out = dtvalue;
        *mask_out |= mask_bit;
    } else {
        *param_out = default_value;
        *mask_out |= mask_bit;
    }
}

/*
 * Parse out the port number from an admin_server setting.  Modify server to
 * contain just the hostname or address.  If a port is given, set *port, and
 * set the appropriate bit in *mask.
 */
static void
parse_admin_server_port(char *server, int *port, long *mask)
{
    char *end, *portstr;

    /* Allow the name or addr to be enclosed in brackets, for IPv6 addrs. */
    if (*server == '[' && (end = strchr(server + 1, ']')) != NULL) {
        portstr = (*(end + 1) == ':') ? end + 2 : NULL;
        /* Shift the bracketed name or address back into server. */
        memmove(server, server + 1, end - (server + 1));
        *(end - 1) = '\0';
    } else {
        /* Terminate the name at the colon, if any. */
        end = server + strcspn(server, ":");
        portstr = (*end == ':') ? end + 1 : NULL;
        *end = '\0';
    }

    /* If we found a port string, parse it and set the appropriate bit. */
    if (portstr) {
        *port = atoi(portstr);
        *mask |= KADM5_CONFIG_KADMIND_PORT;
    }
}

/*
 * Function: kadm5_get_config_params
 *
 * Purpose: Merge configuration parameters provided by the caller with values
 * specified in configuration files and with default values.
 *
 * Arguments:
 *
 *        context     (r) krb5_context to use
 *        profile     (r) profile file to use
 *        envname     (r) envname that contains a profile name to
 *                        override profile
 *        params_in   (r) params structure containing user-supplied
 *                        values, or NULL
 *        params_out  (w) params structure to be filled in
 *
 * Effects:
 *
 * The fields and mask of params_out are filled in with values obtained from
 * params_in, the specified profile, and default values.  Only and all fields
 * specified in params_out->mask are set.  The context of params_out must be
 * freed with kadm5_free_config_params.
 *
 * params_in and params_out may be the same pointer.  However, all pointers in
 * params_in for which the mask is set will be re-assigned to newly copied
 * versions, overwriting the old pointer value.
 */
krb5_error_code kadm5_get_config_params(krb5_context context,
                                        int use_kdc_config,
                                        kadm5_config_params *params_in,
                                        kadm5_config_params *params_out)
{
    char *filename, *envname, *lrealm, *svalue, *sp, *ep, *tp;
    krb5_pointer aprofile = 0;
    const char *hierarchy[4];
    krb5_int32 ivalue;
    kadm5_config_params params, empty_params;
    krb5_boolean bvalue;
    krb5_error_code ret = 0;

    memset(&params, 0, sizeof(params));
    memset(&empty_params, 0, sizeof(empty_params));

    if (params_in == NULL)
        params_in = &empty_params;

    if (params_in->mask & KADM5_CONFIG_REALM) {
        lrealm = params.realm = strdup(params_in->realm);
        if (params.realm != NULL)
            params.mask |= KADM5_CONFIG_REALM;
    } else {
        ret = krb5_get_default_realm(context, &lrealm);
        if (ret)
            goto cleanup;
        params.realm = lrealm;
        params.mask |= KADM5_CONFIG_REALM;
    }

    if (params_in->mask & KADM5_CONFIG_KVNO) {
        params.kvno = params_in->kvno;
        params.mask |= KADM5_CONFIG_KVNO;
    }
    /*
     * XXX These defaults should to work on both client and
     * server.  kadm5_get_config_params can be implemented as a
     * wrapper function in each library that provides correct
     * defaults for NULL values.
     */
    if (use_kdc_config) {
        filename = DEFAULT_KDC_PROFILE;
        envname = KDC_PROFILE_ENV;
    } else {
        filename = DEFAULT_PROFILE_PATH;
        envname = "KRB5_CONFIG";
    }
    if (context->profile_secure == TRUE)
        envname = NULL;

    ret = krb5_aprof_init(filename, envname, &aprofile);
    if (ret)
        goto cleanup;

    /* Initialize realm parameters. */
    hierarchy[0] = KRB5_CONF_REALMS;
    hierarchy[1] = lrealm;
    hierarchy[3] = NULL;

#define GET_STRING_PARAM(FIELD, BIT, CONFTAG, DEFAULT)          \
    get_string_param(&params.FIELD, params_in->FIELD,           \
                     &params.mask, params_in->mask, BIT,        \
                     aprofile, hierarchy, CONFTAG, DEFAULT)

    /* Get the value for the admin server. */
    GET_STRING_PARAM(admin_server, KADM5_CONFIG_ADMIN_SERVER,
                     KRB5_CONF_ADMIN_SERVER, NULL);

    if (params.mask & KADM5_CONFIG_ADMIN_SERVER) {
        parse_admin_server_port(params.admin_server, &params.kadmind_port,
                                &params.mask);
    }

    /* Get the value for the database. */
    GET_STRING_PARAM(dbname, KADM5_CONFIG_DBNAME, KRB5_CONF_DATABASE_NAME,
                     DEFAULT_KDB_FILE);

    /* Get the name of the acl file. */
    GET_STRING_PARAM(acl_file, KADM5_CONFIG_ACL_FILE, KRB5_CONF_ACL_FILE,
                     DEFAULT_KADM5_ACL_FILE);

    /* Get the name of the dict file. */
    GET_STRING_PARAM(dict_file, KADM5_CONFIG_DICT_FILE, KRB5_CONF_DICT_FILE,
                     NULL);

    /* Get the kadmind listen addresses. */
    GET_STRING_PARAM(kadmind_listen, KADM5_CONFIG_KADMIND_LISTEN,
                     KRB5_CONF_KADMIND_LISTEN, NULL);
    GET_STRING_PARAM(kpasswd_listen, KADM5_CONFIG_KPASSWD_LISTEN,
                     KRB5_CONF_KPASSWD_LISTEN, NULL);
    GET_STRING_PARAM(iprop_listen, KADM5_CONFIG_IPROP_LISTEN,
                     KRB5_CONF_IPROP_LISTEN, NULL);

#define GET_PORT_PARAM(FIELD, BIT, CONFTAG, DEFAULT)            \
    get_port_param(&params.FIELD, params_in->FIELD,             \
                   &params.mask, params_in->mask, BIT,          \
                   aprofile, hierarchy, CONFTAG, DEFAULT)

    /* Get the value for the kadmind port. */
    GET_PORT_PARAM(kadmind_port, KADM5_CONFIG_KADMIND_PORT,
                   KRB5_CONF_KADMIND_PORT, DEFAULT_KADM5_PORT);

    /* Get the value for the kpasswd port. */
    GET_PORT_PARAM(kpasswd_port, KADM5_CONFIG_KPASSWD_PORT,
                   KRB5_CONF_KPASSWD_PORT, DEFAULT_KPASSWD_PORT);

    /* Get the value for the master key name. */
    GET_STRING_PARAM(mkey_name, KADM5_CONFIG_MKEY_NAME,
                     KRB5_CONF_MASTER_KEY_NAME, NULL);

    /* Get the value for the master key type. */
    hierarchy[2] = KRB5_CONF_MASTER_KEY_TYPE;
    if (params_in->mask & KADM5_CONFIG_ENCTYPE) {
        params.mask |= KADM5_CONFIG_ENCTYPE;
        params.enctype = params_in->enctype;
    } else if (aprofile != NULL &&
               !krb5_aprof_get_string(aprofile, hierarchy, TRUE, &svalue)) {
        if (!krb5_string_to_enctype(svalue, &params.enctype)) {
            params.mask |= KADM5_CONFIG_ENCTYPE;
            free(svalue);
        }
    } else {
        params.mask |= KADM5_CONFIG_ENCTYPE;
        params.enctype = DEFAULT_KDC_ENCTYPE;
    }

    /* Get the value for mkey_from_kbd. */
    if (params_in->mask & KADM5_CONFIG_MKEY_FROM_KBD) {
        params.mask |= KADM5_CONFIG_MKEY_FROM_KBD;
        params.mkey_from_kbd = params_in->mkey_from_kbd;
    }

    /* Get the value for the stashfile. */
    GET_STRING_PARAM(stash_file, KADM5_CONFIG_STASH_FILE,
                     KRB5_CONF_KEY_STASH_FILE, NULL);

    /* Get the value for maximum ticket lifetime. */
#define GET_DELTAT_PARAM(FIELD, BIT, CONFTAG, DEFAULT)          \
    get_deltat_param(&params.FIELD, params_in->FIELD,           \
                     &params.mask, params_in->mask, BIT,        \
                     aprofile, hierarchy, CONFTAG, DEFAULT)

    GET_DELTAT_PARAM(max_life, KADM5_CONFIG_MAX_LIFE, KRB5_CONF_MAX_LIFE,
                     24 * 60 * 60); /* 1 day */

    /* Get the value for maximum renewable ticket lifetime. */
    GET_DELTAT_PARAM(max_rlife, KADM5_CONFIG_MAX_RLIFE,
                     KRB5_CONF_MAX_RENEWABLE_LIFE, 0);

    /* Get the value for the default principal expiration */
    hierarchy[2] = KRB5_CONF_DEFAULT_PRINCIPAL_EXPIRATION;
    if (params_in->mask & KADM5_CONFIG_EXPIRATION) {
        params.mask |= KADM5_CONFIG_EXPIRATION;
        params.expiration = params_in->expiration;
    } else if (aprofile &&
               !krb5_aprof_get_string(aprofile, hierarchy, TRUE, &svalue)) {
        if (!krb5_string_to_timestamp(svalue, &params.expiration)) {
            params.mask |= KADM5_CONFIG_EXPIRATION;
            free(svalue);
        }
    } else {
        params.mask |= KADM5_CONFIG_EXPIRATION;
        params.expiration = 0;
    }

    /* Get the value for the default principal flags */
    hierarchy[2] = KRB5_CONF_DEFAULT_PRINCIPAL_FLAGS;
    if (params_in->mask & KADM5_CONFIG_FLAGS) {
        params.mask |= KADM5_CONFIG_FLAGS;
        params.flags = params_in->flags;
    } else if (aprofile != NULL &&
               !krb5_aprof_get_string(aprofile, hierarchy, TRUE, &svalue)) {
        sp = svalue;
        params.flags = 0;
        while (sp != NULL) {
            if ((ep = strchr(sp, ',')) != NULL ||
                (ep = strchr(sp, ' ')) != NULL ||
                (ep = strchr(sp, '\t')) != NULL) {
                /* Fill in trailing whitespace of sp. */
                tp = ep - 1;
                while (isspace((unsigned char)*tp) && tp > sp) {
                    *tp = '\0';
                    tp--;
                }
                *ep = '\0';
                ep++;
                /* Skip over trailing whitespace of ep. */
                while (isspace((unsigned char)*ep) && *ep != '\0')
                    ep++;
            }
            /* Convert this flag. */
            if (krb5_flagspec_to_mask(sp, &params.flags, &params.flags))
                break;
            sp = ep;
        }
        if (sp == NULL)
            params.mask |= KADM5_CONFIG_FLAGS;
        free(svalue);
    } else {
        params.mask |= KADM5_CONFIG_FLAGS;
        params.flags = KRB5_KDB_DEF_FLAGS;
    }

    /* Get the value for the supported enctype/salttype matrix. */
    hierarchy[2] = KRB5_CONF_SUPPORTED_ENCTYPES;
    if (params_in->mask & KADM5_CONFIG_ENCTYPES) {
        if (params_in->keysalts) {
            params.keysalts = copy_key_salt_tuple(params_in->keysalts,
                                                  params_in->num_keysalts);
            if (params.keysalts) {
                params.mask |= KADM5_CONFIG_ENCTYPES;
                params.num_keysalts = params_in->num_keysalts;
            }
        } else {
            params.mask |= KADM5_CONFIG_ENCTYPES;
            params.keysalts = NULL;
            params.num_keysalts = params_in->num_keysalts;
        }
    } else {
        svalue = NULL;
        if (aprofile != NULL)
            krb5_aprof_get_string(aprofile, hierarchy, TRUE, &svalue);
        if (svalue == NULL)
            svalue = strdup(KRB5_DEFAULT_SUPPORTED_ENCTYPES);

        params.keysalts = NULL;
        params.num_keysalts = 0;
        krb5_string_to_keysalts(svalue,
                                NULL, /* Tuple separators */
                                NULL, /* Key/salt separators */
                                0,      /* No duplicates */
                                &params.keysalts,
                                &params.num_keysalts);
        if (params.num_keysalts)
            params.mask |= KADM5_CONFIG_ENCTYPES;

        free(svalue);
    }

    hierarchy[2] = KRB5_CONF_IPROP_ENABLE;

    params.iprop_enabled = FALSE;
    params.mask |= KADM5_CONFIG_IPROP_ENABLED;

    if (params_in->mask & KADM5_CONFIG_IPROP_ENABLED) {
        params.mask |= KADM5_CONFIG_IPROP_ENABLED;
        params.iprop_enabled = params_in->iprop_enabled;
    } else {
        if (aprofile &&
            !krb5_aprof_get_boolean(aprofile, hierarchy, TRUE, &bvalue)) {
            params.iprop_enabled = bvalue;
            params.mask |= KADM5_CONFIG_IPROP_ENABLED;
        }
    }

    if (!GET_STRING_PARAM(iprop_logfile, KADM5_CONFIG_IPROP_LOGFILE,
                          KRB5_CONF_IPROP_LOGFILE, NULL)) {
        if (params.mask & KADM5_CONFIG_DBNAME) {
            if (asprintf(&params.iprop_logfile, "%s.ulog",
                         params.dbname) >= 0)
                params.mask |= KADM5_CONFIG_IPROP_LOGFILE;
        }
    }

    GET_PORT_PARAM(iprop_port, KADM5_CONFIG_IPROP_PORT, KRB5_CONF_IPROP_PORT,
                   0);

    /* 5 min for large KDBs */
    GET_DELTAT_PARAM(iprop_resync_timeout, KADM5_CONFIG_IPROP_RESYNC_TIMEOUT,
                     KRB5_CONF_IPROP_RESYNC_TIMEOUT, 60 * 5);

    hierarchy[2] = KRB5_CONF_IPROP_MASTER_ULOGSIZE;

    params.iprop_ulogsize = DEF_ULOGENTRIES;
    params.mask |= KADM5_CONFIG_ULOG_SIZE;

    if (params_in->mask & KADM5_CONFIG_ULOG_SIZE) {
        params.mask |= KADM5_CONFIG_ULOG_SIZE;
        params.iprop_ulogsize = params_in->iprop_ulogsize;
    } else {
        if (aprofile != NULL &&
            !krb5_aprof_get_int32(aprofile, hierarchy, TRUE, &ivalue)) {
            if (ivalue <= 0)
                params.iprop_ulogsize = DEF_ULOGENTRIES;
            else
                params.iprop_ulogsize = ivalue;
            params.mask |= KADM5_CONFIG_ULOG_SIZE;
        }
    }

    GET_DELTAT_PARAM(iprop_poll_time, KADM5_CONFIG_POLL_TIME,
                     KRB5_CONF_IPROP_SLAVE_POLL, 2 * 60); /* 2m */

    *params_out = params;

cleanup:
    krb5_aprof_finish(aprofile);
    if (ret) {
        kadm5_free_config_params(context, &params);
        params_out->mask = 0;
    }
    return ret;
}

/*
 * kadm5_free_config_params()        - Free data allocated by above.
 */
krb5_error_code
kadm5_free_config_params(krb5_context context, kadm5_config_params *params)
{
    if (params == NULL)
        return 0;
    free(params->dbname);
    free(params->mkey_name);
    free(params->stash_file);
    free(params->keysalts);
    free(params->admin_server);
    free(params->dict_file);
    free(params->acl_file);
    free(params->realm);
    free(params->iprop_logfile);
    return 0;
}

krb5_error_code
kadm5_get_admin_service_name(krb5_context ctx, char *realm_in,
                             char *admin_name, size_t maxlen)
{
    krb5_error_code ret;
    kadm5_config_params params_in, params_out;
    char *canonhost = NULL;

    memset(&params_in, 0, sizeof(params_in));
    memset(&params_out, 0, sizeof(params_out));

    params_in.mask |= KADM5_CONFIG_REALM;
    params_in.realm = realm_in;
    ret = kadm5_get_config_params(ctx, 0, &params_in, &params_out);
    if (ret)
        return ret;

    if (!(params_out.mask & KADM5_CONFIG_ADMIN_SERVER)) {
        ret = KADM5_MISSING_KRB5_CONF_PARAMS;
        goto err_params;
    }

    ret = krb5_expand_hostname(ctx, params_out.admin_server, &canonhost);
    if (ret)
        goto err_params;

    if (strlen(canonhost) + sizeof("kadmin/") > maxlen) {
        ret = ENOMEM;
        goto err_params;
    }
    snprintf(admin_name, maxlen, "kadmin/%s", canonhost);

err_params:
    krb5_free_string(ctx, canonhost);
    kadm5_free_config_params(ctx, &params_out);
    return ret;
}
