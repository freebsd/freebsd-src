/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)sccs.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <signal.h>
#include <sysexits.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include "pathnames.h"

/*
**  SCCS.C -- human-oriented front end to the SCCS system.
**
**	Without trying to add any functionality to speak of, this
**	program tries to make SCCS a little more accessible to human
**	types.  The main thing it does is automatically put the
**	string "SCCS/s." on the front of names.  Also, it has a
**	couple of things that are designed to shorten frequent
**	combinations, e.g., "delget" which expands to a "delta"
**	and a "get".
**
**	This program can also function as a setuid front end.
**	To do this, you should copy the source, renaming it to
**	whatever you want, e.g., "syssccs".  Change any defaults
**	in the program (e.g., syssccs might default -d to
**	"/usr/src/sys").  Then recompile and put the result
**	as setuid to whomever you want.  In this mode, sccs
**	knows to not run setuid for certain programs in order
**	to preserve security, and so forth.
**
**	Usage:
**		sccs [flags] command [args]
**
**	Flags:
**		-d<dir>		<dir> represents a directory to search
**				out of.  It should be a full pathname
**				for general usage.  E.g., if <dir> is
**				"/usr/src/sys", then a reference to the
**				file "dev/bio.c" becomes a reference to
**				"/usr/src/sys/dev/bio.c".
**		-p<path>	prepends <path> to the final component
**				of the pathname.  By default, this is
**				"SCCS".  For example, in the -d example
**				above, the path then gets modified to
**				"/usr/src/sys/dev/SCCS/s.bio.c".  In
**				more common usage (without the -d flag),
**				"prog.c" would get modified to
**				"SCCS/s.prog.c".  In both cases, the
**				"s." gets automatically prepended.
**		-r		run as the real user.
**
**	Commands:
**		admin,
**		get,
**		delta,
**		rmdel,
**		cdc,
**		etc.		Straight out of SCCS; only difference
**				is that pathnames get modified as
**				described above.
**		enter		Front end doing "sccs admin -i<name> <name>"
**		create		Macro for "enter" followed by "get".
**		edit		Macro for "get -e".
**		unedit		Removes a file being edited, knowing
**				about p-files, etc.
**		delget		Macro for "delta" followed by "get".
**		deledit		Macro for "delta" followed by "get -e".
**		branch		Macro for "get -b -e", followed by "delta
**				-s -n", followd by "get -e -t -g".
**		diffs		"diff" the specified version of files
**				and the checked-out version.
**		print		Macro for "prs -e" followed by "get -p -m".
**		tell		List what files are being edited.
**		info		Print information about files being edited.
**		clean		Remove all files that can be
**				regenerated from SCCS files.
**		check		Like info, but return exit status, for
**				use in makefiles.
**		fix		Remove a top delta & reedit, but save
**				the previous changes in that delta.
**
**	Compilation Flags:
**		UIDUSER -- determine who the user is by looking at the
**			uid rather than the login name -- for machines
**			where SCCS gets the user in this way.
**		SCCSDIR -- if defined, forces the -d flag to take on
**			this value.  This is so that the setuid
**			aspects of this program cannot be abused.
**			This flag also disables the -p flag.
**		SCCSPATH -- the default for the -p flag.
**		MYNAME -- the title this program should print when it
**			gives error messages.
**
**	Compilation Instructions:
**		cc -O -n -s sccs.c
**		The flags listed above can be -D defined to simplify
**			recompilation for variant versions.
**
**	Author:
**		Eric Allman, UCB/INGRES
**		Copyright 1980 Regents of the University of California
*/


/*******************  Configuration Information  ********************/

# ifndef SCCSPATH
# define SCCSPATH	"SCCS"	/* pathname in which to find s-files */
# endif NOT SCCSPATH

# ifndef MYNAME
# define MYNAME		"sccs"	/* name used for printing errors */
# endif NOT MYNAME

/****************  End of Configuration Information  ****************/

typedef char	bool;
# define TRUE	1
# define FALSE	0

# define bitset(bit, word)	((bool) ((bit) & (word)))

struct sccsprog
{
	char	*sccsname;	/* name of SCCS routine */
	short	sccsoper;	/* opcode, see below */
	short	sccsflags;	/* flags, see below */
	char	*sccspath;	/* pathname of binary implementing */
};

/* values for sccsoper */
# define PROG		0	/* call a program */
# define CMACRO		1	/* command substitution macro */
# define FIX		2	/* fix a delta */
# define CLEAN		3	/* clean out recreatable files */
# define UNEDIT		4	/* unedit a file */
# define SHELL		5	/* call a shell file (like PROG) */
# define DIFFS		6	/* diff between sccs & file out */
# define DODIFF		7	/* internal call to diff program */
# define ENTER		8	/* enter new files */

/* bits for sccsflags */
# define NO_SDOT	0001	/* no s. on front of args */
# define REALUSER	0002	/* protected (e.g., admin) */

/* modes for the "clean", "info", "check" ops */
# define CLEANC		0	/* clean command */
# define INFOC		1	/* info command */
# define CHECKC		2	/* check command */
# define TELLC		3	/* give list of files being edited */

/*
**  Description of commands known to this program.
**	First argument puts the command into a class.  Second arg is
**	info regarding treatment of this command.  Third arg is a
**	list of flags this command accepts from macros, etc.  Fourth
**	arg is the pathname of the implementing program, or the
**	macro definition, or the arg to a sub-algorithm.
*/

struct sccsprog SccsProg[] = {
	"admin",	PROG,	REALUSER,	_PATH_SCCSADMIN,
	"cdc",		PROG,	0,		_PATH_SCCSRMDEL,
	"comb",		PROG,	0,		_PATH_SCCSCOMB,
	"delta",	PROG,	0,		_PATH_SCCSDELTA,
	"get",		PROG,	0,		_PATH_SCCSGET,
	"help",		PROG,	NO_SDOT,	_PATH_SCCSHELP,
	"prs",		PROG,	0,		_PATH_SCCSPRS,
	"prt",		PROG,	0,		_PATH_SCCSPRT,
	"rmdel",	PROG,	REALUSER,	_PATH_SCCSRMDEL,
	"val",		PROG,	0,		_PATH_SCCSVAL,
	"what",		PROG,	NO_SDOT,	_PATH_SCCSWHAT,
	"sccsdiff",	SHELL,	REALUSER,	_PATH_SCCSDIFF,
	"edit",		CMACRO,	NO_SDOT,	"get -e",
	"delget",	CMACRO,	NO_SDOT,	"delta:mysrp/get:ixbeskcl -t",
	"deledit",	CMACRO,	NO_SDOT,
					"delta:mysrp -n/get:ixbskcl -e -t -g",
	"fix",		FIX,	NO_SDOT,	NULL,
	"clean",	CLEAN,	REALUSER|NO_SDOT,
						(char *) CLEANC,
	"info",		CLEAN,	REALUSER|NO_SDOT,
						(char *) INFOC,
	"check",	CLEAN,	REALUSER|NO_SDOT,
						(char *) CHECKC,
	"tell",		CLEAN,	REALUSER|NO_SDOT,
						(char *) TELLC,
	"unedit",	UNEDIT,	NO_SDOT,	NULL,
	"diffs",	DIFFS,	NO_SDOT|REALUSER,
						NULL,
	"-diff",	DODIFF,	NO_SDOT|REALUSER,
						_PATH_SCCSBDIFF,
	"print",	CMACRO,	0,		"prs -e/get -p -m -s",
	"branch",	CMACRO,	NO_SDOT,
	    "get:ixrc -e -b/delta: -s -n -ybranch-place-holder/get:pl -e -t -g",
	"enter",	ENTER,	NO_SDOT,	NULL,
	"create",	CMACRO,	NO_SDOT,	"enter/get:ixbeskcl -t",
	NULL,		-1,	0,		NULL
};

/* one line from a p-file */
struct pfile
{
	char	*p_osid;	/* old SID */
	char	*p_nsid;	/* new SID */
	char	*p_user;	/* user who did edit */
	char	*p_date;	/* date of get */
	char	*p_time;	/* time of get */
	char	*p_aux;		/* extra info at end */
};

char	*SccsPath = SCCSPATH;	/* pathname of SCCS files */
# ifdef SCCSDIR
char	*SccsDir = SCCSDIR;	/* directory to begin search from */
# else
char	*SccsDir = "";
# endif
char	MyName[] = MYNAME;	/* name used in messages */
int	OutFile = -1;		/* override output file for commands */
bool	RealUser;		/* if set, running as real user */
# ifdef DEBUG
bool	Debug;			/* turn on tracing */
# endif
# ifndef V6
extern char	*getenv();
# endif V6

char *gstrcat(), *strcat();
char *gstrncat(), *strncat();
char *gstrcpy(), *strcpy();
#define	FBUFSIZ	BUFSIZ
#define	PFILELG	120

main(argc, argv)
	int argc;
	char **argv;
{
	register char *p;
	extern struct sccsprog *lookup();
	register int i;
# ifndef V6
# ifndef SCCSDIR
	register struct passwd *pw;
	extern struct passwd *getpwnam();
	char buf[FBUFSIZ];

	/* pull "SccsDir" out of the environment (possibly) */
	p = getenv("PROJECTDIR");
	if (p != NULL && p[0] != '\0')
	{
		if (p[0] == '/')
			SccsDir = p;
		else
		{
			pw = getpwnam(p);
			if (pw == NULL)
			{
				usrerr("user %s does not exist", p);
				exit(EX_USAGE);
			}
			gstrcpy(buf, pw->pw_dir, sizeof(buf));
			gstrcat(buf, "/src", sizeof(buf));
			if (access(buf, 0) < 0)
			{
				gstrcpy(buf, pw->pw_dir, sizeof(buf));
				gstrcat(buf, "/source", sizeof(buf));
				if (access(buf, 0) < 0)
				{
					usrerr("project %s has no source!", p);
					exit(EX_USAGE);
				}
			}
			SccsDir = buf;
		}
	}
# endif SCCSDIR
# endif V6

	/*
	**  Detect and decode flags intended for this program.
	*/

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [flags] command [flags]\n", MyName);
		exit(EX_USAGE);
	}
	argv[argc] = NULL;

	if (lookup(argv[0]) == NULL)
	{
		while ((p = *++argv) != NULL)
		{
			if (*p != '-')
				break;
			switch (*++p)
			{
			  case 'r':		/* run as real user */
				setuid(getuid());
				RealUser++;
				break;

# ifndef SCCSDIR
			  case 'p':		/* path of sccs files */
				SccsPath = ++p;
				if (SccsPath[0] == '\0' && argv[1] != NULL)
					SccsPath = *++argv;
				break;

			  case 'd':		/* directory to search from */
				SccsDir = ++p;
				if (SccsDir[0] == '\0' && argv[1] != NULL)
					SccsDir = *++argv;
				break;
# endif

# ifdef DEBUG
			  case 'T':		/* trace */
				Debug++;
				break;
# endif

			  default:
				usrerr("unknown option -%s", p);
				break;
			}
		}
		if (SccsPath[0] == '\0')
			SccsPath = ".";
	}

	i = command(argv, FALSE, "");
	exit(i);
}

/*
**  COMMAND -- look up and perform a command
**
**	This routine is the guts of this program.  Given an
**	argument vector, it looks up the "command" (argv[0])
**	in the configuration table and does the necessary stuff.
**
**	Parameters:
**		argv -- an argument vector to process.
**		forkflag -- if set, fork before executing the command.
**		editflag -- if set, only include flags listed in the
**			sccsklets field of the command descriptor.
**		arg0 -- a space-seperated list of arguments to insert
**			before argv.
**
**	Returns:
**		zero -- command executed ok.
**		else -- error status.
**
**	Side Effects:
**		none.
*/

command(argv, forkflag, arg0)
	char **argv;
	bool forkflag;
	char *arg0;
{
	register struct sccsprog *cmd;
	register char *p;
	char buf[FBUFSIZ];
	extern struct sccsprog *lookup();
	char *nav[1000];
	char **np;
	register char **ap;
	register int i;
	register char *q;
	extern bool unedit();
	int rval = 0;
	extern char *index();
	extern char *makefile();
	char *editchs;
	extern char *tail();

# ifdef DEBUG
	if (Debug)
	{
		printf("command:\n\t\"%s\"\n", arg0);
		for (np = argv; *np != NULL; np++)
			printf("\t\"%s\"\n", *np);
	}
# endif

	/*
	**  Copy arguments.
	**	Copy from arg0 & if necessary at most one arg
	**	from argv[0].
	*/

	np = ap = &nav[1];
	editchs = NULL;
	for (p = arg0, q = buf; *p != '\0' && *p != '/'; )
	{
		*np++ = q;
		while (*p == ' ')
			p++;
		while (*p != ' ' && *p != '\0' && *p != '/' && *p != ':')
			*q++ = *p++;
		*q++ = '\0';
		if (*p == ':')
		{
			editchs = q;
			while (*++p != '\0' && *p != '/' && *p != ' ')
				*q++ = *p;
			*q++ = '\0';
		}
	}
	*np = NULL;
	if (*ap == NULL)
		*np++ = *argv++;

	/*
	**  Look up command.
	**	At this point, *ap is the command name.
	*/

	cmd = lookup(*ap);
	if (cmd == NULL)
	{
		usrerr("Unknown command \"%s\"", *ap);
		return (EX_USAGE);
	}

	/*
	**  Copy remaining arguments doing editing as appropriate.
	*/

	for (; *argv != NULL; argv++)
	{
		p = *argv;
		if (*p == '-')
		{
			if (p[1] == '\0' || editchs == NULL || index(editchs, p[1]) != NULL)
				*np++ = p;
		}
		else
		{
			if (!bitset(NO_SDOT, cmd->sccsflags))
				p = makefile(p);
			if (p != NULL)
				*np++ = p;
		}
	}
	*np = NULL;

	/*
	**  Interpret operation associated with this command.
	*/

	switch (cmd->sccsoper)
	{
	  case SHELL:		/* call a shell file */
		*ap = cmd->sccspath;
		*--ap = "sh";
		rval = callprog(_PATH_BSHELL, cmd->sccsflags, ap, forkflag);
		break;

	  case PROG:		/* call an sccs prog */
		rval = callprog(cmd->sccspath, cmd->sccsflags, ap, forkflag);
		break;

	  case CMACRO:		/* command macro */
		/* step through & execute each part of the macro */
		for (p = cmd->sccspath; *p != '\0'; p++)
		{
			q = p;
			while (*p != '\0' && *p != '/')
				p++;
			rval = command(&ap[1], *p != '\0', q);
			if (rval != 0)
				break;
		}
		break;

	  case FIX:		/* fix a delta */
		if (ap[1]==0 || strncmp(ap[1], "-r", 2)!=0)
		{
			usrerr("-r flag needed for fix command");
			rval = EX_USAGE;
			break;
		}

		/* get the version with all changes */
		rval = command(&ap[1], TRUE, "get -k");

		/* now remove that version from the s-file */
		if (rval == 0)
			rval = command(&ap[1], TRUE, "rmdel:r");

		/* and edit the old version (but don't clobber new vers) */
		if (rval == 0)
			rval = command(&ap[2], FALSE, "get -e -g");
		break;

	  case CLEAN:
		rval = clean((int) cmd->sccspath, ap);
		break;

	  case UNEDIT:
		for (argv = np = &ap[1]; *argv != NULL; argv++)
		{
			if (unedit(*argv))
				*np++ = *argv;
		}
		*np = NULL;

		/* get all the files that we unedited successfully */
		if (np > &ap[1])
			rval = command(&ap[1], FALSE, "get");
		break;

	  case DIFFS:		/* diff between s-file & edit file */
		/* find the end of the flag arguments */
		for (np = &ap[1]; *np != NULL && **np == '-'; np++)
			continue;
		argv = np;

		/* for each file, do the diff */
		p = argv[1];
		while (*np != NULL)
		{
			/* messy, but we need a null terminated argv */
			*argv = *np++;
			argv[1] = NULL;
			i = dodiff(ap, tail(*argv));
			if (rval == 0)
				rval = i;
			argv[1] = p;
		}
		break;

	  case DODIFF:		/* internal diff call */
		setuid(getuid());
		for (np = ap; *np != NULL; np++)
		{
			if ((*np)[0] == '-' && (*np)[1] == 'C')
				(*np)[1] = 'c';
		}

		/* insert "-" argument */
		np[1] = NULL;
		np[0] = np[-1];
		np[-1] = "-";

		/* execute the diff program of choice */
# ifndef V6
		execvp("diff", ap);
# endif
		execv(cmd->sccspath, argv);
		syserr("cannot exec %s", cmd->sccspath);
		exit(EX_OSERR);

	  case ENTER:		/* enter new sccs files */
		/* skip over flag arguments */
		for (np = &ap[1]; *np != NULL && **np == '-'; np++)
			continue;
		argv = np;

		/* do an admin for each file */
		p = argv[1];
		while (*np != NULL)
		{
			printf("\n%s:\n", *np);
			strcpy(buf, "-i");
			gstrcat(buf, *np, sizeof(buf));
			ap[0] = buf;
			argv[0] = tail(*np);
			argv[1] = NULL;
			rval = command(ap, TRUE, "admin");
			argv[1] = p;
			if (rval == 0)
			{
				strcpy(buf, ",");
				gstrcat(buf, tail(*np), sizeof(buf));
				if (link(*np, buf) >= 0)
					unlink(*np);
			}
			np++;
		}
		break;

	  default:
		syserr("oper %d", cmd->sccsoper);
		exit(EX_SOFTWARE);
	}
# ifdef DEBUG
	if (Debug)
		printf("command: rval=%d\n", rval);
# endif
	return (rval);
}

/*
**  LOOKUP -- look up an SCCS command name.
**
**	Parameters:
**		name -- the name of the command to look up.
**
**	Returns:
**		ptr to command descriptor for this command.
**		NULL if no such entry.
**
**	Side Effects:
**		none.
*/

struct sccsprog *
lookup(name)
	char *name;
{
	register struct sccsprog *cmd;

	for (cmd = SccsProg; cmd->sccsname != NULL; cmd++)
	{
		if (strcmp(cmd->sccsname, name) == 0)
			return (cmd);
	}
	return (NULL);
}

/*
**  CALLPROG -- call a program
**
**	Used to call the SCCS programs.
**
**	Parameters:
**		progpath -- pathname of the program to call.
**		flags -- status flags from the command descriptors.
**		argv -- an argument vector to pass to the program.
**		forkflag -- if true, fork before calling, else just
**			exec.
**
**	Returns:
**		The exit status of the program.
**		Nothing if forkflag == FALSE.
**
**	Side Effects:
**		Can exit if forkflag == FALSE.
*/

callprog(progpath, flags, argv, forkflag)
	char *progpath;
	short flags;
	char **argv;
	bool forkflag;
{
	register int i;
	register int wpid;
	auto int st;
	register int sigcode;
	register int coredumped;
	register const char *sigmsg;
	char sigmsgbuf[10+1];	/* "Signal 127" + terminating '\0' */

# ifdef DEBUG
	if (Debug)
	{
		printf("callprog:\n");
		for (i = 0; argv[i] != NULL; i++)
			printf("\t\"%s\"\n", argv[i]);
	}
# endif

	if (*argv == NULL)
		return (-1);

	/*
	**  Fork if appropriate.
	*/

	if (forkflag)
	{
# ifdef DEBUG
		if (Debug)
			printf("Forking\n");
# endif
		i = fork();
		if (i < 0)
		{
			syserr("cannot fork");
			exit(EX_OSERR);
		}
		else if (i > 0)
		{
			while ((wpid = wait(&st)) != -1 && wpid != i)
				;
			if ((sigcode = st & 0377) == 0)
				st = (st >> 8) & 0377;
			else
			{
				coredumped = sigcode & 0200;
				sigcode &= 0177;
				if (sigcode != SIGINT && sigcode != SIGPIPE)
				{
					if (sigcode < NSIG)
						sigmsg = sys_siglist[sigcode];
					else
					{
						sprintf(sigmsgbuf, "Signal %d",
						    sigcode);
						sigmsg = sigmsgbuf;
					}
					fprintf(stderr, "sccs: %s: %s%s", argv[0],
					    sigmsg,
					    coredumped ? " - core dumped": "");
				}
				st = EX_SOFTWARE;
			}
			if (OutFile >= 0)
			{
				close(OutFile);
				OutFile = -1;
			}
			return (st);
		}
	}
	else if (OutFile >= 0)
	{
		syserr("callprog: setting stdout w/o forking");
		exit(EX_SOFTWARE);
	}

	/* set protection as appropriate */
	if (bitset(REALUSER, flags))
		setuid(getuid());

	/* change standard input & output if needed */
	if (OutFile >= 0)
	{
		close(1);
		dup(OutFile);
		close(OutFile);
	}
	
	/* call real SCCS program */
	execv(progpath, argv);
	syserr("cannot execute %s", progpath);
	exit(EX_UNAVAILABLE);
	/*NOTREACHED*/
}

/*
**  MAKEFILE -- make filename of SCCS file
**
**	If the name passed is already the name of an SCCS file,
**	just return it.  Otherwise, munge the name into the name
**	of the actual SCCS file.
**
**	There are cases when it is not clear what you want to
**	do.  For example, if SccsPath is an absolute pathname
**	and the name given is also an absolute pathname, we go
**	for SccsPath (& only use the last component of the name
**	passed) -- this is important for security reasons (if
**	sccs is being used as a setuid front end), but not
**	particularly intuitive.
**
**	Parameters:
**		name -- the file name to be munged.
**
**	Returns:
**		The pathname of the sccs file.
**		NULL on error.
**
**	Side Effects:
**		none.
*/

char *
makefile(name)
	char *name;
{
	register char *p;
	char buf[3*FBUFSIZ];
	extern char *malloc();
	extern char *rindex();
	extern bool safepath();
	extern bool isdir();
	register char *q;

	p = rindex(name, '/');
	if (p == NULL)
		p = name;
	else
		p++;

	/*
	**  Check to see that the path is "safe", i.e., that we
	**  are not letting some nasty person use the setuid part
	**  of this program to look at or munge some presumably
	**  hidden files.
	*/

	if (SccsDir[0] == '/' && !safepath(name))
		return (NULL);

	/*
	**  Create the base pathname.
	*/

	/* first the directory part */
	if (SccsDir[0] != '\0' && name[0] != '/' && strncmp(name, "./", 2) != 0)
	{
		gstrcpy(buf, SccsDir, sizeof(buf));
		gstrcat(buf, "/", sizeof(buf));
	}
	else
		gstrcpy(buf, "", sizeof(buf));
	
	/* then the head of the pathname */
	gstrncat(buf, name, p - name, sizeof(buf));
	q = &buf[strlen(buf)];

	/* now copy the final part of the name, in case useful */
	gstrcpy(q, p, sizeof(buf));

	/* so is it useful? */
	if (strncmp(p, "s.", 2) != 0 && !isdir(buf))
	{
		/* sorry, no; copy the SCCS pathname & the "s." */
		gstrcpy(q, SccsPath, sizeof(buf));
		gstrcat(buf, "/s.", sizeof(buf));

		/* and now the end of the name */
		gstrcat(buf, p, sizeof(buf));
	}

	/* if i haven't changed it, why did I do all this? */
	if (strcmp(buf, name) == 0)
		p = name;
	else
	{
		/* but if I have, squirrel it away */
		p = malloc(strlen(buf) + 1);
		if (p == NULL)
		{
			perror("Sccs: no mem");
			exit(EX_OSERR);
		}
		strcpy(p, buf);
	}

	return (p);
}

/*
**  ISDIR -- return true if the argument is a directory.
**
**	Parameters:
**		name -- the pathname of the file to check.
**
**	Returns:
**		TRUE if 'name' is a directory, FALSE otherwise.
**
**	Side Effects:
**		none.
*/

bool
isdir(name)
	char *name;
{
	struct stat stbuf;

	return (stat(name, &stbuf) >= 0 && (stbuf.st_mode & S_IFMT) == S_IFDIR);
}

/*
**  SAFEPATH -- determine whether a pathname is "safe"
**
**	"Safe" pathnames only allow you to get deeper into the
**	directory structure, i.e., full pathnames and ".." are
**	not allowed.
**
**	Parameters:
**		p -- the name to check.
**
**	Returns:
**		TRUE -- if the path is safe.
**		FALSE -- if the path is not safe.
**
**	Side Effects:
**		Prints a message if the path is not safe.
*/

bool
safepath(p)
	register char *p;
{
	extern char *index();

	if (*p != '/')
	{
		while (strncmp(p, "../", 3) != 0 && strcmp(p, "..") != 0)
		{
			p = index(p, '/');
			if (p == NULL)
				return (TRUE);
			p++;
		}
	}

	printf("You may not use full pathnames or \"..\"\n");
	return (FALSE);
}

/*
**  CLEAN -- clean out recreatable files
**
**	Any file for which an "s." file exists but no "p." file
**	exists in the current directory is purged.
**
**	Parameters:
**		mode -- tells whether this came from a "clean", "info", or
**			"check" command.
**		argv -- the rest of the argument vector.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Removes files in the current directory.
**		Prints information regarding files being edited.
**		Exits if a "check" command.
*/

clean(mode, argv)
	int mode;
	char **argv;
{
	struct direct *dir;
	char buf[FBUFSIZ];
	char *bufend;
	register DIR *dirp;
	register char *basefile;
	bool gotedit;
	bool gotpfent;
	FILE *pfp;
	bool nobranch = FALSE;
	extern struct pfile *getpfent();
	register struct pfile *pf;
	register char **ap;
	extern char *username();
	char *usernm = NULL;
	char *subdir = NULL;
	char *cmdname;

	/*
	**  Process the argv
	*/

	cmdname = *argv;
	for (ap = argv; *++ap != NULL; )
	{
		if (**ap == '-')
		{
			/* we have a flag */
			switch ((*ap)[1])
			{
			  case 'b':
				nobranch = TRUE;
				break;

			  case 'u':
				if ((*ap)[2] != '\0')
					usernm = &(*ap)[2];
				else if (ap[1] != NULL && ap[1][0] != '-')
					usernm = *++ap;
				else
					usernm = username();
				break;
			}
		}
		else
		{
			if (subdir != NULL)
				usrerr("too many args");
			else
				subdir = *ap;
		}
	}

	/*
	**  Find and open the SCCS directory.
	*/

	gstrcpy(buf, SccsDir, sizeof(buf));
	if (buf[0] != '\0')
		gstrcat(buf, "/", sizeof(buf));
	if (subdir != NULL)
	{
		gstrcat(buf, subdir, sizeof(buf));
		gstrcat(buf, "/", sizeof(buf));
	}
	gstrcat(buf, SccsPath, sizeof(buf));
	bufend = &buf[strlen(buf)];

	dirp = opendir(buf);
	if (dirp == NULL)
	{
		usrerr("cannot open %s", buf);
		return (EX_NOINPUT);
	}

	/*
	**  Scan the SCCS directory looking for s. files.
	**	gotedit tells whether we have tried to clean any
	**		files that are being edited.
	*/

	gotedit = FALSE;
	while (dir = readdir(dirp)) {
		if (strncmp(dir->d_name, "s.", 2) != 0)
			continue;
		
		/* got an s. file -- see if the p. file exists */
		gstrcpy(bufend, "/p.", sizeof(buf));
		basefile = bufend + 3;
		gstrcpy(basefile, &dir->d_name[2], sizeof(buf));

		/*
		**  open and scan the p-file.
		**	'gotpfent' tells if we have found a valid p-file
		**		entry.
		*/

		pfp = fopen(buf, "r");
		gotpfent = FALSE;
		if (pfp != NULL)
		{
			/* the file exists -- report it's contents */
			while ((pf = getpfent(pfp)) != NULL)
			{
				if (nobranch && isbranch(pf->p_nsid))
					continue;
				if (usernm != NULL && strcmp(usernm, pf->p_user) != 0 && mode != CLEANC)
					continue;
				gotedit = TRUE;
				gotpfent = TRUE;
				if (mode == TELLC)
				{
					printf("%s\n", basefile);
					break;
				}
				printf("%12s: being edited: ", basefile);
				putpfent(pf, stdout);
			}
			fclose(pfp);
		}
		
		/* the s. file exists and no p. file exists -- unlink the g-file */
		if (mode == CLEANC && !gotpfent)
		{
			char	unlinkbuf[FBUFSIZ];
			gstrcpy(unlinkbuf, &dir->d_name[2], sizeof(unlinkbuf));
			unlink(unlinkbuf);
		}
	}

	/* cleanup & report results */
	closedir(dirp);
	if (!gotedit && mode == INFOC)
	{
		printf("Nothing being edited");
		if (nobranch)
			printf(" (on trunk)");
		if (usernm == NULL)
			printf("\n");
		else
			printf(" by %s\n", usernm);
	}
	if (mode == CHECKC)
		exit(gotedit);
	return (EX_OK);
}

/*
**  ISBRANCH -- is the SID a branch?
**
**	Parameters:
**		sid -- the sid to check.
**
**	Returns:
**		TRUE if the sid represents a branch.
**		FALSE otherwise.
**
**	Side Effects:
**		none.
*/

isbranch(sid)
	char *sid;
{
	register char *p;
	int dots;

	dots = 0;
	for (p = sid; *p != '\0'; p++)
	{
		if (*p == '.')
			dots++;
		if (dots > 1)
			return (TRUE);
	}
	return (FALSE);
}

/*
**  UNEDIT -- unedit a file
**
**	Checks to see that the current user is actually editting
**	the file and arranges that s/he is not editting it.
**
**	Parameters:
**		fn -- the name of the file to be unedited.
**
**	Returns:
**		TRUE -- if the file was successfully unedited.
**		FALSE -- if the file was not unedited for some
**			reason.
**
**	Side Effects:
**		fn is removed
**		entries are removed from pfile.
*/

bool
unedit(fn)
	char *fn;
{
	register FILE *pfp;
	char *cp, *pfn;
	static char tfn[] = _PATH_TMP;
	FILE *tfp;
	register char *q;
	bool delete = FALSE;
	bool others = FALSE;
	char *myname;
	extern char *username();
	struct pfile *pent;
	extern struct pfile *getpfent();
	char buf[PFILELG];
	extern char *makefile(), *rindex(), *tail();

	/* make "s." filename & find the trailing component */
	pfn = makefile(fn);
	if (pfn == NULL)
		return (FALSE);
	q = rindex(pfn, '/');
	if (q == NULL)
		q = &pfn[-1];
	if (q[1] != 's' || q[2] != '.')
	{
		usrerr("bad file name \"%s\"", fn);
		return (FALSE);
	}

	/* turn "s." into "p." & try to open it */
	*++q = 'p';

	pfp = fopen(pfn, "r");
	if (pfp == NULL)
	{
		printf("%12s: not being edited\n", fn);
		return (FALSE);
	}

	/* create temp file for editing p-file */
	mktemp(tfn);
	tfp = fopen(tfn, "w");
	if (tfp == NULL)
	{
		usrerr("cannot create \"%s\"", tfn);
		exit(EX_OSERR);
	}

	/* figure out who I am */
	myname = username();

	/*
	**  Copy p-file to temp file, doing deletions as needed.
	*/

	while ((pent = getpfent(pfp)) != NULL)
	{
		if (strcmp(pent->p_user, myname) == 0)
		{
			/* a match */
			delete++;
		}
		else
		{
			/* output it again */
			putpfent(pent, tfp);
			others++;
		}
	}

	/*
	 * Before changing anything, make sure we can remove
	 * the file in question (assuming it exists).
	 */
	if (delete) {
		extern int errno;

		cp = tail(fn);
		errno = 0;
		if (access(cp, 0) < 0 && errno != ENOENT)
			goto bad;
		if (errno == 0)
			/*
			 * This is wrong, but the rest of the program
			 * has built in assumptions about "." as well,
			 * so why make unedit a special case?
			 */
			if (access(".", 2) < 0) {
	bad:
				printf("%12s: can't remove\n", cp);
				fclose(tfp);
				fclose(pfp);
				unlink(tfn);
				return (FALSE);
			}
	}
	/* do final cleanup */
	if (others)
	{
		/* copy it back (perhaps it should be linked?) */
		if (freopen(tfn, "r", tfp) == NULL)
		{
			syserr("cannot reopen \"%s\"", tfn);
			exit(EX_OSERR);
		}
		if (freopen(pfn, "w", pfp) == NULL)
		{
			usrerr("cannot create \"%s\"", pfn);
			return (FALSE);
		}
		while (fgets(buf, sizeof buf, tfp) != NULL)
			fputs(buf, pfp);
	}
	else
	{
		/* it's empty -- remove it */
		unlink(pfn);
	}
	fclose(tfp);
	fclose(pfp);
	unlink(tfn);

	/* actually remove the g-file */
	if (delete)
	{
		/*
		 * Since we've checked above, we can
		 * use the return from unlink to
		 * determine if the file existed or not.
		 */
		if (unlink(cp) >= 0)
			printf("%12s: removed\n", cp);
		return (TRUE);
	}
	else
	{
		printf("%12s: not being edited by you\n", fn);
		return (FALSE);
	}
}

/*
**  DODIFF -- diff an s-file against a g-file
**
**	Parameters:
**		getv -- argv for the 'get' command.
**		gfile -- name of the g-file to diff against.
**
**	Returns:
**		Result of get.
**
**	Side Effects:
**		none.
*/

dodiff(getv, gfile)
	char **getv;
	char *gfile;
{
	int pipev[2];
	int rval;
	register int i;
	register int pid;
	auto int st;
	extern int errno;
	sig_t osig;

	printf("\n------- %s -------\n", gfile);
	fflush(stdout);

	/* create context for diff to run in */
	if (pipe(pipev) < 0)
	{
		syserr("dodiff: pipe failed");
		exit(EX_OSERR);
	}
	if ((pid = fork()) < 0)
	{
		syserr("dodiff: fork failed");
		exit(EX_OSERR);
	}
	else if (pid > 0)
	{
		/* in parent; run get */
		OutFile = pipev[1];
		close(pipev[0]);
		rval = command(&getv[1], TRUE, "get:rcixt -s -k -p");
		osig = signal(SIGINT, SIG_IGN);
		while (((i = wait(&st)) >= 0 && i != pid) || errno == EINTR)
			errno = 0;
		signal(SIGINT, osig);
		/* ignore result of diff */
	}
	else
	{
		/* in child, run diff */
		if (close(pipev[1]) < 0 || close(0) < 0 ||
		    dup(pipev[0]) != 0 || close(pipev[0]) < 0)
		{
			syserr("dodiff: magic failed");
			exit(EX_OSERR);
		}
		command(&getv[1], FALSE, "-diff:elsfhbC");
	}
	return (rval);
}

/*
**  TAIL -- return tail of filename.
**
**	Parameters:
**		fn -- the filename.
**
**	Returns:
**		a pointer to the tail of the filename; e.g., given
**		"cmd/ls.c", "ls.c" is returned.
**
**	Side Effects:
**		none.
*/

char *
tail(fn)
	register char *fn;
{
	register char *p;

	for (p = fn; *p != 0; p++)
		if (*p == '/' && p[1] != '\0' && p[1] != '/')
			fn = &p[1];
	return (fn);
}

/*
**  GETPFENT -- get an entry from the p-file
**
**	Parameters:
**		pfp -- p-file file pointer
**
**	Returns:
**		pointer to p-file struct for next entry
**		NULL on EOF or error
**
**	Side Effects:
**		Each call wipes out results of previous call.
*/

struct pfile *
getpfent(pfp)
	FILE *pfp;
{
	static struct pfile ent;
	static char buf[PFILELG];
	register char *p;
	extern char *nextfield();

	if (fgets(buf, sizeof buf, pfp) == NULL)
		return (NULL);

	ent.p_osid = p = buf;
	ent.p_nsid = p = nextfield(p);
	ent.p_user = p = nextfield(p);
	ent.p_date = p = nextfield(p);
	ent.p_time = p = nextfield(p);
	ent.p_aux = p = nextfield(p);

	return (&ent);
}


char *
nextfield(p)
	register char *p;
{
	if (p == NULL || *p == '\0')
		return (NULL);
	while (*p != ' ' && *p != '\n' && *p != '\0')
		p++;
	if (*p == '\n' || *p == '\0')
	{
		*p = '\0';
		return (NULL);
	}
	*p++ = '\0';
	return (p);
}
/*
**  PUTPFENT -- output a p-file entry to a file
**
**	Parameters:
**		pf -- the p-file entry
**		f -- the file to put it on.
**
**	Returns:
**		none.
**
**	Side Effects:
**		pf is written onto file f.
*/

putpfent(pf, f)
	register struct pfile *pf;
	register FILE *f;
{
	fprintf(f, "%s %s %s %s %s", pf->p_osid, pf->p_nsid,
		pf->p_user, pf->p_date, pf->p_time);
	if (pf->p_aux != NULL)
		fprintf(f, " %s", pf->p_aux);
	else
		fprintf(f, "\n");
}

/*
**  USRERR -- issue user-level error
**
**	Parameters:
**		f -- format string.
**		p1-p3 -- parameters to a printf.
**
**	Returns:
**		-1
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
usrerr(f, p1, p2, p3)
	char *f;
{
	fprintf(stderr, "\n%s: ", MyName);
	fprintf(stderr, f, p1, p2, p3);
	fprintf(stderr, "\n");

	return (-1);
}

/*
**  SYSERR -- print system-generated error.
**
**	Parameters:
**		f -- format string to a printf.
**		p1, p2, p3 -- parameters to f.
**
**	Returns:
**		never.
**
**	Side Effects:
**		none.
*/

/*VARARGS1*/
syserr(f, p1, p2, p3)
	char *f;
{
	extern int errno;

	fprintf(stderr, "\n%s SYSERR: ", MyName);
	fprintf(stderr, f, p1, p2, p3);
	fprintf(stderr, "\n");
	if (errno == 0)
		exit(EX_SOFTWARE);
	else
	{
		perror(NULL);
		exit(EX_OSERR);
	}
}
/*
**  USERNAME -- return name of the current user
**
**	Parameters:
**		none
**
**	Returns:
**		name of current user
**
**	Side Effects:
**		none
*/

char *
username()
{
# ifdef UIDUSER
	extern struct passwd *getpwuid();
	register struct passwd *pw;

	pw = getpwuid(getuid());
	if (pw == NULL)
	{
		syserr("who are you? (uid=%d)", getuid());
		exit(EX_OSERR);
	}
	return (pw->pw_name);
# else
	extern char *getlogin();
	register char *p;

	p = getenv("USER");
	if (p == NULL || p[0] == '\0')
		p = getlogin();
	return (p);
# endif UIDUSER
}

/*
**	Guarded string manipulation routines; the last argument
**	is the length of the buffer into which the strcpy or strcat
**	is to be done.
*/
char *gstrcat(to, from, length)
	char	*to, *from;
	int	length;
{
	if (strlen(from) + strlen(to) >= length) {
		gstrbotch(to, from);
	}
	return(strcat(to, from));
}

char *gstrncat(to, from, n, length)
	char	*to, *from;
	int	n;
	int	length;
{
	if (n + strlen(to) >= length) {
		gstrbotch(to, from);
	}
	return(strncat(to, from, n));
}

char *gstrcpy(to, from, length)
	char	*to, *from;
	int	length;
{
	if (strlen(from) >= length) {
		gstrbotch(from, (char *)0);
	}
	return(strcpy(to, from));
}
gstrbotch(str1, str2)
	char	*str1, *str2;
{
	usrerr("Filename(s) too long: %s %s", str1, str2);
}
