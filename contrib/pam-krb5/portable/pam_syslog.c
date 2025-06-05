/*
 * Replacement for a missing pam_syslog.
 *
 * Implements pam_syslog in terms of pam_vsyslog (which itself may be a
 * replacement) if the PAM implementation does not provide it.  This is a
 * Linux PAM extension.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
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

void
pam_syslog(const pam_handle_t *pamh, int priority, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    pam_vsyslog(pamh, priority, fmt, args);
    va_end(args);
}
