/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1992,1993 Trusted Information Systems, Inc.
 *
 * Permission to include this software in the Kerberos V5 distribution
 * was graciously provided by Trusted Information Systems.
 *
 * Trusted Information Systems makes no representation about the
 * suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * Copyright (C) 1994 Massachusetts Institute of Technology
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

/* Split out from "#ifdef STANDALONE" code previously in trval.c, so
   that trval.o could be linked into other tests too without the
   -DSTANDALONE code.  */
#include "trval.c"

static void usage()
{
    fprintf(stderr, "Usage: trval [--types] [--krb5] [--krb5decode] [--hex] [-notypebytes] [file]\n");
    exit(1);
}

/*
 * Returns true if the option was selected.  Allow "-option" and
 * "--option" syntax, since we used to accept only "-option"
 */
static
int check_option(word, option)
    char *word;
    char *option;
{
    if (word[0] != '-')
        return 0;
    if (word[1] == '-')
        word++;
    if (strcmp(word+1, option))
        return 0;
    return 1;
}

int main(argc, argv)
    int argc;
    char **argv;
{
    int optflg = 1;
    FILE *fp;
    int r = 0;

    while (--argc > 0) {
        argv++;
        if (optflg && *(argv)[0] == '-') {
            if (check_option(*argv, "help"))
                usage();
            else if (check_option(*argv, "types"))
                print_types = 1;
            else if (check_option(*argv, "notypes"))
                print_types = 0;
            else if (check_option(*argv, "krb5"))
                print_krb5_types = 1;
            else if (check_option(*argv, "hex"))
                do_hex = 1;
            else if (check_option(*argv, "notypebytes"))
                print_id_and_len = 0;
            else if (check_option(*argv, "krb5decode")) {
                print_id_and_len = 0;
                print_krb5_types = 1;
                print_types = 1;
            } else {
                fprintf(stderr,"trval: unknown option: %s\n", *argv);
                usage();
            }
        } else {
            optflg = 0;
            if ((fp = fopen(*argv,"r")) == NULL) {
                fprintf(stderr,"trval: unable to open %s\n", *argv);
                continue;
            }
            r = trval(fp, stdout);
            fclose(fp);
        }
    }
    if (optflg) r = trval(stdin, stdout);

    exit(r);
}
