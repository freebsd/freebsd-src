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

/*
 * main.c
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine
 *			Takes a line of arguments, breaks them and
 *			treats them as if they were given when first
 *			invoked. Used by the parse module to implement
 *			the .MFLAGS target.
 */

#ifndef MACHINE
#include <sys/utsname.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arch.h"
#include "buf.h"
#include "config.h"
#include "dir.h"
#include "globals.h"
#include "job.h"
#include "make.h"
#include "parse.h"
#include "pathnames.h"
#include "shell.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

extern char **environ;	/* XXX what header declares this variable? */

#define WANT_ENV_MKLVL	1
#define	MKLVL_MAXVAL	500
#define	MKLVL_ENVVAR	"__MKLVL__"

/* ordered list of makefiles to read */
static Lst makefiles = Lst_Initializer(makefiles);

/* list of variables to print */
static Lst variables = Lst_Initializer(variables);

static Boolean	expandVars;	/* fully expand printed variables */
static Boolean	noBuiltins;	/* -r flag */
static Boolean	forceJobs;      /* -j argument given */
static char	*curdir;	/* startup directory */
static char	*objdir;	/* where we chdir'ed to */

/* (-E) vars to override from env */
Lst envFirstVars = Lst_Initializer(envFirstVars);

/* Targets to be made */
Lst create = Lst_Initializer(create);

Boolean		allPrecious;	/* .PRECIOUS given on line by itself */
Boolean		beSilent;	/* -s flag */
Boolean		beVerbose;	/* -v flag */
Boolean		compatMake;	/* -B argument */
int		debug;		/* -d flag */
Boolean		ignoreErrors;	/* -i flag */
int		jobLimit;	/* -j argument */
Boolean		jobsRunning;	/* TRUE if the jobs might be running */
Boolean		keepgoing;	/* -k flag */
Boolean		noExecute;	/* -n flag */
Boolean		queryFlag;	/* -q flag */
Boolean		touchFlag;	/* -t flag */
Boolean		usePipes;	/* !-P flag */
uint32_t	warn_cmd;	/* command line warning flags */
uint32_t	warn_flags;	/* actual warning flags */
uint32_t	warn_nocmd;	/* command line no-warning flags */

time_t		now;		/* Time at start of make */
struct GNode	*DEFAULT;	/* .DEFAULT node */

/**
 * Exit with usage message.
 */
static void
usage(void)
{
	fprintf(stderr,
	    "usage: make [-BPSXeiknqrstv] [-C directory] [-D variable]\n"
	    "\t[-d flags] [-E variable] [-f makefile] [-I directory]\n"
	    "\t[-j max_jobs] [-m directory] [-V variable]\n"
	    "\t[variable=value] [target ...]\n");
	exit(2);
}

/**
 * MFLAGS_append
 *	Append a flag with an optional argument to MAKEFLAGS and MFLAGS
 */
static void
MFLAGS_append(const char *flag, char *arg)
{
	char *str;

	Var_Append(".MAKEFLAGS", flag, VAR_GLOBAL);
	if (arg != NULL) {
		str = MAKEFLAGS_quote(arg);
		Var_Append(".MAKEFLAGS", str, VAR_GLOBAL);
		free(str);
	}

	Var_Append("MFLAGS", flag, VAR_GLOBAL);
	if (arg != NULL) {
		str = MAKEFLAGS_quote(arg);
		Var_Append("MFLAGS", str, VAR_GLOBAL);
		free(str);
	}
}

/**
 * Main_ParseWarn
 *
 *	Handle argument to warning option.
 */
int
Main_ParseWarn(const char *arg, int iscmd)
{
	int i, neg;

	static const struct {
		const char	*option;
		uint32_t	flag;
	} options[] = {
		{ "dirsyntax",	WARN_DIRSYNTAX },
		{ NULL,		0 }
	};

	neg = 0;
	if (arg[0] == 'n' && arg[1] == 'o') {
		neg = 1;
		arg += 2;
	}

	for (i = 0; options[i].option != NULL; i++)
		if (strcmp(arg, options[i].option) == 0)
			break;

	if (options[i].option == NULL)
		/* unknown option */
		return (-1);

	if (iscmd) {
		if (!neg) {
			warn_cmd |= options[i].flag;
			warn_nocmd &= ~options[i].flag;
			warn_flags |= options[i].flag;
		} else {
			warn_nocmd |= options[i].flag;
			warn_cmd &= ~options[i].flag;
			warn_flags &= ~options[i].flag;
		}
	} else {
		if (!neg) {
			warn_flags |= (options[i].flag & ~warn_nocmd);
		} else {
			warn_flags &= ~(options[i].flag | warn_cmd);
		}
	}
	return (0);
}

/**
 * Open and parse the given makefile.
 *
 * Results:
 *	TRUE if ok. FALSE if couldn't open file.
 */
static Boolean
ReadMakefile(const char p[])
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
		Var_SetGlobal("MAKEFILE", "");
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
		name = Path_FindFile(fname, &parseIncPath);
		if (!name)
			name = Path_FindFile(fname, &sysIncPath);
		if (!name || !(stream = fopen(name, "r"))) {
			free(fname);
			return (FALSE);
		}
		MAKEFILE = fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:
		if (setMAKEFILE)
			Var_SetGlobal("MAKEFILE", MAKEFILE);
		Parse_File(fname, stream);
	}
	free(fname);
	return (TRUE);
}

/**
 * MainParseArgs
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(int argc, char **argv)
{
	int c;
	Boolean	found_dd = FALSE;

rearg:
	optind = 1;	/* since we're called more than once */
	optreset = 1;
#define OPTFLAGS "ABC:D:E:I:PSV:Xd:ef:ij:km:nqrstvx:"
	for (;;) {
		if ((optind < argc) && strcmp(argv[optind], "--") == 0) {
			found_dd = TRUE;
		}
		if ((c = getopt(argc, argv, OPTFLAGS)) == -1) {
			break;
		}
		switch(c) {

		case 'A':
			arch_fatal = FALSE;
			MFLAGS_append("-A", NULL);
			break;
		case 'C':
			if (chdir(optarg) == -1)
				err(1, "chdir %s", optarg);
			break;
		case 'D':
			Var_SetGlobal(optarg, "1");
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
					warnx("illegal argument to d option "
					    "-- %c", *modules);
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
			jobLimit = strtol(optarg, &endptr, 10);
			if (jobLimit <= 0 || *endptr != '\0') {
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
			Path_AddDir(&sysIncPath, optarg);
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
		case 'x':
			if (Main_ParseWarn(optarg, 1) != -1)
				MFLAGS_append("-x", optarg);
			break;
				
		default:
		case '?':
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	oldVars = TRUE;

	/*
	 * Parse the rest of the arguments.
	 *	o Check for variable assignments and perform them if so.
	 *	o Check for more flags and restart getopt if so.
	 *      o Anything else is taken to be a target and added
	 *	  to the end of the "create" list.
	 */
	for (; *argv != NULL; ++argv, --argc) {
		if (Parse_IsVar(*argv)) {
			char *ptr = MAKEFLAGS_quote(*argv);

			Var_Append(".MAKEFLAGS", ptr, VAR_GLOBAL);
			Parse_DoVar(*argv, VAR_CMD);
			free(ptr);

		} else if ((*argv)[0] == '-') {
			if ((*argv)[1] == '\0') {
				/*
				 * (*argv) is a single dash, so we
				 * just ignore it.
				 */
			} else if (found_dd) {
				/*
				 * Double dash has been found, ignore
				 * any more options.  But what do we do
				 * with it?  For now treat it like a target.
				 */
				Lst_AtEnd(&create, estrdup(*argv));
			} else {
				/*
				 * (*argv) is a -flag, so backup argv and
				 * argc.  getopt() expects options to start
				 * in the 2nd position.
				 */
				argc++;
				argv--;
				goto rearg;
			}

		} else if ((*argv)[0] == '\0') {
			Punt("illegal (null) argument.");

		} else {
			Lst_AtEnd(&create, estrdup(*argv));
		}
	}
}

/**
 * Main_ParseArgLine
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(char *line, int mflags)
{
	ArgArray	aa;

	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

	if (mflags)
		MAKEFLAGS_break(&aa, line);
	else
		brk_string(&aa, line, TRUE);

	MainParseArgs(aa.argc, aa.argv);
	ArgArray_Done(&aa);
}

static char *
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

/**
 * In lieu of a good way to prevent every possible looping in make(1), stop
 * there from being more than MKLVL_MAXVAL processes forked by make(1), to
 * prevent a forkbomb from happening, in a dumb and mechanical way.
 *
 * Side Effects:
 *	Creates or modifies enviornment variable MKLVL_ENVVAR via setenv().
 */
static void
check_make_level(void)
{
#ifdef WANT_ENV_MKLVL
	char	*value = getenv(MKLVL_ENVVAR);
	int	level = (value == NULL) ? 0 : atoi(value);

	if (level < 0) {
		errc(2, EAGAIN, "Invalid value for recursion level (%d).",
		    level);
	} else if (level > MKLVL_MAXVAL) {
		errc(2, EAGAIN, "Max recursion level (%d) exceeded.",
		    MKLVL_MAXVAL);
	} else {
		char new_value[32];
		sprintf(new_value, "%d", level + 1);
		setenv(MKLVL_ENVVAR, new_value, 1);
	}
#endif /* WANT_ENV_MKLVL */
}

/**
 * main
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
    	const char *machine;
	const char *machine_arch;
	const char *machine_cpu;
	Boolean outOfDate = TRUE; 	/* FALSE if all targets up to date */
	const char *p;
	const char *pathp;
	const char *path;
	char mdpath[MAXPATHLEN];
	char obpath[MAXPATHLEN];
	char cdpath[MAXPATHLEN];
	char *cp = NULL, *start;

	/*
	 * Initialize file global variables.
	 */
	expandVars = TRUE;
	noBuiltins = FALSE;		/* Read the built-in rules */
	forceJobs = FALSE;              /* No -j flag */
	curdir = cdpath;

	/*
	 * Initialize program global variables.
	 */
	beSilent = FALSE;		/* Print commands as executed */
	ignoreErrors = FALSE;		/* Pay attention to non-zero returns */
	noExecute = FALSE;		/* Execute all commands */
	keepgoing = FALSE;		/* Stop on error */
	allPrecious = FALSE;		/* Remove targets when interrupted */
	queryFlag = FALSE;		/* This is not just a check-run */
	touchFlag = FALSE;		/* Actually update targets */
	usePipes = TRUE;		/* Catch child output in pipes */
	debug = 0;			/* No debug verbosity, please. */
	jobsRunning = FALSE;

	jobLimit = DEFMAXJOBS;
	compatMake = FALSE;		/* No compat mode */

	check_make_level();

#ifdef RLIMIT_NOFILE
	/*
	 * get rid of resource limit on file descriptors
	 */
	{
		struct rlimit rl;
		if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
			err(2, "getrlimit");
		}
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
			err(2, "setrlimit");
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
	if ((machine = getenv("MACHINE")) == NULL) {
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
	if (machine == NULL) {
#ifdef MACHINE
		machine = MACHINE;
#else
		static struct utsname utsname;

		if (uname(&utsname) == -1)
			err(2, "uname");
		machine = utsname.machine;
#endif
	}

	if ((machine_arch = getenv("MACHINE_ARCH")) == NULL) {
#ifdef MACHINE_ARCH
		machine_arch = MACHINE_ARCH;
#else
		machine_arch = "unknown";
#endif
	}

	/*
	 * Set machine_cpu to the minumum supported CPU revision based
	 * on the target architecture, if not already set.
	 */
	if ((machine_cpu = getenv("MACHINE_CPU")) == NULL) {
		if (!strcmp(machine_arch, "i386"))
			machine_cpu = "i386";
		else if (!strcmp(machine_arch, "alpha"))
			machine_cpu = "ev4";
		else
			machine_cpu = "unknown";
	}

	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */
	Proc_Init();

	Dir_Init();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
	Var_Init(environ);	/* As well as the lists of variables for
				 * parsing arguments */

	/*
	 * Initialize the Shell so that we have a shell for != assignments
	 * on the command line.
	 */
	Shell_Init();

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_SetGlobal("MAKE", argv[0]);
	Var_SetGlobal(".MAKEFLAGS", "");
	Var_SetGlobal("MFLAGS", "");
	Var_SetGlobal("MACHINE", machine);
	Var_SetGlobal("MACHINE_ARCH", machine_arch);
	Var_SetGlobal("MACHINE_CPU", machine_cpu);
#ifdef MAKE_VERSION
	Var_SetGlobal("MAKE_VERSION", MAKE_VERSION);
#endif

	/*
	 * First snag things out of the MAKEFLAGS environment
	 * variable.  Then parse the command line arguments.
	 */
	Main_ParseArgLine(getenv("MAKEFLAGS"), 1);

	MainParseArgs(argc, argv);

	/*
	 * Find where we are...
	 */
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
	 *	3. PATH_OBJDIR.${MACHINE}
	 *	4. PATH_OBJDIR
	 *	5. PATH_OBJDIRPREFIX`cwd`
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
			path = PATH_OBJDIR;
			pathp = PATH_OBJDIRPREFIX;
			snprintf(mdpath, MAXPATHLEN, "%s.%s", path, machine);
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
		Path_AddDir(&dirSearchPath, curdir);
	Var_SetGlobal(".ST_EXPORTVAR", "YES");
	Var_SetGlobal(".CURDIR", curdir);
	Var_SetGlobal(".OBJDIR", objdir);

	if (getenv("MAKE_JOBS_FIFO") != NULL)
		forceJobs = TRUE;
	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turned compatibility on
	 */
	if (!compatMake && !forceJobs)
		compatMake = TRUE;

	/*
	 * Initialize target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Targ_Init();
	Suff_Init();

	DEFAULT = NULL;
	time(&now);

	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (Lst_IsEmpty(&create)) {
		Var_SetGlobal(".TARGETS", "");
	} else {
		LstNode *ln;

		for (ln = Lst_First(&create); ln != NULL; ln = Lst_Succ(ln)) {
			char *name = Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	}


	/*
	 * If no user-supplied system path was given (through the -m option)
	 * add the directories from the DEFSYSPATH (more than one may be given
	 * as dir1:...:dirn) to the system include path.
	 */
	if (TAILQ_EMPTY(&sysIncPath)) {
		char syspath[] = PATH_DEFSYSPATH;

		for (start = syspath; *start != '\0'; start = cp) {
			for (cp = start; *cp != '\0' && *cp != ':'; cp++)
				continue;
			if (*cp == '\0') {
				Path_AddDir(&sysIncPath, start);
			} else {
				*cp++ = '\0';
				Path_AddDir(&sysIncPath, start);
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
		char	defsysmk[] = PATH_DEFSYSMK;

		Path_Expand(defsysmk, &sysIncPath, &sysMkPath);
		if (Lst_IsEmpty(&sysMkPath))
			Fatal("make: no system rules (%s).", PATH_DEFSYSMK);
		LST_FOREACH(ln, &sysMkPath) {
			if (!ReadMakefile(Lst_Datum(ln)))
				break;
		}
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
		Lst_Destroy(&sysMkPath, free);
	}

	if (!Lst_IsEmpty(&makefiles)) {
		LstNode *ln;

		LST_FOREACH(ln, &makefiles) {
			if (!ReadMakefile(Lst_Datum(ln)))
				break;
		}
		if (ln != NULL)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
	} else if (!ReadMakefile("BSDmakefile"))
	    if (!ReadMakefile("makefile"))
		ReadMakefile("Makefile");

	ReadMakefile(".depend");

	/* Install all the flags into the MAKE envariable. */
	if (((p = Var_Value(".MAKEFLAGS", VAR_GLOBAL)) != NULL) && *p)
		setenv("MAKEFLAGS", p, 1);

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
		Buffer	*buf;
		char	*vpath;
		char	*ptr;
		char	savec;

		buf = Var_Subst(VPATH, VAR_CMD, FALSE);

		vpath = Buf_Data(buf);
		do {
			/* skip to end of directory */
			for (ptr = vpath; *ptr != ':' && *ptr != '\0'; ptr++)
				;

			/* Save terminator character so know when to stop */
			savec = *ptr;
			*ptr = '\0';

			/* Add directory to search path */
			Path_AddDir(&dirSearchPath, vpath);

			vpath = ptr + 1;
		} while (savec != '\0');

		Buf_Destroy(buf, TRUE);
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
	if (Lst_IsEmpty(&variables)) {
		/*
		 * Since the user has not requested that any variables
		 * be printed, we can build targets.
		 *
		 * Have read the entire graph and need to make a list of targets
		 * to create. If none was given on the command line, we consult
		 * the parsing module to find the main target(s) to create.
		 */
		Lst targs = Lst_Initializer(targs);

		if (Lst_IsEmpty(&create))
			Parse_MainName(&targs);
		else
			Targ_FindList(&targs, &create, TARG_CREATE);

		if (compatMake) {
			/*
			 * Compat_Init will take care of creating
			 * all the targets as well as initializing
			 * the module.
			 */
			Compat_Run(&targs);
			outOfDate = 0;
		} else {
			/*
			 * Initialize job module before traversing
			 * the graph, now that any .BEGIN and .END
			 * targets have been read.  This is done
			 * only if the -q flag wasn't given (to
			 * prevent the .BEGIN from being executed
			 * should it exist).
			 */
			if (!queryFlag) {
				Job_Init(jobLimit);
				jobsRunning = TRUE;
			}

			/* Traverse the graph, checking on all the targets */
			outOfDate = Make_Run(&targs);
		}
		Lst_Destroy(&targs, NOFREE);

	} else {
		Var_Print(&variables, expandVars);
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

