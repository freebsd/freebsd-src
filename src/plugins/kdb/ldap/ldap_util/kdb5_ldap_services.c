/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_util/kdb5_ldap_services.c */
/* Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Create / Delete / Modify / View / List service objects.
 */

/*
 * Service objects have rights over realm objects and principals. The following
 * functions manage the service objects.
 */

#include <k5-int.h>
#include "kdb5_ldap_util.h"
#include "kdb5_ldap_list.h"

/* Get the configured LDAP service password file.  The caller should free the
 * result with profile_release_string(). */
static krb5_error_code
get_conf_service_file(profile_t profile, const char *realm, char **path_out)
{
    char *subsection, *path;
    long ret;

    *path_out = NULL;

    /* Get the [dbmodules] subsection for realm. */
    ret = profile_get_string(profile, KDB_REALM_SECTION, realm,
                             KDB_MODULE_POINTER, realm, &subsection);
    if (ret)
        return ret;

    /* Look up the password file in the [dbmodules] subsection. */
    ret = profile_get_string(profile, KDB_MODULE_SECTION, subsection,
                             KRB5_CONF_LDAP_SERVICE_PASSWORD_FILE, NULL,
                             &path);
    profile_release_string(subsection);
    if (ret)
        return ret;

    if (path == NULL) {
        /* Look up the password file in [dbdefaults] as a fallback. */
        ret = profile_get_string(profile, KDB_MODULE_DEF_SECTION,
                                 KRB5_CONF_LDAP_SERVICE_PASSWORD_FILE, NULL,
                                 NULL, &path);
        if (ret)
            return ret;
    }

    if (path == NULL) {
        k5_setmsg(util_context, ENOENT,
                  _("ldap_service_password_file not configured"));
        return ENOENT;
    }

    *path_out = path;
    return 0;
}

/*
 * Convert the user supplied password into hexadecimal and stash it. Only a
 * little more secure than storing plain password in the file ...
 */
void
kdb5_ldap_stash_service_password(int argc, char **argv)
{
    int ret = 0;
    unsigned int passwd_len = 0;
    char *me = progname;
    char *service_object = NULL;
    char *file_name = NULL, *tmp_file = NULL;
    char passwd[MAX_SERVICE_PASSWD_LEN];
    char *str = NULL;
    char line[MAX_LEN];
    FILE *pfile = NULL;
    krb5_boolean print_usage = FALSE;
    krb5_data hexpasswd = {0, 0, NULL};
    mode_t old_mode = 0;

    /*
     * Format:
     *   stashsrvpw [-f filename] service_dn
     * where
     *   'service_dn' is the DN of the service object
     *   'filename' is the path of the stash file
     */
    if (argc != 2 && argc != 4) {
        print_usage = TRUE;
        goto cleanup;
    }

    if (argc == 4) {
        /* Find the stash file name */
        if (strcmp (argv[1], "-f") == 0) {
            if (((file_name = strdup (argv[2])) == NULL) ||
                ((service_object = strdup (argv[3])) == NULL)) {
                com_err(me, ENOMEM,
                        _("while setting service object password"));
                goto cleanup;
            }
        } else if (strcmp (argv[2], "-f") == 0) {
            if (((file_name = strdup (argv[3])) == NULL) ||
                ((service_object = strdup (argv[1])) == NULL)) {
                com_err(me, ENOMEM,
                        _("while setting service object password"));
                goto cleanup;
            }
        } else {
            print_usage = TRUE;
            goto cleanup;
        }
        if (file_name == NULL) {
            com_err(me, ENOMEM, _("while setting service object password"));
            goto cleanup;
        }
    } else { /* argc == 2 */
        service_object = strdup (argv[1]);
        if (service_object == NULL) {
            com_err(me, ENOMEM, _("while setting service object password"));
            goto cleanup;
        }

        ret = get_conf_service_file(util_context->profile,
                                    util_context->default_realm, &file_name);
        if (ret) {
            com_err(me, ret, _("while getting service password filename"));
            goto cleanup;
        }
    }

    /* Get password from user */
    {
        char prompt1[256], prompt2[256];

        /* Get the service object password from the terminal */
        memset(passwd, 0, sizeof (passwd));
        passwd_len = sizeof (passwd);

        snprintf(prompt1, sizeof(prompt1), _("Password for \"%s\""),
                 service_object);

        snprintf(prompt2, sizeof(prompt2), _("Re-enter password for \"%s\""),
                 service_object);

        ret = krb5_read_password(util_context, prompt1, prompt2, passwd, &passwd_len);
        if (ret != 0) {
            com_err(me, ret, _("while setting service object password"));
            memset(passwd, 0, sizeof (passwd));
            goto cleanup;
        }

        if (passwd_len == 0) {
            printf(_("%s: Invalid password\n"), me);
            memset(passwd, 0, MAX_SERVICE_PASSWD_LEN);
            goto cleanup;
        }
    }

    /* Convert the password to hexadecimal */
    {
        krb5_data pwd;

        pwd.length = passwd_len;
        pwd.data = passwd;

        ret = tohex(pwd, &hexpasswd);
        if (ret != 0) {
            com_err(me, ret,
                    _("Failed to convert the password to hexadecimal"));
            memset(passwd, 0, passwd_len);
            goto cleanup;
        }
    }
    memset(passwd, 0, passwd_len);

    /* TODO: file lock for the service password file */

    /* set password in the file */
    old_mode = umask(0177);
    pfile = fopen(file_name, "a+");
    if (pfile == NULL) {
        com_err(me, errno, _("Failed to open file %s: %s"), file_name,
                strerror (errno));
        goto cleanup;
    }
    set_cloexec_file(pfile);
    rewind (pfile);
    umask(old_mode);

    while (fgets (line, MAX_LEN, pfile) != NULL) {
        if ((str = strstr (line, service_object)) != NULL) {
            /* White spaces not allowed */
            if (line [strlen (service_object)] == '#')
                break;
            str = NULL;
        }
    }

    if (str == NULL) {
        if (feof(pfile)) {
            /* If the service object dn is not present in the service password file */
            if (fprintf(pfile, "%s#{HEX}%s\n", service_object, hexpasswd.data) < 0) {
                com_err(me, errno,
                        _("Failed to write service object password to file"));
                fclose(pfile);
                goto cleanup;
            }
        } else {
            com_err(me, errno,
                    _("Error reading service object password file"));
            fclose(pfile);
            goto cleanup;
        }
        fclose(pfile);
    } else {
        /*
         * Password entry for the service object is already present in the file
         * Delete the existing entry and add the new entry
         */
        FILE *newfile;

        mode_t omask;

        /* Create a new file with the extension .tmp */
        if (asprintf(&tmp_file,"%s.tmp",file_name) < 0) {
            com_err(me, ENOMEM, _("while setting service object password"));
            fclose(pfile);
            goto cleanup;
        }

        omask = umask(077);
        newfile = fopen(tmp_file, "w");
        umask (omask);
        if (newfile == NULL) {
            com_err(me, errno, _("Error creating file %s"), tmp_file);
            fclose(pfile);
            goto cleanup;
        }
        set_cloexec_file(newfile);

        fseek(pfile, 0, SEEK_SET);
        while (fgets(line, MAX_LEN, pfile) != NULL) {
            if (((str = strstr(line, service_object)) != NULL) &&
                (line[strlen(service_object)] == '#')) {
                if (fprintf(newfile, "%s#{HEX}%s\n", service_object, hexpasswd.data) < 0) {
                    com_err(me, errno, _("Failed to write service object "
                                         "password to file"));
                    fclose(newfile);
                    unlink(tmp_file);
                    fclose(pfile);
                    goto cleanup;
                }
            } else {
                if (fprintf (newfile, "%s", line) < 0) {
                    com_err(me, errno, _("Failed to write service object "
                                         "password to file"));
                    fclose(newfile);
                    unlink(tmp_file);
                    fclose(pfile);
                    goto cleanup;
                }
            }
        }

        if (!feof(pfile)) {
            com_err(me, errno,
                    _("Error reading service object password file"));
            fclose(newfile);
            unlink(tmp_file);
            fclose(pfile);
            goto cleanup;
        }

        /* TODO: file lock for the service passowrd file */

        fclose(pfile);
        fclose(newfile);

        ret = rename(tmp_file, file_name);
        if (ret != 0) {
            com_err(me, errno,
                    _("Failed to write service object password to file"));
            goto cleanup;
        }
    }
    ret = 0;

cleanup:

    if (hexpasswd.length != 0) {
        memset(hexpasswd.data, 0, hexpasswd.length);
        free(hexpasswd.data);
    }

    if (service_object)
        free(service_object);

    profile_release_string(file_name);

    if (tmp_file)
        free(tmp_file);

    if (print_usage)
        usage();
/*      db_usage(STASH_SRV_PW); */

    if (ret)
        exit_status++;
}
