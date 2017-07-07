/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/t_tdumputil.c - test tdumputil.c functions */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tdumputil.h"

static char *argv0;

static void
usage(void)
{
    fprintf(stderr,
            "usage: %s: [-T rectype] [-c] nfields {fieldnames} {fields}\n",
            argv0);
    exit(1);
}

int
main(int argc, char **argv)
{
    int ch, csv = 0, i, nf;
    char **a, *rectype = NULL;
    struct rechandle *h;

    argv0 = argv[0];
    while ((ch = getopt(argc, argv, "T:c")) != -1) {
        switch (ch) {
        case 'T':
            rectype = optarg;
            break;
        case 'c':
            csv = 1;
            break;
        default:
            usage();
            break;
        }
    }
    argc -= optind;
    argv += optind;

    if (csv)
        h = rechandle_csv(stdout, rectype);
    else
        h = rechandle_tabsep(stdout, rectype);
    if (h == NULL)
        exit(1);

    if (*argv == NULL)
        usage();
    nf = atoi(*argv);
    argc--;
    argv++;
    a = calloc(nf + 1, sizeof(*a));
    if (a == NULL)
        exit(1);

    for (i = 0; argv[i] != NULL && i < nf; i++)
        a[i] = argv[i];
    if (i != nf)
        usage();
    argv += nf;
    a[nf] = NULL;

    if (rectype == NULL && writeheader(h, a) < 0)
        exit(1);
    free(a);

    while (*argv != NULL) {
        if (startrec(h) < 0)
            exit(1);
        for (i = 0; argv[i] != NULL && i < nf; i++) {
            if (writefield(h, "%s", argv[i]) < 0)
                exit(1);
        }
        if (i != nf)
            usage();
        argv += nf;
        if (endrec(h) < 0)
            exit(1);
    }
    exit(0);
}
