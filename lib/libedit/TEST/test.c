/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if !defined(lint) && !defined(SCCSID)
static char sccsid[] = "@(#)test.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint && not SCCSID */

/*
 * test.c: A little test program
 */
#include "sys.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "histedit.h"
#include "tokenizer.h"

static int continuation = 0;
static EditLine *el = NULL;

static char *
/*ARGSUSED*/
prompt(el)
    EditLine *el;
{
    static char a[] = "Edit$";
    static char b[] = "Edit>";
    return continuation ? b : a;
}

static void
sig(i)
    int i;
{
    (void) fprintf(stderr, "Got signal %d.\n", i);
    el_reset(el);
}

static unsigned char
/*ARGSUSED*/
complete(el, ch)
    EditLine *el;
    int ch;
{
    DIR *dd = opendir("."); 
    struct dirent *dp;
    const char* ptr;
    const LineInfo *lf = el_line(el);
    int len;

    /*
     * Find the last word
     */
    for (ptr = lf->cursor - 1; !isspace(*ptr) && ptr > lf->buffer; ptr--)
	continue;
    len = lf->cursor - ++ptr;

    for (dp = readdir(dd); dp != NULL; dp = readdir(dd)) {
	if (len > strlen(dp->d_name))
	    continue;
	if (strncmp(dp->d_name, ptr, len) == 0) {
	    closedir(dd);
	    if (el_insertstr(el, &dp->d_name[len]) == -1)
		return CC_ERROR;
	    else
		return CC_REFRESH;
	}
    }

    closedir(dd);
    return CC_ERROR;
}

int
/*ARGSUSED*/
main(argc, argv)
    int argc;
    char *argv[];
{
    int num;
    const char *buf;
    Tokenizer *tok;
    History *hist;

    (void) signal(SIGINT, sig);
    (void) signal(SIGQUIT, sig);
    (void) signal(SIGHUP, sig);
    (void) signal(SIGTERM, sig);

    hist = history_init();		/* Init the builtin history	*/
    history(hist, H_EVENT, 100);	/* Remember 100 events		*/

    tok  = tok_init(NULL);		/* Initialize the tokenizer	*/

    el = el_init(*argv, stdin, stdout);	/* Initialize editline		*/

    el_set(el, EL_EDITOR, "vi");	/* Default editor is vi 	*/
    el_set(el, EL_SIGNAL, 1);		/* Handle signals gracefully	*/
    el_set(el, EL_PROMPT, prompt);	/* Set the prompt function	*/

    /* Tell editline to use this history interface			*/
    el_set(el, EL_HIST, history, hist);

    /* Add a user-defined function 					*/
    el_set(el, EL_ADDFN, "ed-complete", "Complete argument", complete);

    el_set(el, EL_BIND, "^I", "ed-complete", NULL);/* Bind tab to it 	*/

    /*
     * Bind j, k in vi command mode to previous and next line, instead
     * of previous and next history.
     */
    el_set(el, EL_BIND, "-a", "k", "ed-prev-line", NULL);
    el_set(el, EL_BIND, "-a", "j", "ed-next-line", NULL);

    /*
     * Source the user's defaults file.
     */
    el_source(el, NULL);

    while ((buf = el_gets(el, &num)) != NULL && num != 0)  {
	int ac;
	char **av;
#ifdef DEBUG
	(void) fprintf(stderr, "got %d %s", num, buf);
#endif
	if (!continuation && num == 1)
	    continue;
	if (tok_line(tok, buf, &ac, &av) > 0) {
	    history(hist, continuation ? H_ADD : H_ENTER, buf);
	    continuation = 1;
	    continue;
	}
	history(hist, continuation ? H_ADD : H_ENTER, buf);

	continuation = 0;
	if (el_parse(el, ac, av) != -1) {
	    tok_reset(tok);
	    continue;
	}

	switch (fork()) {
	case 0:
	    execvp(av[0], av);
	    perror(av[0]);
	    _exit(1);
	    /*NOTREACHED*/
	    break;

	case -1:
	    perror("fork");
	    break;

	default:
	    if (wait(&num) == -1)
		perror("wait");
	    (void) fprintf(stderr, "Exit %x\n", num);
	    break;
	}
	tok_reset(tok);
    }

    el_end(el);
    tok_end(tok);
    history_end(hist);

    return 0;
}
