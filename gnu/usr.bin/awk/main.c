/*
 * main.c -- Expression tree constructors and main program for gawk. 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
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
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "getopt.h"
#include "awk.h"
#include "patchlevel.h"

static void usage P((int exitval));
static void copyleft P((void));
static void cmdline_fs P((char *str));
static void init_args P((int argc0, int argc, char *argv0, char **argv));
static void init_vars P((void));
static void pre_assign P((char *v));
SIGTYPE catchsig P((int sig, int code));
static void gawk_option P((char *optstr));
static void nostalgia P((void));
static void version P((void));
char *gawk_name P((char *filespec));

#ifdef MSDOS
extern int isatty P((int));
#endif

extern void resetup P((void));

/* These nodes store all the special variables AWK uses */
NODE *FS_node, *NF_node, *RS_node, *NR_node;
NODE *FILENAME_node, *OFS_node, *ORS_node, *OFMT_node;
NODE *CONVFMT_node;
NODE *ERRNO_node;
NODE *FNR_node, *RLENGTH_node, *RSTART_node, *SUBSEP_node;
NODE *ENVIRON_node, *IGNORECASE_node;
NODE *ARGC_node, *ARGV_node, *ARGIND_node;
NODE *FIELDWIDTHS_node;

long NF;
long NR;
long FNR;
int IGNORECASE;
char *RS;
char *OFS;
char *ORS;
char *OFMT;
char *CONVFMT;

/*
 * The parse tree and field nodes are stored here.  Parse_end is a dummy item
 * used to free up unneeded fields without freeing the program being run 
 */
int errcount = 0;	/* error counter, used by yyerror() */

/* The global null string */
NODE *Nnull_string;

/* The name the program was invoked under, for error messages */
const char *myname;

/* A block of AWK code to be run before running the program */
NODE *begin_block = 0;

/* A block of AWK code to be run after the last input file */
NODE *end_block = 0;

int exiting = 0;		/* Was an "exit" statement executed? */
int exit_val = 0;		/* optional exit value */

#if defined(YYDEBUG) || defined(DEBUG)
extern int yydebug;
#endif

struct src *srcfiles = NULL;		/* source file name(s) */
int numfiles = -1;		/* how many source files */

int do_unix = 0;		/* turn off gnu extensions */
int do_posix = 0;		/* turn off gnu and unix extensions */
int do_lint = 0;		/* provide warnings about questionable stuff */
int do_nostalgia = 0;		/* provide a blast from the past */

int in_begin_rule = 0;		/* we're in a BEGIN rule */
int in_end_rule = 0;		/* we're in a END rule */

int output_is_tty = 0;		/* control flushing of output */

extern char *version_string;	/* current version, for printing */

NODE *expression_value;

static struct option optab[] = {
	{ "compat",		no_argument,		& do_unix,	1 },
	{ "lint",		no_argument,		& do_lint,	1 },
	{ "posix",		no_argument,		& do_posix,	1 },
	{ "nostalgia",		no_argument,		& do_nostalgia,	1 },
	{ "copyleft",		no_argument,		NULL,		'C' },
	{ "copyright",		no_argument,		NULL,		'C' },
	{ "field-separator",	required_argument,	NULL,		'F' },
	{ "file",		required_argument,	NULL,		'f' },
	{ "assign",		required_argument,	NULL,		'v' },
	{ "version",		no_argument,		NULL,		'V' },
	{ "usage",		no_argument,		NULL,		'u' },
	{ "help",		no_argument,		NULL,		'u' },
	{ "source",		required_argument,	NULL,		's' },
#ifdef DEBUG
	{ "parsedebug",		no_argument,		NULL,		'D' },
#endif
	{ 0, 0, 0, 0 }
};

int
main(argc, argv)
int argc;
char **argv;
{
	int c;
	char *scan;
	/* the + on the front tells GNU getopt not to rearrange argv */
	const char *optlist = "+F:f:v:W:m:";
	int stopped_early = 0;
	int old_optind;
	extern int optind;
	extern int opterr;
	extern char *optarg;

#ifdef __EMX__
	_response(&argc, &argv);
	_wildcard(&argc, &argv);
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
#endif

	(void) signal(SIGFPE,  (SIGTYPE (*) P((int))) catchsig);
	(void) signal(SIGSEGV, (SIGTYPE (*) P((int))) catchsig);
#ifdef SIGBUS
	(void) signal(SIGBUS,  (SIGTYPE (*) P((int))) catchsig);
#endif

	myname = gawk_name(argv[0]);
        argv[0] = (char *)myname;
#ifdef VMS
	vms_arg_fixup(&argc, &argv); /* emulate redirection, expand wildcards */
#endif

	/* remove sccs gunk */
	if (strncmp(version_string, "@(#)", 4) == 0)
		version_string += 4;

	if (argc < 2)
		usage(1);

	/* initialize the null string */
	Nnull_string = make_string("", 0);
	Nnull_string->numbr = 0.0;
	Nnull_string->type = Node_val;
	Nnull_string->flags = (PERM|STR|STRING|NUM|NUMBER);

	/* Set up the special variables */
	/*
	 * Note that this must be done BEFORE arg parsing else -F
	 * breaks horribly 
	 */
	init_vars();

	/* worst case */
	emalloc(srcfiles, struct src *, argc * sizeof(struct src), "main");
	memset(srcfiles, '\0', argc * sizeof(struct src));

	/* Tell the regex routines how they should work. . . */
	resetup();

#ifdef fpsetmask
	fpsetmask(~0xff);
#endif
	/* we do error messages ourselves on invalid options */
	opterr = 0;

	/* option processing. ready, set, go! */
	for (optopt = 0, old_optind = 1;
	     (c = getopt_long(argc, argv, optlist, optab, NULL)) != EOF;
	     optopt = 0, old_optind = optind) {
		if (do_posix)
			opterr = 1;
		switch (c) {
		case 'F':
			cmdline_fs(optarg);
			break;

		case 'f':
			/*
			 * a la MKS awk, allow multiple -f options.
			 * this makes function libraries real easy.
			 * most of the magic is in the scanner.
			 */
			/* The following is to allow for whitespace at the end
			 * of a #! /bin/gawk line in an executable file
			 */
			scan = optarg;
			while (isspace(*scan))
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
			 *	-mf=nnn		set # fields, gawk ignores
			 *	-mr=nnn		set record length, ditto
			 */
			if (do_lint)
				warning("-m[fr] option irrelevant");
			if ((optarg[0] != 'r' && optarg[0] != 'f')
			    || optarg[1] != '=')
				warning("-m option usage: -m[fn]=nnn");
			break;

		case 'W':       /* gawk specific options */
			gawk_option(optarg);
			break;

		/* These can only come from long form options */
		case 'V':
			version();
			break;

		case 'C':
			copyleft();
			break;

		case 'u':
			usage(0);
			break;

		case 's':
			if (optarg[0] == '\0')
				warning("empty argument to --source ignored");
			else {
				srcfiles[++numfiles].stype = CMDLINE;
				srcfiles[numfiles].val = optarg;
			}
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
			    && (optopt == 0 || strchr(optlist, optopt) == NULL)) {
				/*
				 * can't just do optind--. In case of an
				 * option with >=2 letters, getopt_long
				 * won't have incremented optind.
				 */
				optind = old_optind;
				stopped_early = 1;
				goto out;
			} else if (optopt)
				/* Use 1003.2 required message format */
				fprintf (stderr,
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
		do_posix = 1;
		if (do_lint)
			warning(
	"environment variable `POSIXLY_CORRECT' set: turning on --posix");
	}

	/* POSIX compliance also implies no Unix extensions either */
	if (do_posix)
		do_unix = 1;

#ifdef DEBUG
	setbuf(stdout, (char *) NULL);	/* make debugging easier */
#endif
	if (isatty(fileno(stdout)))
		output_is_tty = 1;
	/* No -f or --source options, use next arg */
	if (numfiles == -1) {
		if (optind > argc - 1 || stopped_early) /* no args left or no program */
			usage(1);
		srcfiles[++numfiles].stype = CMDLINE;
		srcfiles[numfiles].val = argv[optind];
		optind++;
	}
	init_args(optind, argc, (char *) myname, argv);
	(void) tokexpand();

	/* Read in the program */
	if (yyparse() || errcount)
		exit(1);

	/* Set up the field variables */
	init_fields();

	if (do_lint && begin_block == NULL && expression_value == NULL
	     && end_block == NULL)
		warning("no program");

	if (begin_block) {
		in_begin_rule = 1;
		(void) interpret(begin_block);
	}
	in_begin_rule = 0;
	if (!exiting && (expression_value || end_block))
		do_input();
	if (end_block) {
		in_end_rule = 1;
		(void) interpret(end_block);
	}
	in_end_rule = 0;
	if (close_io() != 0 && exit_val == 0)
		exit_val = 1;
	exit(exit_val);		/* more portable */
	return exit_val;	/* to suppress warnings */
}

/* usage --- print usage information and exit */

static void
usage(exitval)
int exitval;
{
	const char *opt1 = " -f progfile [--]";
#if defined(MSDOS) || defined(OS2) || defined(VMS)
	const char *opt2 = " [--] \"program\"";
#else
	const char *opt2 = " [--] 'program'";
#endif
	const char *regops = " [POSIX or GNU style options]";

	fprintf(stderr, "Usage:\t%s%s%s file ...\n\t%s%s%s file ...\n",
		myname, regops, opt1, myname, regops, opt2);

	/* GNU long options info. Gack. */
	fputs("POSIX options:\t\tGNU long options:\n", stderr);
	fputs("\t-f progfile\t\t--file=progfile\n", stderr);
	fputs("\t-F fs\t\t\t--field-separator=fs\n", stderr);
	fputs("\t-v var=val\t\t--assign=var=val\n", stderr);
	fputs("\t-m[fr]=val\n", stderr);
	fputs("\t-W compat\t\t--compat\n", stderr);
	fputs("\t-W copyleft\t\t--copyleft\n", stderr);
	fputs("\t-W copyright\t\t--copyright\n", stderr);
	fputs("\t-W help\t\t\t--help\n", stderr);
	fputs("\t-W lint\t\t\t--lint\n", stderr);
#ifdef NOSTALGIA
	fputs("\t-W nostalgia\t\t--nostalgia\n", stderr);
#endif
#ifdef DEBUG
	fputs("\t-W parsedebug\t\t--parsedebug\n", stderr);
#endif
	fputs("\t-W posix\t\t--posix\n", stderr);
	fputs("\t-W source=program-text\t--source=program-text\n", stderr);
	fputs("\t-W usage\t\t--usage\n", stderr);
	fputs("\t-W version\t\t--version\n", stderr);
	exit(exitval);
}

static void
copyleft ()
{
	static char blurb_part1[] =
"Copyright (C) 1989, 1991, 1992, Free Software Foundation.\n\
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n";

	fputs(blurb_part1, stderr);
	fputs(blurb_part2, stderr);
	fputs(blurb_part3, stderr);
	fflush(stderr);
}

static void
cmdline_fs(str)
char *str;
{
	register NODE **tmp;
	/* int len = strlen(str); *//* don't do that - we want to
	                               avoid mismatched types */

	tmp = get_lhs(FS_node, (Func_ptr *) 0);
	unref(*tmp);
	/*
	 * Only if in full compatibility mode check for the stupid special
	 * case so -F\t works as documented in awk even though the shell
	 * hands us -Ft.  Bleah!
	 *
	 * Thankfully, Posix didn't propogate this "feature".
	 */
	if (str[0] == 't' && str[1] == '\0') {
		if (do_lint)
			warning("-Ft does not set FS to tab in POSIX awk");
		if (do_unix && ! do_posix)
			str[0] = '\t';
	}
	*tmp = make_str_node(str, strlen(str), SCAN); /* do process escapes */
	set_FS();
}

static void
init_args(argc0, argc, argv0, argv)
int argc0, argc;
char *argv0;
char **argv;
{
	int i, j;
	NODE **aptr;

	ARGV_node = install("ARGV", node(Nnull_string, Node_var, (NODE *)NULL));
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
{&NF_node,	"NF",		Node_NF,		0,	-1, set_NF },
{&FIELDWIDTHS_node, "FIELDWIDTHS", Node_FIELDWIDTHS,	"",	0,  0 },
{&NR_node,	"NR",		Node_NR,		0,	0,  set_NR },
{&FNR_node,	"FNR",		Node_FNR,		0,	0,  set_FNR },
{&FS_node,	"FS",		Node_FS,		" ",	0,  0 },
{&RS_node,	"RS",		Node_RS,		"\n",	0,  set_RS },
{&IGNORECASE_node, "IGNORECASE", Node_IGNORECASE,	0,	0,  set_IGNORECASE },
{&FILENAME_node, "FILENAME",	Node_var,		"",	0,  0 },
{&OFS_node,	"OFS",		Node_OFS,		" ",	0,  set_OFS },
{&ORS_node,	"ORS",		Node_ORS,		"\n",	0,  set_ORS },
{&OFMT_node,	"OFMT",		Node_OFMT,		"%.6g",	0,  set_OFMT },
{&CONVFMT_node,	"CONVFMT",	Node_CONVFMT,		"%.6g",	0,  set_CONVFMT },
{&RLENGTH_node, "RLENGTH",	Node_var,		0,	0,  0 },
{&RSTART_node,	"RSTART",	Node_var,		0,	0,  0 },
{&SUBSEP_node,	"SUBSEP",	Node_var,		"\034",	0,  0 },
{&ARGIND_node,	"ARGIND",	Node_var,		0,	0,  0 },
{&ERRNO_node,	"ERRNO",	Node_var,		0,	0,  0 },
{0,		0,		Node_illegal,		0,	0,  0 },
};

static void
init_vars()
{
	register struct varinit *vp;

	for (vp = varinit; vp->name; vp++) {
		*(vp->spec) = install((char *) vp->name,
		  node(vp->strval == 0 ? make_number(vp->numval)
				: make_string((char *) vp->strval,
					strlen(vp->strval)),
		       vp->type, (NODE *) NULL));
		if (vp->assign)
			(*(vp->assign))();
	}
}

void
load_environ()
{
#if !defined(MSDOS) && !defined(OS2) && !(defined(VMS) && defined(__DECC))
	extern char **environ;
#endif
	register char *var, *val;
	NODE **aptr;
	register int i;

	ENVIRON_node = install("ENVIRON", 
			node(Nnull_string, Node_var, (NODE *) NULL));
	for (i = 0; environ[i]; i++) {
		static char nullstr[] = "";

		var = environ[i];
		val = strchr(var, '=');
		if (val)
			*val++ = '\0';
		else
			val = nullstr;
		aptr = assoc_lookup(ENVIRON_node, tmp_string(var, strlen (var)));
		*aptr = make_string(val, strlen (val));
		(*aptr)->flags |= MAYBE_NUM;

		/* restore '=' so that system() gets a valid environment */
		if (val != nullstr)
			*--val = '=';
	}
}

/* Process a command-line assignment */
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
		badvar = 0;
		if (! isalpha(arg[0]) && arg[0] != '_')
			badvar = 1;
		else
			for (cp2 = arg+1; *cp2; cp2++)
				if (! isalnum(*cp2) && *cp2 != '_') {
					badvar = 1;
					break;
				}
		if (badvar)
			fatal("illegal name `%s' in variable assignment", arg);

		/*
		 * Recent versions of nawk expand escapes inside assignments.
		 * This makes sense, so we do it too.
		 */
		it = make_str_node(cp, strlen(cp), SCAN);
		it->flags |= MAYBE_NUM;
		var = variable(arg, 0);
		lhs = get_lhs(var, &after_assign);
		unref(*lhs);
		*lhs = it;
		if (after_assign)
			(*after_assign)();
		*--cp = '=';	/* restore original text of ARGV */
	}
	return cp;
}

static void
pre_assign(v)
char *v;
{
	if (!arg_assign(v)) {
		fprintf (stderr,
			"%s: '%s' argument to -v not in 'var=value' form\n",
				myname, v);
		usage(1);
	}
}

SIGTYPE
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
		msg("fatal error: internal error");
		/* fatal won't abort() if not compiled for debugging */
		abort();
	} else
		cant_happen();
	/* NOTREACHED */
}

/* gawk_option --- do gawk specific things */

static void
gawk_option(optstr)
char *optstr;
{
	char *cp;

	for (cp = optstr; *cp; cp++) {
		switch (*cp) {
		case ' ':
		case '\t':
		case ',':
			break;
		case 'v':
		case 'V':
			/* print version */
			if (strncasecmp(cp, "version", 7) != 0)
				goto unknown;
			else
				cp += 6;
			version();
			break;
		case 'c':
		case 'C':
			if (strncasecmp(cp, "copyright", 9) == 0) {
				cp += 8;
				copyleft();
			} else if (strncasecmp(cp, "copyleft", 8) == 0) {
				cp += 7;
				copyleft();
			} else if (strncasecmp(cp, "compat", 6) == 0) {
				cp += 5;
				do_unix = 1;
			} else
				goto unknown;
			break;
		case 'n':
		case 'N':
			/*
			 * Undocumented feature,
			 * inspired by nostalgia, and a T-shirt
			 */
			if (strncasecmp(cp, "nostalgia", 9) != 0)
				goto unknown;
			nostalgia();
			break;
		case 'p':
		case 'P':
#ifdef DEBUG
			if (strncasecmp(cp, "parsedebug", 10) == 0) {
				cp += 9;
				yydebug = 2;
				break;
			}
#endif
			if (strncasecmp(cp, "posix", 5) != 0)
				goto unknown;
			cp += 4;
			do_posix = do_unix = 1;
			break;
		case 'l':
		case 'L':
			if (strncasecmp(cp, "lint", 4) != 0)
				goto unknown;
			cp += 3;
			do_lint = 1;
			break;
		case 'H':
		case 'h':
			if (strncasecmp(cp, "help", 4) != 0)
				goto unknown;
			cp += 3;
			usage(0);
			break;
		case 'U':
		case 'u':
			if (strncasecmp(cp, "usage", 5) != 0)
				goto unknown;
			cp += 4;
			usage(0);
			break;
		case 's':
		case 'S':
			if (strncasecmp(cp, "source=", 7) != 0)
				goto unknown;
			cp += 7;
			if (cp[0] == '\0')
				warning("empty argument to -Wsource ignored");
			else {
				srcfiles[++numfiles].stype = CMDLINE;
				srcfiles[numfiles].val = cp;
				return;
			}
			break;
		default:
		unknown:
			fprintf(stderr, "'%c' -- unknown option, ignored\n",
				*cp);
			break;
		}
	}
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
	fprintf(stderr, "%s, patchlevel %d\n", version_string, PATCHLEVEL);
	/* per GNU coding standards, exit successfully, do nothing else */
	exit(0);
}

/* this mess will improve in 2.16 */
char *
gawk_name(filespec)
char *filespec;
{
	char *p;
	
#ifdef VMS	/* "device:[root.][directory.subdir]GAWK.EXE;n" -> "GAWK" */
	char *q;

	p = strrchr(filespec, ']');  /* directory punctuation */
	q = strrchr(filespec, '>');  /* alternate <international> punct */

	if (p == NULL || q > p) p = q;
	p = strdup(p == NULL ? filespec : (p + 1));
	if ((q = strrchr(p, '.')) != NULL)  *q = '\0';  /* strip .typ;vers */

	return p;
#endif /*VMS*/

#if defined(MSDOS) || defined(OS2) || defined(atarist)
	char *q;

	for (p = filespec; (p = strchr(p, '\\')); *p = '/')
		;
	p = filespec;
	if ((q = strrchr(p, '/')))
		p = q + 1;
	if ((q = strchr(p, '.')))
		*q = '\0';
	strlwr(p);

	return (p == NULL ? filespec : p);
#endif /* MSDOS || atarist */

	/* "path/name" -> "name" */
	p = strrchr(filespec, '/');
	return (p == NULL ? filespec : p + 1);
}
