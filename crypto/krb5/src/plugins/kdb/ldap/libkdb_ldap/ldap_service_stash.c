/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/libkdb_ldap/ldap_service_stash.c */
/*
 * Copyright (c) 2004-2005, Novell, Inc.
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

#include "ldap_main.h"
#include "kdb_ldap.h"
#include "ldap_service_stash.h"
#include <k5-hex.h>
#include <ctype.h>

/* Decode a password of the form {HEX}<hexstring>. */
static krb5_error_code
dec_password(krb5_context context, const char *str, char **password_out)
{
    krb5_error_code ret;
    uint8_t *bytes;
    size_t len;

    *password_out = NULL;

    if (strncmp(str, "{HEX}", 5) != 0) {
        k5_setmsg(context, EINVAL, _("Not a hexadecimal password"));
        return EINVAL;
    }

    ret = k5_hex_decode(str + 5, &bytes, &len);
    if (ret) {
        if (ret == EINVAL)
            k5_setmsg(context, ret, _("Password corrupt"));
        return ret;
    }

    *password_out = (char *)bytes;
    return 0;
}

krb5_error_code
krb5_ldap_readpassword(krb5_context context, const char *filename,
                       const char *name, char **password_out)
{
    krb5_error_code ret;
    char line[RECORDLEN], *end;
    const char *start, *sep, *val = NULL;
    int namelen = strlen(name);
    FILE *fp;

    *password_out = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        ret = errno;
        k5_setmsg(context, ret, _("Cannot open LDAP password file '%s': %s"),
                  filename, error_message(ret));
        return ret;
    }
    set_cloexec_file(fp);

    while (fgets(line, RECORDLEN, fp) != NULL) {
        /* Remove trailing newline. */
        end = line + strlen(line);
        if (end > line && end[-1] == '\n')
            end[-1] = '\0';

        /* Skip past leading whitespace. */
        for (start = line; isspace(*start); ++start);

        /* Ignore comment lines */
        if (*start == '!' || *start == '#')
            continue;

        sep = strchr(start, '#');
        if (sep != NULL && sep - start == namelen &&
            strncasecmp(start, name, namelen) == 0) {
            val = sep + 1;
            break;
        }
    }
    fclose(fp);

    if (val == NULL) {
        k5_setmsg(context, KRB5_KDB_SERVER_INTERNAL_ERR,
                  _("Bind DN entry '%s' missing in LDAP password file '%s'"),
                  name, filename);
        return KRB5_KDB_SERVER_INTERNAL_ERR;
    }

    /* Extract the plain password information. */
    return dec_password(context, val, password_out);
}
