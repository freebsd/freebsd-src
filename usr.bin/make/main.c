/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * @(#)main.c      8.3 (Berkeley) 3/19/94
 */

#ifndef lint
#if 0
static char copyright[] =
"@(#) Copyright (c) 1988, 1989, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
 */

#ifndef MACHINE
#include <sys/utsname.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arch.h"
#include "buf.h"
#include "compat.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "job.h"
#include "make.h"
#include "nonints.h"
#include "parse.h"
#include "pathnames.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

#define WANT_ENV_MKLVL	1
#define	MKLVL_MAXVAL	500
#define	MKLVL_ENVVAR	"__MKLVL__"

#define	MAKEFLAGS	".MAKEFLAGS"

/* Targets to be made */
Lst create = Lst_Initializer(create);

time_t			now;		/* Time at start of make */
struct GNode		*DEFAULT;	/* .DEFAULT node */
Boolean			allPrecious;	/* .PRECIOUS given on line by itself */

static Boolean		noBuiltins;	/* -r flag */

/* ordered list of makefiles to read */
static Lst makefiles = Lst_Initializer(makefiles);

static Boolean		expandVars;	/* fully expand printed variables */

/* list of variables to print */
static Lst variables = Lst_Initializer(variables);

int			maxJobs;	/* -j argument */
static Boolean          forceJobs;      /* -j argument given */
Boolean			compatMake;	/* -B argument */
Boolean			debug;		/* -d flag */
Boolean			noExecute;	/* -n flag */
Boolean			keepgoing;	/* -k flag */
Boolean			queryFlag;	/* -q flag */
Boolean			touchFlag;	/* -t flag */
Boolean			usePipes;	/* !-P flag */
Boolean			ignoreErrors;	/* -i flag */
Boolean			beSilent;	/* -s flag */
Boolean			beVerbose;	/* -v flag */
Boolean			oldVars;	/* variable substitution style */
Boolean			checkEnvFirst;	/* -e flag */

/* (-E) vars to override from env */
Lst envFirstVars = Lst_Initializer(envFirstVars);

Boolean			jobsRunning;	/* TRUE if the jobs might be running */

static void		MainParseArgs(int, char **);
char			*chdir_verify_path(const char *, char *);
static int		ReadMakefile(const void *, const void *);
static void		usage(void);

static char *curdir;			/* startup directory */
static char *objdir;			/* where we chdir'ed to */

/*
 * Append a flag with an optional argument to MAKEFLAGS and MFLAGS
 */
static void
MFLAGS_append(const char *flag, char *arg)
{
	char *str;

	Var_Append(MAKEFLAGS, flag, VAR_GLOBAL);
	if (arg != NULL) {
		str = MAKEFLAGS_quote(arg);
		Var_Append(MAKEFLAGS, str, VAR_GLOBAL);
		free(str);
	}

	Var_Append("MFLAGS", flag, VAR_GLOBAL);
	if (arg != NULL) {
		str = MAKEFLAGS_quote(arg);
		Var_Append("MFLAGS", str, VAR_GLOBAL);
		free(str);
	}
}

/*-
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(int argc, char **argv)
{
	int c;

	optind = 1;	/* since we're called more than once */
#define OPTFLAGS "BC:D:E:I:PSV:Xd:ef:ij:km:nqrstv"
rearg:	while((c = getopt(argc, argv, OPTFLAGS)) != -1) {
		switch(c) {
		case 'C':
			if (chdir(optarg) == -1)
				err(1, "chdir %s", optarg);
			break;
		case 'D':
			Var_Set(optarg, "1", VAR_GLOBAL);
			MFLAGS_append("-D", optarg);
			break;
		case 'I':
			Parse_AddIncludeDir(optarg);
			MFLAGS_append("-I", optarg);
			break;
		case 'V':
			Lst_AtEnd(&variables, estrdup(optarg));
			MFLAGS_append("-V", optarg);
			break;
		case 'X':
			expandVars = FALSE;
			break;
		case 'B':
			compatMake = TRUE;
			MFLAGS_append("-B", NULL);
			unsetenv("MAKE_JOBS_FIFO");
			break;
		case 'P':
			usePipes = FALSE;
			MFLAGS_append("-P", NULL);
			break;
		case 'S':
			keepgoing = FALSE;
			MFLAGS_append("-S", NULL);
			break;
		case 'd': {
			char *modules = optarg;

			for (; *modules; ++modules)
				switch (*modules) {
				case 'A':
					debug = ~0;
					break;
				case 'a':
					debug |= DEBUG_ARCH;
					break;
				case 'c':
					debug |= DEBUG_COND;
					break;
				case 'd':
					debug |= DEBUG_DIR;
					break;
				case 'f':
					debug |= DEBUG_FOR;
					break;
				case 'g':
					if (modules[1] == '1') {
						debug |= DEBUG_GRAPH1;
						++modules;
					}
					else if (modules[1] == '2') {
						debug |= DEBUG_GRAPH2;
						++modules;
					}
					break;
				case 'j':
					debug |= DEBUG_JOB;
					break;
				case 'l':
					debug |= DEBUG_LOUD;
					break;
				case 'm':
					debug |= DEBUG_MAKE;
					break;
				case 's':
					debug |= DEBUG_SUFF;
					break;
				case 't':
					debug |= DEBUG_TARG;
					break;
				case 'v':
					debug |= DEBUG_VAR;
					break;
				default:
					warnx("illegal argument to d option -- %c", *modules);
					usage();
				}
			MFLAGS_append("-d", optarg);
			break;
		}
		case 'E':
			Lst_AtEnd(&envFirstVars, estrdup(optarg));
			MFLAGS_append("-E", optarg);
			break;
		case 'e':
			checkEnvFirst = TRUE;
			MFLAGS_append("-e", NULL);
			break;
		case 'f':
			Lst_AtEnd(&makefiles, estrdup(optarg));
			break;
		case 'i':
			ignoreErrors = TRUE;
			MFLAGS_append("-i", NULL);
			break;
		case 'j': {
			char *endptr;

			forceJobs = TRUE;
			maxJobs = strtol(optarg, &endptr, 10);
			if (maxJobs <= 0 || *endptr != '\0') {
				warnx("illegal number, -j argument -- %s",
				    optarg);
				usage();
			}
			MFLAGS_append("-j", optarg);
			break;
		}
		case 'k':
			keepgoing = TRUE;
			MFLAGS_append("-k", NULL);
			break;
		case 'm':
			Dir_AddDir(&sysIncPath, optarg);
			MFLAGS_append("-m", optarg);
			break;
		case 'n':
			noExecute = TRUE;
			MFLAGS_append("-n", NULL);
			break;
		case 'q':
			queryFlag = TRUE;
			/* Kind of nonsensical, wot? */
			MFLAGS_append("-q", NULL);
			break;
		case 'r':
			noBuiltins = TRUE;
			MFLAGS_append("-r", NULL);
			break;
		case 's':
			beSilent = TRUE;
			MFLAGS_append("-s", NULL);
			break;
		case 't':
			touchFlag = TRUE;
			MFLAGS_append("-t", NULL);
			break;
		case 'v':
			beVerbose = TRUE;
			MFLAGS_append("-v", NULL);
			break;
		default:
		case '?':
			usage();
		}
	}

	oldVars = TRUE;

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (argv += optind, argc -= optind; *argv; ++argv, --argc)
		if (Parse_IsVar(*argv)) {
			char *ptr = MAKEFLAGS_quote(*argv);

			Var_Append(MAKEFLAGS, ptr, VAR_GLOBAL);
			free(ptr);

			Parse_DoVar(*argv, VAR_CMD);
		} else {
			if (!**argv)
				Punt("illegal (null) argument.");
			if (**argv == '-') {
				if ((*argv)[1])
					optind = 0;     /* -flag... */
				else
					optind = 1;     /* - */
				goto rearg;
			}
			Lst_AtEnd(&create, estrdup(*argv));
		}
}

/*-
 * Main_ParseArgLine --
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(char *line, int mflags)
{
	char **argv;			/* Manufactured argument vector */
	int argc;			/* Number of arguments in argv */

	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

	if (mflags)
		argv = MAKEFLAGS_break(line, &argc);
	else
		argv = brk_string(line, &argc, TRUE);

	MainParseArgs(argc, argv);
}

char *
chdir_verify_path(const char *path, char *obpath)
{
	struct stat sb;

	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		if (chdir(path) == -1 || getcwd(obpath, MAXPATHLEN) == NULL) {
			warn("warning: %s", path);
			return (NULL);
		}
		return (obpath);
	}

	return (NULL);
}

static void
catch_child(int sig __unused)
{
}

/*
 * In lieu of a good way to prevent every possible looping in
 * make(1), stop there from being more than MKLVL_MAXVAL processes forked
 * by make(1), to prevent a forkbomb from happening, in a dumb and
 * mechanical way.
 */
static void
check_make_level(void)
{
#ifdef WANT_ENV_MKLVL
	char	*value = getenv(MKLVL_ENVVAR);
	int	level = (value == NULL) ? 0 : atoi(value);

	if (level < 0) {
		errc(2, EAGAIN, "Invalid value for recursion level (%d).", level);
	} else if (level > MKLVL_MAXVAL) {
		errc(2, EAGAIN, "Max recursion level (%d) exceeded.", MKLVL_MAXVAL);
	} else {
		char new_value[32];
		sprintf(new_value, "%d", level + 1);
		setenv(MKLVL_ENVVAR, new_value, 1);
	}
#endif /* WANT_ENV_MKLVL */
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
	Boolean outOfDate = TRUE; 	/* FALSE if all targets up to date */
	char *p, *p1, *pathp;
	char *path;
	char mdpath[MAXPATHLEN];
	char obpath[MAXPATHLEN];
	char cdpath[MAXPATHLEN];
    	const char *machine = getenv("MACHINE");
	const char *machine_arch = getenv("MACHINE_ARCH");
	const char *machine_cpu = getenv("MACHINE_CPU");
	char *cp = NULL, *start;

					/* avoid faults on read-only strings */
	static char syspath[] = _PATH_DEFSYSPATH;

	{
	/*
	 * Catch SIGCHLD so that we get kicked out of select() when we
	 * need to look at a child.  This is only known to matter for the
	 * -j case (perhaps without -P).
	 *
	 * XXX this is intentionally misplaced.
	 */
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sa.sa_handler = catch_child;
	sigaction(SIGCHLD, &sa, NULL);
	}

	check_make_level();

#if DEFSHELL == 2
	/*
	 * Turn off ENV to make ksh happier.
	 */
	unsetenv("ENV");
#endif

#ifdef RLIMIT_NOFILE
	/*
	 * get rid of resource limit on file descriptors
	 */
	{
		struct rlimit rl;
		if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
		    rl.rlim_cur != rl.rlim_max) {
			rl.rlim_cur = rl.rlim_max;
			setrlimit(RLIMIT_NOFILE, &rl);
		}
	}
#endif

	/*
	 * PC-98 kernel sets the `i386' string to the utsname.machine and
	 * it cannot be distinguished from IBM-PC by uname(3).  Therefore,
	 * we check machine.ispc98 and adjust the machine variable before
	 * using usname(3) below.
	 * NOTE: machdep.ispc98 was defined on 1998/8/31. At that time,
	 * __FreeBSD_version was defined as 300003. So, this check can
	 * safely be done with any kernel with version > 300003.
	 */
	if (!machine) {
		int	ispc98;
		size_t	len;

		len = sizeof(ispc98);
		if (!sysctlbyname("machdep.ispc98", &ispc98, &len, NULL, 0)) {
			if (ispc98)
				machine = "pc98";
		}
	}

	/*
	 * Get the name of this type of MACHINE from utsname
	 * so we can share an executable for similar machines.
	 * (i.e. m68k: amiga hp300, mac68k, sun3, ...)
	 *
	 * Note that while MACHINE is decided at run-time,
	 * MACHINE_ARCH is always known at compile time.
	 */
	if (!machine) {
#ifndef MACHINE
	    struct utsname utsname;

	    if (uname(&utsname) == -1)
		    err(2, "uname");
	    machine = utsname.machine;
#else
	    machine = MACHINE;
#endif
	}

	if (!machine_arch) {
#ifndef MACHINE_ARCH
		machine_arch = "unknown";
#else
		machine_arch = MACHINE_ARCH;
#endif
	}

	/*
	 * Set machine_cpu to the minumum supported CPU revision based
	 * on the target architecture, if not already set.
	 */
	if (!machine_cpu) {
		if (!strcmp(machine_arch, "i386"))
			machine_cpu = "i386";
		else if (!strcmp(machine_arch, "alpha"))
			machine_cpu = "ev4";
		else
			machine_cpu = "unknown";
	}

	expandVars = TRUE;
	beSilent = FALSE;		/* Print commands as executed */
	ignoreErrors = FALSE;		/* Pay attention to non-zero returns */
	noExecute = FALSE;		/* Execute all commands */
	keepgoing = FALSE;		/* Stop on error */
	allPrecious = FALSE;		/* Remove targets when interrupted */
	queryFlag = FALSE;		/* This is not just a check-run */
	noBuiltins = FALSE;		/* Read the built-in rules */
	touchFlag = FALSE;		/* Actually update targets */
	usePipes = TRUE;		/* Catch child output in pipes */
	debug = 0;			/* No debug verbosity, please. */
	jobsRunning = FALSE;

	maxJobs = DEFMAXJOBS;
	forceJobs = FALSE;              /* No -j flag */
	compatMake = FALSE;		/* No compat mode */

	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */
	Dir_Init();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
	Parse_Init();		/* Need to initialize the paths of #include
				 * directories */
	Var_Init();		/* As well as the lists of variables for
				 * parsing arguments */
        str_init();

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_Set("MAKE", argv[0], VAR_GLOBAL);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL);
	Var_Set("MFLAGS", "", VAR_GLOBAL);
	Var_Set("MACHINE", machine, VAR_GLOBAL);
	Var_Set("MACHINE_ARCH", machine_arch, VAR_GLOBAL);
	Var_Set("MACHINE_CPU", machine_cpu, VAR_GLOBAL);
#ifdef MAKE_VERSION
	Var_Set("MAKE_VERSION", MAKE_VERSION, VAR_GLOBAL);
#endif

	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
	Main_ParseArgLine(getenv("MAKEFLAGS"), 1);

	MainParseArgs(argc, argv);

	/*
	 * Find where we are...
	 * All this code is so that we know where we are when we start up
	 * on a different machine with pmake.
	 */
	curdir = cdpath;
	if (getcwd(curdir, MAXPATHLEN) == NULL)
		err(2, NULL);

	{
	struct stat sa;

	if (stat(curdir, &sa) == -1)
	    err(2, "%s", curdir);
	}

	/*
	 * The object directory location is determined using the
	 * following order of preference:
	 *
	 *	1. MAKEOBJDIRPREFIX`cwd`
	 *	2. MAKEOBJDIR
	 *	3. _PATH_OBJDIR.${MACHINE}
	 *	4. _PATH_OBJDIR
	 *	5. _PATH_OBJDIRPREFIX`cwd`
	 *
	 * If one of the first two fails, use the current directory.
	 * If the remaining three all fail, use the current directory.
	 *
	 * Once things are initted,
	 * have to add the original directory to the search path,
	 * and modify the paths for the Makefiles apropriately.  The
	 * current directory is also placed as a variable for make scripts.
	 */
	if (!(pathp = getenv("MAKEOBJDIRPREFIX"))) {
		if (!(path = getenv("MAKEOBJDIR"))) {
			path = _PATH_OBJDIR;
			pathp = _PATH_OBJDIRPREFIX;
			snprintf(mdpath, MAXPATHLEN, "%s.%s",
					path, machine);
			if (!(objdir = chdir_verify_path(mdpath, obpath)))
				if (!(objdir=chdir_verify_path(path, obpath))) {
					snprintf(mdpath, MAXPATHLEN,
							"%s%s", pathp, curdir);
					if (!(objdir=chdir_verify_path(mdpath,
								       obpath)))
						objdir = curdir;
				}
		}
		else if (!(objdir = chdir_verify_path(path, obpath)))
			objdir = curdir;
	}
	else {
		snprintf(mdpath, MAXPATHLEN, "%s%s", pathp, curdir);
		if (!(objdir = chdir_verify_path(mdpath, obpath)))
			objdir = curdir;
	}
	Dir_InitDot();		/* Initialize the "." directory */
	if (objdir != curdir)
		Dir_AddDir(&dirSearchPath, curdir);
	Var_Set(".CURDIR", curdir, VAR_GLOBAL);
	Var_Set(".OBJDIR", objdir, VAR_GLOBAL);

	if (getenv("MAKE_JOBS_FIFO") != NULL)
		forceJobs = TRUE;
	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turned compatibility on
	 */
	if (!compatMake && !forceJobs)
		compatMake = TRUE;

	/*
	 * Initialize archive, target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Arch_Init();
	Targ_Init();
	Suff_Init();

	DEFAULT = NULL;
	time(&now);

	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(&create)) {
		LstNode *ln;

		for (ln = Lst_First(&create); ln != NULL; ln = Lst_Succ(ln)) {
			char *name = Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	} else
		Var_Set(".TARGETS", "", VAR_GLOBAL);


	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	if (Lst_IsEmpty(&sysIncPath)) {
		for (start = syspath; *start != '\0'; start = cp) {
			for (cp = start; *cp != '\0' && *cp != ':'; cp++)
				continue;
			if (*cp == '\0') {
				Dir_AddDir(&sysIncPath, start);
			} else {
				*cp++ = '\0';
				Dir_AddDir(&sysIncPath, start);
			}
		}
	}

	/*
	 * Read in the built-in rules first, followed by the specified
	 * makefile, if it was (makefile != (char *) NULL), or the default
	 * Makefile and makefile, in that order, if it wasn't.
	 */
	if (!noBuiltins) {
		/* Path of sys.mk */
		Lst sysMkPath = Lst_Initializer(sysMkPath);
		LstNode *ln;

		Dir_Expand(_PATH_DEFSYSMK, &sysIncPath, &sysMkPath);
		if (Lst_IsEmpty(&sysMkPath))
			Fatal("make: no system rules (%s).", _PATH_DEFSYSMK);
		ln = Lst_Find(&sysMkPath, NULL, ReadMakefile);
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
		Lst_Destroy(&sysMkPath, free);
	}

	if (!Lst_IsEmpty(&makefiles)) {
		LstNode *ln;

		ln = Lst_Find(&makefiles, NULL, ReadMakefile);
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
	} else if (!ReadMakefile("BSDmakefile", NULL))
	    if (!ReadMakefile("makefile", NULL))
		ReadMakefile("Makefile", NULL);

	ReadMakefile(".depend", NULL);

	/* Install all the flags into the MAKE envariable. */
	if (((p = Var_Value(MAKEFLAGS, VAR_GLOBAL, &p1)) != NULL) && *p)
		setenv("MAKEFLAGS", p, 1);
	free(p1);

	/*
	 * For compatibility, look at the directories in the VPATH variable
	 * and add them to the search path, if the variable is defined. The
	 * variable's value is in the same format as the PATH envariable, i.e.
	 * <directory>:<directory>:<directory>...
	 */
	if (Var_Exists("VPATH", VAR_CMD)) {
		/*
		 * GCC stores string constants in read-only memory, but
		 * Var_Subst will want to write this thing, so store it
		 * in an array
		 */
		static char VPATH[] = "${VPATH}";
		char *vpath;
		char *start;
		char *ptr;
		char savec;

		vpath = Var_Subst(NULL, VPATH, VAR_CMD, FALSE);
		start = vpath;
		do {
			/* skip to end of directory */
			for (ptr = start; *ptr != ':' && *ptr != '\0'; ptr++)
				;

			/* Save terminator character so know when to stop */
			savec = *ptr;
			*ptr = '\0';

			/* Add directory to search path */
			Dir_AddDir(&dirSearchPath, start);

			start = ptr + 1;
		} while (savec != '\0');
		free(vpath);
	}

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_DoPaths();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/* print the values of any variables requested by the user */
	if (!Lst_IsEmpty(&variables)) {
		LstNode *ln;

		for (ln = Lst_First(&variables); ln != NULL;
		    ln = Lst_Succ(ln)) {
			char *value;
			if (expandVars) {
				p1 = emalloc(strlen(Lst_Datum(ln)) + 1 + 3);
				/* This sprintf is safe, because of the malloc above */
				sprintf(p1, "${%s}", (char *)Lst_Datum(ln));
				value = Var_Subst(NULL, p1, VAR_GLOBAL, FALSE);
			} else {
				value = Var_Value(Lst_Datum(ln),
						  VAR_GLOBAL, &p1);
			}
			printf("%s\n", value ? value : "");
			if (p1)
				free(p1);
		}
	} else {

		/*
		 * Have now read the entire graph and need to make a list of targets
		 * to create. If none was given on the command line, we consult the
		 * parsing module to find the main target(s) to create.
		 */
		Lst targs = Lst_Initializer(targs);

		if (Lst_IsEmpty(&create))
			Parse_MainName(&targs);
		else
			Targ_FindList(&targs, &create, TARG_CREATE);

		if (!compatMake) {
			/*
			 * Initialize job module before traversing the graph, now that
			 * any .BEGIN and .END targets have been read.  This is done
			 * only if the -q flag wasn't given (to prevent the .BEGIN from
			 * being executed should it exist).
			 */
			if (!queryFlag) {
				Job_Init(maxJobs);
				jobsRunning = TRUE;
			}

			/* Traverse the graph, checking on all the targets */
			outOfDate = Make_Run(&targs);
		} else {
			/*
			 * Compat_Init will take care of creating all the targets as
			 * well as initializing the module.
			 */
			Compat_Run(&targs);
			outOfDate = 0;
		}
		Lst_Destroy(&targs, NOFREE);
	}

	Lst_Destroy(&variables, free);
	Lst_Destroy(&makefiles, free);
	Lst_Destroy(&create, free);

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	if (queryFlag && outOfDate)
		return (1);
	else
		return (0);
}

/*-
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	TRUE if ok. FALSE if couldn't open file.
 *
 * Side Effects:
 *	lots
 */
static Boolean
ReadMakefile(const void *p, const void *q __unused)
{
	char *fname;			/* makefile to read */
	FILE *stream;
	char *name, path[MAXPATHLEN];
	char *MAKEFILE;
	int setMAKEFILE;

	/* XXX - remove this once constification is done */
	fname = estrdup(p);

	if (!strcmp(fname, "-")) {
		Parse_File("(stdin)", stdin);
		Var_Set("MAKEFILE", "", VAR_GLOBAL);
	} else {
		setMAKEFILE = strcmp(fname, ".depend");

		/* if we've chdir'd, rebuild the path name */
		if (curdir != objdir && *fname != '/') {
			snprintf(path, MAXPATHLEN, "%s/%s", curdir, fname);
			/*
			 * XXX The realpath stuff breaks relative includes
			 * XXX in some cases.   The problem likely is in
			 * XXX parse.c where it does special things in
			 * XXX ParseDoInclude if the file is relateive
			 * XXX or absolute and not a system file.  There
			 * XXX it assumes that if the current file that's
			 * XXX being included is absolute, that any files
			 * XXX that it includes shouldn't do the -I path
			 * XXX stuff, which is inconsistant with historical
			 * XXX behavior.  However, I can't pentrate the mists
			 * XXX further, so I'm putting this workaround in
			 * XXX here until such time as the underlying bug
			 * XXX can be fixed.
			 */
#if THIS_BREAKS_THINGS
			if (realpath(path, path) != NULL &&
			    (stream = fopen(path, "r")) != NULL) {
				MAKEFILE = fname;
				fname = path;
				goto found;
			}
		} else if (realpath(fname, path) != NULL) {
			MAKEFILE = fname;
			fname = path;
			if ((stream = fopen(fname, "r")) != NULL)
				goto found;
		}
#else
			if ((stream = fopen(path, "r")) != NULL) {
				MAKEFILE = fname;
				fname = path;
				goto found;
			}
		} else {
			MAKEFILE = fname;
			if ((stream = fopen(fname, "r")) != NULL)
				goto found;
		}
#endif
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, &parseIncPath);
		if (!name)
			name = Dir_FindFile(fname, &sysIncPath);
		if (!name || !(stream = fopen(name, "r")))
			return (FALSE);
		MAKEFILE = fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:
		if (setMAKEFILE)
			Var_Set("MAKEFILE", MAKEFILE, VAR_GLOBAL);
		Parse_File(fname, stream);
		fclose(stream);
	}
	return (TRUE);
}

/*-
 * Cmd_Exec --
 *	Execute the command in cmd, and return the output of that command
 *	in a string.
 *
 * Results:
 *	A string containing the output of the command, or the empty string
 *	If error is not NULL, it contains the reason for the command failure
 *
 * Side Effects:
 *	The string must be freed by the caller.
 */
Buffer *
Cmd_Exec(char *cmd, const char **error)
{
    int 	fds[2];	    	/* Pipe streams */
    int 	cpid;	    	/* Child PID */
    int 	pid;	    	/* PID from wait() */
    int		status;		/* command exit status */
    Buffer	*buf;		/* buffer to store the result */
    ssize_t	rcnt;

    *error = NULL;
    buf = Buf_Init(0);

    if (shellPath == NULL)
	Shell_Init();
    /*
     * Open a pipe for fetching its output
     */
    if (pipe(fds) == -1) {
	*error = "Couldn't create pipe for \"%s\"";
	return (buf);
    }

    /*
     * Fork
     */
    switch (cpid = vfork()) {
    case 0:
	/*
	 * Close input side of pipe
	 */
	close(fds[0]);

	/*
	 * Duplicate the output stream to the shell's output, then
	 * shut the extra thing down. Note we don't fetch the error
	 * stream...why not? Why?
	 */
	dup2(fds[1], 1);
	close(fds[1]);

	{
	    char	*args[4];

	    /* Set up arguments for shell */
	    args[0] = shellName;
	    args[1] = "-c";
	    args[2] = cmd;
	    args[3] = NULL;

	    execv(shellPath, args);
	    _exit(1);
	    /*NOTREACHED*/
	}

    case -1:
	*error = "Couldn't exec \"%s\"";
	return (buf);

    default:
	/*
	 * No need for the writing half
	 */
	close(fds[1]);

	do {
	    char   result[BUFSIZ];
	    rcnt = read(fds[0], result, sizeof(result));
	    if (rcnt != -1)
		Buf_AddBytes(buf, (size_t)rcnt, (Byte *)result);
	} while (rcnt > 0 || (rcnt == -1 && errno == EINTR));

	if (rcnt == -1)
	    *error = "Error reading shell's output for \"%s\"";

	/*
	 * Close the input side of the pipe.
	 */
	close(fds[0]);

	/*
	 * Wait for the process to exit.
	 */
	while (((pid = wait(&status)) != cpid) && (pid >= 0))
	    continue;

	if (status)
	    *error = "\"%s\" returned non-zero status";

	Buf_StripNewlines(buf);

	break;
    }
    return (buf);
}

/*
 * usage --
 *	exit with usage message
 */
static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
"usage: make [-BPSXeiknqrstv] [-C directory] [-D variable] [-d flags]",
"            [-E variable] [-f makefile] [-I directory] [-j max_jobs]",
"            [-m directory] [-V variable] [variable=value] [target ...]");
	exit(2);
}
