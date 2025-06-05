/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/expand_path.c - Parameterized path expansion facility */
/*
 * Copyright (c) 2009, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-int.h"
#include "os-proto.h"

typedef int PTYPE;

#ifdef _WIN32
#include <shlobj.h>
#include <sddl.h>

/*
 * Expand a %{TEMP} token
 *
 * The %{TEMP} token expands to the temporary path for the current
 * user as returned by GetTempPath().
 *
 * @note: Since the GetTempPath() function relies on the TMP or TEMP
 * environment variables, this function will failover to the system
 * temporary directory until the user profile is loaded.  In addition,
 * the returned path may or may not exist.
 */
static krb5_error_code
expand_temp_folder(krb5_context context, PTYPE param, const char *postfix,
                   char **ret)
{
    TCHAR tpath[MAX_PATH];
    size_t len;

    if (!GetTempPath(sizeof(tpath) / sizeof(tpath[0]), tpath)) {
        k5_setmsg(context, EINVAL, "Failed to get temporary path (GLE=%d)",
                  GetLastError());
        return EINVAL;
    }

    len = strlen(tpath);

    if (len > 0 && tpath[len - 1] == '\\')
        tpath[len - 1] = '\0';

    *ret = strdup(tpath);

    if (*ret == NULL)
        return ENOMEM;

    return 0;
}

/*
 * Expand a %{BINDIR} token
 *
 * This is also used to expand a few other tokens on Windows, since
 * most of the executable binaries end up in the same directory.  The
 * "bin" directory is considered to be the directory in which the
 * krb5.dll is located.
 */
static krb5_error_code
expand_bin_dir(krb5_context context, PTYPE param, const char *postfix,
               char **ret)
{
    TCHAR path[MAX_PATH];
    TCHAR *lastSlash;
    DWORD nc;

    nc = GetModuleFileName(get_lib_instance(), path,
                           sizeof(path) / sizeof(path[0]));
    if (nc == 0 ||
        nc == sizeof(path) / sizeof(path[0])) {
        return EINVAL;
    }

    lastSlash = strrchr(path, '\\');
    if (lastSlash != NULL) {
        TCHAR *fslash = strrchr(lastSlash, '/');

        if (fslash != NULL)
            lastSlash = fslash;

        *lastSlash = '\0';
    }

    if (postfix) {
        if (strlcat(path, postfix, sizeof(path) / sizeof(path[0])) >=
            sizeof(path) / sizeof(path[0]))
            return EINVAL;
    }

    *ret = strdup(path);
    if (*ret == NULL)
        return ENOMEM;

    return 0;
}

/*
 *  Expand a %{USERID} token
 *
 *  The %{USERID} token expands to the string representation of the
 *  user's SID.  The user account that will be used is the account
 *  corresponding to the current thread's security token.  This means
 *  that:
 *
 *  - If the current thread token has the anonymous impersonation
 *    level, the call will fail.
 *
 *  - If the current thread is impersonating a token at
 *    SecurityIdentification level the call will fail.
 *
 */
static krb5_error_code
expand_userid(krb5_context context, PTYPE param, const char *postfix,
              char **ret)
{
    int rv = EINVAL;
    HANDLE hThread = NULL;
    HANDLE hToken = NULL;
    PTOKEN_OWNER pOwner = NULL;
    DWORD len = 0;
    LPTSTR strSid = NULL;

    hThread = GetCurrentThread();

    if (!OpenThreadToken(hThread, TOKEN_QUERY,
                         FALSE, /* Open the thread token as the
                                   current thread user. */
                         &hToken)) {

        DWORD le = GetLastError();

        if (le == ERROR_NO_TOKEN) {
            HANDLE hProcess = GetCurrentProcess();

            le = 0;
            if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
                le = GetLastError();
        }

        if (le != 0) {
            k5_setmsg(context, rv, "Can't open thread token (GLE=%d)", le);
            goto cleanup;
        }
    }

    if (!GetTokenInformation(hToken, TokenOwner, NULL, 0, &len)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            k5_setmsg(context, rv,
                      "Unexpected error reading token information (GLE=%d)",
                      GetLastError());
            goto cleanup;
        }

        if (len == 0) {
            k5_setmsg(context, rv,
                      "GetTokenInformation() returned truncated buffer");
            goto cleanup;
        }

        pOwner = malloc(len);
        if (pOwner == NULL) {
            rv = ENOMEM;
            goto cleanup;
        }
    } else {
        k5_setmsg(context, rv,
                  "GetTokenInformation() returned truncated buffer");
        goto cleanup;
    }

    if (!GetTokenInformation(hToken, TokenOwner, pOwner, len, &len)) {
        k5_setmsg(context, rv,
                  "GetTokenInformation() failed.  GLE=%d", GetLastError());
        goto cleanup;
    }

    if (!ConvertSidToStringSid(pOwner->Owner, &strSid)) {
        k5_setmsg(context, rv,
                  "Can't convert SID to string.  GLE=%d", GetLastError());
        goto cleanup;
    }

    *ret = strdup(strSid);
    if (*ret == NULL) {
        rv = ENOMEM;
        goto cleanup;
    }

    rv = 0;

cleanup:
    if (hToken != NULL)
        CloseHandle(hToken);

    if (pOwner != NULL)
        free(pOwner);

    if (strSid != NULL)
        LocalFree(strSid);

    return rv;
}

/*
 * Expand a folder identified by a CSIDL
 */
static krb5_error_code
expand_csidl(krb5_context context, PTYPE folder, const char *postfix,
             char **ret)
{
    TCHAR path[MAX_PATH];
    size_t len;

    if (SHGetFolderPath(NULL, folder, NULL, SHGFP_TYPE_CURRENT,
                        path) != S_OK) {
        k5_setmsg(context, EINVAL, "Unable to determine folder path");
        return EINVAL;
    }

    len = strlen(path);

    if (len > 0 && path[len - 1] == '\\')
        path[len - 1] = '\0';

    if (postfix &&
        strlcat(path, postfix, sizeof(path) / sizeof(path[0])) >=
        sizeof(path)/sizeof(path[0]))
        return ENOMEM;

    *ret = strdup(path);
    if (*ret == NULL)
        return ENOMEM;
    return 0;
}

#else /* not _WIN32 */
#include <pwd.h>

static krb5_error_code
expand_path(krb5_context context, PTYPE param, const char *postfix, char **ret)
{
    *ret = strdup(postfix);
    if (*ret == NULL)
        return ENOMEM;
    return 0;
}

static krb5_error_code
expand_temp_folder(krb5_context context, PTYPE param, const char *postfix,
                   char **ret)
{
    const char *p = NULL;

    if (context == NULL || !context->profile_secure)
        p = secure_getenv("TMPDIR");
    *ret = strdup((p != NULL) ? p : "/tmp");
    if (*ret == NULL)
        return ENOMEM;
    return 0;
}

static krb5_error_code
expand_userid(krb5_context context, PTYPE param, const char *postfix,
              char **str)
{
    if (asprintf(str, "%lu", (unsigned long)getuid()) < 0)
        return ENOMEM;
    return 0;
}

static krb5_error_code
expand_euid(krb5_context context, PTYPE param, const char *postfix, char **str)
{
    if (asprintf(str, "%lu", (unsigned long)geteuid()) < 0)
        return ENOMEM;
    return 0;
}

static krb5_error_code
expand_username(krb5_context context, PTYPE param, const char *postfix,
                char **str)
{
    uid_t euid = geteuid();
    struct passwd *pw, pwx;
    char pwbuf[BUFSIZ];

    if (k5_getpwuid_r(euid, &pwx, pwbuf, sizeof(pwbuf), &pw) != 0) {
        k5_setmsg(context, ENOENT, _("Can't find username for uid %lu"),
                  (unsigned long)euid);
        return ENOENT;
    }
    *str = strdup(pw->pw_name);
    if (*str == NULL)
        return ENOMEM;
    return 0;
}

#endif /* not _WIN32 */

/*
 * Expand an extra token
 */
static krb5_error_code
expand_extra_token(krb5_context context, const char *value, char **ret)
{
    *ret = strdup(value);
    if (*ret == NULL)
        return ENOMEM;
    return 0;
}

/*
 * Expand a %{null} token
 *
 * The expansion of a %{null} token is always the empty string.
 */
static krb5_error_code
expand_null(krb5_context context, PTYPE param, const char *postfix, char **ret)
{
    *ret = strdup("");
    if (*ret == NULL)
        return ENOMEM;
    return 0;
}

static const struct {
    const char *tok;
    PTYPE param;
    const char *postfix;
    int (*exp_func)(krb5_context, PTYPE, const char *, char **);
} tokens[] = {
#ifdef _WIN32
    /* Roaming application data (for current user) */
    {"APPDATA", CSIDL_APPDATA, NULL, expand_csidl},
    /* Application data (all users) */
    {"COMMON_APPDATA", CSIDL_COMMON_APPDATA, NULL, expand_csidl},
    /* Local application data (for current user) */
    {"LOCAL_APPDATA", CSIDL_LOCAL_APPDATA, NULL, expand_csidl},
    /* Windows System folder (e.g. %WINDIR%\System32) */
    {"SYSTEM", CSIDL_SYSTEM, NULL, expand_csidl},
    /* Windows folder */
    {"WINDOWS", CSIDL_WINDOWS, NULL, expand_csidl},
    /* Per user MIT krb5 configuration file directory */
    {"USERCONFIG", CSIDL_APPDATA, "\\MIT\\Kerberos5",
     expand_csidl},
    /* Common MIT krb5 configuration file directory */
    {"COMMONCONFIG", CSIDL_COMMON_APPDATA, "\\MIT\\Kerberos5",
     expand_csidl},
    {"LIBDIR", 0, NULL, expand_bin_dir},
    {"BINDIR", 0, NULL, expand_bin_dir},
    {"SBINDIR", 0, NULL, expand_bin_dir},
    {"euid", 0, NULL, expand_userid},
#else
    {"LIBDIR", 0, LIBDIR, expand_path},
    {"BINDIR", 0, BINDIR, expand_path},
    {"SBINDIR", 0, SBINDIR, expand_path},
    {"euid", 0, NULL, expand_euid},
    {"username", 0, NULL, expand_username},
#endif
    {"TEMP", 0, NULL, expand_temp_folder},
    {"USERID", 0, NULL, expand_userid},
    {"uid", 0, NULL, expand_userid},
    {"null", 0, NULL, expand_null}
};

static krb5_error_code
expand_token(krb5_context context, const char *token, const char *token_end,
             char **extra_tokens, char **ret)
{
    size_t i;
    char **p;

    *ret = NULL;

    if (token[0] != '%' || token[1] != '{' || token_end[0] != '}' ||
        token_end - token <= 2) {
        k5_setmsg(context, EINVAL, _("Invalid token"));
        return EINVAL;
    }

    for (p = extra_tokens; p != NULL && *p != NULL; p += 2) {
        if (strncmp(token + 2, *p, (token_end - token) - 2) == 0)
            return expand_extra_token(context, p[1], ret);
    }

    for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++) {
        if (!strncmp(token + 2, tokens[i].tok, (token_end - token) - 2)) {
            return tokens[i].exp_func(context, tokens[i].param,
                                      tokens[i].postfix, ret);
        }
    }

    k5_setmsg(context, EINVAL, _("Invalid token"));
    return EINVAL;
}

/*
 * Expand tokens in path_in to produce *path_out.  The caller should free
 * *path_out with free().
 */
krb5_error_code
k5_expand_path_tokens(krb5_context context, const char *path_in,
                      char **path_out)
{
    return k5_expand_path_tokens_extra(context, path_in, path_out, NULL);
}

static void
free_extra_tokens(char **extra_tokens)
{
    char **p;

    for (p = extra_tokens; p != NULL && *p != NULL; p++)
        free(*p);
    free(extra_tokens);
}

/*
 * Expand tokens in path_in to produce *path_out.  Arguments after path_out are
 * pairs of extra token names and replacement values, terminated by a NULL.
 * The caller should free *path_out with free().
 */
krb5_error_code
k5_expand_path_tokens_extra(krb5_context context, const char *path_in,
                            char **path_out, ...)
{
    krb5_error_code ret;
    struct k5buf buf;
    char *tok_begin, *tok_end, *tok_val, **extra_tokens = NULL, *path;
    const char *path_left;
    size_t nargs = 0, i;
    va_list ap;

    *path_out = NULL;

    k5_buf_init_dynamic(&buf);

    /* Count extra tokens. */
    va_start(ap, path_out);
    while (va_arg(ap, const char *) != NULL)
        nargs++;
    va_end(ap);
    if (nargs % 2 != 0)
        return EINVAL;

    /* Get extra tokens. */
    if (nargs > 0) {
        extra_tokens = k5calloc(nargs + 1, sizeof(char *), &ret);
        if (extra_tokens == NULL)
            goto cleanup;
        va_start(ap, path_out);
        for (i = 0; i < nargs; i++) {
            extra_tokens[i] = strdup(va_arg(ap, const char *));
            if (extra_tokens[i] == NULL) {
                ret = ENOMEM;
                va_end(ap);
                goto cleanup;
            }
        }
        va_end(ap);
    }

    path_left = path_in;
    while (TRUE) {
        /* Find the next token in path_left and add the literal text up to it.
         * If there are no more tokens, we can finish up. */
        tok_begin = strstr(path_left, "%{");
        if (tok_begin == NULL) {
            k5_buf_add(&buf, path_left);
            break;
        }
        k5_buf_add_len(&buf, path_left, tok_begin - path_left);

        /* Find the end of this token. */
        tok_end = strchr(tok_begin, '}');
        if (tok_end == NULL) {
            ret = EINVAL;
            k5_setmsg(context, ret, _("variable missing }"));
            goto cleanup;
        }

        /* Expand this token and add its value. */
        ret = expand_token(context, tok_begin, tok_end, extra_tokens,
                           &tok_val);
        if (ret)
            goto cleanup;
        k5_buf_add(&buf, tok_val);
        free(tok_val);
        path_left = tok_end + 1;
    }

    path = k5_buf_cstring(&buf);
    if (path == NULL) {
        ret = ENOMEM;
        goto cleanup;
    }

#ifdef _WIN32
    /* Also deal with slashes. */
    {
        char *p;
        for (p = path; *p != '\0'; p++) {
            if (*p == '/')
                *p = '\\';
        }
    }
#endif
    *path_out = path;
    memset(&buf, 0, sizeof(buf));
    ret = 0;

cleanup:
    k5_buf_free(&buf);
    free_extra_tokens(extra_tokens);
    return ret;
}
