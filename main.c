/*	$NetBSD: main.c,v 1.421 2020/11/01 00:24:57 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

/*-
 * main.c --
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine	Takes a line of arguments, breaks them and
 *				treats them as if they were given when first
 *				invoked. Used by the parse module to implement
 *				the .MFLAGS target.
 *
 *	Error			Print a tagged error message. The global
 *				MAKE variable must have been defined. This
 *				takes a format string and optional arguments
 *				for it.
 *
 *	Fatal			Print an error message and exit. Also takes
 *				a format string and arguments for it.
 *
 *	Punt			Aborts all jobs and exits with a message. Also
 *				takes a format string and arguments for it.
 *
 *	Finish			Finish things up by printing the number of
 *				errors which occurred, as passed to it, and
 *				exiting.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#if defined(MAKE_NATIVE) && defined(HAVE_SYSCTL)
#include <sys/sysctl.h>
#endif
#include <sys/utsname.h>
#include "wait.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#include "make.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"
#include "trace.h"

/*	"@(#)main.c	8.3 (Berkeley) 3/19/94"	*/
MAKE_RCSID("$NetBSD: main.c,v 1.421 2020/11/01 00:24:57 rillig Exp $");
#if defined(MAKE_NATIVE) && !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1988, 1989, 1990, 1993 "
	    "The Regents of the University of California.  "
	    "All rights reserved.");
#endif

#ifndef	DEFMAXLOCAL
#define	DEFMAXLOCAL DEFMAXJOBS
#endif

#ifndef __arraycount
# define __arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#endif

CmdOpts opts;
time_t			now;		/* Time at start of make */
GNode			*DEFAULT;	/* .DEFAULT node */
Boolean			allPrecious;	/* .PRECIOUS given on line by itself */
Boolean			deleteOnError;	/* .DELETE_ON_ERROR: set */

static int		maxJobTokens;	/* -j argument */
Boolean			enterFlagObj;	/* -w and objdir != srcdir */

Boolean			oldVars;	/* variable substitution style */
static int jp_0 = -1, jp_1 = -1;	/* ends of parent job pipe */
Boolean			doing_depend;	/* Set while reading .depend */
static Boolean		jobsRunning;	/* TRUE if the jobs might be running */
static const char *	tracefile;
static int		ReadMakefile(const char *);
static void		usage(void) MAKE_ATTR_DEAD;
static void		purge_cached_realpaths(void);

static Boolean		ignorePWD;	/* if we use -C, PWD is meaningless */
static char objdir[MAXPATHLEN + 1];	/* where we chdir'ed to */
char curdir[MAXPATHLEN + 1];		/* Startup directory */
char *progname;				/* the program name */
char *makeDependfile;
pid_t myPid;
int makelevel;

Boolean forceJobs = FALSE;
static int errors = 0;

/*
 * On some systems MACHINE is defined as something other than
 * what we want.
 */
#ifdef FORCE_MACHINE
# undef MACHINE
# define MACHINE FORCE_MACHINE
#endif

extern SearchPath *parseIncPath;

/*
 * For compatibility with the POSIX version of MAKEFLAGS that includes
 * all the options with out -, convert flags to -f -l -a -g -s.
 */
static char *
explode(const char *flags)
{
    size_t len;
    char *nf, *st;
    const char *f;

    if (flags == NULL)
	return NULL;

    for (f = flags; *f; f++)
	if (!ch_isalpha(*f))
	    break;

    if (*f)
	return bmake_strdup(flags);

    len = strlen(flags);
    st = nf = bmake_malloc(len * 3 + 1);
    while (*flags) {
	*nf++ = '-';
	*nf++ = *flags++;
	*nf++ = ' ';
    }
    *nf = '\0';
    return st;
}

static void
parse_debug_option_F(const char *modules)
{
    const char *mode;
    size_t len;
    char *fname;

    if (opts.debug_file != stdout && opts.debug_file != stderr)
	fclose(opts.debug_file);

    if (*modules == '+') {
	modules++;
	mode = "a";
    } else
	mode = "w";

    if (strcmp(modules, "stdout") == 0) {
	opts.debug_file = stdout;
	return;
    }
    if (strcmp(modules, "stderr") == 0) {
	opts.debug_file = stderr;
	return;
    }

    len = strlen(modules);
    fname = bmake_malloc(len + 20);
    memcpy(fname, modules, len + 1);

    /* Let the filename be modified by the pid */
    if (strcmp(fname + len - 3, ".%d") == 0)
	snprintf(fname + len - 2, 20, "%d", getpid());

    opts.debug_file = fopen(fname, mode);
    if (!opts.debug_file) {
	fprintf(stderr, "Cannot open debug file %s\n",
		fname);
	usage();
    }
    free(fname);
}

static void
parse_debug_options(const char *argvalue)
{
	const char *modules;

	for (modules = argvalue; *modules; ++modules) {
		switch (*modules) {
		case '0':	/* undocumented, only intended for tests */
			opts.debug &= DEBUG_LINT;
			break;
		case 'A':
			opts.debug = ~(0|DEBUG_LINT);
			break;
		case 'a':
			opts.debug |= DEBUG_ARCH;
			break;
		case 'C':
			opts.debug |= DEBUG_CWD;
			break;
		case 'c':
			opts.debug |= DEBUG_COND;
			break;
		case 'd':
			opts.debug |= DEBUG_DIR;
			break;
		case 'e':
			opts.debug |= DEBUG_ERROR;
			break;
		case 'f':
			opts.debug |= DEBUG_FOR;
			break;
		case 'g':
			if (modules[1] == '1') {
				opts.debug |= DEBUG_GRAPH1;
				++modules;
			}
			else if (modules[1] == '2') {
				opts.debug |= DEBUG_GRAPH2;
				++modules;
			}
			else if (modules[1] == '3') {
				opts.debug |= DEBUG_GRAPH3;
				++modules;
			}
			break;
		case 'h':
			opts.debug |= DEBUG_HASH;
			break;
		case 'j':
			opts.debug |= DEBUG_JOB;
			break;
		case 'L':
			opts.debug |= DEBUG_LINT;
			break;
		case 'l':
			opts.debug |= DEBUG_LOUD;
			break;
		case 'M':
			opts.debug |= DEBUG_META;
			break;
		case 'm':
			opts.debug |= DEBUG_MAKE;
			break;
		case 'n':
			opts.debug |= DEBUG_SCRIPT;
			break;
		case 'p':
			opts.debug |= DEBUG_PARSE;
			break;
		case 's':
			opts.debug |= DEBUG_SUFF;
			break;
		case 't':
			opts.debug |= DEBUG_TARG;
			break;
		case 'V':
			opts.debugVflag = TRUE;
			break;
		case 'v':
			opts.debug |= DEBUG_VAR;
			break;
		case 'x':
			opts.debug |= DEBUG_SHELL;
			break;
		case 'F':
			parse_debug_option_F(modules + 1);
			goto debug_setbuf;
		default:
			(void)fprintf(stderr,
			    "%s: illegal argument to d option -- %c\n",
			    progname, *modules);
			usage();
		}
	}
debug_setbuf:
	/*
	 * Make the debug_file unbuffered, and make
	 * stdout line buffered (unless debugfile == stdout).
	 */
	setvbuf(opts.debug_file, NULL, _IONBF, 0);
	if (opts.debug_file != stdout) {
		setvbuf(stdout, NULL, _IOLBF, 0);
	}
}

/*
 * does path contain any relative components
 */
static Boolean
is_relpath(const char *path)
{
	const char *cp;

	if (path[0] != '/')
		return TRUE;
	cp = path;
	while ((cp = strstr(cp, "/.")) != NULL) {
		cp += 2;
		if (cp[0] == '/' || cp[0] == '\0')
			return TRUE;
		else if (cp[0] == '.') {
			if (cp[1] == '/' || cp[1] == '\0')
				return TRUE;
		}
	}
	return FALSE;
}

static void
MainParseArgChdir(const char *argvalue)
{
	struct stat sa, sb;

	if (chdir(argvalue) == -1) {
		(void)fprintf(stderr, "%s: chdir %s: %s\n",
		    progname, argvalue, strerror(errno));
		exit(1);
	}
	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "%s: %s.\n", progname, strerror(errno));
		exit(2);
	}
	if (!is_relpath(argvalue) &&
	    stat(argvalue, &sa) != -1 &&
	    stat(curdir, &sb) != -1 &&
	    sa.st_ino == sb.st_ino &&
	    sa.st_dev == sb.st_dev)
		strncpy(curdir, argvalue, MAXPATHLEN);
	ignorePWD = TRUE;
}

static void
MainParseArgJobsInternal(const char *argvalue)
{
	if (sscanf(argvalue, "%d,%d", &jp_0, &jp_1) != 2) {
		(void)fprintf(stderr,
			      "%s: internal error -- J option malformed (%s)\n",
			      progname, argvalue);
		usage();
	}
	if ((fcntl(jp_0, F_GETFD, 0) < 0) ||
	    (fcntl(jp_1, F_GETFD, 0) < 0)) {
#if 0
		(void)fprintf(stderr,
		    "%s: ###### warning -- J descriptors were closed!\n",
		    progname);
		exit(2);
#endif
		jp_0 = -1;
		jp_1 = -1;
		opts.compatMake = TRUE;
	} else {
		Var_Append(MAKEFLAGS, "-J", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
	}
}

static void
MainParseArgJobs(const char *argvalue)
{
	char *p;

	forceJobs = TRUE;
	opts.maxJobs = (int)strtol(argvalue, &p, 0);
	if (*p != '\0' || opts.maxJobs < 1) {
		(void)fprintf(stderr,
		    "%s: illegal argument to -j -- must be positive integer!\n",
		    progname);
		exit(1);	/* XXX: why not 2? */
	}
	Var_Append(MAKEFLAGS, "-j", VAR_GLOBAL);
	Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
	Var_Set(".MAKE.JOBS", argvalue, VAR_GLOBAL);
	maxJobTokens = opts.maxJobs;
}

static void
MainParseArgSysInc(const char *argvalue)
{
	/* look for magic parent directory search string */
	if (strncmp(".../", argvalue, 4) == 0) {
		char *found_path = Dir_FindHereOrAbove(curdir, argvalue + 4);
		if (found_path == NULL)
			return;
		(void)Dir_AddDir(sysIncPath, found_path);
		free(found_path);
	} else {
		(void)Dir_AddDir(sysIncPath, argvalue);
	}
	Var_Append(MAKEFLAGS, "-m", VAR_GLOBAL);
	Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
}

static Boolean
MainParseArg(char c, const char *argvalue)
{
	switch (c) {
	case '\0':
		break;
	case 'B':
		opts.compatMake = TRUE;
		Var_Append(MAKEFLAGS, "-B", VAR_GLOBAL);
		Var_Set(MAKE_MODE, "compat", VAR_GLOBAL);
		break;
	case 'C':
		MainParseArgChdir(argvalue);
		break;
	case 'D':
		if (argvalue[0] == '\0') return FALSE;
		Var_Set(argvalue, "1", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, "-D", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
		break;
	case 'I':
		Parse_AddIncludeDir(argvalue);
		Var_Append(MAKEFLAGS, "-I", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
		break;
	case 'J':
		MainParseArgJobsInternal(argvalue);
		break;
	case 'N':
		opts.noExecute = TRUE;
		opts.noRecursiveExecute = TRUE;
		Var_Append(MAKEFLAGS, "-N", VAR_GLOBAL);
		break;
	case 'S':
		opts.keepgoing = FALSE;
		Var_Append(MAKEFLAGS, "-S", VAR_GLOBAL);
		break;
	case 'T':
		tracefile = bmake_strdup(argvalue);
		Var_Append(MAKEFLAGS, "-T", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
		break;
	case 'V':
	case 'v':
		opts.printVars = c == 'v' ? EXPAND_VARS : COMPAT_VARS;
		Lst_Append(opts.variables, bmake_strdup(argvalue));
		/* XXX: Why always -V? */
		Var_Append(MAKEFLAGS, "-V", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
		break;
	case 'W':
		opts.parseWarnFatal = TRUE;
		break;
	case 'X':
		opts.varNoExportEnv = TRUE;
		Var_Append(MAKEFLAGS, "-X", VAR_GLOBAL);
		break;
	case 'd':
		/* If '-d-opts' don't pass to children */
		if (argvalue[0] == '-')
			argvalue++;
		else {
			Var_Append(MAKEFLAGS, "-d", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, argvalue, VAR_GLOBAL);
		}
		parse_debug_options(argvalue);
		break;
	case 'e':
		opts.checkEnvFirst = TRUE;
		Var_Append(MAKEFLAGS, "-e", VAR_GLOBAL);
		break;
	case 'f':
		Lst_Append(opts.makefiles, bmake_strdup(argvalue));
		break;
	case 'i':
		opts.ignoreErrors = TRUE;
		Var_Append(MAKEFLAGS, "-i", VAR_GLOBAL);
		break;
	case 'j':
		MainParseArgJobs(argvalue);
		break;
	case 'k':
		opts.keepgoing = TRUE;
		Var_Append(MAKEFLAGS, "-k", VAR_GLOBAL);
		break;
	case 'm':
		MainParseArgSysInc(argvalue);
		break;
	case 'n':
		opts.noExecute = TRUE;
		Var_Append(MAKEFLAGS, "-n", VAR_GLOBAL);
		break;
	case 'q':
		opts.queryFlag = TRUE;
		/* Kind of nonsensical, wot? */
		Var_Append(MAKEFLAGS, "-q", VAR_GLOBAL);
		break;
	case 'r':
		opts.noBuiltins = TRUE;
		Var_Append(MAKEFLAGS, "-r", VAR_GLOBAL);
		break;
	case 's':
		opts.beSilent = TRUE;
		Var_Append(MAKEFLAGS, "-s", VAR_GLOBAL);
		break;
	case 't':
		opts.touchFlag = TRUE;
		Var_Append(MAKEFLAGS, "-t", VAR_GLOBAL);
		break;
	case 'w':
		opts.enterFlag = TRUE;
		Var_Append(MAKEFLAGS, "-w", VAR_GLOBAL);
		break;
	default:
	case '?':
		usage();
	}
	return TRUE;
}

/* Parse the given arguments.  Called from main() and from
 * Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 * The arguments must be treated as read-only and will be freed after the
 * call.
 *
 * XXX: Deal with command line overriding .MAKEFLAGS in makefile */
static void
MainParseArgs(int argc, char **argv)
{
	char c;
	int arginc;
	char *argvalue;
	char *optscan;
	Boolean inOption, dashDash = FALSE;

	const char *optspecs = "BC:D:I:J:NST:V:WXd:ef:ij:km:nqrstv:w";
/* Can't actually use getopt(3) because rescanning is not portable */

rearg:
	inOption = FALSE;
	optscan = NULL;
	while (argc > 1) {
		const char *optspec;
		if (!inOption)
			optscan = argv[1];
		c = *optscan++;
		arginc = 0;
		if (inOption) {
			if (c == '\0') {
				++argv;
				--argc;
				inOption = FALSE;
				continue;
			}
		} else {
			if (c != '-' || dashDash)
				break;
			inOption = TRUE;
			c = *optscan++;
		}
		/* '-' found at some earlier point */
		optspec = strchr(optspecs, c);
		if (c != '\0' && optspec != NULL && optspec[1] == ':') {
			/* -<something> found, and <something> should have an arg */
			inOption = FALSE;
			arginc = 1;
			argvalue = optscan;
			if (*argvalue == '\0') {
				if (argc < 3)
					goto noarg;
				argvalue = argv[2];
				arginc = 2;
			}
		} else {
			argvalue = NULL;
		}
		switch (c) {
		case '\0':
			arginc = 1;
			inOption = FALSE;
			break;
		case '-':
			dashDash = TRUE;
			break;
		default:
			if (!MainParseArg(c, argvalue))
				goto noarg;
		}
		argv += arginc;
		argc -= arginc;
	}

	oldVars = TRUE;

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (; argc > 1; ++argv, --argc) {
		VarAssign var;
		if (Parse_IsVar(argv[1], &var)) {
			Parse_DoVar(&var, VAR_CMDLINE);
		} else {
			if (!*argv[1])
				Punt("illegal (null) argument.");
			if (*argv[1] == '-' && !dashDash)
				goto rearg;
			Lst_Append(opts.create, bmake_strdup(argv[1]));
		}
	}

	return;
noarg:
	(void)fprintf(stderr, "%s: option requires an argument -- %c\n",
	    progname, c);
	usage();
}

/* Break a line of arguments into words and parse them.
 *
 * Used when a .MFLAGS or .MAKEFLAGS target is encountered during parsing and
 * by main() when reading the MAKEFLAGS environment variable. */
void
Main_ParseArgLine(const char *line)
{
	Words words;
	void *p1;
	const char *argv0 = Var_Value(".MAKE", VAR_GLOBAL, &p1);
	char *buf;

	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

#ifndef POSIX
	{
		/*
		 * $MAKE may simply be naming the make(1) binary
		 */
		char *cp;

		if (!(cp = strrchr(line, '/')))
			cp = line;
		if ((cp = strstr(cp, "make")) &&
		    strcmp(cp, "make") == 0)
			return;
	}
#endif
	buf = str_concat3(argv0, " ", line);
	free(p1);

	words = Str_Words(buf, TRUE);
	if (words.words == NULL) {
		Error("Unterminated quoted string [%s]", buf);
		free(buf);
		return;
	}
	free(buf);
	MainParseArgs((int)words.len, words.words);

	Words_Free(words);
}

Boolean
Main_SetObjdir(const char *fmt, ...)
{
	struct stat sb;
	char *path;
	char buf[MAXPATHLEN + 1];
	char buf2[MAXPATHLEN + 1];
	Boolean rc = FALSE;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(path = buf, MAXPATHLEN, fmt, ap);
	va_end(ap);

	if (path[0] != '/') {
		snprintf(buf2, MAXPATHLEN, "%s/%s", curdir, path);
		path = buf2;
	}

	/* look for the directory and try to chdir there */
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		/* if not .CURDIR it must be writable */
		if ((strcmp(path, curdir) != 0 && access(path, W_OK) != 0) ||
		    chdir(path)) {
			(void)fprintf(stderr, "make warning: %s: %s.\n",
				      path, strerror(errno));
		} else {
			snprintf(objdir, sizeof objdir, "%s", path);
			Var_Set(".OBJDIR", objdir, VAR_GLOBAL);
			setenv("PWD", objdir, 1);
			Dir_InitDot();
			purge_cached_realpaths();
			rc = TRUE;
			if (opts.enterFlag && strcmp(objdir, curdir) != 0)
				enterFlagObj = TRUE;
		}
	}

	return rc;
}

static Boolean
Main_SetVarObjdir(const char *var, const char *suffix)
{
	void *path_freeIt;
	const char *path = Var_Value(var, VAR_CMDLINE, &path_freeIt);
	const char *xpath;
	char *xpath_freeIt;

	if (path == NULL || path[0] == '\0') {
		bmake_free(path_freeIt);
		return FALSE;
	}

	/* expand variable substitutions */
	xpath = path;
	xpath_freeIt = NULL;
	if (strchr(path, '$') != 0) {
		(void)Var_Subst(path, VAR_GLOBAL, VARE_WANTRES, &xpath_freeIt);
		/* TODO: handle errors */
		xpath = xpath_freeIt;
	}

	(void)Main_SetObjdir("%s%s", xpath, suffix);

	bmake_free(xpath_freeIt);
	bmake_free(path_freeIt);
	return TRUE;
}

/* Read and parse the makefile.
 * Return TRUE if reading the makefile succeeded. */
static int
ReadMakefileSucceeded(void *fname, void *unused)
{
	return ReadMakefile(fname) == 0;
}

int
str2Lst_Append(StringList *lp, char *str, const char *sep)
{
    char *cp;
    int n;

    if (!sep)
	sep = " \t";

    for (n = 0, cp = strtok(str, sep); cp; cp = strtok(NULL, sep)) {
	Lst_Append(lp, cp);
	n++;
    }
    return n;
}

#ifdef SIGINFO
/*ARGSUSED*/
static void
siginfo(int signo MAKE_ATTR_UNUSED)
{
	char dir[MAXPATHLEN];
	char str[2 * MAXPATHLEN];
	int len;
	if (getcwd(dir, sizeof(dir)) == NULL)
		return;
	len = snprintf(str, sizeof(str), "%s: Working in: %s\n", progname, dir);
	if (len > 0)
		(void)write(STDERR_FILENO, str, (size_t)len);
}
#endif

/*
 * Allow makefiles some control over the mode we run in.
 */
void
MakeMode(const char *mode)
{
    char *mode_freeIt = NULL;

    if (mode == NULL) {
	(void)Var_Subst("${" MAKE_MODE ":tl}",
		  VAR_GLOBAL, VARE_WANTRES, &mode_freeIt);
	/* TODO: handle errors */
	mode = mode_freeIt;
    }

    if (mode[0] != '\0') {
	if (strstr(mode, "compat")) {
	    opts.compatMake = TRUE;
	    forceJobs = FALSE;
	}
#if USE_META
	if (strstr(mode, "meta"))
	    meta_mode_init(mode);
#endif
    }

    free(mode_freeIt);
}

static void
PrintVar(const char *varname, Boolean expandVars)
{
	if (strchr(varname, '$')) {
		char *evalue;
		(void)Var_Subst(varname, VAR_GLOBAL, VARE_WANTRES, &evalue);
		/* TODO: handle errors */
		printf("%s\n", evalue);
		bmake_free(evalue);

	} else if (expandVars) {
		char *expr = str_concat3("${", varname, "}");
		char *evalue;
		(void)Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES, &evalue);
		/* TODO: handle errors */
		free(expr);
		printf("%s\n", evalue);
		bmake_free(evalue);

	} else {
		void *freeIt;
		const char *value = Var_Value(varname, VAR_GLOBAL, &freeIt);
		printf("%s\n", value ? value : "");
		bmake_free(freeIt);
	}
}

static void
doPrintVars(void)
{
	StringListNode *ln;
	Boolean expandVars;

	if (opts.printVars == EXPAND_VARS)
		expandVars = TRUE;
	else if (opts.debugVflag)
		expandVars = FALSE;
	else
		expandVars = getBoolean(".MAKE.EXPAND_VARIABLES", FALSE);

	for (ln = opts.variables->first; ln != NULL; ln = ln->next) {
		const char *varname = ln->datum;
		PrintVar(varname, expandVars);
	}
}

static Boolean
runTargets(void)
{
	GNodeList *targs; /* target nodes to create -- passed to Make_Init */
	Boolean outOfDate;	/* FALSE if all targets up to date */

	/*
	 * Have now read the entire graph and need to make a list of
	 * targets to create. If none was given on the command line,
	 * we consult the parsing module to find the main target(s)
	 * to create.
	 */
	if (Lst_IsEmpty(opts.create))
		targs = Parse_MainName();
	else
		targs = Targ_FindList(opts.create);

	if (!opts.compatMake) {
		/*
		 * Initialize job module before traversing the graph
		 * now that any .BEGIN and .END targets have been read.
		 * This is done only if the -q flag wasn't given
		 * (to prevent the .BEGIN from being executed should
		 * it exist).
		 */
		if (!opts.queryFlag) {
			Job_Init();
			jobsRunning = TRUE;
		}

		/* Traverse the graph, checking on all the targets */
		outOfDate = Make_Run(targs);
	} else {
		/*
		 * Compat_Init will take care of creating all the
		 * targets as well as initializing the module.
		 */
		Compat_Run(targs);
		outOfDate = FALSE;
	}
	Lst_Free(targs);
	return outOfDate;
}

/*
 * Set up the .TARGETS variable to contain the list of targets to be
 * created. If none specified, make the variable empty -- the parser
 * will fill the thing in with the default or .MAIN target.
 */
static void
InitVarTargets(void)
{
	StringListNode *ln;

	if (Lst_IsEmpty(opts.create)) {
		Var_Set(".TARGETS", "", VAR_GLOBAL);
		return;
	}

	for (ln = opts.create->first; ln != NULL; ln = ln->next) {
		char *name = ln->datum;
		Var_Append(".TARGETS", name, VAR_GLOBAL);
	}
}

static void
InitRandom(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srandom((unsigned int)(tv.tv_sec + tv.tv_usec));
}

static const char *
init_machine(const struct utsname *utsname)
{
#ifdef FORCE_MACHINE
	const char *machine = FORCE_MACHINE;
#else
    	const char *machine = getenv("MACHINE");
#endif
	if (machine != NULL)
		return machine;

#ifdef MAKE_NATIVE
	return utsname->machine;
#else
#ifdef MAKE_MACHINE
	return MAKE_MACHINE;
#else
	return "unknown";
#endif
#endif
}

static const char *
init_machine_arch(void)
{
	const char *env = getenv("MACHINE_ARCH");
	if (env != NULL)
		return env;

#if defined(MAKE_NATIVE) && defined(CTL_HW)
	{
		struct utsname utsname;
		static char machine_arch_buf[sizeof(utsname.machine)];
		const int mib[2] = { CTL_HW, HW_MACHINE_ARCH };
		size_t len = sizeof(machine_arch_buf);

		if (sysctl(mib, __arraycount(mib), machine_arch_buf,
			&len, NULL, 0) < 0) {
		    (void)fprintf(stderr, "%s: sysctl failed (%s).\n", progname,
			strerror(errno));
		    exit(2);
		}

		return machine_arch_buf;
	}
#else
#ifndef MACHINE_ARCH
#ifdef MAKE_MACHINE_ARCH
	return MAKE_MACHINE_ARCH;
#else
	return "unknown";
#endif
#else
	return MACHINE_ARCH;
#endif
#endif
}

#ifndef NO_PWD_OVERRIDE
/*
 * All this code is so that we know where we are when we start up
 * on a different machine with pmake.
 *
 * Overriding getcwd() with $PWD totally breaks MAKEOBJDIRPREFIX
 * since the value of curdir can vary depending on how we got
 * here.  Ie sitting at a shell prompt (shell that provides $PWD)
 * or via subdir.mk in which case its likely a shell which does
 * not provide it.
 *
 * So, to stop it breaking this case only, we ignore PWD if
 * MAKEOBJDIRPREFIX is set or MAKEOBJDIR contains a variable expression.
 */
static void
HandlePWD(const struct stat *curdir_st)
{
	char *pwd;
	void *prefix_freeIt, *makeobjdir_freeIt;
	const char *makeobjdir;
	struct stat pwd_st;

	if (ignorePWD || (pwd = getenv("PWD")) == NULL)
		return;

	if (Var_Value("MAKEOBJDIRPREFIX", VAR_CMDLINE, &prefix_freeIt) != NULL) {
		bmake_free(prefix_freeIt);
		return;
	}

	makeobjdir = Var_Value("MAKEOBJDIR", VAR_CMDLINE, &makeobjdir_freeIt);
	if (makeobjdir != NULL && strchr(makeobjdir, '$') != NULL)
		goto ignore_pwd;

	if (stat(pwd, &pwd_st) == 0 &&
	    curdir_st->st_ino == pwd_st.st_ino &&
	    curdir_st->st_dev == pwd_st.st_dev)
		(void)strncpy(curdir, pwd, MAXPATHLEN);

ignore_pwd:
	bmake_free(makeobjdir_freeIt);
}
#endif

/*
 * Find the .OBJDIR.  If MAKEOBJDIRPREFIX, or failing that,
 * MAKEOBJDIR is set in the environment, try only that value
 * and fall back to .CURDIR if it does not exist.
 *
 * Otherwise, try _PATH_OBJDIR.MACHINE-MACHINE_ARCH, _PATH_OBJDIR.MACHINE,
 * and * finally _PATH_OBJDIRPREFIX`pwd`, in that order.  If none
 * of these paths exist, just use .CURDIR.
 */
static void
InitObjdir(const char *machine, const char *machine_arch)
{
	Dir_InitDir(curdir);
	(void)Main_SetObjdir("%s", curdir);

	if (!Main_SetVarObjdir("MAKEOBJDIRPREFIX", curdir) &&
	    !Main_SetVarObjdir("MAKEOBJDIR", "") &&
	    !Main_SetObjdir("%s.%s-%s", _PATH_OBJDIR, machine, machine_arch) &&
	    !Main_SetObjdir("%s.%s", _PATH_OBJDIR, machine) &&
	    !Main_SetObjdir("%s", _PATH_OBJDIR))
		(void)Main_SetObjdir("%s%s", _PATH_OBJDIRPREFIX, curdir);
}

/* get rid of resource limit on file descriptors */
static void
UnlimitFiles(void)
{
#if defined(MAKE_NATIVE) || (defined(HAVE_SETRLIMIT) && defined(RLIMIT_NOFILE))
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
	    rl.rlim_cur != rl.rlim_max) {
		rl.rlim_cur = rl.rlim_max;
		(void)setrlimit(RLIMIT_NOFILE, &rl);
	}
#endif
}

static void
CmdOpts_Init(void)
{
	opts.compatMake = FALSE;	/* No compat mode */
	opts.debug = 0;			/* No debug verbosity, please. */
	/* opts.debug_file has been initialized earlier */
	opts.debugVflag = FALSE;
	opts.checkEnvFirst = FALSE;
	opts.makefiles = Lst_New();
	opts.ignoreErrors = FALSE;	/* Pay attention to non-zero returns */
	opts.maxJobs = DEFMAXLOCAL;	/* Set default local max concurrency */
	opts.keepgoing = FALSE;		/* Stop on error */
	opts.noRecursiveExecute = FALSE; /* Execute all .MAKE targets */
	opts.noExecute = FALSE;		/* Execute all commands */
	opts.queryFlag = FALSE;		/* This is not just a check-run */
	opts.noBuiltins = FALSE;	/* Read the built-in rules */
	opts.beSilent = FALSE;		/* Print commands as executed */
	opts.touchFlag = FALSE;		/* Actually update targets */
	opts.printVars = 0;
	opts.variables = Lst_New();
	opts.parseWarnFatal = FALSE;
	opts.enterFlag = FALSE;
	opts.varNoExportEnv = FALSE;
	opts.create = Lst_New();
}

/* Initialize MAKE and .MAKE to the path of the executable, so that it can be
 * found by execvp(3) and the shells, even after a chdir.
 *
 * If it's a relative path and contains a '/', resolve it to an absolute path.
 * Otherwise keep it as is, assuming it will be found in the PATH. */
static void
InitVarMake(const char *argv0)
{
	const char *make = argv0;

	if (argv0[0] != '/' && strchr(argv0, '/') != NULL) {
		char pathbuf[MAXPATHLEN];
		const char *abs = cached_realpath(argv0, pathbuf);
		struct stat st;
		if (abs != NULL && abs[0] == '/' && stat(make, &st) == 0)
			make = abs;
	}

	Var_Set("MAKE", make, VAR_GLOBAL);
	Var_Set(".MAKE", make, VAR_GLOBAL);
}

static void
InitDefSysIncPath(char *syspath)
{
	static char defsyspath[] = _PATH_DEFSYSPATH;
	char *start, *cp;

	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	/* XXX: mismatch: the -m option sets sysIncPath, not syspath */
	if (syspath == NULL || syspath[0] == '\0')
		syspath = defsyspath;
	else
		syspath = bmake_strdup(syspath);

	for (start = syspath; *start != '\0'; start = cp) {
		for (cp = start; *cp != '\0' && *cp != ':'; cp++)
			continue;
		if (*cp == ':') {
			*cp++ = '\0';
		}
		/* look for magic parent directory search string */
		if (strncmp(".../", start, 4) != 0) {
			(void)Dir_AddDir(defSysIncPath, start);
		} else {
			char *dir = Dir_FindHereOrAbove(curdir, start + 4);
			if (dir != NULL) {
				(void)Dir_AddDir(defSysIncPath, dir);
				free(dir);
			}
		}
	}

	if (syspath != defsyspath)
		free(syspath);
}

static void
ReadBuiltinRules(void)
{
	StringList *sysMkPath = Lst_New();
	Dir_Expand(_PATH_DEFSYSMK,
		   Lst_IsEmpty(sysIncPath) ? defSysIncPath : sysIncPath,
		   sysMkPath);
	if (Lst_IsEmpty(sysMkPath))
		Fatal("%s: no system rules (%s).", progname, _PATH_DEFSYSMK);
	if (!Lst_ForEachUntil(sysMkPath, ReadMakefileSucceeded, NULL))
		Fatal("%s: cannot open %s.", progname,
		      (char *)sysMkPath->first->datum);
	/* XXX: sysMkPath is not freed */
}

static void
InitMaxJobs(void)
{
	char *value;
	int n;

	if (forceJobs || opts.compatMake ||
	    !Var_Exists(".MAKE.JOBS", VAR_GLOBAL))
		return;

	(void)Var_Subst("${.MAKE.JOBS}", VAR_GLOBAL, VARE_WANTRES, &value);
	/* TODO: handle errors */
	n = (int)strtol(value, NULL, 0);
	if (n < 1) {
		(void)fprintf(stderr,
			      "%s: illegal value for .MAKE.JOBS "
			      "-- must be positive integer!\n",
			      progname);
		exit(1);
	}

	if (n != opts.maxJobs) {
		Var_Append(MAKEFLAGS, "-j", VAR_GLOBAL);
		Var_Append(MAKEFLAGS, value, VAR_GLOBAL);
	}

	opts.maxJobs = n;
	maxJobTokens = opts.maxJobs;
	forceJobs = TRUE;
	free(value);
}

/*
 * For compatibility, look at the directories in the VPATH variable
 * and add them to the search path, if the variable is defined. The
 * variable's value is in the same format as the PATH environment
 * variable, i.e. <directory>:<directory>:<directory>...
 */
static void
InitVpath(void)
{
	char *vpath, savec, *path;
	if (!Var_Exists("VPATH", VAR_CMDLINE))
		return;

	(void)Var_Subst("${VPATH}", VAR_CMDLINE, VARE_WANTRES, &vpath);
	/* TODO: handle errors */
	path = vpath;
	do {
		char *cp;
		/* skip to end of directory */
		for (cp = path; *cp != ':' && *cp != '\0'; cp++)
			continue;
		/* Save terminator character so know when to stop */
		savec = *cp;
		*cp = '\0';
		/* Add directory to search path */
		(void)Dir_AddDir(dirSearchPath, path);
		*cp = savec;
		path = cp + 1;
	} while (savec == ':');
	free(vpath);
}

static void
ReadMakefiles(void)
{
	if (opts.makefiles->first != NULL) {
		StringListNode *ln;

		for (ln = opts.makefiles->first; ln != NULL; ln = ln->next) {
			if (ReadMakefile(ln->datum) != 0)
				Fatal("%s: cannot open %s.",
				      progname, (char *)ln->datum);
		}
	} else {
		char *p1;
		(void)Var_Subst("${" MAKEFILE_PREFERENCE "}",
				VAR_CMDLINE, VARE_WANTRES, &p1);
		/* TODO: handle errors */
		(void)str2Lst_Append(opts.makefiles, p1, NULL);
		(void)Lst_ForEachUntil(opts.makefiles,
				       ReadMakefileSucceeded, NULL);
		free(p1);
	}
}

static void
CleanUp(void)
{
#ifdef CLEANUP
	Lst_Destroy(opts.variables, free);
	Lst_Free(opts.makefiles);	/* don't free, may be used in GNodes */
	Lst_Destroy(opts.create, free);
#endif

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	Trace_Log(MAKEEND, 0);

	if (enterFlagObj)
		printf("%s: Leaving directory `%s'\n", progname, objdir);
	if (opts.enterFlag)
		printf("%s: Leaving directory `%s'\n", progname, curdir);

#ifdef USE_META
	meta_finish();
#endif
	Suff_End();
	Targ_End();
	Arch_End();
	Var_End();
	Parse_End();
	Dir_End();
	Job_End();
	Trace_End();
}

/*-
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 */
int
main(int argc, char **argv)
{
	Boolean outOfDate;	/* FALSE if all targets up to date */
	struct stat sa;
	const char *machine;
	const char *machine_arch;
	char *syspath = getenv("MAKESYSPATH");
	struct utsname utsname;

	/* default to writing debug to stderr */
	opts.debug_file = stderr;

#ifdef SIGINFO
	(void)bmake_signal(SIGINFO, siginfo);
#endif

	InitRandom();

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	UnlimitFiles();

	if (uname(&utsname) == -1) {
	    (void)fprintf(stderr, "%s: uname failed (%s).\n", progname,
		strerror(errno));
	    exit(2);
	}

	/*
	 * Get the name of this type of MACHINE from utsname
	 * so we can share an executable for similar machines.
	 * (i.e. m68k: amiga hp300, mac68k, sun3, ...)
	 *
	 * Note that both MACHINE and MACHINE_ARCH are decided at
	 * run-time.
	 */
	machine = init_machine(&utsname);
	machine_arch = init_machine_arch();

	myPid = getpid();		/* remember this for vFork() */

	/*
	 * Just in case MAKEOBJDIR wants us to do something tricky.
	 */
	Var_Init();		/* Initialize the lists of variables for
				 * parsing arguments */
	Var_Set(".MAKE.OS", utsname.sysname, VAR_GLOBAL);
	Var_Set("MACHINE", machine, VAR_GLOBAL);
	Var_Set("MACHINE_ARCH", machine_arch, VAR_GLOBAL);
#ifdef MAKE_VERSION
	Var_Set("MAKE_VERSION", MAKE_VERSION, VAR_GLOBAL);
#endif
	Var_Set(".newline", "\n", VAR_GLOBAL); /* handy for :@ loops */
	/*
	 * This is the traditional preference for makefiles.
	 */
#ifndef MAKEFILE_PREFERENCE_LIST
# define MAKEFILE_PREFERENCE_LIST "makefile Makefile"
#endif
	Var_Set(MAKEFILE_PREFERENCE, MAKEFILE_PREFERENCE_LIST,
		VAR_GLOBAL);
	Var_Set(MAKE_DEPENDFILE, ".depend", VAR_GLOBAL);

	CmdOpts_Init();
	allPrecious = FALSE;		/* Remove targets when interrupted */
	deleteOnError = FALSE;		/* Historical default behavior */
	jobsRunning = FALSE;

	maxJobTokens = opts.maxJobs;
	ignorePWD = FALSE;

	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Parse_Init();
	InitVarMake(argv[0]);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL);
	Var_Set(MAKEOVERRIDES, "", VAR_GLOBAL);
	Var_Set("MFLAGS", "", VAR_GLOBAL);
	Var_Set(".ALLTARGETS", "", VAR_GLOBAL);
	/* some makefiles need to know this */
	Var_Set(MAKE_LEVEL ".ENV", MAKE_LEVEL_ENV, VAR_CMDLINE);

	/*
	 * Set some other useful macros
	 */
	{
	    char tmp[64], *ep;

	    makelevel = ((ep = getenv(MAKE_LEVEL_ENV)) && *ep) ? atoi(ep) : 0;
	    if (makelevel < 0)
		makelevel = 0;
	    snprintf(tmp, sizeof(tmp), "%d", makelevel);
	    Var_Set(MAKE_LEVEL, tmp, VAR_GLOBAL);
	    snprintf(tmp, sizeof(tmp), "%u", myPid);
	    Var_Set(".MAKE.PID", tmp, VAR_GLOBAL);
	    snprintf(tmp, sizeof(tmp), "%u", getppid());
	    Var_Set(".MAKE.PPID", tmp, VAR_GLOBAL);
	}
	if (makelevel > 0) {
		char pn[1024];
		snprintf(pn, sizeof(pn), "%s[%d]", progname, makelevel);
		progname = bmake_strdup(pn);
	}

#ifdef USE_META
	meta_init();
#endif
	Dir_Init();

	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
#ifdef POSIX
	{
	    char *p1 = explode(getenv("MAKEFLAGS"));
	    Main_ParseArgLine(p1);
	    free(p1);
	}
#else
	Main_ParseArgLine(getenv("MAKE"));
#endif

	/*
	 * Find where we are (now).
	 * We take care of PWD for the automounter below...
	 */
	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "%s: getcwd: %s.\n",
		    progname, strerror(errno));
		exit(2);
	}

	MainParseArgs(argc, argv);

	if (opts.enterFlag)
		printf("%s: Entering directory `%s'\n", progname, curdir);

	/*
	 * Verify that cwd is sane.
	 */
	if (stat(curdir, &sa) == -1) {
	    (void)fprintf(stderr, "%s: %s: %s.\n",
		 progname, curdir, strerror(errno));
	    exit(2);
	}

#ifndef NO_PWD_OVERRIDE
	HandlePWD(&sa);
#endif
	Var_Set(".CURDIR", curdir, VAR_GLOBAL);

	InitObjdir(machine, machine_arch);

	/*
	 * Initialize archive, target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Arch_Init();
	Targ_Init();
	Suff_Init();
	Trace_Init(tracefile);

	DEFAULT = NULL;
	(void)time(&now);

	Trace_Log(MAKESTART, NULL);

	InitVarTargets();

	InitDefSysIncPath(syspath);

	/*
	 * Read in the built-in rules first, followed by the specified
	 * makefiles, or the default makefile and Makefile, in that order,
	 * if no makefiles were given on the command line.
	 */
	if (!opts.noBuiltins)
		ReadBuiltinRules();
	ReadMakefiles();
	
	/* In particular suppress .depend for '-r -V .OBJDIR -f /dev/null' */
	if (!opts.noBuiltins || !opts.printVars) {
	    /* ignore /dev/null and anything starting with "no" */
	    (void)Var_Subst("${.MAKE.DEPENDFILE:N/dev/null:Nno*:T}",
			    VAR_CMDLINE, VARE_WANTRES, &makeDependfile);
	    if (makeDependfile[0] != '\0') {
		/* TODO: handle errors */
		doing_depend = TRUE;
		(void)ReadMakefile(makeDependfile);
		doing_depend = FALSE;
	    }
	}

	if (enterFlagObj)
		printf("%s: Entering directory `%s'\n", progname, objdir);

	MakeMode(NULL);

	{
	    void *freeIt;
	    Var_Append("MFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL, &freeIt),
		       VAR_GLOBAL);
	    bmake_free(freeIt);

	}

	InitMaxJobs();

	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turned compatibility on
	 */
	if (!opts.compatMake && !forceJobs) {
	    opts.compatMake = TRUE;
	}

	if (!opts.compatMake)
	    Job_ServerStart(maxJobTokens, jp_0, jp_1);
	DEBUG5(JOB, "job_pipe %d %d, maxjobs %d, tokens %d, compat %d\n",
	       jp_0, jp_1, opts.maxJobs, maxJobTokens, opts.compatMake ? 1 : 0);

	if (!opts.printVars)
	    Main_ExportMAKEFLAGS(TRUE);	/* initial export */

	InitVpath();

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_DoPaths();

	/*
	 * Propagate attributes through :: dependency lists.
	 */
	Targ_Propagate();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/* print the values of any variables requested by the user */
	if (opts.printVars) {
		doPrintVars();
		outOfDate = FALSE;
	} else {
		outOfDate = runTargets();
	}

	CleanUp();

	if (DEBUG(LINT) && (errors > 0 || Parse_GetFatals() > 0))
	    return 2;		/* Not 1 so -q can distinguish error */
	return outOfDate ? 1 : 0;
}

/* Open and parse the given makefile, with all its side effects.
 *
 * Results:
 *	0 if ok. -1 if couldn't open file.
 */
static int
ReadMakefile(const char *fname)
{
	int fd;
	char *name, *path = NULL;

	if (!strcmp(fname, "-")) {
		Parse_File(NULL /*stdin*/, -1);
		Var_Set("MAKEFILE", "", VAR_INTERNAL);
	} else {
		/* if we've chdir'd, rebuild the path name */
		if (strcmp(curdir, objdir) && *fname != '/') {
			path = str_concat3(curdir, "/", fname);
			fd = open(path, O_RDONLY);
			if (fd != -1) {
				fname = path;
				goto found;
			}
			free(path);

			/* If curdir failed, try objdir (ala .depend) */
			path = str_concat3(objdir, "/", fname);
			fd = open(path, O_RDONLY);
			if (fd != -1) {
				fname = path;
				goto found;
			}
		} else {
			fd = open(fname, O_RDONLY);
			if (fd != -1)
				goto found;
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, parseIncPath);
		if (!name) {
			SearchPath *sysInc = Lst_IsEmpty(sysIncPath)
					     ? defSysIncPath : sysIncPath;
			name = Dir_FindFile(fname, sysInc);
		}
		if (!name || (fd = open(name, O_RDONLY)) == -1) {
			free(name);
			free(path);
			return -1;
		}
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:
		if (!doing_depend)
			Var_Set("MAKEFILE", fname, VAR_INTERNAL);
		Parse_File(fname, fd);
	}
	free(path);
	return 0;
}



/*-
 * Cmd_Exec --
 *	Execute the command in cmd, and return the output of that command
 *	in a string.  In the output, newlines are replaced with spaces.
 *
 * Results:
 *	A string containing the output of the command, or the empty string.
 *	*errfmt returns a format string describing the command failure,
 *	if any, using a single %s conversion specification.
 *
 * Side Effects:
 *	The string must be freed by the caller.
 */
char *
Cmd_Exec(const char *cmd, const char **errfmt)
{
    const char	*args[4];	/* Args for invoking the shell */
    int		fds[2];		/* Pipe streams */
    int		cpid;		/* Child PID */
    int		pid;		/* PID from wait() */
    WAIT_T	status;		/* command exit status */
    Buffer	buf;		/* buffer to store the result */
    ssize_t	bytes_read;
    char	*res;		/* result */
    size_t	res_len;
    char	*cp;
    int		savederr;	/* saved errno */

    *errfmt = NULL;

    if (!shellName)
	Shell_Init();
    /*
     * Set up arguments for shell
     */
    args[0] = shellName;
    args[1] = "-c";
    args[2] = cmd;
    args[3] = NULL;

    /*
     * Open a pipe for fetching its output
     */
    if (pipe(fds) == -1) {
	*errfmt = "Couldn't create pipe for \"%s\"";
	goto bad;
    }

    /*
     * Fork
     */
    switch (cpid = vFork()) {
    case 0:
	/*
	 * Close input side of pipe
	 */
	(void)close(fds[0]);

	/*
	 * Duplicate the output stream to the shell's output, then
	 * shut the extra thing down. Note we don't fetch the error
	 * stream...why not? Why?
	 */
	(void)dup2(fds[1], 1);
	(void)close(fds[1]);

	Var_ExportVars();

	(void)execv(shellPath, UNCONST(args));
	_exit(1);
	/*NOTREACHED*/

    case -1:
	*errfmt = "Couldn't exec \"%s\"";
	goto bad;

    default:
	/*
	 * No need for the writing half
	 */
	(void)close(fds[1]);

	savederr = 0;
	Buf_Init(&buf, 0);

	do {
	    char   result[BUFSIZ];
	    bytes_read = read(fds[0], result, sizeof(result));
	    if (bytes_read > 0)
		Buf_AddBytes(&buf, result, (size_t)bytes_read);
	}
	while (bytes_read > 0 || (bytes_read == -1 && errno == EINTR));
	if (bytes_read == -1)
	    savederr = errno;

	/*
	 * Close the input side of the pipe.
	 */
	(void)close(fds[0]);

	/*
	 * Wait for the process to exit.
	 */
	while(((pid = waitpid(cpid, &status, 0)) != cpid) && (pid >= 0)) {
	    JobReapChild(pid, status, FALSE);
	    continue;
	}
	res_len = Buf_Len(&buf);
	res = Buf_Destroy(&buf, FALSE);

	if (savederr != 0)
	    *errfmt = "Couldn't read shell's output for \"%s\"";

	if (WIFSIGNALED(status))
	    *errfmt = "\"%s\" exited on a signal";
	else if (WEXITSTATUS(status) != 0)
	    *errfmt = "\"%s\" returned non-zero status";

	/* Convert newlines to spaces.  A final newline is just stripped */
	if (res_len > 0 && res[res_len - 1] == '\n')
	    res[res_len - 1] = '\0';
	for (cp = res; *cp != '\0'; cp++)
	    if (*cp == '\n')
		*cp = ' ';
	break;
    }
    return res;
bad:
    return bmake_strdup("");
}

/* Print a printf-style error message.
 *
 * This error message has no consequences, in particular it does not affect
 * the exit status. */
void
Error(const char *fmt, ...)
{
	va_list ap;
	FILE *err_file;

	err_file = opts.debug_file;
	if (err_file == stdout)
		err_file = stderr;
	(void)fflush(stdout);
	for (;;) {
		va_start(ap, fmt);
		fprintf(err_file, "%s: ", progname);
		(void)vfprintf(err_file, fmt, ap);
		va_end(ap);
		(void)fprintf(err_file, "\n");
		(void)fflush(err_file);
		if (err_file == stderr)
			break;
		err_file = stderr;
	}
	errors++;
}

/* Produce a Fatal error message, then exit immediately.
 *
 * If jobs are running, wait for them to finish. */
void
Fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (jobsRunning)
		Job_Wait();

	(void)fflush(stdout);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	PrintOnError(NULL, NULL);

	if (DEBUG(GRAPH2) || DEBUG(GRAPH3))
		Targ_PrintGraph(2);
	Trace_Log(MAKEERROR, 0);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/* Major exception once jobs are being created.
 * Kills all jobs, prints a message and exits. */
void
Punt(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)fflush(stdout);
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	PrintOnError(NULL, NULL);

	DieHorribly();
}

/* Exit without giving a message. */
void
DieHorribly(void)
{
	if (jobsRunning)
		Job_AbortAll();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	Trace_Log(MAKEERROR, 0);
	exit(2);		/* Not 1, so -q can distinguish error */
}

/* Called when aborting due to errors in child shell to signal abnormal exit.
 * The program exits.
 * Errors is the number of errors encountered in Make_Make. */
void
Finish(int errs)
{
	if (dieQuietly(NULL, -1))
		exit(2);
	Fatal("%d error%s", errs, errs == 1 ? "" : "s");
}

/*
 * eunlink --
 *	Remove a file carefully, avoiding directories.
 */
int
eunlink(const char *file)
{
	struct stat st;

	if (lstat(file, &st) == -1)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		return -1;
	}
	return unlink(file);
}

static void
write_all(int fd, const void *data, size_t n)
{
	const char *mem = data;

	while (n > 0) {
		ssize_t written = write(fd, mem, n);
		if (written == -1 && errno == EAGAIN)
			continue;
		if (written == -1)
			break;
		mem += written;
		n -= (size_t)written;
	}
}

/*
 * execDie --
 *	Print why exec failed, avoiding stdio.
 */
void MAKE_ATTR_DEAD
execDie(const char *af, const char *av)
{
	Buffer buf;

	Buf_Init(&buf, 0);
	Buf_AddStr(&buf, progname);
	Buf_AddStr(&buf, ": ");
	Buf_AddStr(&buf, af);
	Buf_AddStr(&buf, "(");
	Buf_AddStr(&buf, av);
	Buf_AddStr(&buf, ") failed (");
	Buf_AddStr(&buf, strerror(errno));
	Buf_AddStr(&buf, ")\n");

	write_all(STDERR_FILENO, Buf_GetAll(&buf, NULL), Buf_Len(&buf));

	Buf_Destroy(&buf, TRUE);
	_exit(1);
}

/*
 * usage --
 *	exit with usage message
 */
static void
usage(void)
{
	char *p;
	if ((p = strchr(progname, '[')) != NULL)
		*p = '\0';

	(void)fprintf(stderr,
"usage: %s [-BeikNnqrstWwX] \n"
"            [-C directory] [-D variable] [-d flags] [-f makefile]\n"
"            [-I directory] [-J private] [-j max_jobs] [-m directory] [-T file]\n"
"            [-V variable] [-v variable] [variable=value] [target ...]\n",
	    progname);
	exit(2);
}

/*
 * realpath(3) can get expensive, cache results...
 */
static GNode *cached_realpaths = NULL;

static GNode *
get_cached_realpaths(void)
{

    if (!cached_realpaths) {
	cached_realpaths = Targ_NewGN("Realpath");
#ifndef DEBUG_REALPATH_CACHE
	cached_realpaths->flags = INTERNAL;
#endif
    }

    return cached_realpaths;
}

/* purge any relative paths */
static void
purge_cached_realpaths(void)
{
    GNode *cache = get_cached_realpaths();
    HashEntry *he, *nhe;
    HashIter hi;

    HashIter_Init(&hi, &cache->context);
    he = HashIter_Next(&hi);
    while (he != NULL) {
	nhe = HashIter_Next(&hi);
	if (he->key[0] != '/') {
	    if (DEBUG(DIR))
		fprintf(stderr, "cached_realpath: purging %s\n", he->key);
	    HashTable_DeleteEntry(&cache->context, he);
	}
	he = nhe;
    }
}

char *
cached_realpath(const char *pathname, char *resolved)
{
    GNode *cache;
    const char *rp;
    void *freeIt;

    if (!pathname || !pathname[0])
	return NULL;

    cache = get_cached_realpaths();

    if ((rp = Var_Value(pathname, cache, &freeIt)) != NULL) {
	/* a hit */
	strlcpy(resolved, rp, MAXPATHLEN);
    } else if ((rp = realpath(pathname, resolved)) != NULL) {
	Var_Set(pathname, rp, cache);
    } /* else should we negative-cache? */

    bmake_free(freeIt);
    return rp ? resolved : NULL;
}

/*
 * Return true if we should die without noise.
 * For example our failing child was a sub-make
 * or failure happend elsewhere.
 */
int
dieQuietly(GNode *gn, int bf)
{
    static int quietly = -1;

    if (quietly < 0) {
	if (DEBUG(JOB) || !getBoolean(".MAKE.DIE_QUIETLY", TRUE))
	    quietly = 0;
	else if (bf >= 0)
	    quietly = bf;
	else
	    quietly = gn != NULL ? ((gn->type  & (OP_MAKE)) != 0) : 0;
    }
    return quietly;
}

static void
SetErrorVars(GNode *gn)
{
    StringListNode *ln;

    /*
     * We can print this even if there is no .ERROR target.
     */
    Var_Set(".ERROR_TARGET", gn->name, VAR_GLOBAL);
    Var_Delete(".ERROR_CMD", VAR_GLOBAL);

    for (ln = gn->commands->first; ln != NULL; ln = ln->next) {
	const char *cmd = ln->datum;

	if (cmd == NULL)
	    break;
	Var_Append(".ERROR_CMD", cmd, VAR_GLOBAL);
    }
}

void
PrintOnError(GNode *gn, const char *s)
{
    static GNode *en = NULL;
    const char *expr;
    char *cp;

    if (DEBUG(HASH)) {
	Targ_Stats();
	Var_Stats();
    }

    /* we generally want to keep quiet if a sub-make died */
    if (dieQuietly(gn, -1))
	return;

    if (s)
	printf("%s", s);

    printf("\n%s: stopped in %s\n", progname, curdir);

    if (en)
	return;				/* we've been here! */
    if (gn)
	SetErrorVars(gn);
    expr = "${MAKE_PRINT_VAR_ON_ERROR:@v@$v='${$v}'\n@}";
    (void)Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES, &cp);
    /* TODO: handle errors */
    printf("%s", cp);
    free(cp);
    fflush(stdout);

    /*
     * Finally, see if there is a .ERROR target, and run it if so.
     */
    en = Targ_FindNode(".ERROR");
    if (en) {
	en->type |= OP_SPECIAL;
	Compat_Make(en, en);
    }
}

void
Main_ExportMAKEFLAGS(Boolean first)
{
    static Boolean once = TRUE;
    const char *expr;
    char *s;

    if (once != first)
	return;
    once = FALSE;

    expr = "${.MAKEFLAGS} ${.MAKEOVERRIDES:O:u:@v@$v=${$v:Q}@}";
    (void)Var_Subst(expr, VAR_CMDLINE, VARE_WANTRES, &s);
    /* TODO: handle errors */
    if (s[0] != '\0') {
#ifdef POSIX
	setenv("MAKEFLAGS", s, 1);
#else
	setenv("MAKE", s, 1);
#endif
    }
}

char *
getTmpdir(void)
{
    static char *tmpdir = NULL;

    if (!tmpdir) {
	struct stat st;

	/*
	 * Honor $TMPDIR but only if it is valid.
	 * Ensure it ends with /.
	 */
	(void)Var_Subst("${TMPDIR:tA:U" _PATH_TMP "}/", VAR_GLOBAL,
			VARE_WANTRES, &tmpdir);
	/* TODO: handle errors */
	if (stat(tmpdir, &st) < 0 || !S_ISDIR(st.st_mode)) {
	    free(tmpdir);
	    tmpdir = bmake_strdup(_PATH_TMP);
	}
    }
    return tmpdir;
}

/*
 * Create and open a temp file using "pattern".
 * If out_fname is provided, set it to a copy of the filename created.
 * Otherwise unlink the file once open.
 */
int
mkTempFile(const char *pattern, char **out_fname)
{
    static char *tmpdir = NULL;
    char tfile[MAXPATHLEN];
    int fd;

    if (pattern != NULL)
	pattern = TMPPAT;
    if (tmpdir == NULL)
	tmpdir = getTmpdir();
    if (pattern[0] == '/') {
	snprintf(tfile, sizeof(tfile), "%s", pattern);
    } else {
	snprintf(tfile, sizeof(tfile), "%s%s", tmpdir, pattern);
    }
    if ((fd = mkstemp(tfile)) < 0)
	Punt("Could not create temporary file %s: %s", tfile, strerror(errno));
    if (out_fname) {
	*out_fname = bmake_strdup(tfile);
    } else {
	unlink(tfile);			/* we just want the descriptor */
    }
    return fd;
}

/*
 * Convert a string representation of a boolean.
 * Anything that looks like "No", "False", "Off", "0" etc,
 * is FALSE, otherwise TRUE.
 */
Boolean
s2Boolean(const char *s, Boolean bf)
{
    switch(s[0]) {
    case '\0':			/* not set - the default wins */
	break;
    case '0':
    case 'F':
    case 'f':
    case 'N':
    case 'n':
	return FALSE;
    case 'O':
    case 'o':
	return s[1] != 'F' && s[1] != 'f';
    default:
	return TRUE;
    }
    return bf;
}

/*
 * Return a Boolean based on a variable.
 *
 * If the knob is not set, return the fallback.
 * If set, anything that looks or smells like "No", "False", "Off", "0", etc.
 * is FALSE, otherwise TRUE.
 */
Boolean
getBoolean(const char *varname, Boolean fallback)
{
    char *expr = str_concat3("${", varname, ":U}");
    char *value;
    Boolean res;

    (void)Var_Subst(expr, VAR_GLOBAL, VARE_WANTRES, &value);
    /* TODO: handle errors */
    res = s2Boolean(value, fallback);
    free(value);
    free(expr);
    return res;
}
