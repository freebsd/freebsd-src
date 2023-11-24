/*	$NetBSD: main.c,v 1.599 2023/09/10 21:52:36 rillig Exp $	*/

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

/*
 * The main file for this entire program. Exit routines etc. reside here.
 *
 * Utility functions defined in this file:
 *
 *	Main_ParseArgLine
 *			Parse and process command line arguments from a
 *			single string.  Used to implement the special targets
 *			.MFLAGS and .MAKEFLAGS.
 *
 *	Error		Print a tagged error message.
 *
 *	Fatal		Print an error message and exit.
 *
 *	Punt		Abort all jobs and exit with a message.
 *
 *	Finish		Finish things up by printing the number of errors
 *			that occurred, and exit.
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
MAKE_RCSID("$NetBSD: main.c,v 1.599 2023/09/10 21:52:36 rillig Exp $");
#if defined(MAKE_NATIVE) && !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1988, 1989, 1990, 1993 "
	    "The Regents of the University of California.  "
	    "All rights reserved.");
#endif

#ifndef __arraycount
# define __arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#endif

CmdOpts opts;
time_t now;			/* Time at start of make */
GNode *defaultNode;		/* .DEFAULT node */
bool allPrecious;		/* .PRECIOUS given on line by itself */
bool deleteOnError;		/* .DELETE_ON_ERROR: set */

static int maxJobTokens;	/* -j argument */
static bool enterFlagObj;	/* -w and objdir != srcdir */

static int jp_0 = -1, jp_1 = -1; /* ends of parent job pipe */
bool doing_depend;		/* Set while reading .depend */
static bool jobsRunning;	/* true if the jobs might be running */
static const char *tracefile;
static bool ReadMakefile(const char *);
static void purge_relative_cached_realpaths(void);

static bool ignorePWD;		/* if we use -C, PWD is meaningless */
static char objdir[MAXPATHLEN + 1]; /* where we chdir'ed to */
char curdir[MAXPATHLEN + 1];	/* Startup directory */
const char *progname;
char *makeDependfile;
pid_t myPid;
int makelevel;

bool forceJobs = false;
static int main_errors = 0;
static HashTable cached_realpaths;

/*
 * For compatibility with the POSIX version of MAKEFLAGS that includes
 * all the options without '-', convert 'flags' to '-f -l -a -g -s'.
 */
static char *
explode(const char *flags)
{
	char *exploded, *ep;
	const char *p;

	if (flags == NULL)
		return NULL;

	for (p = flags; *p != '\0'; p++)
		if (!ch_isalpha(*p))
			return bmake_strdup(flags);

	exploded = bmake_malloc((size_t)(p - flags) * 3 + 1);
	for (p = flags, ep = exploded; *p != '\0'; p++) {
		*ep++ = '-';
		*ep++ = *p;
		*ep++ = ' ';
	}
	*ep = '\0';
	return exploded;
}

MAKE_ATTR_DEAD static void
usage(void)
{
	size_t prognameLen = strcspn(progname, "[");

	(void)fprintf(stderr,
"usage: %.*s [-BeikNnqrSstWwX]\n"
"            [-C directory] [-D variable] [-d flags] [-f makefile]\n"
"            [-I directory] [-J private] [-j max_jobs] [-m directory] [-T file]\n"
"            [-V variable] [-v variable] [variable=value] [target ...]\n",
	    (int)prognameLen, progname);
	exit(2);
}

static void
MainParseArgDebugFile(const char *arg)
{
	const char *mode;
	size_t len;
	char *fname;

	if (opts.debug_file != stdout && opts.debug_file != stderr)
		fclose(opts.debug_file);

	if (*arg == '+') {
		arg++;
		mode = "a";
	} else
		mode = "w";

	if (strcmp(arg, "stdout") == 0) {
		opts.debug_file = stdout;
		return;
	}
	if (strcmp(arg, "stderr") == 0) {
		opts.debug_file = stderr;
		return;
	}

	len = strlen(arg);
	fname = bmake_malloc(len + 20);
	memcpy(fname, arg, len + 1);

	/* Replace the trailing '%d' after '.%d' with the pid. */
	if (len >= 3 && memcmp(fname + len - 3, ".%d", 3) == 0)
		snprintf(fname + len - 2, 20, "%d", getpid());

	opts.debug_file = fopen(fname, mode);
	if (opts.debug_file == NULL) {
		fprintf(stderr, "Cannot open debug file \"%s\"\n",
		    fname);
		exit(2);
	}
	free(fname);
}

static void
MainParseArgDebug(const char *argvalue)
{
	const char *modules;
	DebugFlags debug = opts.debug;

	for (modules = argvalue; *modules != '\0'; modules++) {
		switch (*modules) {
		case '0':	/* undocumented, only intended for tests */
			memset(&debug, 0, sizeof(debug));
			break;
		case 'A':
			memset(&debug, ~0, sizeof(debug));
			break;
		case 'a':
			debug.DEBUG_ARCH = true;
			break;
		case 'C':
			debug.DEBUG_CWD = true;
			break;
		case 'c':
			debug.DEBUG_COND = true;
			break;
		case 'd':
			debug.DEBUG_DIR = true;
			break;
		case 'e':
			debug.DEBUG_ERROR = true;
			break;
		case 'f':
			debug.DEBUG_FOR = true;
			break;
		case 'g':
			if (modules[1] == '1') {
				debug.DEBUG_GRAPH1 = true;
				modules++;
			} else if (modules[1] == '2') {
				debug.DEBUG_GRAPH2 = true;
				modules++;
			} else if (modules[1] == '3') {
				debug.DEBUG_GRAPH3 = true;
				modules++;
			}
			break;
		case 'h':
			debug.DEBUG_HASH = true;
			break;
		case 'j':
			debug.DEBUG_JOB = true;
			break;
		case 'L':
			opts.strict = true;
			break;
		case 'l':
			debug.DEBUG_LOUD = true;
			break;
		case 'M':
			debug.DEBUG_META = true;
			break;
		case 'm':
			debug.DEBUG_MAKE = true;
			break;
		case 'n':
			debug.DEBUG_SCRIPT = true;
			break;
		case 'p':
			debug.DEBUG_PARSE = true;
			break;
		case 's':
			debug.DEBUG_SUFF = true;
			break;
		case 't':
			debug.DEBUG_TARG = true;
			break;
		case 'V':
			opts.debugVflag = true;
			break;
		case 'v':
			debug.DEBUG_VAR = true;
			break;
		case 'x':
			debug.DEBUG_SHELL = true;
			break;
		case 'F':
			MainParseArgDebugFile(modules + 1);
			goto finish;
		default:
			(void)fprintf(stderr,
			    "%s: illegal argument to d option -- %c\n",
			    progname, *modules);
			usage();
		}
	}

finish:
	opts.debug = debug;

	setvbuf(opts.debug_file, NULL, _IONBF, 0);
	if (opts.debug_file != stdout)
		setvbuf(stdout, NULL, _IOLBF, 0);
}

/* Is path relative or does it contain any relative component "." or ".."? */
static bool
IsRelativePath(const char *path)
{
	const char *p;

	if (path[0] != '/')
		return true;
	p = path;
	while ((p = strstr(p, "/.")) != NULL) {
		p += 2;
		if (*p == '.')
			p++;
		if (*p == '/' || *p == '\0')
			return true;
	}
	return false;
}

static void
MainParseArgChdir(const char *argvalue)
{
	struct stat sa, sb;

	if (chdir(argvalue) == -1) {
		(void)fprintf(stderr, "%s: chdir %s: %s\n",
		    progname, argvalue, strerror(errno));
		exit(2);	/* Not 1 so -q can distinguish error */
	}
	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "%s: %s.\n", progname, strerror(errno));
		exit(2);
	}
	if (!IsRelativePath(argvalue) &&
	    stat(argvalue, &sa) != -1 &&
	    stat(curdir, &sb) != -1 &&
	    sa.st_ino == sb.st_ino &&
	    sa.st_dev == sb.st_dev)
		strncpy(curdir, argvalue, MAXPATHLEN);
	ignorePWD = true;
}

static void
MainParseArgJobsInternal(const char *argvalue)
{
	char end;
	if (sscanf(argvalue, "%d,%d%c", &jp_0, &jp_1, &end) != 2) {
		(void)fprintf(stderr,
		    "%s: internal error -- J option malformed (%s)\n",
		    progname, argvalue);
		usage();
	}
	if ((fcntl(jp_0, F_GETFD, 0) < 0) ||
	    (fcntl(jp_1, F_GETFD, 0) < 0)) {
		jp_0 = -1;
		jp_1 = -1;
		opts.compatMake = true;
	} else {
		Global_Append(MAKEFLAGS, "-J");
		Global_Append(MAKEFLAGS, argvalue);
	}
}

static void
MainParseArgJobs(const char *arg)
{
	const char *p;
	char *end;
	char v[12];

	forceJobs = true;
	opts.maxJobs = (int)strtol(arg, &end, 0);
	p = arg + (end - arg);
#ifdef _SC_NPROCESSORS_ONLN
	if (*p != '\0') {
		double d;

		if (*p == 'C') {
			d = (opts.maxJobs > 0) ? opts.maxJobs : 1;
		} else if (*p == '.') {
			d = strtod(arg, &end);
			p = arg + (end - arg);
		} else
			d = 0;
		if (d > 0) {
			p = "";
			opts.maxJobs = (int)sysconf(_SC_NPROCESSORS_ONLN);
			opts.maxJobs = (int)(d * (double)opts.maxJobs);
		}
	}
#endif
	if (*p != '\0' || opts.maxJobs < 1) {
		(void)fprintf(stderr,
		    "%s: argument '%s' to option '-j' "
		    "must be a positive number\n",
		    progname, arg);
		exit(2);	/* Not 1 so -q can distinguish error */
	}
	snprintf(v, sizeof(v), "%d", opts.maxJobs);
	Global_Append(MAKEFLAGS, "-j");
	Global_Append(MAKEFLAGS, v);
	Global_Set(".MAKE.JOBS", v);
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
		(void)SearchPath_Add(sysIncPath, found_path);
		free(found_path);
	} else {
		(void)SearchPath_Add(sysIncPath, argvalue);
	}
	Global_Append(MAKEFLAGS, "-m");
	Global_Append(MAKEFLAGS, argvalue);
	Dir_SetSYSPATH();
}

static bool
MainParseOption(char c, const char *argvalue)
{
	switch (c) {
	case '\0':
		break;
	case 'B':
		opts.compatMake = true;
		Global_Append(MAKEFLAGS, "-B");
		Global_Set(".MAKE.MODE", "compat");
		break;
	case 'C':
		MainParseArgChdir(argvalue);
		break;
	case 'D':
		if (argvalue[0] == '\0')
			return false;
		Var_SetExpand(SCOPE_GLOBAL, argvalue, "1");
		Global_Append(MAKEFLAGS, "-D");
		Global_Append(MAKEFLAGS, argvalue);
		break;
	case 'I':
		Parse_AddIncludeDir(argvalue);
		Global_Append(MAKEFLAGS, "-I");
		Global_Append(MAKEFLAGS, argvalue);
		break;
	case 'J':
		MainParseArgJobsInternal(argvalue);
		break;
	case 'N':
		opts.noExecute = true;
		opts.noRecursiveExecute = true;
		Global_Append(MAKEFLAGS, "-N");
		break;
	case 'S':
		opts.keepgoing = false;
		Global_Append(MAKEFLAGS, "-S");
		break;
	case 'T':
		tracefile = bmake_strdup(argvalue);
		Global_Append(MAKEFLAGS, "-T");
		Global_Append(MAKEFLAGS, argvalue);
		break;
	case 'V':
	case 'v':
		opts.printVars = c == 'v' ? PVM_EXPANDED : PVM_UNEXPANDED;
		Lst_Append(&opts.variables, bmake_strdup(argvalue));
		/* XXX: Why always -V? */
		Global_Append(MAKEFLAGS, "-V");
		Global_Append(MAKEFLAGS, argvalue);
		break;
	case 'W':
		opts.parseWarnFatal = true;
		/* XXX: why no Global_Append? */
		break;
	case 'X':
		opts.varNoExportEnv = true;
		Global_Append(MAKEFLAGS, "-X");
		break;
	case 'd':
		/* If '-d-opts' don't pass to children */
		if (argvalue[0] == '-')
			argvalue++;
		else {
			Global_Append(MAKEFLAGS, "-d");
			Global_Append(MAKEFLAGS, argvalue);
		}
		MainParseArgDebug(argvalue);
		break;
	case 'e':
		opts.checkEnvFirst = true;
		Global_Append(MAKEFLAGS, "-e");
		break;
	case 'f':
		Lst_Append(&opts.makefiles, bmake_strdup(argvalue));
		break;
	case 'i':
		opts.ignoreErrors = true;
		Global_Append(MAKEFLAGS, "-i");
		break;
	case 'j':
		MainParseArgJobs(argvalue);
		break;
	case 'k':
		opts.keepgoing = true;
		Global_Append(MAKEFLAGS, "-k");
		break;
	case 'm':
		MainParseArgSysInc(argvalue);
		/* XXX: why no Var_Append? */
		break;
	case 'n':
		opts.noExecute = true;
		Global_Append(MAKEFLAGS, "-n");
		break;
	case 'q':
		opts.query = true;
		/* Kind of nonsensical, wot? */
		Global_Append(MAKEFLAGS, "-q");
		break;
	case 'r':
		opts.noBuiltins = true;
		Global_Append(MAKEFLAGS, "-r");
		break;
	case 's':
		opts.silent = true;
		Global_Append(MAKEFLAGS, "-s");
		break;
	case 't':
		opts.touch = true;
		Global_Append(MAKEFLAGS, "-t");
		break;
	case 'w':
		opts.enterFlag = true;
		Global_Append(MAKEFLAGS, "-w");
		break;
	default:
		usage();
	}
	return true;
}

/*
 * Parse the given arguments.  Called from main() and from
 * Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 * The arguments must be treated as read-only and will be freed after the
 * call.
 *
 * XXX: Deal with command line overriding .MAKEFLAGS in makefile
 */
static void
MainParseArgs(int argc, char **argv)
{
	char c;
	int arginc;
	char *argvalue;
	char *optscan;
	bool inOption, dashDash = false;

	const char *optspecs = "BC:D:I:J:NST:V:WXd:ef:ij:km:nqrstv:w";
/* Can't actually use getopt(3) because rescanning is not portable */

rearg:
	inOption = false;
	optscan = NULL;
	while (argc > 1) {
		const char *optspec;
		if (!inOption)
			optscan = argv[1];
		c = *optscan++;
		arginc = 0;
		if (inOption) {
			if (c == '\0') {
				argv++;
				argc--;
				inOption = false;
				continue;
			}
		} else {
			if (c != '-' || dashDash)
				break;
			inOption = true;
			c = *optscan++;
		}
		/* '-' found at some earlier point */
		optspec = strchr(optspecs, c);
		if (c != '\0' && optspec != NULL && optspec[1] == ':') {
			/*
			 * -<something> found, and <something> should have an
			 * argument
			 */
			inOption = false;
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
			inOption = false;
			break;
		case '-':
			dashDash = true;
			break;
		default:
			if (!MainParseOption(c, argvalue))
				goto noarg;
		}
		argv += arginc;
		argc -= arginc;
	}

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (; argc > 1; argv++, argc--) {
		if (!Parse_VarAssign(argv[1], false, SCOPE_CMDLINE)) {
			if (argv[1][0] == '\0')
				Punt("illegal (null) argument.");
			if (argv[1][0] == '-' && !dashDash)
				goto rearg;
			Lst_Append(&opts.create, bmake_strdup(argv[1]));
		}
	}

	return;
noarg:
	(void)fprintf(stderr, "%s: option requires an argument -- %c\n",
	    progname, c);
	usage();
}

/*
 * Break a line of arguments into words and parse them.
 *
 * Used when a .MFLAGS or .MAKEFLAGS target is encountered during parsing and
 * by main() when reading the MAKEFLAGS environment variable.
 */
void
Main_ParseArgLine(const char *line)
{
	Words words;
	char *buf;

	if (line == NULL)
		return;
	/* XXX: don't use line as an iterator variable */
	for (; *line == ' '; line++)
		continue;
	if (line[0] == '\0')
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
	{
		FStr argv0 = Var_Value(SCOPE_GLOBAL, ".MAKE");
		buf = str_concat3(argv0.str, " ", line);
		FStr_Done(&argv0);
	}

	words = Str_Words(buf, true);
	if (words.words == NULL) {
		Error("Unterminated quoted string [%s]", buf);
		free(buf);
		return;
	}
	free(buf);
	MainParseArgs((int)words.len, words.words);

	Words_Free(words);
}

bool
Main_SetObjdir(bool writable, const char *fmt, ...)
{
	struct stat sb;
	char *path;
	char buf[MAXPATHLEN + 1];
	char buf2[MAXPATHLEN + 1];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(path = buf, MAXPATHLEN, fmt, ap);
	va_end(ap);

	if (path[0] != '/') {
		if (snprintf(buf2, MAXPATHLEN, "%s/%s", curdir, path) <= MAXPATHLEN)
			path = buf2;
		else
			return false;
	}

	/* look for the directory and try to chdir there */
	if (stat(path, &sb) != 0 || !S_ISDIR(sb.st_mode))
		return false;

	if ((writable && access(path, W_OK) != 0) || chdir(path) != 0) {
		(void)fprintf(stderr, "%s: warning: %s: %s.\n",
		    progname, path, strerror(errno));
		return false;
	}

	snprintf(objdir, sizeof objdir, "%s", path);
	Global_Set(".OBJDIR", objdir);
	setenv("PWD", objdir, 1);
	Dir_InitDot();
	purge_relative_cached_realpaths();
	if (opts.enterFlag && strcmp(objdir, curdir) != 0)
		enterFlagObj = true;
	return true;
}

static bool
SetVarObjdir(bool writable, const char *var, const char *suffix)
{
	FStr path = Var_Value(SCOPE_CMDLINE, var);

	if (path.str == NULL || path.str[0] == '\0') {
		FStr_Done(&path);
		return false;
	}

	Var_Expand(&path, SCOPE_GLOBAL, VARE_WANTRES);

	(void)Main_SetObjdir(writable, "%s%s", path.str, suffix);

	FStr_Done(&path);
	return true;
}

/*
 * Splits str into words, adding them to the list.
 * The string must be kept alive as long as the list.
 */
int
str2Lst_Append(StringList *lp, char *str)
{
	char *cp;
	int n;

	const char *sep = " \t";

	for (n = 0, cp = strtok(str, sep); cp != NULL; cp = strtok(NULL, sep)) {
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
	if (getcwd(dir, sizeof dir) == NULL)
		return;
	len = snprintf(str, sizeof str, "%s: Working in: %s\n", progname, dir);
	if (len > 0)
		(void)write(STDERR_FILENO, str, (size_t)len);
}
#endif

/* Allow makefiles some control over the mode we run in. */
static void
MakeMode(void)
{
	char *mode = Var_Subst("${.MAKE.MODE:tl}",
	    SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */

	if (mode[0] != '\0') {
		if (strstr(mode, "compat") != NULL) {
			opts.compatMake = true;
			forceJobs = false;
		}
#if USE_META
		if (strstr(mode, "meta") != NULL)
			meta_mode_init(mode);
#endif
		if (strstr(mode, "randomize-targets") != NULL)
			opts.randomizeTargets = true;
	}

	free(mode);
}

static void
PrintVar(const char *varname, bool expandVars)
{
	if (strchr(varname, '$') != NULL) {
		char *evalue = Var_Subst(varname, SCOPE_GLOBAL, VARE_WANTRES);
		/* TODO: handle errors */
		printf("%s\n", evalue);
		free(evalue);

	} else if (expandVars) {
		char *expr = str_concat3("${", varname, "}");
		char *evalue = Var_Subst(expr, SCOPE_GLOBAL, VARE_WANTRES);
		/* TODO: handle errors */
		free(expr);
		printf("%s\n", evalue);
		free(evalue);

	} else {
		FStr value = Var_Value(SCOPE_GLOBAL, varname);
		printf("%s\n", value.str != NULL ? value.str : "");
		FStr_Done(&value);
	}
}

/*
 * Return a bool based on a variable.
 *
 * If the knob is not set, return the fallback.
 * If set, anything that looks or smells like "No", "False", "Off", "0", etc.
 * is false, otherwise true.
 */
bool
GetBooleanExpr(const char *expr, bool fallback)
{
	char *value;
	bool res;

	value = Var_Subst(expr, SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */
	res = ParseBoolean(value, fallback);
	free(value);
	return res;
}

static void
doPrintVars(void)
{
	StringListNode *ln;
	bool expandVars;

	if (opts.printVars == PVM_EXPANDED)
		expandVars = true;
	else if (opts.debugVflag)
		expandVars = false;
	else
		expandVars = GetBooleanExpr("${.MAKE.EXPAND_VARIABLES}",
		    false);

	for (ln = opts.variables.first; ln != NULL; ln = ln->next) {
		const char *varname = ln->datum;
		PrintVar(varname, expandVars);
	}
}

static bool
runTargets(void)
{
	GNodeList targs = LST_INIT;	/* target nodes to create */
	bool outOfDate;		/* false if all targets up to date */

	/*
	 * Have now read the entire graph and need to make a list of
	 * targets to create. If none was given on the command line,
	 * we consult the parsing module to find the main target(s)
	 * to create.
	 */
	if (Lst_IsEmpty(&opts.create))
		Parse_MainName(&targs);
	else
		Targ_FindList(&targs, &opts.create);

	if (!opts.compatMake) {
		/*
		 * Initialize job module before traversing the graph
		 * now that any .BEGIN and .END targets have been read.
		 * This is done only if the -q flag wasn't given
		 * (to prevent the .BEGIN from being executed should
		 * it exist).
		 */
		if (!opts.query) {
			Job_Init();
			jobsRunning = true;
		}

		/* Traverse the graph, checking on all the targets */
		outOfDate = Make_Run(&targs);
	} else {
		Compat_MakeAll(&targs);
		outOfDate = false;
	}
	Lst_Done(&targs);	/* Don't free the targets themselves. */
	return outOfDate;
}

/*
 * Set up the .TARGETS variable to contain the list of targets to be created.
 * If none specified, make the variable empty for now, the parser will fill
 * in the default or .MAIN target later.
 */
static void
InitVarTargets(void)
{
	StringListNode *ln;

	if (Lst_IsEmpty(&opts.create)) {
		Global_Set(".TARGETS", "");
		return;
	}

	for (ln = opts.create.first; ln != NULL; ln = ln->next) {
		const char *name = ln->datum;
		Global_Append(".TARGETS", name);
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
InitVarMachine(const struct utsname *utsname MAKE_ATTR_UNUSED)
{
#ifdef FORCE_MACHINE
	return FORCE_MACHINE;
#else
    	const char *machine = getenv("MACHINE");

	if (machine != NULL)
		return machine;

#if defined(MAKE_NATIVE)
	return utsname->machine;
#elif defined(MAKE_MACHINE)
	return MAKE_MACHINE;
#else
	return "unknown";
#endif
#endif
}

static const char *
InitVarMachineArch(void)
{
#ifdef FORCE_MACHINE_ARCH
	return FORCE_MACHINE_ARCH;
#else
	const char *env = getenv("MACHINE_ARCH");
	if (env != NULL)
		return env;

#if defined(MAKE_NATIVE) && defined(CTL_HW)
	{
		struct utsname utsname;
		static char machine_arch_buf[sizeof utsname.machine];
		const int mib[2] = { CTL_HW, HW_MACHINE_ARCH };
		size_t len = sizeof machine_arch_buf;

		if (sysctl(mib, (unsigned int)__arraycount(mib),
		    machine_arch_buf, &len, NULL, 0) < 0) {
			(void)fprintf(stderr, "%s: sysctl failed (%s).\n",
			    progname, strerror(errno));
			exit(2);
		}

		return machine_arch_buf;
	}
#elif defined(MACHINE_ARCH)
	return MACHINE_ARCH;
#elif defined(MAKE_MACHINE_ARCH)
	return MAKE_MACHINE_ARCH;
#else
	return "unknown";
#endif
#endif
}

#ifndef NO_PWD_OVERRIDE
/*
 * All this code is so that we know where we are when we start up
 * on a different machine with pmake.
 *
 * XXX: Make no longer has "local" and "remote" mode.  Is this code still
 * necessary?
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
	FStr makeobjdir;
	struct stat pwd_st;

	if (ignorePWD || (pwd = getenv("PWD")) == NULL)
		return;

	if (Var_Exists(SCOPE_CMDLINE, "MAKEOBJDIRPREFIX"))
		return;

	makeobjdir = Var_Value(SCOPE_CMDLINE, "MAKEOBJDIR");
	if (makeobjdir.str != NULL && strchr(makeobjdir.str, '$') != NULL)
		goto ignore_pwd;

	if (stat(pwd, &pwd_st) == 0 &&
	    curdir_st->st_ino == pwd_st.st_ino &&
	    curdir_st->st_dev == pwd_st.st_dev)
		(void)strncpy(curdir, pwd, MAXPATHLEN);

ignore_pwd:
	FStr_Done(&makeobjdir);
}
#endif

/*
 * Find the .OBJDIR.  If MAKEOBJDIRPREFIX, or failing that, MAKEOBJDIR is set
 * in the environment, try only that value and fall back to .CURDIR if it
 * does not exist.
 *
 * Otherwise, try _PATH_OBJDIR.MACHINE-MACHINE_ARCH, _PATH_OBJDIR.MACHINE,
 * and finally _PATH_OBJDIRPREFIX`pwd`, in that order.  If none of these
 * paths exist, just use .CURDIR.
 */
static void
InitObjdir(const char *machine, const char *machine_arch)
{
	bool writable;

	Dir_InitCur(curdir);
	writable = GetBooleanExpr("${MAKE_OBJDIR_CHECK_WRITABLE}", true);
	(void)Main_SetObjdir(false, "%s", curdir);

	if (!SetVarObjdir(writable, "MAKEOBJDIRPREFIX", curdir) &&
	    !SetVarObjdir(writable, "MAKEOBJDIR", "") &&
	    !Main_SetObjdir(writable, "%s.%s-%s", _PATH_OBJDIR, machine, machine_arch) &&
	    !Main_SetObjdir(writable, "%s.%s", _PATH_OBJDIR, machine) &&
	    !Main_SetObjdir(writable, "%s", _PATH_OBJDIR))
		(void)Main_SetObjdir(writable, "%s%s", _PATH_OBJDIRPREFIX, curdir);
}

/* get rid of resource limit on file descriptors */
static void
UnlimitFiles(void)
{
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rl;
	if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
	    rl.rlim_cur != rl.rlim_max) {
#ifdef BMAKE_NOFILE_MAX
		if (BMAKE_NOFILE_MAX < rl.rlim_max)
			rl.rlim_cur = BMAKE_NOFILE_MAX;
		else
#endif
		rl.rlim_cur = rl.rlim_max;
		(void)setrlimit(RLIMIT_NOFILE, &rl);
	}
#endif
}

static void
CmdOpts_Init(void)
{
	opts.compatMake = false;
	memset(&opts.debug, 0, sizeof(opts.debug));
	/* opts.debug_file has already been initialized earlier */
	opts.strict = false;
	opts.debugVflag = false;
	opts.checkEnvFirst = false;
	Lst_Init(&opts.makefiles);
	opts.ignoreErrors = false;	/* Pay attention to non-zero returns */
	opts.maxJobs = 1;
	opts.keepgoing = false;		/* Stop on error */
	opts.noRecursiveExecute = false; /* Execute all .MAKE targets */
	opts.noExecute = false;		/* Execute all commands */
	opts.query = false;
	opts.noBuiltins = false;	/* Read the built-in rules */
	opts.silent = false;		/* Print commands as executed */
	opts.touch = false;
	opts.printVars = PVM_NONE;
	Lst_Init(&opts.variables);
	opts.parseWarnFatal = false;
	opts.enterFlag = false;
	opts.varNoExportEnv = false;
	Lst_Init(&opts.create);
}

/*
 * Initialize MAKE and .MAKE to the path of the executable, so that it can be
 * found by execvp(3) and the shells, even after a chdir.
 *
 * If it's a relative path and contains a '/', resolve it to an absolute path.
 * Otherwise keep it as is, assuming it will be found in the PATH.
 */
static void
InitVarMake(const char *argv0)
{
	const char *make = argv0;

	if (argv0[0] != '/' && strchr(argv0, '/') != NULL) {
		char pathbuf[MAXPATHLEN];
		const char *abspath = cached_realpath(argv0, pathbuf);
		struct stat st;
		if (abspath != NULL && abspath[0] == '/' &&
		    stat(make, &st) == 0)
			make = abspath;
	}

	Global_Set("MAKE", make);
	Global_Set(".MAKE", make);
}

/*
 * Add the directories from the colon-separated syspath to defSysIncPath.
 * After returning, the contents of syspath is unspecified.
 */
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
	if (syspath == NULL || syspath[0] == '\0')
		syspath = defsyspath;
	else
		syspath = bmake_strdup(syspath);

	for (start = syspath; *start != '\0'; start = cp) {
		for (cp = start; *cp != '\0' && *cp != ':'; cp++)
			continue;
		if (*cp == ':')
			*cp++ = '\0';

		/* look for magic parent directory search string */
		if (strncmp(start, ".../", 4) == 0) {
			char *dir = Dir_FindHereOrAbove(curdir, start + 4);
			if (dir != NULL) {
				(void)SearchPath_Add(defSysIncPath, dir);
				free(dir);
			}
		} else {
			(void)SearchPath_Add(defSysIncPath, start);
		}
	}

	if (syspath != defsyspath)
		free(syspath);
}

static void
ReadBuiltinRules(void)
{
	StringListNode *ln;
	StringList sysMkFiles = LST_INIT;

	SearchPath_Expand(
	    Lst_IsEmpty(&sysIncPath->dirs) ? defSysIncPath : sysIncPath,
	    _PATH_DEFSYSMK,
	    &sysMkFiles);
	if (Lst_IsEmpty(&sysMkFiles))
		Fatal("%s: no system rules (%s).", progname, _PATH_DEFSYSMK);

	for (ln = sysMkFiles.first; ln != NULL; ln = ln->next)
		if (ReadMakefile(ln->datum))
			break;

	if (ln == NULL)
		Fatal("%s: cannot open %s.",
		    progname, (const char *)sysMkFiles.first->datum);

	Lst_DoneCall(&sysMkFiles, free);
}

static void
InitMaxJobs(void)
{
	char *value;
	int n;

	if (forceJobs || opts.compatMake ||
	    !Var_Exists(SCOPE_GLOBAL, ".MAKE.JOBS"))
		return;

	value = Var_Subst("${.MAKE.JOBS}", SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */
	n = (int)strtol(value, NULL, 0);
	if (n < 1) {
		(void)fprintf(stderr,
		    "%s: illegal value for .MAKE.JOBS "
		    "-- must be positive integer!\n",
		    progname);
		exit(2);	/* Not 1 so -q can distinguish error */
	}

	if (n != opts.maxJobs) {
		Global_Append(MAKEFLAGS, "-j");
		Global_Append(MAKEFLAGS, value);
	}

	opts.maxJobs = n;
	maxJobTokens = opts.maxJobs;
	forceJobs = true;
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
	if (!Var_Exists(SCOPE_CMDLINE, "VPATH"))
		return;

	vpath = Var_Subst("${VPATH}", SCOPE_CMDLINE, VARE_WANTRES);
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
		(void)SearchPath_Add(&dirSearchPath, path);
		*cp = savec;
		path = cp + 1;
	} while (savec == ':');
	free(vpath);
}

static void
ReadAllMakefiles(const StringList *makefiles)
{
	StringListNode *ln;

	for (ln = makefiles->first; ln != NULL; ln = ln->next) {
		const char *fname = ln->datum;
		if (!ReadMakefile(fname))
			Fatal("%s: cannot open %s.", progname, fname);
	}
}

static void
ReadFirstDefaultMakefile(void)
{
	StringList makefiles = LST_INIT;
	StringListNode *ln;
	char *prefs = Var_Subst("${.MAKE.MAKEFILE_PREFERENCE}",
	    SCOPE_CMDLINE, VARE_WANTRES);
	/* TODO: handle errors */

	(void)str2Lst_Append(&makefiles, prefs);

	for (ln = makefiles.first; ln != NULL; ln = ln->next)
		if (ReadMakefile(ln->datum))
			break;

	Lst_Done(&makefiles);
	free(prefs);
}

/*
 * Initialize variables such as MAKE, MACHINE, .MAKEFLAGS.
 * Initialize a few modules.
 * Parse the arguments from MAKEFLAGS and the command line.
 */
static void
main_Init(int argc, char **argv)
{
	struct stat sa;
	const char *machine;
	const char *machine_arch;
	char *syspath = getenv("MAKESYSPATH");
	struct utsname utsname;

	/* default to writing debug to stderr */
	opts.debug_file = stderr;

	Str_Intern_Init();
	HashTable_Init(&cached_realpaths);

#ifdef SIGINFO
	(void)bmake_signal(SIGINFO, siginfo);
#endif

	InitRandom();

	progname = str_basename(argv[0]);

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
	machine = InitVarMachine(&utsname);
	machine_arch = InitVarMachineArch();

	myPid = getpid();	/* remember this for vFork() */

	/*
	 * Just in case MAKEOBJDIR wants us to do something tricky.
	 */
	Targ_Init();
	Var_Init();
	Global_Set_ReadOnly(".MAKE.OS", utsname.sysname);
	Global_Set("MACHINE", machine);
	Global_Set("MACHINE_ARCH", machine_arch);
#ifdef MAKE_VERSION
	Global_Set("MAKE_VERSION", MAKE_VERSION);
#endif
	Global_Set_ReadOnly(".newline", "\n");	/* handy for :@ loops */
#ifndef MAKEFILE_PREFERENCE_LIST
	/* This is the traditional preference for makefiles. */
# define MAKEFILE_PREFERENCE_LIST "makefile Makefile"
#endif
	Global_Set(".MAKE.MAKEFILE_PREFERENCE", MAKEFILE_PREFERENCE_LIST);
	Global_Set(".MAKE.DEPENDFILE", ".depend");
	/* Tell makefiles like jobs.mk whether we support -jC */
#ifdef _SC_NPROCESSORS_ONLN
	Global_Set_ReadOnly(".MAKE.JOBS.C", "yes");
#else
	Global_Set_ReadOnly(".MAKE.JOBS.C", "no");
#endif

	CmdOpts_Init();
	allPrecious = false;	/* Remove targets when interrupted */
	deleteOnError = false;	/* Historical default behavior */
	jobsRunning = false;

	maxJobTokens = opts.maxJobs;
	ignorePWD = false;

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
	Global_Set(MAKEFLAGS, "");
	Global_Set(".MAKEOVERRIDES", "");
	Global_Set("MFLAGS", "");
	Global_Set(".ALLTARGETS", "");
	Var_Set(SCOPE_CMDLINE, ".MAKE.LEVEL.ENV", MAKE_LEVEL_ENV);

	/* Set some other useful variables. */
	{
		char buf[64], *ep = getenv(MAKE_LEVEL_ENV);

		makelevel = ep != NULL && ep[0] != '\0' ? atoi(ep) : 0;
		if (makelevel < 0)
			makelevel = 0;
		snprintf(buf, sizeof buf, "%d", makelevel);
		Global_Set(".MAKE.LEVEL", buf);
		snprintf(buf, sizeof buf, "%u", myPid);
		Global_Set_ReadOnly(".MAKE.PID", buf);
		snprintf(buf, sizeof buf, "%u", getppid());
		Global_Set_ReadOnly(".MAKE.PPID", buf);
		snprintf(buf, sizeof buf, "%u", getuid());
		Global_Set_ReadOnly(".MAKE.UID", buf);
		snprintf(buf, sizeof buf, "%u", getgid());
		Global_Set_ReadOnly(".MAKE.GID", buf);
	}
	if (makelevel > 0) {
		char pn[1024];
		snprintf(pn, sizeof pn, "%s[%d]", progname, makelevel);
		progname = bmake_strdup(pn);
	}

#ifdef USE_META
	meta_init();
#endif
	Dir_Init();

#ifdef POSIX
	{
		char *makeflags = explode(getenv("MAKEFLAGS"));
		Main_ParseArgLine(makeflags);
		free(makeflags);
	}
#else
	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
	Main_ParseArgLine(getenv("MAKE"));
#endif

	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "%s: getcwd: %s.\n",
		    progname, strerror(errno));
		exit(2);
	}

	MainParseArgs(argc, argv);

	if (opts.enterFlag)
		printf("%s: Entering directory `%s'\n", progname, curdir);

	if (stat(curdir, &sa) == -1) {
		(void)fprintf(stderr, "%s: %s: %s.\n",
		    progname, curdir, strerror(errno));
		exit(2);
	}

#ifndef NO_PWD_OVERRIDE
	HandlePWD(&sa);
#endif
	Global_Set(".CURDIR", curdir);

	InitObjdir(machine, machine_arch);

	Arch_Init();
	Suff_Init();
	Trace_Init(tracefile);

	defaultNode = NULL;
	(void)time(&now);

	Trace_Log(MAKESTART, NULL);

	InitVarTargets();

	InitDefSysIncPath(syspath);
}

/*
 * Read the system makefile followed by either makefile, Makefile or the
 * files given by the -f option. Exit on parse errors.
 */
static void
main_ReadFiles(void)
{

	if (Lst_IsEmpty(&sysIncPath->dirs))
		SearchPath_AddAll(sysIncPath, defSysIncPath);

	Dir_SetSYSPATH();
	if (!opts.noBuiltins)
		ReadBuiltinRules();

	posix_state = PS_MAYBE_NEXT_LINE;
	if (!Lst_IsEmpty(&opts.makefiles))
		ReadAllMakefiles(&opts.makefiles);
	else
		ReadFirstDefaultMakefile();
}

/* Compute the dependency graph. */
static void
main_PrepareMaking(void)
{
	/* In particular suppress .depend for '-r -V .OBJDIR -f /dev/null' */
	if (!opts.noBuiltins || opts.printVars == PVM_NONE) {
		makeDependfile = Var_Subst("${.MAKE.DEPENDFILE}",
		    SCOPE_CMDLINE, VARE_WANTRES);
		if (makeDependfile[0] != '\0') {
			/* TODO: handle errors */
			doing_depend = true;
			(void)ReadMakefile(makeDependfile);
			doing_depend = false;
		}
	}

	if (enterFlagObj)
		printf("%s: Entering directory `%s'\n", progname, objdir);

	MakeMode();

	{
		FStr makeflags = Var_Value(SCOPE_GLOBAL, MAKEFLAGS);
		Global_Append("MFLAGS", makeflags.str);
		FStr_Done(&makeflags);
	}

	InitMaxJobs();

	if (!opts.compatMake && !forceJobs)
		opts.compatMake = true;

	if (!opts.compatMake)
		Job_ServerStart(maxJobTokens, jp_0, jp_1);
	DEBUG5(JOB, "job_pipe %d %d, maxjobs %d, tokens %d, compat %d\n",
	    jp_0, jp_1, opts.maxJobs, maxJobTokens, opts.compatMake ? 1 : 0);

	if (opts.printVars == PVM_NONE)
		Main_ExportMAKEFLAGS(true);	/* initial export */

	InitVpath();

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_ExtendPaths();

	/*
	 * Propagate attributes through :: dependency lists.
	 */
	Targ_Propagate();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);
}

/*
 * Make the targets.
 * If the -v or -V options are given, print variables instead.
 * Return whether any of the targets is out-of-date.
 */
static bool
main_Run(void)
{
	if (opts.printVars != PVM_NONE) {
		/* print the values of any variables requested by the user */
		doPrintVars();
		return false;
	} else {
		return runTargets();
	}
}

/* Clean up after making the targets. */
static void
main_CleanUp(void)
{
#ifdef CLEANUP
	Lst_DoneCall(&opts.variables, free);
	Lst_DoneCall(&opts.makefiles, free);
	Lst_DoneCall(&opts.create, free);
#endif

	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	Trace_Log(MAKEEND, NULL);

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
	Str_Intern_End();
}

/* Determine the exit code. */
static int
main_Exit(bool outOfDate)
{
	if (opts.strict && (main_errors > 0 || Parse_NumErrors() > 0))
		return 2;	/* Not 1 so -q can distinguish error */
	return outOfDate ? 1 : 0;
}

int
main(int argc, char **argv)
{
	bool outOfDate;

	main_Init(argc, argv);
	main_ReadFiles();
	main_PrepareMaking();
	outOfDate = main_Run();
	main_CleanUp();
	return main_Exit(outOfDate);
}

/*
 * Open and parse the given makefile, with all its side effects.
 * Return false if the file could not be opened.
 */
static bool
ReadMakefile(const char *fname)
{
	int fd;
	char *name, *path = NULL;

	if (strcmp(fname, "-") == 0) {
		Parse_File("(stdin)", -1);
		Var_Set(SCOPE_INTERNAL, "MAKEFILE", "");
	} else {
		/* if we've chdir'd, rebuild the path name */
		if (strcmp(curdir, objdir) != 0 && *fname != '/') {
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
		if (name == NULL) {
			SearchPath *sysInc = Lst_IsEmpty(&sysIncPath->dirs)
			    ? defSysIncPath : sysIncPath;
			name = Dir_FindFile(fname, sysInc);
		}
		if (name == NULL || (fd = open(name, O_RDONLY)) == -1) {
			free(name);
			free(path);
			return false;
		}
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:
		if (!doing_depend)
			Var_Set(SCOPE_INTERNAL, "MAKEFILE", fname);
		Parse_File(fname, fd);
	}
	free(path);
	return true;
}

/*
 * Execute the command in cmd, and return its output (only stdout, not
 * stderr, possibly empty).  In the output, replace newlines with spaces.
 */
char *
Cmd_Exec(const char *cmd, char **error)
{
	const char *args[4];	/* Arguments for invoking the shell */
	int pipefds[2];
	int cpid;		/* Child PID */
	int pid;		/* PID from wait() */
	int status;		/* command exit status */
	Buffer buf;		/* buffer to store the result */
	ssize_t bytes_read;
	char *output;
	char *cp;
	int saved_errno;

	if (shellName == NULL)
		Shell_Init();

	args[0] = shellName;
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;
	DEBUG1(VAR, "Capturing the output of command \"%s\"\n", cmd);

	if (pipe(pipefds) == -1) {
		*error = str_concat3(
		    "Couldn't create pipe for \"", cmd, "\"");
		return bmake_strdup("");
	}

	Var_ReexportVars();

	switch (cpid = vfork()) {
	case 0:
		(void)close(pipefds[0]);
		(void)dup2(pipefds[1], STDOUT_FILENO);
		(void)close(pipefds[1]);

		(void)execv(shellPath, UNCONST(args));
		_exit(1);
		/* NOTREACHED */

	case -1:
		*error = str_concat3("Couldn't exec \"", cmd, "\"");
		return bmake_strdup("");
	}

	(void)close(pipefds[1]);	/* No need for the writing half */

	saved_errno = 0;
	Buf_Init(&buf);

	do {
		char result[BUFSIZ];
		bytes_read = read(pipefds[0], result, sizeof result);
		if (bytes_read > 0)
			Buf_AddBytes(&buf, result, (size_t)bytes_read);
	} while (bytes_read > 0 || (bytes_read == -1 && errno == EINTR));
	if (bytes_read == -1)
		saved_errno = errno;

	(void)close(pipefds[0]); /* Close the input side of the pipe. */

	while ((pid = waitpid(cpid, &status, 0)) != cpid && pid >= 0)
		JobReapChild(pid, status, false);

	if (Buf_EndsWith(&buf, '\n'))
		buf.data[buf.len - 1] = '\0';

	output = Buf_DoneData(&buf);
	for (cp = output; *cp != '\0'; cp++)
		if (*cp == '\n')
			*cp = ' ';

	if (WIFSIGNALED(status))
		*error = str_concat3("\"", cmd, "\" exited on a signal");
	else if (WEXITSTATUS(status) != 0)
		*error = str_concat3(
		    "\"", cmd, "\" returned non-zero status");
	else if (saved_errno != 0)
		*error = str_concat3(
		    "Couldn't read shell's output for \"", cmd, "\"");
	else
		*error = NULL;
	return output;
}

/*
 * Print a printf-style error message.
 *
 * In default mode, this error message has no consequences, for compatibility
 * reasons, in particular it does not affect the exit status.  Only in lint
 * mode (-dL) it does.
 */
void
Error(const char *fmt, ...)
{
	va_list ap;
	FILE *f;

	f = opts.debug_file;
	if (f == stdout)
		f = stderr;
	(void)fflush(stdout);

	for (;;) {
		fprintf(f, "%s: ", progname);
		va_start(ap, fmt);
		(void)vfprintf(f, fmt, ap);
		va_end(ap);
		(void)fprintf(f, "\n");
		(void)fflush(f);
		if (f == stderr)
			break;
		f = stderr;
	}
	main_errors++;
}

/*
 * Wait for any running jobs to finish, then produce an error message,
 * finally exit immediately.
 *
 * Exiting immediately differs from Parse_Error, which exits only after the
 * current top-level makefile has been parsed completely.
 */
void
Fatal(const char *fmt, ...)
{
	va_list ap;

	if (jobsRunning)
		Job_Wait();

	(void)fflush(stdout);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);
	PrintStackTrace(true);

	PrintOnError(NULL, "\n");

	if (DEBUG(GRAPH2) || DEBUG(GRAPH3))
		Targ_PrintGraph(2);
	Trace_Log(MAKEERROR, NULL);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Major exception once jobs are being created.
 * Kills all jobs, prints a message and exits.
 */
void
Punt(const char *fmt, ...)
{
	va_list ap;

	(void)fflush(stdout);
	(void)fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	PrintOnError(NULL, "\n");

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
	Trace_Log(MAKEERROR, NULL);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Called when aborting due to errors in child shell to signal abnormal exit.
 * The program exits.
 * Errors is the number of errors encountered in Make_Make.
 */
void
Finish(int errs)
{
	if (shouldDieQuietly(NULL, -1))
		exit(2);
	Fatal("%d error%s", errs, errs == 1 ? "" : "s");
}

int
unlink_file(const char *file)
{
	struct stat st;

	if (lstat(file, &st) == -1)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		/*
		 * POSIX says for unlink: "The path argument shall not name
		 * a directory unless [...]".
		 */
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
		/* XXX: Should this EAGAIN be EINTR? */
		if (written == -1 && errno == EAGAIN)
			continue;
		if (written == -1)
			break;
		mem += written;
		n -= (size_t)written;
	}
}

/* Print why exec failed, avoiding stdio. */
void MAKE_ATTR_DEAD
execDie(const char *af, const char *av)
{
	Buffer buf;

	Buf_Init(&buf);
	Buf_AddStr(&buf, progname);
	Buf_AddStr(&buf, ": ");
	Buf_AddStr(&buf, af);
	Buf_AddStr(&buf, "(");
	Buf_AddStr(&buf, av);
	Buf_AddStr(&buf, ") failed (");
	Buf_AddStr(&buf, strerror(errno));
	Buf_AddStr(&buf, ")\n");

	write_all(STDERR_FILENO, buf.data, buf.len);

	Buf_Done(&buf);
	_exit(1);
}

static void
purge_relative_cached_realpaths(void)
{
	HashEntry *he, *nhe;
	HashIter hi;

	HashIter_Init(&hi, &cached_realpaths);
	he = HashIter_Next(&hi);
	while (he != NULL) {
		nhe = HashIter_Next(&hi);
		if (he->key[0] != '/') {
			DEBUG1(DIR, "cached_realpath: purging %s\n", he->key);
			HashTable_DeleteEntry(&cached_realpaths, he);
			/*
			 * XXX: What about the allocated he->value? Either
			 * free them or document why they cannot be freed.
			 */
		}
		he = nhe;
	}
}

const char *
cached_realpath(const char *pathname, char *resolved)
{
	const char *rp;

	if (pathname == NULL || pathname[0] == '\0')
		return NULL;

	rp = HashTable_FindValue(&cached_realpaths, pathname);
	if (rp != NULL) {
		/* a hit */
		strncpy(resolved, rp, MAXPATHLEN);
		resolved[MAXPATHLEN - 1] = '\0';
		return resolved;
	}

	rp = realpath(pathname, resolved);
	if (rp != NULL) {
		HashTable_Set(&cached_realpaths, pathname, bmake_strdup(rp));
		DEBUG2(DIR, "cached_realpath: %s -> %s\n", pathname, rp);
		return resolved;
	}

	/* should we negative-cache? */
	return NULL;
}

/*
 * Return true if we should die without noise.
 * For example our failing child was a sub-make or failure happened elsewhere.
 */
bool
shouldDieQuietly(GNode *gn, int bf)
{
	static int quietly = -1;

	if (quietly < 0) {
		if (DEBUG(JOB) ||
		    !GetBooleanExpr("${.MAKE.DIE_QUIETLY}", true))
			quietly = 0;
		else if (bf >= 0)
			quietly = bf;
		else
			quietly = (gn != NULL && (gn->type & OP_MAKE)) ? 1 : 0;
	}
	return quietly != 0;
}

static void
SetErrorVars(GNode *gn)
{
	StringListNode *ln;

	/*
	 * We can print this even if there is no .ERROR target.
	 */
	Global_Set(".ERROR_TARGET", gn->name);
	Global_Delete(".ERROR_CMD");

	for (ln = gn->commands.first; ln != NULL; ln = ln->next) {
		const char *cmd = ln->datum;

		if (cmd == NULL)
			break;
		Global_Append(".ERROR_CMD", cmd);
	}
}

/*
 * Print some helpful information in case of an error.
 * The caller should exit soon after calling this function.
 */
void
PrintOnError(GNode *gn, const char *msg)
{
	static GNode *errorNode = NULL;

	if (DEBUG(HASH)) {
		Targ_Stats();
		Var_Stats();
	}

	if (errorNode != NULL)
		return;		/* we've been here! */

	printf("%s%s: stopped in %s\n", msg, progname, curdir);

	/* we generally want to keep quiet if a sub-make died */
	if (shouldDieQuietly(gn, -1))
		return;

	if (gn != NULL)
		SetErrorVars(gn);

	{
		char *errorVarsValues = Var_Subst(
		    "${MAKE_PRINT_VAR_ON_ERROR:@v@$v='${$v}'\n@}",
		    SCOPE_GLOBAL, VARE_WANTRES);
		/* TODO: handle errors */
		printf("%s", errorVarsValues);
		free(errorVarsValues);
	}

	fflush(stdout);

	/*
	 * Finally, see if there is a .ERROR target, and run it if so.
	 */
	errorNode = Targ_FindNode(".ERROR");
	if (errorNode != NULL) {
		errorNode->type |= OP_SPECIAL;
		Compat_Make(errorNode, errorNode);
	}
}

void
Main_ExportMAKEFLAGS(bool first)
{
	static bool once = true;
	char *flags;

	if (once != first)
		return;
	once = false;

	flags = Var_Subst(
	    "${.MAKEFLAGS} ${.MAKEOVERRIDES:O:u:@v@$v=${$v:Q}@}",
	    SCOPE_CMDLINE, VARE_WANTRES);
	/* TODO: handle errors */
	if (flags[0] != '\0') {
#ifdef POSIX
		setenv("MAKEFLAGS", flags, 1);
#else
		setenv("MAKE", flags, 1);
#endif
	}
}

char *
getTmpdir(void)
{
	static char *tmpdir = NULL;
	struct stat st;

	if (tmpdir != NULL)
		return tmpdir;

	/* Honor $TMPDIR if it is valid, strip a trailing '/'. */
	tmpdir = Var_Subst("${TMPDIR:tA:U" _PATH_TMP ":S,/$,,W}/",
	    SCOPE_GLOBAL, VARE_WANTRES);
	/* TODO: handle errors */

	if (stat(tmpdir, &st) < 0 || !S_ISDIR(st.st_mode)) {
		free(tmpdir);
		tmpdir = bmake_strdup(_PATH_TMP);
	}
	return tmpdir;
}

/*
 * Create and open a temp file using "pattern".
 * If out_fname is provided, set it to a copy of the filename created.
 * Otherwise unlink the file once open.
 */
int
mkTempFile(const char *pattern, char *tfile, size_t tfile_sz)
{
	static char *tmpdir = NULL;
	char tbuf[MAXPATHLEN];
	int fd;

	if (pattern == NULL)
		pattern = TMPPAT;
	if (tmpdir == NULL)
		tmpdir = getTmpdir();
	if (tfile == NULL) {
		tfile = tbuf;
		tfile_sz = sizeof tbuf;
	}

	if (pattern[0] == '/')
		snprintf(tfile, tfile_sz, "%s", pattern);
	else
		snprintf(tfile, tfile_sz, "%s%s", tmpdir, pattern);

	if ((fd = mkstemp(tfile)) < 0)
		Punt("Could not create temporary file %s: %s", tfile,
		    strerror(errno));
	if (tfile == tbuf)
		unlink(tfile);	/* we just want the descriptor */

	return fd;
}

/*
 * Convert a string representation of a boolean into a boolean value.
 * Anything that looks like "No", "False", "Off", "0" etc. is false,
 * the empty string is the fallback, everything else is true.
 */
bool
ParseBoolean(const char *s, bool fallback)
{
	char ch = ch_tolower(s[0]);
	if (ch == '\0')
		return fallback;
	if (ch == '0' || ch == 'f' || ch == 'n')
		return false;
	if (ch == 'o')
		return ch_tolower(s[1]) != 'f';
	return true;
}
