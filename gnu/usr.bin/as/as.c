/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 */

#ifndef lint
static char sccsid[] = "@(#)as.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/* as.c - GAS main program.
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Main program for AS; a 32-bit assembler of GNU.
 * Understands command arguments.
 * Has a few routines that don't fit in other modules because they
 * are shared.
 *
 *
 *			bugs
 *
 * : initialisers
 *	Since no-one else says they will support them in future: I
 * don't support them now.
 *
 */

#ifdef _POSIX_SOURCE
#include <sys/types.h>	/* For pid_t in signal.h */
#endif
#include <signal.h>

#define COMMON
#include "as.h"
#include "struc-symbol.h"
#include "write.h"
		/* Warning!  This may have some slightly strange side effects
		   if you try to compile two or more assemblers in the same
		   directory!
		 */

#ifndef SIGTY
#define SIGTY int
#endif

SIGTY got_sig();

#ifdef DONTDEF
static char * gdb_symbol_file_name;
long int gdb_begin();
#endif

char *myname;		/* argv[0] */
extern char version_string[];

main(argc,argv)
int	argc;
char	**argv;
{
	int	work_argc;	/* variable copy of argc */
	char	**work_argv;	/* variable copy of argv */
	char	*arg;		/* an arg to program */
	char	a;		/* an arg flag (after -) */
	static const int sig[] = { SIGHUP, SIGINT, SIGPIPE, SIGTERM, 0};

	extern int bad_error;	/* Did we hit a bad error ? */

	char	*stralloc();	/* Make a (safe) copy of a string. */
	void	symbol_begin();
	void	read_begin();
	void	write_object_file();

	for(a=0;sig[a]!=0;a++)
		if(signal(sig[a], SIG_IGN) != SIG_IGN)
			signal(sig[a], got_sig);

	myname=argv[0];
	bzero (flagseen, sizeof(flagseen)); /* aint seen nothing yet */
	out_file_name	= "a.out";	/* default .o file */
	symbol_begin();		/* symbols.c */
	subsegs_begin();		/* subsegs.c */
	read_begin();			/* read.c */
	md_begin();			/* MACHINE.c */
	input_scrub_begin();		/* input_scrub.c */
#ifdef DONTDEF
	gdb_symbol_file_name = 0;
#endif
	/*
	 * Parse arguments, but we are only interested in flags.
	 * When we find a flag, we process it then make it's argv[] NULL.
	 * This helps any future argv[] scanners avoid what we processed.
	 * Since it is easy to do here we interpret the special arg "-"
	 * to mean "use stdin" and we set that argv[] pointing to "".
	 * After we have munged argv[], the only things left are source file
	 * name(s) and ""(s) denoting stdin. These file names are used
	 * (perhaps more than once) later.
	 */
	work_argc = argc-1;		/* don't count argv[0] */
	work_argv = argv+1;		/* skip argv[0] */
	for (;work_argc--;work_argv++) {
		arg = * work_argv;	/* work_argv points to this argument */

		if (*arg!='-')		/* Filename. We need it later. */
			continue;	/* Keep scanning args looking for flags. */
		if (arg[1] == '-' && arg[2] == 0) {
			/* "--" as an argument means read STDIN */
			/* on this scan, we don't want to think about filenames */
			* work_argv = "";	/* Code that means 'use stdin'. */
			continue;
		}
				/* This better be a switch. */
		arg ++;		/* -> letter. */

		while (a = * arg)  {/* scan all the 1-char flags */
			arg ++;	/* arg -> after letter. */
			a &= 0x7F;	/* ascii only please */
			if (flagseen[a])
				as_warn("%s: Flag option -%c has already been seen!",myname,a);
			flagseen[a] = TRUE;
			switch (a) {
			case 'f':
				break;	/* -f means fast - no need for "app" preprocessor. */

			case 'D':
				/* DEBUG is implemented: it debugs different */
				/* things to other people's assemblers. */
				break;

#ifdef DONTDEF
			case 'G':	/* GNU AS switch: include gdbsyms. */
				if (*arg)	/* Rest of argument is file-name. */
					gdb_symbol_file_name = stralloc (arg);
				else if (work_argc) {	/* Next argument is file-name. */
					work_argc --;
					* work_argv = NULL; /* Not a source file-name. */
					gdb_symbol_file_name = * ++ work_argv;
				} else
					as_warn( "%s: I expected a filename after -G",myname);
				arg = "";	/* Finished with this arg. */
				break;
#endif

#ifndef WORKING_DOT_WORD
			case 'k':
				break;
#endif

			case 'L': /* -L means keep L* symbols */
				break;

			case 'o':
				if (*arg)	/* Rest of argument is object file-name. */
					out_file_name = stralloc (arg);
				else if (work_argc) {	/* Want next arg for a file-name. */
					* work_argv = NULL; /* This is not a file-name. */
					work_argc--;
					out_file_name = * ++ work_argv;
				} else
					as_warn("%s: I expected a filename after -o. \"%s\" assumed.",myname,out_file_name);
				arg = "";	/* Finished with this arg. */
				break;

			case 'R':
				/* -R means put data into text segment */
				break;

			case 'v':
#ifdef	VMS
				{
				extern char *compiler_version_string;
				compiler_version_string = arg;
				}
#else /* not VMS */
				fprintf(stderr,version_string);
				if(*arg && strcmp(arg,"ersion"))
					as_warn("Unknown -v option ignored");
#endif
				while(*arg) arg++;	/* Skip the rest */
				break;

			case 'W':
				/* -W means don't warn about things */
				break;

			case 'g':
				/*
				 * -g asks gas to produce gdb/dbx line number
				 * and file name stabs so that an assembly
				 * file can be handled by a source debugger.
				 */
				break;

			default:
				--arg;
				if(md_parse_option(&arg,&work_argc,&work_argv)==0)
					as_warn("%s: I don't understand '%c' flag!",myname,a);
				if(arg && *arg)
					arg++;
				break;
			}
		}
		/*
		 * We have just processed a "-..." arg, which was not a
		 * file-name. Smash it so the
		 * things that look for filenames won't ever see it.
		 *
		 * Whatever work_argv points to, it has already been used
		 * as part of a flag, so DON'T re-use it as a filename.
		 */
		*work_argv = NULL; /* NULL means 'not a file-name' */
	}
#ifdef DONTDEF
	if (gdb_begin(gdb_symbol_file_name) == 0)
		flagseen ['G'] = 0;	/* Don't do any gdbsym stuff. */
#endif
	/* Here with flags set up in flagseen[]. */
	perform_an_assembly_pass(argc,argv); /* Assemble it. */
	if (seen_at_least_1_file() && !bad_error)
		write_object_file();/* relax() addresses then emit object file */
	input_scrub_end();
	md_end();			/* MACHINE.c */
#ifndef	VMS
	exit(bad_error);			/* WIN */
#else	/* VMS */
	exit(!bad_error);			/* WIN */
#endif	/* VMS */
}
 

/*			perform_an_assembly_pass()
 *
 * Here to attempt 1 pass over each input file.
 * We scan argv[*] looking for filenames or exactly "" which is
 * shorthand for stdin. Any argv that is NULL is not a file-name.
 * We set need_pass_2 TRUE if, after this, we still have unresolved
 * expressions of the form (unknown value)+-(unknown value).
 *
 * Note the un*x semantics: there is only 1 logical input file, but it
 * may be a catenation of many 'physical' input files.
 */
perform_an_assembly_pass (argc, argv)
int	argc;
char **	argv;
{
	char *	buffer;		/* Where each bufferful of lines will start. */
	void	read_a_source_file();
	int saw_a_file = 0;

	text_fix_root		= NULL;
	data_fix_root		= NULL;
	need_pass_2		= FALSE;

	argv++;			/* skip argv[0] */
	argc--;			/* skip argv[0] */
	while (argc--) {
		if (*argv) {		/* Is it a file-name argument? */
			/* argv -> "" if stdin desired, else -> filename */
			if (buffer = input_scrub_new_file (*argv) ) {
				saw_a_file++;
				read_a_source_file(buffer);
			}
		}
		argv++;			/* completed that argv */
	}
	if(!saw_a_file)
		if(buffer = input_scrub_new_file("") )
			read_a_source_file(buffer);
}

/*
 *			stralloc()
 *
 * Allocate memory for a new copy of a string. Copy the string.
 * Return the address of the new string. Die if there is any error.
 */

char *
stralloc (str)
char *	str;
{
	register char *	retval;
	register long int	len;

	len = strlen (str) + 1;
	retval = xmalloc (len);
	(void)strcpy (retval, str);
	return (retval);
}

lose()
{
	as_fatal( "%s: 2nd pass not implemented - get your code from random(3)",myname );
}

SIGTY
got_sig(sig)
int sig;
{
	static here_before = 0;

	as_bad("Interrupted by signal %d",sig);
	if(here_before++)
		exit(1);
}

/* end: as.c */
