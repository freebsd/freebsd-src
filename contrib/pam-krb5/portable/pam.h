/*
 * Portability wrapper around PAM header files.
 *
 * This header file includes the various PAM headers, wherever they may be
 * found on the system, and defines replacements for PAM functions that may
 * not be available on the local system.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_PAM_H
#define PORTABLE_PAM_H 1

#include <config.h>
#include <portable/macros.h>

/* Linux PAM 1.1.0 requires sys/types.h before security/pam_modutil.h. */
#include <sys/types.h>

#ifndef HAVE_PAM_MODUTIL_GETPWNAM
#    include <pwd.h>
#endif
#if defined(HAVE_SECURITY_PAM_APPL_H)
#    include <security/pam_appl.h>
#    include <security/pam_modules.h>
#elif defined(HAVE_PAM_PAM_APPL_H)
#    include <pam/pam_appl.h>
#    include <pam/pam_modules.h>
#endif
#if defined(HAVE_SECURITY_PAM_EXT_H)
#    include <security/pam_ext.h>
#elif defined(HAVE_PAM_PAM_EXT_H)
#    include <pam/pam_ext.h>
#endif
#if defined(HAVE_SECURITY_PAM_MODUTIL_H)
#    include <security/pam_modutil.h>
#elif defined(HAVE_PAM_PAM_MODUTIL_H)
#    include <pam/pam_modutil.h>
#endif
#include <stdarg.h>

/* Solaris doesn't have these. */
#ifndef PAM_CONV_AGAIN
#    define PAM_CONV_AGAIN 0
#    define PAM_INCOMPLETE PAM_SERVICE_ERR
#endif

/* Solaris 8 has deficient PAM. */
#ifndef PAM_AUTHTOK_RECOVER_ERR
#    define PAM_AUTHTOK_RECOVER_ERR PAM_AUTHTOK_ERR
#endif

/*
 * Mac OS X 10 doesn't define these.  They're meant to be logically or'd with
 * an exit status in pam_set_data, so define them to 0 if not defined to
 * deactivate them.
 */
#ifndef PAM_DATA_REPLACE
#    define PAM_DATA_REPLACE 0
#endif
#ifndef PAM_DATA_SILENT
#    define PAM_DATA_SILENT 0
#endif

/*
 * Mac OS X 10 apparently doesn't use PAM_BAD_ITEM and returns PAM_SYMBOL_ERR
 * instead.
 */
#ifndef PAM_BAD_ITEM
#    define PAM_BAD_ITEM PAM_SYMBOL_ERR
#endif

/* We use this as a limit on password length, so make sure it's defined. */
#ifndef PAM_MAX_RESP_SIZE
#    define PAM_MAX_RESP_SIZE 512
#endif

/*
 * Some PAM implementations support building the module static and exporting
 * the call points via a struct instead.  (This is the default in OpenPAM, for
 * example.)  To support this, the pam_sm_* functions are declared PAM_EXTERN.
 * Ensure that's defined for implementations that don't have this.
 */
#ifndef PAM_EXTERN
#    define PAM_EXTERN
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/*
 * If pam_modutil_getpwnam is missing, ideally we should roll our own using
 * getpwnam_r.  However, this is a fair bit of work, since we have to stash
 * the allocated memory in the PAM data so that it will be freed properly.
 * Bail for right now.
 */
#if !HAVE_PAM_MODUTIL_GETPWNAM
#    define pam_modutil_getpwnam(h, u) getpwnam(u)
#endif

/* Prototype missing optional PAM functions. */
#if !HAVE_PAM_SYSLOG
void pam_syslog(const pam_handle_t *, int, const char *, ...);
#endif
#if !HAVE_PAM_VSYSLOG
void pam_vsyslog(const pam_handle_t *, int, const char *, va_list);
#endif

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PORTABLE_PAM_H */
