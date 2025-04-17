/*
 * Replacement for a missing pam_vsyslog.
 *
 * Provides close to the same functionality as the Linux PAM function
 * pam_vsyslog for other PAM implementations.  The logging prefix will not be
 * quite as good, since we don't have access to the PAM group name.
 *
 * To use this replacement, the Autoconf script for the package must define
 * MODULE_NAME to the name of the PAM module.  (PACKAGE isn't used since it
 * may use dashes where the module uses underscores.)
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <config.h>
#include <portable/pam.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#ifndef LOG_AUTHPRIV
#    define LOG_AUTHPRIV LOG_AUTH
#endif

void
pam_vsyslog(const pam_handle_t *pamh, int priority, const char *fmt,
            va_list args)
{
    char *msg = NULL;
    const char *service = NULL;
    int retval;

    retval = pam_get_item(pamh, PAM_SERVICE, (PAM_CONST void **) &service);
    if (retval != PAM_SUCCESS)
        service = NULL;
    if (vasprintf(&msg, fmt, args) < 0) {
        syslog(LOG_CRIT | LOG_AUTHPRIV,
               "cannot allocate memory in vasprintf: %m");
        return;
    }
    /* clang-format off */
    syslog(priority | LOG_AUTHPRIV, MODULE_NAME "%s%s%s: %s",
           (service == NULL) ? "" : "(",
           (service == NULL) ? "" : service,
           (service == NULL) ? "" : ")", msg);
    /* clang-format on */
    free(msg);
}
