/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
 * tput.c -- shellscript access to terminal capabilities
 *
 * by Eric S. Raymond <esr@snark.thyrsus.com>, portions based on code from
 * Ross Ridge's mytinfo package.
 */

#include <progs.priv.h>
#ifndef	PURE_TERMINFO
#include <termsort.c>
#endif

MODULE_ID("$Id: tput.c,v 1.14 1999/07/31 21:18:29 Goran.Uddeborg Exp $")

#define PUTS(s)		fputs(s, stdout)
#define PUTCHAR(c)	putchar(c)
#define FLUSH		fflush(stdout)

static char *prg_name;

static void quit(int status, const char *fmt, ...)
{
va_list argp;

	va_start(argp,fmt);
	vfprintf (stderr, fmt, argp);
	fprintf(stderr, "\n");
	va_end(argp);
	exit(status);
}

static void usage(void)
{
	fprintf(stderr, "usage: %s [-S] [-T term] capname\n", prg_name);
	exit(EXIT_FAILURE);
}

static int tput(int argc, char *argv[])
{
char *name;
char *s;
int i, j, c;
int reset, status;
FILE *f;

	reset = 0;
	name = argv[0];
	if (strcmp(name, "reset") == 0) {
		reset = 1;
	}
	if (reset || strcmp(name, "init") == 0) {
		if (init_prog != NULL) {
			system(init_prog);
		}
		FLUSH;

		if (reset && reset_1string != NULL) {
			PUTS(reset_1string);
		} else if (init_1string != NULL) {
			PUTS(init_1string);
		}
		FLUSH;

		if (reset && reset_2string != NULL) {
			PUTS(reset_2string);
		} else if (init_2string != NULL) {
			PUTS(init_2string);
		}
		FLUSH;

		if (set_lr_margin != NULL) {
			PUTS(tparm(set_lr_margin, 0, columns - 1));
		} else if (set_left_margin_parm != NULL
			   && set_right_margin_parm != NULL) {
			PUTS(tparm(set_left_margin_parm, 0));
			PUTS(tparm(set_right_margin_parm, columns - 1));
		} else if (clear_margins != NULL && set_left_margin != NULL
			   && set_right_margin != NULL) {
			PUTS(clear_margins);
			if (carriage_return != NULL) {
				PUTS(carriage_return);
			} else {
				PUTCHAR('\r');
			}
			PUTS(set_left_margin);
			if (parm_right_cursor) {
				PUTS(tparm(parm_right_cursor, columns - 1));
			} else {
				for(i = 0; i < columns - 1; i++) {
					PUTCHAR(' ');
				}
			}
			PUTS(set_right_margin);
			if (carriage_return != NULL) {
				PUTS(carriage_return);
			} else {
				PUTCHAR('\r');
			}
		}
		FLUSH;

		if (init_tabs != 8) {
			if (clear_all_tabs != NULL && set_tab != NULL) {
				for(i = 0; i < columns - 1; i += 8) {
					if (parm_right_cursor) {
						PUTS(tparm(parm_right_cursor, 8));
					} else {
						for(j = 0; j < 8; j++)
							PUTCHAR(' ');
					}
					PUTS(set_tab);
				}
				FLUSH;
			}
		}

		if (reset && reset_file != NULL) {
			f = fopen(reset_file, "r");
			if (f == NULL) {
				quit(errno, "Can't open reset_file: '%s'", reset_file);
			}
			while((c = fgetc(f)) != EOF) {
				PUTCHAR(c);
			}
			fclose(f);
		} else if (init_file != NULL) {
			f = fopen(init_file, "r");
			if (f == NULL) {
				quit(errno, "Can't open init_file: '%s'", init_file);
			}
			while((c = fgetc(f)) != EOF) {
				PUTCHAR(c);
			}
			fclose(f);
		}
		FLUSH;

		if (reset && reset_3string != NULL) {
			PUTS(reset_3string);
		} else if (init_2string != NULL) {
			PUTS(init_2string);
		}
		FLUSH;
		return 0;
	}

	if (strcmp(name, "longname") == 0) {
		PUTS(longname());
		return 0;
	}

#ifndef	PURE_TERMINFO
	{
		const struct name_table_entry 	*np;

		if ((np = _nc_find_entry(name, _nc_get_hash_table(1))) != 0)
			switch(np->nte_type)
			{
			case BOOLEAN:
				if (bool_from_termcap[np->nte_index])
					name = boolnames[np->nte_index];
				break;

			case NUMBER:
				if (num_from_termcap[np->nte_index])
					name = numnames[np->nte_index];
				break;

			case STRING:
				if (str_from_termcap[np->nte_index])
					name = strnames[np->nte_index];
				break;
			}
	}
#endif

	if ((status = tigetflag(name)) != -1)
		return(status != 0);
	else if ((status = tigetnum(name)) != CANCELLED_NUMERIC) {
		(void) printf("%d\n", status);
		return(0);
	}
	else if ((s = tigetstr(name)) == CANCELLED_STRING)
		quit(4, "%s: unknown terminfo capability '%s'", prg_name, name);
	else if (s != (char *)NULL) {
		if (argc > 1) {
		int k;

			/* Nasty hack time. The tparm function needs to see numeric
			 * parameters as numbers, not as pointers to their string
			 * representations
			 */

			 for (k = 1; k < argc; k++)
			 	if (isdigit(argv[k][0])) {
			 		long val = atol(argv[k]);
			 		argv[k] = (char *)val;
				}

				s = tparm(s,argv[1],argv[2],argv[3],argv[4],
					    argv[5],argv[6],argv[7],argv[8],
					    argv[9]);
		}

		/* use putp() in order to perform padding */
		putp(s);
		return(0);
	}
	return(0);
}

int main(int argc, char **argv)
{
char *s, *term;
int errret, cmdline = 1;
int c;
char	buf[BUFSIZ];
int errors = 0;

	prg_name = argv[0];
	s = strrchr(prg_name, '/');
	if (s != NULL && *++s != '\0')
	prg_name = s;

	term = getenv("TERM");

	while ((c = getopt (argc, argv, "ST:")) != EOF)
	    switch (c)
	    {
	    case 'S':
		cmdline = 0;
		break;
	    case 'T':
		use_env(FALSE);
		term = optarg;
		break;
	    default:
		usage();
		/* NOTREACHED */
	    }
	argc -= optind;
	argv += optind;

	if (cmdline && argc == 0) {
		usage();
		/* NOTREACHED */
	}

	if (term == NULL || *term == '\0')
		quit(2, "No value for $TERM and no -T specified");

	if (setupterm(term, STDOUT_FILENO, &errret) != OK && errret <= 0) 
		quit(3, "unknown terminal \"%s\"", term);

	if (cmdline)
		return tput(argc, argv);

	while (fgets(buf, sizeof(buf), stdin) != (char *)NULL) {
		char	*argvec[16];	/* command, 9 parms, null, & slop */
		int	 argnum = 0;
		char    *cp;

		/* crack the argument list into a dope vector */
		for (cp = buf; *cp; cp++) {
			if (isspace(*cp))
				*cp = '\0';
			else if (cp == buf || cp[-1] == 0)
				argvec[argnum++] = cp;
		}
		argvec[argnum] = (char *)NULL;

		if (tput(argnum, argvec) != 0)
			errors++;
	}

	return errors > 0;
}

