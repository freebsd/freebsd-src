/*
 * main.c -- Expression tree constructors and main program for gawk. 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1999 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $FreeBSD: src/contrib/awk/main.c,v 1.4 1999/09/27 08:56:57 sheldonh Exp $
 */

#include "awk.h"
#include "getopt.h"
#include "patchlevel.h"

static void usage P((int exitval, FILE *fp));
static void copyleft P((void));
static void cmdline_fs P((char *str));
static void init_args P((int argc0, int argc, char *argv0, char **argv));
static void init_vars P((void));
static void pre_assign P((char *v));
RETSIGTYPE catchsig P((int sig, int code));
static void nostalgia P((void));
static void version P((void));

/* These nodes store all the special variables AWK uses */
NODE *ARGC_node, *ARGIND_node, *ARGV_node, *CONVFMT_node, *ENVIRON_node;
NODE *ERRNO_node, *FIELDWIDTHS_node, *FILENAME_node, *FNR_node, *FS_node;
NODE *IGNORECASE_node, *NF_node, *NR_node, *OFMT_node, *OFS_node;
NODE *ORS_node, *RLENGTH_node, *RSTART_node, *RS_node, *RT_node, *SUBSEP_node;
 
long NF;
long NR;
long FNR;
int IGNORECASE;
char *OFS;
char *ORS;
char *OFMT;

/*
 * CONVFMT is a convenience pointer for the current number to string format.
 * We must supply an initial value to avoid recursion problems of
 *	set_CONVFMT -> fmt_index -> r_force_string: gets NULL CONVFMT
 * Fun, fun, fun, fun.
 */
char *CONVFMT = "%.6g";

int errcount = 0;		/* error counter, used by yyerror() */

NODE *Nnull_string;		/* The global null string */

/* The name the program was invoked under, for error messages */
const char *myname;

/* A block of AWK code to be run before running the program */
NODE *begin_block = NULL;

/* A block of AWK code to be run after the last input file */
NODE *end_block = NULL;

int exiting = FALSE;		/* Was an "exit" statement executed? */
int exit_val = 0;		/* optional exit value */

#if defined(YYDEBUG) || defined(DEBUG)
extern int yydebug;
#endif

struct src *srcfiles = NULL;	/* source file name(s) */
long numfiles = -1;		/* how many source files */

int do_traditional = FALSE;	/* no gnu extensions, add traditional weirdnesses */
int do_posix = FALSE;		/* turn off gnu and unix extensions */
int do_lint = FALSE;		/* provide warnings about questionable stuff */
int do_lint_old = FALSE;	/* warn about stuff not in V7 awk */
int do_nostalgia = FALSE;	/* provide a blast from the past */
int do_intervals = FALSE;	/* allow {...,...} in regexps */

int in_begin_rule = FALSE;	/* we're in a BEGIN rule */
int in_end_rule = FALSE;	/* we're in a END rule */

int output_is_tty = FALSE;	/* control flushing of output */

extern char *version_string;	/* current version, for printing */

/* The parse tree is stored here.  */
NODE *expression_value;

static struct option optab[] = {
	{ "compat",		no_argument,		& do_traditional,	1 },
	{ "traditional",	no_argument,		& do_traditional,	1 },
	{ "lint",		no_argument,		& do_lint,	1 },
	{ "lint-old",		no_argument,		& do_lint_old,	1 },
	{ "posix",		no_argument,		& do_posix,	1 },
	{ "nostalgia",		no_argument,		& do_nostalgia,	1 },
	{ "copyleft",		no_argument,		NULL,		'C' },
	{ "copyright",		no_argument,		NULL,		'C' },
	{ "field-separator",	required_argument,	NULL,		'F' },
	{ "file",		required_argument,	NULL,		'f' },
	{ "re-interval",		no_argument,	& do_intervals,		1 },
	{ "source",		required_argument,	NULL,		's' },
	{ "assign",		required_argument,	NULL,		'v' },
	{ "version",		no_argument,		NULL,		'V' },
	{ "usage",		no_argument,		NULL,		'u' },
	{ "help",		no_argument,		NULL,		'u' },
#ifdef DEBUG
	{ "parsedebug",		no_argument,		NULL,		'D' },
#endif
	{ NULL, 0, NULL, '\0' }
};

/* main --- process args, parse program, run it, clean up */

int
main(argc, argv)
int argc;
char **argv;
{
	int c;
	char *scan;
	/* the + on the front tells GNU getopt not to rearrange argv */
	const char *optlist = "+F:f:v:W;m:";
	int stopped_early = FALSE;
	int old_optind;
	extern int optind;
	extern int opterr;
	extern char *optarg;

	setlocale(LC_CTYPE, "");
	setlocale(LC_COLLATE, "");

	(void) signal(SIGFPE,  (RETSIGTYPE (*) P((int))) catchsig);
	(void) signal(SIGSEGV, (RETSIGTYPE (*) P((int))) catchsig);
#ifdef SIGBUS
	(void) signal(SIGBUS,  (RETSIGTYPE (*) P((int))) catchsig);
#endif

	myname = gawk_name(argv[0]);
        argv[0] = (char *) myname;
	os_arg_fixup(&argc, &argv); /* emulate redirection, expand wildcards */

	/* remove sccs gunk */
	if (strncmp(version_string, "@(#)", 4) == 0)
		version_string += 4;

	if (argc < 2)
		usage(1, stderr);

	/* initialize the null string */
	Nnull_string = make_string("", 0);
	Nnull_string->numbr = 0.0;
	Nnull_string->type = Node_val;
	Nnull_string->flags = (PERM|STR|STRING|NUM|NUMBER);

	/*
	 * Tell the regex routines how they should work.
	 * Do this before initializing variables, since
	 * they could want to do a regexp compile.
	 */
	resetup();

	/* Set up the special variables */
	/*
	 * Note that this must be done BEFORE arg parsing else -F
	 * breaks horribly 
	 */
	init_vars();

	/* worst case */
	emalloc(srcfiles, struct src *, argc * sizeof(struct src), "main");
	memset(srcfiles, '\0', argc * sizeof(struct src));

	/* we do error messages ourselves on invalid options */
	opterr = FALSE;

	/* option processing. ready, set, go! */
	for (optopt = 0, old_optind = 1;
	     (c = getopt_long(argc, argv, optlist, optab, NULL)) != EOF;
	     optopt = 0, old_optind = optind) {
		if (do_posix)
			opterr = TRUE;

		switch (c) {
		case 'F':
			cmdline_fs(optarg);
			break;

		case 'f':
			/*
			 * a la MKS awk, allow multiple -f options.
			 * this makes function libraries real easy.
			 * most of the magic is in the scanner.
			 *
			 * The following is to allow for whitespace at the end
			 * of a #! /bin/gawk line in an executable file
			 */
			scan = optarg;
			while (ISSPACE(*scan))
				scan++;

			++numfiles;
			srcfiles[numfiles].stype = SOURCEFILE;
			if (*scan == '\0')
				srcfiles[numfiles].val = argv[optind++];
			else
				srcfiles[numfiles].val = optarg;
			break;

		case 'v':
			pre_assign(optarg);
			break;

		case 'm':
			/*
			 * Research awk extension.
			 *	-mf nnn		set # fields, gawk ignores
			 *	-mr nnn		set record length, ditto
			 */
			if (do_lint)
				warning("-m[fr] option irrelevant in gawk");
			if (optarg[0] != 'r' && optarg[0] != 'f')
				warning("-m option usage: `-m[fr] nnn'");
			if (optarg[1] == '\0')
				optind++;
			break;

		case 'W':       /* gawk specific options - now in getopt_long */
			fprintf(stderr, "%s: option `-W %s' unrecognized, ignored\n",
				argv[0], optarg);
			break;

		/* These can only come from long form options */
		case 'C':
			copyleft();
			break;

		case 's':
			if (optarg[0] == '\0')
				warning("empty argument to --source ignored");
			else {
				srcfiles[++numfiles].stype = CMDLINE;
				srcfiles[numfiles].val = optarg;
			}
			break;

		case 'u':
			usage(0, stdout);	/* per coding stds */
			break;

		case 'V':
			version();
			break;

#ifdef DEBUG
		case 'D':
			yydebug = 2;
			break;
#endif

		case 0:
			/*
			 * getopt_long found an option that sets a variable
			 * instead of returning a letter. Do nothing, just
			 * cycle around for the next one.
			 */
			break;

		case '?':
		default:
			/*
			 * New behavior.  If not posix, an unrecognized
			 * option stops argument processing so that it can
			 * go into ARGV for the awk program to see. This
			 * makes use of ``#! /bin/gawk -f'' easier.
			 *
			 * However, it's never simple. If optopt is set,
			 * an option that requires an argument didn't get the
			 * argument. We care because if opterr is 0, then
			 * getopt_long won't print the error message for us.
			 */
			if (! do_posix
			    && (optopt == '\0' || strchr(optlist, optopt) == NULL)) {
				/*
				 * can't just do optind--. In case of an
				 * option with >= 2 letters, getopt_long
				 * won't have incremented optind.
				 */
				optind = old_optind;
				stopped_early = TRUE;
				goto out;
			} else if (optopt != '\0')
				/* Use 1003.2 required message format */
				fprintf(stderr,
				"%s: option requires an argument -- %c\n",
					myname, optopt);
			/* else
				let getopt print error message for us */
			break;
		}
	}
out:

	if (do_nostalgia)
		nostalgia();

	/* check for POSIXLY_CORRECT environment variable */
	if (! do_posix && getenv("POSIXLY_CORRECT") != NULL) {
		do_posix = TRUE;
		if (do_lint)
			warning(
	"environment variable `POSIXLY_CORRECT' set: turning on --posix");
	}

	if (do_posix) {
		if (do_traditional)	/* both on command line */
			warning("--posix overrides --traditional");
		else
			do_traditional = TRUE;
			/*
			 * POSIX compliance also implies
			 * no GNU extensions either.
			 */
	}

	/*
	 * Tell the regex routines how they should work.
	 * Do this again, after argument processing, since do_posix
	 * and do_traditional are now paid attention to by resetup().
	 */
	if (do_traditional || do_posix || do_intervals) {
		resetup();

		/* now handle RS and FS. have to be careful with FS */
		set_RS();
		if (using_fieldwidths()) {
			set_FS();
			set_FIELDWIDTHS();
		} else
			set_FS();
	}

#ifdef DEBUG
	setbuf(stdout, (char *) NULL);	/* make debugging easier */
#endif
	if (isatty(fileno(stdout)))
		output_is_tty = TRUE;
	/* No -f or --source options, use next arg */
	if (numfiles == -1) {
		if (optind > argc - 1 || stopped_early) /* no args left or no program */
			usage(1, stderr);
		srcfiles[++numfiles].stype = CMDLINE;
		srcfiles[numfiles].val = argv[optind];
		optind++;
	}

	init_args(optind, argc, (char *) myname, argv);
	(void) tokexpand();

	/* Read in the program */
	if (yyparse() != 0 || errcount != 0)
		exit(1);
	/* recover any space from C based alloca */
#ifdef C_ALLOCA
	(void) alloca(0);
#endif

	/* Set up the field variables */
	init_fields();

	if (do_lint && begin_block == NULL && expression_value == NULL
	     && end_block == NULL)
		warning("no program");

	if (begin_block != NULL) {
		in_begin_rule = TRUE;
		(void) interpret(begin_block);
	}
	in_begin_rule = FALSE;
	if (! exiting && (expression_value != NULL || end_block != NULL))
		do_input();
	if (end_block != NULL) {
		in_end_rule = TRUE;
		(void) interpret(end_block);
	}
	in_end_rule = FALSE;
	if (close_io() != 0 && exit_val == 0)
		exit_val = 1;
	exit(exit_val);		/* more portable */
	return exit_val;	/* to suppress warnings */
}

/* usage --- print usage information and exit */

static void
usage(exitval, fp)
int exitval;
FILE *fp;
{
	char *opt1 = " -f progfile [--]";
	char *regops = " [POSIX or GNU style options]";

	fprintf(fp, "Usage: %s%s%s file ...\n\t%s%s [--] %cprogram%c file ...\n",
		myname, regops, opt1, myname, regops, quote, quote);

	/* GNU long options info. Gack. */
	fputs("POSIX options:\t\tGNU long options:\n", fp);
	fputs("\t-f progfile\t\t--file=progfile\n", fp);
	fputs("\t-F fs\t\t\t--field-separator=fs\n", fp);
	fputs("\t-v var=val\t\t--assign=var=val\n", fp);
	fputs("\t-m[fr] val\n", fp);
	fputs("\t-W compat\t\t--compat\n", fp);
	fputs("\t-W copyleft\t\t--copyleft\n", fp);
	fputs("\t-W copyright\t\t--copyright\n", fp);
	fputs("\t-W help\t\t\t--help\n", fp);
	fputs("\t-W lint\t\t\t--lint\n", fp);
	fputs("\t-W lint-old\t\t--lint-old\n", fp);
#ifdef NOSTALGIA
	fputs("\t-W nostalgia\t\t--nostalgia\n", fp);
#endif
#ifdef DEBUG
	fputs("\t-W parsedebug\t\t--parsedebug\n", fp);
#endif
	fputs("\t-W posix\t\t--posix\n", fp);
	fputs("\t-W re-interval\t\t--re-interval\n", fp);
	fputs("\t-W source=program-text\t--source=program-text\n", fp);
	fputs("\t-W traditional\t\t--traditional\n", fp);
	fputs("\t-W usage\t\t--usage\n", fp);
	fputs("\t-W version\t\t--version\n", fp);
	fputs("\nReport bugs to bug-gnu-utils@gnu.org,\n", fp);
	fputs("with a Cc: to arnold@gnu.org\n", fp);
	exit(exitval);
}

/* copyleft --- print out the short GNU copyright information */

static void
copyleft()
{
	static char blurb_part1[] =
"Copyright (C) 1989, 1991-1999 Free Software Foundation.\n\
\n\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or\n\
(at your option) any later version.\n\
\n";
	static char blurb_part2[] =
"This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n";
	static char blurb_part3[] =
"You should have received a copy of the GNU General Public License\n\
along with this program; if not, write to the Free Software\n\
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n";

	/* multiple blurbs are needed for some brain dead compilers. */
	fputs(blurb_part1, stdout);
	fputs(blurb_part2, stdout);
	fputs(blurb_part3, stdout);
	fflush(stdout);
	exit(0);
}

/* cmdline_fs --- set FS from the command line */

static void
cmdline_fs(str)
char *str;
{
	register NODE **tmp;

	tmp = get_lhs(FS_node, (Func_ptr *) 0);
	unref(*tmp);
	/*
	 * Only if in full compatibility mode check for the stupid special
	 * case so -F\t works as documented in awk book even though the shell
	 * hands us -Ft.  Bleah!
	 *
	 * Thankfully, Posix didn't propogate this "feature".
	 */
	if (str[0] == 't' && str[1] == '\0') {
		if (do_lint)
			warning("-Ft does not set FS to tab in POSIX awk");
		if (do_traditional && ! do_posix)
			str[0] = '\t';
	}
	*tmp = make_str_node(str, strlen(str), SCAN); /* do process escapes */
	set_FS();
}

/* init_args --- set up ARGV from stuff on the command line */

static void
init_args(argc0, argc, argv0, argv)
int argc0, argc;
char *argv0;
char **argv;
{
	int i, j;
	NODE **aptr;

	ARGV_node = install("ARGV", node(Nnull_string, Node_var_array, (NODE *) NULL));
	aptr = assoc_lookup(ARGV_node, tmp_number(0.0));
	*aptr = make_string(argv0, strlen(argv0));
	(*aptr)->flags |= MAYBE_NUM;
	for (i = argc0, j = 1; i < argc; i++) {
		aptr = assoc_lookup(ARGV_node, tmp_number((AWKNUM) j));
		*aptr = make_string(argv[i], strlen(argv[i]));
		(*aptr)->flags |= MAYBE_NUM;
		j++;
	}
	ARGC_node = install("ARGC",
			node(make_number((AWKNUM) j), Node_var, (NODE *) NULL));
}

/*
 * Set all the special variables to their initial values.
 * Note that some of the variables that have set_FOO routines should
 * *N*O*T* have those routines called upon initialization, and thus
 * they have NULL entries in that field. This is notably true of FS
 * and IGNORECASE.
 */
struct varinit {
	NODE **spec;
	const char *name;
	NODETYPE type;
	const char *strval;
	AWKNUM numval;
	Func_ptr assign;
};
static struct varinit varinit[] = {
{&CONVFMT_node,	"CONVFMT",	Node_CONVFMT,		"%.6g",	0,  set_CONVFMT },
{&NF_node,	"NF",		Node_NF,		NULL,	-1, set_NF },
{&FIELDWIDTHS_node, "FIELDWIDTHS", Node_FIELDWIDTHS,	"",	0,  NULL },
{&NR_node,	"NR",		Node_NR,		NULL,	0,  set_NR },
{&FNR_node,	"FNR",		Node_FNR,		NULL,	0,  set_FNR },
{&FS_node,	"FS",		Node_FS,		" ",	0,  NULL },
{&RS_node,	"RS",		Node_RS,		"\n",	0,  set_RS },
{&IGNORECASE_node, "IGNORECASE", Node_IGNORECASE,	NULL,	0,  NULL },
{&FILENAME_node, "FILENAME",	Node_var,		"",	0,  NULL },
{&OFS_node,	"OFS",		Node_OFS,		" ",	0,  set_OFS },
{&ORS_node,	"ORS",		Node_ORS,		"\n",	0,  set_ORS },
{&OFMT_node,	"OFMT",		Node_OFMT,		"%.6g",	0,  set_OFMT },
{&RLENGTH_node, "RLENGTH",	Node_var,		NULL,	0,  NULL },
{&RSTART_node,	"RSTART",	Node_var,		NULL,	0,  NULL },
{&SUBSEP_node,	"SUBSEP",	Node_var,		"\034",	0,  NULL },
{&ARGIND_node,	"ARGIND",	Node_var,		NULL,	0,  NULL },
{&ERRNO_node,	"ERRNO",	Node_var,		NULL,	0,  NULL },
{&RT_node,	"RT",		Node_var,		"",	0,  NULL },
{0,		NULL,		Node_illegal,		NULL,	0,  NULL },
};

/* init_vars --- actually initialize everything in the symbol table */

static void
init_vars()
{
	register struct varinit *vp;

	for (vp = varinit; vp->name; vp++) {
		*(vp->spec) = install((char *) vp->name,
		  node(vp->strval == NULL ? make_number(vp->numval)
				: make_string((char *) vp->strval,
					strlen(vp->strval)),
		       vp->type, (NODE *) NULL));
		(*(vp->spec))->flags |= SCALAR;
		if (vp->assign)
			(*(vp->assign))();
	}
}

/* load_environ --- populate the ENVIRON array */

void
load_environ()
{
#if ! (defined(MSDOS) && !defined(DJGPP)) && ! defined(OS2) && ! (defined(VMS) && defined(__DECC))
	extern char **environ;
#endif
	register char *var, *val, *cp;
	NODE **aptr;
	register int i;

	ENVIRON_node = install("ENVIRON", 
			node(Nnull_string, Node_var, (NODE *) NULL));
	for (i = 0; environ[i] != NULL; i++) {
		static char nullstr[] = "";

		var = environ[i];
		val = strchr(var, '=');
		if (val != NULL)
			*val++ = '\0';
		else
			val = nullstr;
		aptr = assoc_lookup(ENVIRON_node, tmp_string(var, strlen(var)));
		*aptr = make_string(val, strlen(val));
		(*aptr)->flags |= (MAYBE_NUM|SCALAR);

		/* restore '=' so that system() gets a valid environment */
		if (val != nullstr)
			*--val = '=';
	}
	/*
	 * Put AWKPATH into ENVIRON if it's not there.
	 * This allows querying it from outside gawk.
	 */
	if ((cp = getenv("AWKPATH")) == NULL) {
		aptr = assoc_lookup(ENVIRON_node, tmp_string("AWKPATH", 7));
		*aptr = make_string(defpath, strlen(defpath));
		(*aptr)->flags |= SCALAR;
	}
}

/* arg_assign --- process a command-line assignment */

char *
arg_assign(arg)
char *arg;
{
	char *cp, *cp2;
	int badvar;
	Func_ptr after_assign = NULL;
	NODE *var;
	NODE *it;
	NODE **lhs;

	cp = strchr(arg, '=');
	if (cp != NULL) {
		*cp++ = '\0';
		/* first check that the variable name has valid syntax */
		badvar = FALSE;
		if (! isalpha(arg[0]) && arg[0] != '_')
			badvar = TRUE;
		else
			for (cp2 = arg+1; *cp2; cp2++)
				if (! isalnum(*cp2) && *cp2 != '_') {
					badvar = TRUE;
					break;
				}
		if (badvar)
			fatal("illegal name `%s' in variable assignment", arg);

		/*
		 * Recent versions of nawk expand escapes inside assignments.
		 * This makes sense, so we do it too.
		 */
		it = make_str_node(cp, strlen(cp), SCAN);
		it->flags |= (MAYBE_NUM|SCALAR);
		var = variable(arg, FALSE, Node_var);
		lhs = get_lhs(var, &after_assign);
		unref(*lhs);
		*lhs = it;
		if (after_assign != NULL)
			(*after_assign)();
		*--cp = '=';	/* restore original text of ARGV */
	}
	return cp;
}

/* pre_assign --- handle -v, print a message and die if a problem */

static void
pre_assign(v)
char *v;
{
	if (arg_assign(v) == NULL) {
		fprintf(stderr,
			"%s: `%s' argument to `-v' not in `var=value' form\n",
				myname, v);
		usage(1, stderr);
	}
}

/* catchsig --- catch signals */

RETSIGTYPE
catchsig(sig, code)
int sig, code;
{
#ifdef lint
	code = 0; sig = code; code = sig;
#endif
	if (sig == SIGFPE) {
		fatal("floating point exception");
	} else if (sig == SIGSEGV
#ifdef SIGBUS
	        || sig == SIGBUS
#endif
	) {
		set_loc(__FILE__, __LINE__);
		msg("fatal error: internal error");
		/* fatal won't abort() if not compiled for debugging */
		abort();
	} else
		cant_happen();
	/* NOTREACHED */
}

/* nostalgia --- print the famous error message and die */

static void
nostalgia()
{
	fprintf(stderr, "awk: bailing out near line 1\n");
	abort();
}

/* version --- print version message */

static void
version()
{
	printf("%s.%d\n", version_string, PATCHLEVEL);
	/*
	 * Per GNU coding standards, print copyright info,
	 * then exit successfully, do nothing else.
	 */
	copyleft();
	exit(0);
}
