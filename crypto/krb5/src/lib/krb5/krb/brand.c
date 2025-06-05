/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/brand.c */
/*
 * Copyright (C) 2004 by the Massachusetts Institute of Technology.
 * All rights reserved.
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
 * This file is used to put a "release brand" on a Krb5 library before
 * it is released via some release engineering process.  This gives us
 * an easy way to tell where a binary came from.
 *
 * It depends on patchlevel.h for the master version stamp info.
 */

/* Format: "KRB5_BRAND: <cvs_tag> <release_name> <date>" */

#include "patchlevel.h"

#define XSTR(x) #x
#define STR(x) XSTR(x)

#ifdef KRB5_RELTAG
#define RELTAG KRB5_RELTAG
#else
#define RELTAG "[untagged]"
#endif

#define MAJOR_MINOR STR(KRB5_MAJOR_RELEASE) "." STR(KRB5_MINOR_RELEASE)

#if KRB5_PATCHLEVEL != 0
#define MAYBE_PATCH "." STR(KRB5_PATCHLEVEL)
#else
#define MAYBE_PATCH ""
#endif

#ifdef KRB5_RELTAIL
#define RELTAIL "-" KRB5_RELTAIL
#else
#define RELTAIL ""
#endif

#define RELNAME MAJOR_MINOR MAYBE_PATCH RELTAIL

#ifdef KRB5_RELDATE
#define RELDATE KRB5_RELDATE
#else
#define RELDATE "[date unknown]"
#endif

#define BRANDSTR RELTAG " " RELNAME " " RELDATE

char krb5_brand[] = "KRB5_BRAND: " BRANDSTR;
