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
 *	toe.c --- table of entries report generator
 *
 */

#include <progs.priv.h>

#include <sys/stat.h>

#include <dump_entry.h>
#include <term_entry.h>

MODULE_ID("$Id: toe.c,v 0.19 1998/03/08 01:02:46 tom Exp $")

const char *_nc_progname;

static int typelist(int eargc, char *eargv[], bool,
		     void (*)(const char *, TERMTYPE *));
static void deschook(const char *, TERMTYPE *);

#if NO_LEAKS
#undef ExitProgram
static void ExitProgram(int code) GCC_NORETURN;
static void ExitProgram(int code)
{
	_nc_free_entries(_nc_head);
	_nc_leaks_dump_entry();
	_nc_free_and_exit(code);
}
#endif

int main (int argc, char *argv[])
{
    bool	direct_dependencies = FALSE;
    bool	invert_dependencies = FALSE;
    bool	header = FALSE;
    int		i, c, debug_level = 0;
    int		code;

    if ((_nc_progname = strrchr(argv[0], '/')) == NULL)
	_nc_progname = argv[0];
    else
	_nc_progname++;

    while ((c = getopt(argc, argv, "huv:UV")) != EOF)
	switch (c)
	{
	case 'h':
	    header = TRUE;
	    break;
	case 'u':
	    direct_dependencies = TRUE;
	    break;
	case 'v':
	    debug_level = atoi(optarg);
	    _nc_tracing = (1 << debug_level) - 1;
	    break;
	case 'U':
	    invert_dependencies = TRUE;
	    break;
	case 'V':
	    (void) fputs(NCURSES_VERSION, stdout);
	    putchar('\n');
	    ExitProgram(EXIT_SUCCESS);
	default:
	    (void) fprintf (stderr, "usage: toe [-huUV] [-v n] [file...]\n");
	    ExitProgram(EXIT_FAILURE);
	}

    if (direct_dependencies || invert_dependencies)
    {
	if (freopen(argv[optind], "r", stdin) == NULL)
	{
	    (void) fflush(stdout);
	    fprintf(stderr, "%s: can't open %s\n", _nc_progname, argv[optind]);
	    ExitProgram(EXIT_FAILURE);
	}

	/* parse entries out of the source file */
	_nc_set_source(argv[optind]);
	_nc_read_entry_source(stdin, (char *)NULL,
			      FALSE, FALSE,
			      NULLHOOK);
    }

    /* maybe we want a direct-dependency listing? */
    if (direct_dependencies)
    {
	ENTRY	*qp;

	for_entry_list(qp)
	    if (qp->nuses)
	    {
		int		j;

		(void) printf("%s:", _nc_first_name(qp->tterm.term_names));
		for (j = 0; j < qp->nuses; j++)
		    (void) printf(" %s", (char *)(qp->uses[j].parent));
		putchar('\n');
	    }

	ExitProgram(EXIT_SUCCESS);
    }

    /* maybe we want a reverse-dependency listing? */
    if (invert_dependencies)
    {
	ENTRY	*qp, *rp;
	int		matchcount;

	for_entry_list(qp)
	{
	    matchcount = 0;
	    for_entry_list(rp)
	    {
		if (rp->nuses == 0)
		    continue;

		for (i = 0; i < rp->nuses; i++)
		    if (_nc_name_match(qp->tterm.term_names,(char*)rp->uses[i].parent, "|"))
		    {
			if (matchcount++ == 0)
			    (void) printf("%s:",
					  _nc_first_name(qp->tterm.term_names));
			(void) printf(" %s",
				      _nc_first_name(rp->tterm.term_names));
		    }
	    }
	    if (matchcount)
		putchar('\n');
	}

	ExitProgram(EXIT_SUCCESS);
    }

    /*
     * If we get this far, user wants a simple terminal type listing.
     */
    if (optind < argc) {
	code = typelist(argc-optind, argv+optind, header, deschook);
    } else {
	char	*by_env, *home, *eargv[3];
	int	j;

	j = 0;
	if ((by_env = getenv("TERMINFO")) != (char *)NULL)
	    eargv[j++] = by_env;
	else
	{
	    if ((home = getenv("HOME")) != (char *)NULL)
	    {
		char	personal[PATH_MAX];
		struct	stat sb;

		(void) sprintf(personal, PRIVATE_INFO, home);
		if (stat(personal, &sb) == 0
		 && (sb.st_mode & S_IFMT) == S_IFDIR)
		    eargv[j++] = personal;
	    }
	    eargv[j++] = TERMINFO;
	}
	eargv[j] = (char *)NULL;

	code = typelist(j, eargv, header, deschook);
    }

    ExitProgram(code);
}

static void deschook(const char *cn, TERMTYPE *tp)
/* display a description for the type */
{
    const char *desc;

    if ((desc = strrchr(tp->term_names, '|')) == (char *)NULL)
	desc = "(No description)";
    else
	++desc;

    (void) printf("%-10s\t%s\n", cn, desc);
}

static int typelist(int eargc, char *eargv[],
		     bool verbosity,
		     void  (*hook)(const char *, TERMTYPE *tp))
/* apply a function to each entry in given terminfo directories */
{
    int	i;

    for (i = 0; i < eargc; i++)
    {
	DIR	*termdir;
	struct dirent *subdir;

	if ((termdir = opendir(eargv[i])) == (DIR *)NULL)
	{
	    (void) fflush(stdout);
	    (void) fprintf(stderr,
			   "%s: can't open terminfo directory %s\n",
			   _nc_progname, eargv[i]);
	    return(EXIT_FAILURE);
	}
	else if (verbosity)
	    (void) printf("#\n#%s:\n#\n", eargv[i]);

	while ((subdir = readdir(termdir)) != NULL)
	{
	    size_t	len = NAMLEN(subdir);
	    char	buf[PATH_MAX];
	    char	name_1[PATH_MAX];
	    DIR	*entrydir;
	    struct dirent *entry;

	    strncpy(name_1, subdir->d_name, len)[len] = '\0';
	    if (!strcmp(name_1, ".")
		|| !strcmp(name_1, ".."))
		continue;

	    (void) strcpy(buf, eargv[i]);
	    (void) strcat(buf, "/");
	    (void) strcat(buf, name_1);
	    (void) strcat(buf, "/");
	    chdir(buf);
	    entrydir = opendir(".");
	    while ((entry = readdir(entrydir)) != NULL)
	    {
		char		name_2[PATH_MAX];
		TERMTYPE	lterm;
		char		*cn;
		int		status;

		len = NAMLEN(entry);
		strncpy(name_2, entry->d_name, len)[len] = '\0';
		if (!strcmp(name_2, ".")
		    || !strcmp(name_2, ".."))
		    continue;

		status = _nc_read_file_entry(name_2, &lterm);
		if (status <= 0)
		{
		    (void) fflush(stdout);
		    (void) fprintf(stderr,
				   "toe: couldn't open terminfo file %s.\n",
				   name_2);
		    return(EXIT_FAILURE);
		}

		/* only visit things once, by primary name */
		cn = _nc_first_name(lterm.term_names);
		if (!strcmp(cn, name_2))
		{
		    /* apply the selected hook function */
		    (*hook)(cn, &lterm);
		}
		if (lterm.term_names) {
		    free(lterm.term_names);
		    lterm.term_names = NULL;
		}
		if (lterm.str_table) {
		    free(lterm.str_table);
		    lterm.str_table = NULL;
		}
	    }
	    closedir(entrydir);
	}
	closedir(termdir);
    }

    return(EXIT_SUCCESS);
}
