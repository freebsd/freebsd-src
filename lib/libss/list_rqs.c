/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */
#include "copyright.h"
#include "ss_internal.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef lint     /* "lint returns a value which is sometimes ignored" */
#define DONT_USE(x)     x=x;
#else /* !lint */
#define DONT_USE(x)     ;
#endif /* lint */

static char const twentyfive_spaces[26] =
    "                         ";
static char const NL[2] = "\n";

ss_list_requests(argc, argv, sci_idx, info_ptr)
    int argc;
    char **argv;
    int sci_idx;
    pointer info_ptr;
{
    register ss_request_entry *entry;
    register char const * const *name;
    register int spacing;
    register ss_request_table **table;

    char buffer[BUFSIZ];
    FILE *output;
    int fd;
    int mask;
#ifdef POSIX
    void (*func)();
#else
    int (*func)();
#endif
    union wait waitb;

    DONT_USE(argc);
    DONT_USE(argv);

    mask = sigblock(sigmask(SIGINT));
    func = signal(SIGINT, SIG_IGN);
    fd = ss_pager_create();
    output = fdopen(fd, "w");
    sigsetmask(mask);

    fprintf (output, "Available %s requests:\n\n",
	     ss_info (sci_idx) -> subsystem_name);

    for (table = ss_info(sci_idx)->rqt_tables; *table; table++) {
        entry = (*table)->requests;
        for (; entry->command_names; entry++) {
            spacing = -2;
            buffer[0] = '\0';
            if (entry->flags & SS_OPT_DONT_LIST)
                continue;
            for (name = entry->command_names; *name; name++) {
                register int len = strlen(*name);
                strncat(buffer, *name, len);
                spacing += len + 2;
                if (name[1]) {
                    strcat(buffer, ", ");
                }
            }
            if (spacing > 23) {
                strcat(buffer, NL);
                fputs(buffer, output);
                spacing = 0;
                buffer[0] = '\0';
            }
            strncat(buffer, twentyfive_spaces, 25-spacing);
            strcat(buffer, entry->info_string);
            strcat(buffer, NL);
            fputs(buffer, output);
        }
    }
    fclose(output);
#ifndef NO_FORK
    wait(&waitb);
#endif
    (void) signal(SIGINT, func);
}
