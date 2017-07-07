/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-gmt_mktime.h */
/*
 * Copyright 2008 Massachusetts Institute of Technology.
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
 *
 * GMT struct tm conversion
 *
 * Because of ordering of things in the UNIX build, we can't just keep
 * the declaration in k5-int.h and include it in
 * util/support/gmt_mktime.c, since k5-int.h includes krb5.h which
 * hasn't been built when gmt_mktime.c gets compiled.  Hence this
 * silly little helper header.
 */

#ifndef K5_GMT_MKTIME_H
#define K5_GMT_MKTIME_H

#include "autoconf.h"
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#else
#include <time.h>
#endif

time_t krb5int_gmt_mktime (struct tm *);

#endif /* K5_GMT_MKTIME_H */
