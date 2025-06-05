/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/ccache/scc.h */
/*
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * This file contains constant and function declarations used in the
 * file-based credential cache routines.
 */

#ifndef __KRB5_FILE_CCACHE__
#define __KRB5_FILE_CCACHE__

#include "k5-int.h"
#include <stdio.h>

#define KRB5_OK 0

#define KRB5_SCC_MAXLEN 100

/*
 * SCC version 2 contains type information for principals.  SCC
 * version 1 does not.  The code will accept either, and depending on
 * what KRB5_SCC_DEFAULT_FVNO is set to, it will create version 1 or
 * version 2 SCC caches.
 *
 */

#define KRB5_SCC_FVNO_1   0x0501        /* krb v5, scc v1 */
#define KRB5_SCC_FVNO_2   0x0502        /* krb v5, scc v2 */
#define KRB5_SCC_FVNO_3   0x0503        /* krb v5, scc v3 */
#define KRB5_SCC_FVNO_4   0x0504        /* krb v5, scc v4 */

#define SCC_OPEN_AND_ERASE      1
#define SCC_OPEN_RDWR           2
#define SCC_OPEN_RDONLY         3

/* Credential file header tags.
 * The header tags are constructed as:
 *     krb5_ui_2       tag
 *     krb5_ui_2       len
 *     krb5_octet      data[len]
 * This format allows for older versions of the fcc processing code to skip
 * past unrecognized tag formats.
 */
#define SCC_TAG_DELTATIME       1

#ifndef TKT_ROOT
#define TKT_ROOT "/tmp/tkt"
#endif

typedef struct _krb5_scc_data {
    char *filename;
    FILE *file;
    krb5_flags flags;
    char stdio_buffer[BUFSIZ];
    int version;
} krb5_scc_data;

/* An off_t can be arbitrarily complex */
typedef struct _krb5_scc_cursor {
    long pos;
} krb5_scc_cursor;

/* DO NOT ADD ANYTHING AFTER THIS #endif */
#endif /* __KRB5_FILE_CCACHE__ */
