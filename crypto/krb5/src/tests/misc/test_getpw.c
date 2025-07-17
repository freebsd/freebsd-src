/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/misc/test_getpw.c */
/*
 * Copyright (C) 2005 by the Massachusetts Institute of Technology.
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

#include "autoconf.h"
#include "k5-platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

int main()
{
    uid_t my_uid;
    struct passwd *pwd, pwx;
    char pwbuf[BUFSIZ];
    int x;

    my_uid = getuid();
    printf("my uid: %ld\n", (long) my_uid);

    x = k5_getpwuid_r(my_uid, &pwx, pwbuf, sizeof(pwbuf), &pwd);
    printf("k5_getpwuid_r returns %d\n", x);
    if (x != 0)
        exit(1);
    printf("    username is '%s'\n", pwd->pw_name);
    exit(0);
}
