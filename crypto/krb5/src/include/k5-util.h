/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1989-1998,2002 by the Massachusetts Institute of Technology,
 * Cambridge, MA, USA.  All Rights Reserved.
 *
 * This software is being provided to you, the LICENSEE, by the
 * Massachusetts Institute of Technology (M.I.T.) under the following
 * license.  By obtaining, using and/or copying this software, you agree
 * that you have read, understood, and will comply with these terms and
 * conditions:
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify and distribute
 * this software and its documentation for any purpose and without fee or
 * royalty is hereby granted, provided that you agree to comply with the
 * following copyright notice and statements, including the disclaimer, and
 * that the same appear on ALL copies of the software and documentation,
 * including modifications that you make for internal use or for
 * distribution:
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", AND M.I.T. MAKES NO REPRESENTATIONS
 * OR WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 * limitation, M.I.T. MAKES NO REPRESENTATIONS OR WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF
 * THE LICENSED SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY
 * PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
 *
 * The name of the Massachusetts Institute of Technology or M.I.T. may NOT
 * be used in advertising or publicity pertaining to distribution of the
 * software.  Title to copyright in this software and any associated
 * documentation shall at all times remain with M.I.T., and USER agrees to
 * preserve same.
 *
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 */

/*
 * "internal" utility functions used by various applications.
 * They live in libkrb5util.
 */

#include "autoconf.h"

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <errno.h>

#ifndef krb5_seteuid

#if defined(HAVE_SETEUID)
#  define krb5_seteuid(EUID)    (seteuid((uid_t)(EUID)))
#elif defined(HAVE_SETRESUID)
#  define krb5_seteuid(EUID)    setresuid(getuid(), (uid_t)(EUID), geteuid())
#elif defined(HAVE_SETREUID)
#  define krb5_seteuid(EUID)    setreuid(geteuid(), (uid_t)(EUID))
#else
/* You need to add a case to deal with this operating system.*/
#  define krb5_seteuid(EUID)    (errno = EPERM, -1)
#endif

#ifdef HAVE_SETEGID
#  define krb5_setegid(EGID)    (setegid((gid_t)(EGID)))
#elif defined(HAVE_SETRESGID)
#  define krb5_setegid(EGID)    (setresgid(getgid(), (gid_t)(EGID), getegid()))
#elif defined(HAVE_SETREGID)
#  define krb5_setegid(EGID)    (setregid(getegid(), (gid_t)(EGID)))
#else
/* You need to add a case to deal with this operating system.*/
#  define krb5_setegid(EGID)    (errno = EPERM, -1)
#endif

#endif
