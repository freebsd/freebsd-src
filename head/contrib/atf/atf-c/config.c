/*
 * Automated Testing Framework (atf)
 *
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "atf-c/config.h"

#include "detail/env.h"
#include "detail/sanity.h"

static bool initialized = false;

static struct var {
    const char *name;
    const char *default_value;
    const char *value;
    bool can_be_empty;
} vars[] = {
    { "atf_build_cc",       ATF_BUILD_CC,       NULL, false, },
    { "atf_build_cflags",   ATF_BUILD_CFLAGS,   NULL, true,  },
    { "atf_build_cpp",      ATF_BUILD_CPP,      NULL, false, },
    { "atf_build_cppflags", ATF_BUILD_CPPFLAGS, NULL, true,  },
    { "atf_build_cxx",      ATF_BUILD_CXX,      NULL, false, },
    { "atf_build_cxxflags", ATF_BUILD_CXXFLAGS, NULL, true,  },
    { "atf_includedir",     ATF_INCLUDEDIR,     NULL, false, },
    { "atf_libexecdir",     ATF_LIBEXECDIR,     NULL, false, },
    { "atf_pkgdatadir",     ATF_PKGDATADIR,     NULL, false, },
    { "atf_shell",          ATF_SHELL,          NULL, false, },
    { "atf_workdir",        ATF_WORKDIR,        NULL, false, },
    { NULL,                 NULL,               NULL, false, },
};

/* Only used for unit testing, so this prototype is private. */
void __atf_config_reinit(void);

/* ---------------------------------------------------------------------
 * Auxiliary functions.
 * --------------------------------------------------------------------- */

static
char *
string_to_upper(const char *str)
{
    char *uc;

    uc = (char *)malloc(strlen(str) + 1);
    if (uc != NULL) {
        char *ucptr = uc;
        while (*str != '\0') {
            *ucptr = toupper((int)*str);

            str++;
            ucptr++;
        }
        *ucptr = '\0';
    }

    return uc;
}

static
void
initialize_var(struct var *var, const char *envname)
{
    PRE(var->value == NULL);

    if (atf_env_has(envname)) {
        const char *val = atf_env_get(envname);
        if (strlen(val) > 0 || var->can_be_empty)
            var->value = val;
        else
            var->value = var->default_value;
    } else
        var->value = var->default_value;

    POST(var->value != NULL);
}

static
void
initialize(void)
{
    struct var *var;

    PRE(!initialized);

    for (var = vars; var->name != NULL; var++) {
        char *envname;

        envname = string_to_upper(var->name);
        initialize_var(var, envname);
        free(envname);
    }

    initialized = true;
}

/* ---------------------------------------------------------------------
 * Free functions.
 * --------------------------------------------------------------------- */

const char *
atf_config_get(const char *name)
{
    const struct var *var;
    const char *value;

    if (!initialized) {
        initialize();
        INV(initialized);
    }

    value = NULL;
    for (var = vars; value == NULL && var->name != NULL; var++)
        if (strcmp(var->name, name) == 0)
            value = var->value;
    INV(value != NULL);

    return value;
}

void
__atf_config_reinit(void)
{
    struct var *var;

    initialized = false;

    for (var = vars; var->name != NULL; var++)
        var->value = NULL;
}
