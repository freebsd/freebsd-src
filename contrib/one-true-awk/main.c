/****************************************************************
Copyright (C) Lucent Technologies 1997
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name Lucent Technologies or any of
its entities not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
****************************************************************/

const char	*version = "version 20250116";

#define DEBUG
#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "awk.h"

extern	char	**environ;
extern	int	nfields;

int	dbg	= 0;
Awkfloat	srand_seed = 1;
char	*cmdname;	/* gets argv[0] for error messages */
extern	FILE	*yyin;	/* lex input file */
char	*lexprog;	/* points to program argument if it exists */
extern	int errorflag;	/* non-zero if any syntax errors; set by yyerror */
enum compile_states	compile_time = ERROR_PRINTING;

static char	**pfile;	/* program filenames from -f's */
static size_t	maxpfile;	/* max program filename */
static size_t	npfile;		/* number of filenames */
static size_t	curpfile;	/* current filename */

bool	CSV = false;	/* true for csv input */

bool	safe = false;	/* true => "safe" mode */

size_t	awk_mb_cur_max = 1;

static noreturn void fpecatch(int n
#ifdef SA_SIGINFO
	, siginfo_t *si, void *uc
#endif
)
{
#ifdef SA_SIGINFO
	const char *mesg = NULL;

	switch (si->si_code) {
	case FPE_INTDIV:
		mesg = "Integer divide by zero";
		break;
	case FPE_INTOVF:
		mesg = "Integer overflow";
		break;
	case FPE_FLTDIV:
		mesg = "Floating point divide by zero";
		break;
	case FPE_FLTOVF:
		mesg = "Floating point overflow";
		break;
	case FPE_FLTUND:
		mesg = "Floating point underflow";
		break;
	case FPE_FLTRES:
		mesg = "Floating point inexact result";
		break;
	case FPE_FLTINV:
		mesg = "Invalid Floating point operation";
		break;
	case FPE_FLTSUB:
		mesg = "Subscript out of range";
		break;
	case 0:
	default:
		mesg = "Unknown error";
		break;
	}
#endif
	FATAL("floating point exception"
#ifdef SA_SIGINFO
		": %s", mesg
#endif
	    );
}

/* Can this work with recursive calls?  I don't think so.
void segvcatch(int n)
{
	FATAL("segfault.  Do you have an unbounded recursive call?", n);
}
*/

static const char *
setfs(char *p)
{
	/* wart: t=>\t */
	if (p[0] == 't' && p[1] == '\0')
		return "\t";
	return p;
}

static char *
getarg(int *argc, char ***argv, const char *msg)
{
	if ((*argv)[1][2] != '\0') {	/* arg is -fsomething */
		return &(*argv)[1][2];
	} else {			/* arg is -f something */
		(*argc)--; (*argv)++;
		if (*argc <= 1)
			FATAL("%s", msg);
		return (*argv)[1];
	}
}

int main(int argc, char *argv[])
{
	const char *fs = NULL;
	char *fn, *vn;

	setlocale(LC_CTYPE, "");
	setlocale(LC_NUMERIC, "C"); /* for parsing cmdline & prog */
	awk_mb_cur_max = MB_CUR_MAX;
	cmdname = argv[0];
	if (argc == 1) {
		fprintf(stderr,
		  "usage: %s [-F fs | --csv] [-v var=value] [-f progfile | 'prog'] [file ...]\n",
		  cmdname);
		exit(1);
	}
#ifdef SA_SIGINFO
	{
		struct sigaction sa;
		sa.sa_sigaction = fpecatch;
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		(void)sigaction(SIGFPE, &sa, NULL);
	}
#else
	(void)signal(SIGFPE, fpecatch);
#endif
	/*signal(SIGSEGV, segvcatch); experiment */

	/* Set and keep track of the random seed */
	srand_seed = 1;
	srandom((unsigned long) srand_seed);

	yyin = NULL;
	symtab = makesymtab(NSYMTAB/NSYMTAB);
	while (argc > 1 && argv[1][0] == '-' && argv[1][1] != '\0') {
		if (strcmp(argv[1], "-version") == 0 || strcmp(argv[1], "--version") == 0) {
			printf("awk %s\n", version);
			return 0;
		}
		if (strcmp(argv[1], "--") == 0) {	/* explicit end of args */
			argc--;
			argv++;
			break;
		}
		if (strcmp(argv[1], "--csv") == 0) {	/* turn on csv input processing */
			CSV = true;
			argc--;
			argv++;
			continue;
		}
		switch (argv[1][1]) {
		case 's':
			if (strcmp(argv[1], "-safe") == 0)
				safe = true;
			break;
		case 'f':	/* next argument is program filename */
			fn = getarg(&argc, &argv, "no program filename");
			if (npfile >= maxpfile) {
				maxpfile += 20;
				pfile = (char **) realloc(pfile, maxpfile * sizeof(*pfile));
				if (pfile == NULL)
					FATAL("error allocating space for -f options");
 			}
			pfile[npfile++] = fn;
 			break;
		case 'F':	/* set field separator */
			fs = setfs(getarg(&argc, &argv, "no field separator"));
			break;
		case 'v':	/* -v a=1 to be done NOW.  one -v for each */
			vn = getarg(&argc, &argv, "no variable name");
			if (isclvar(vn))
				setclvar(vn);
			else
				FATAL("invalid -v option argument: %s", vn);
			break;
		case 'd':
			dbg = atoi(&argv[1][2]);
			if (dbg == 0)
				dbg = 1;
			printf("awk %s\n", version);
			break;
		default:
			WARNING("unknown option %s ignored", argv[1]);
			break;
		}
		argc--;
		argv++;
	}

	if (CSV && (fs != NULL || lookup("FS", symtab) != NULL))
		WARNING("danger: don't set FS when --csv is in effect");

	/* argv[1] is now the first argument */
	if (npfile == 0) {	/* no -f; first argument is program */
		if (argc <= 1) {
			if (dbg)
				exit(0);
			FATAL("no program given");
		}
		DPRINTF("program = |%s|\n", argv[1]);
		lexprog = argv[1];
		argc--;
		argv++;
	}
	recinit(recsize);
	syminit();
	compile_time = COMPILING;
	argv[0] = cmdname;	/* put prog name at front of arglist */
	DPRINTF("argc=%d, argv[0]=%s\n", argc, argv[0]);
	arginit(argc, argv);
	if (!safe)
		envinit(environ);
	yyparse();
#if 0
	// Doing this would comply with POSIX, but is not compatible with
	// other awks and with what most users expect. So comment it out.
	setlocale(LC_NUMERIC, ""); /* back to whatever it is locally */
#endif
	if (fs)
		*FS = qstring(fs, '\0');
	DPRINTF("errorflag=%d\n", errorflag);
	if (errorflag == 0) {
		compile_time = RUNNING;
		run(winner);
	} else
		bracecheck();
	return(errorflag);
}

int pgetc(void)		/* get 1 character from awk program */
{
	int c;

	for (;;) {
		if (yyin == NULL) {
			if (curpfile >= npfile)
				return EOF;
			if (strcmp(pfile[curpfile], "-") == 0)
				yyin = stdin;
			else if ((yyin = fopen(pfile[curpfile], "r")) == NULL)
				FATAL("can't open file %s", pfile[curpfile]);
			lineno = 1;
		}
		if ((c = getc(yyin)) != EOF)
			return c;
		if (yyin != stdin)
			fclose(yyin);
		yyin = NULL;
		curpfile++;
	}
}

char *cursource(void)	/* current source file name */
{
	if (npfile > 0)
		return pfile[curpfile < npfile ? curpfile : curpfile - 1];
	else
		return NULL;
}
