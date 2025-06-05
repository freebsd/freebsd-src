/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1994 by the Massachusetts Institute of Technology.
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

#include <k5-platform.h>
#include <krb5.h>
#include <locale.h>
#include <ss/ss.h>
#include "kadmin.h"

#ifdef NEED_SS_EXECUTE_COMMAND_PROTO
int ss_execute_command(int, char **);
#endif

extern ss_request_table kadmin_cmds;
extern int exit_status;
extern char *whoami;

int
main(int argc, char *argv[])
{
    char *request, **args;
    krb5_error_code retval;
    int sci_idx, code = 0;

    setlocale(LC_ALL, "");
    whoami = ((whoami = strrchr(argv[0], '/')) ? whoami+1 : argv[0]);

    kadmin_startup(argc, argv, &request, &args);
    sci_idx = ss_create_invocation(whoami, "5.0", NULL, &kadmin_cmds, &retval);
    if (retval) {
        ss_perror(sci_idx, retval, _("creating invocation"));
        exit(1);
    }

    if (*args != NULL) {
        /* Execute post-option arguments as a single script-mode command. */
        code = ss_execute_command(sci_idx, args);
        if (code) {
            ss_perror(sci_idx, code, *args);
            exit_status = 1;
        }
    } else if (request != NULL) {
        /* Execute the -q option as a single interactive command. */
        code = ss_execute_line(sci_idx, request);
        if (code != 0) {
            ss_perror(sci_idx, code, request);
            exit_status = 1;
        }
    } else {
        /* Prompt for commands. */
        (void)ss_listen(sci_idx);
    }

    return quit() ? 1 : exit_status;
}
