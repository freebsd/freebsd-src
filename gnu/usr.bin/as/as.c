/* as.c - GAS main program.
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

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
#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include <stdio.h>
#include <string.h>

#ifdef _POSIX_SOURCE
#include <sys/types.h>	/* For pid_t in signal.h */
#endif
#include <signal.h>

#define COMMON

#include "as.h"
#include "subsegs.h"
#if __STDC__ == 1

/* This prototype for got_sig() is ansi.  If you want
   anything else, then your compiler is lying to you when
   it says that it is __STDC__.  If you want to change it,
   #ifdef protect it from those of us with real ansi
   compilers. */

#define SIGTY void

static void got_sig(int sig);
static char *stralloc(char *str);
static void perform_an_assembly_pass(int argc, char **argv);

#else /* __STDC__ */

#ifndef SIGTY
#define SIGTY int
#endif

static SIGTY got_sig();
static char *stralloc();	/* Make a (safe) copy of a string. */
static void perform_an_assembly_pass();

#endif /* not __STDC__ */

#ifdef DONTDEF
static char * gdb_symbol_file_name;
long gdb_begin();
#endif

int listing; /* true if a listing is wanted */

char *myname;		/* argv[0] */
extern const char version_string[];

int main(argc,argv)
int argc;
char **argv;
{
	int work_argc;	/* variable copy of argc */
	char **work_argv;	/* variable copy of argv */
	char *arg;		/* an arg to program */
	char a;		/* an arg flag (after -) */
	static const int sig[] = { SIGHUP, SIGINT, SIGPIPE, SIGTERM, 0};

	for (a=0;sig[a] != 0;a++)
	    if (signal(sig[a], SIG_IGN) != SIG_IGN)
		signal(sig[a], got_sig);

	myname=argv[0];
	memset(flagseen, '\0', sizeof(flagseen)); /* aint seen nothing yet */
#ifndef OBJ_DEFAULT_OUTPUT_FILE_NAME
#define OBJ_DEFAULT_OUTPUT_FILE_NAME "a.out"
#endif /* OBJ_DEFAULT_OUTPUT_FILE_NAME */
	out_file_name = OBJ_DEFAULT_OUTPUT_FILE_NAME;

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
	/* FIXME-SOMEDAY this should use getopt. */
	work_argc = argc-1;		/* don't count argv[0] */
	work_argv = argv+1;		/* skip argv[0] */
	for (;work_argc--;work_argv++) {
		arg = * work_argv;	/* work_argv points to this argument */

		if (*arg != '-')		/* Filename. We need it later. */
		    continue;	/* Keep scanning args looking for flags. */
		if (arg[1] == '-' && arg[2] == 0) {
			/* "--" as an argument means read STDIN */
			/* on this scan, we don't want to think about filenames */
			* work_argv = "";	/* Code that means 'use stdin'. */
			continue;
		}
		/* This better be a switch. */
		arg ++;		/*->letter. */

		while ((a = * arg) != '\0')  {/* scan all the 1-char flags */
			arg ++;	/* arg->after letter. */
			a &= 0x7F;	/* ascii only please */
			/* if (flagseen[a])
			   as_tsktsk("%s: Flag option - %c has already been seen.", myname, a); */
			flagseen[a] = 1;
			switch (a) {

			case 'a':
				{
					int loop =1;

					while (loop) {
						switch (*arg)
						    {
						    case 'l':
							    listing |= LISTING_LISTING;
							    arg++;
							    break;
						    case 's':
							    listing |= LISTING_SYMBOLS;
							    arg++;
							    break;
						    case 'h':
							    listing |= LISTING_HLL;
							    arg++;
							    break;

						    case 'n':
							    listing |= LISTING_NOFORM;
							    arg++;
							    break;
						    case 'd':
							    listing |= LISTING_NODEBUG;
							    arg++;
							    break;
						    default:
							    if (!listing)
								listing= LISTING_DEFAULT;
							    loop = 0;
							    break;
						    }
					}
				}

				break;


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
				    as_warn("%s: I expected a filename after -G", myname);
				arg = "";	/* Finished with this arg. */
				break;
#endif

			case 'I': { /* Include file directory */

				char *temp = NULL;
				if (*arg)
				    temp = stralloc (arg);
				else if (work_argc) {
					* work_argv = NULL;
					work_argc--;
					temp = * ++ work_argv;
				} else
				    as_warn("%s: I expected a filename after -I", myname);
				add_include_dir (temp);
				arg = "";	/* Finished with this arg. */
				break;
			}

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
				    as_warn("%s: I expected a filename after -o. \"%s\" assumed.", myname, out_file_name);
				arg = "";	/* Finished with this arg. */
				break;

			case 'R':
				/* -R means put data into text segment */
				break;

			case 'v':
#ifdef	OBJ_VMS
				{
					extern char *compiler_version_string;
					compiler_version_string = arg;
				}
#else /* not OBJ_VMS */
				fprintf(stderr,version_string);
				if (*arg && strcmp(arg,"ersion"))
				    as_warn("Unknown -v option ignored");
#endif /* not OBJ_VMS */
				while (*arg) arg++;	/* Skip the rest */
				break;

			case 'W':
				/* -W means don't warn about things */
			case 'X':
				/* -X means treat warnings as errors */
			case 'Z':
				/* -Z means attempt to generate object file even after errors. */
				break;

			default:
				--arg;
				if (md_parse_option(&arg,&work_argc,&work_argv) == 0)
				    as_warn("%s: I don't understand '%c' flag.", myname, a);
				if (arg && *arg)
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
#ifdef PIC
	if (flagseen['K'] || flagseen['k'])
		picmode = 1;
#endif
#ifdef DONTDEF
	if (gdb_begin(gdb_symbol_file_name) == 0)
	    flagseen['G'] = 0;	/* Don't do any gdbsym stuff. */
#endif
	/* Here with flags set up in flagseen[]. */

	perform_an_assembly_pass(argc,argv); /* Assemble it. */
#ifdef TC_I960
	brtab_emit();
#endif
	if (seen_at_least_1_file()
	    && !((had_warnings() && flagseen['Z'])
		 || had_errors() > 0)) {
		write_object_file(); /* relax() addresses then emit object file */
	} /* we also check in write_object_file() just before emit. */

	input_scrub_end();
	md_end();			/* MACHINE.c */

#ifndef NO_LISTING
	listing_print("");
#endif

#ifndef	HO_VMS
	return((had_warnings() && flagseen['Z'])
	       || had_errors() > 0);			/* WIN */
#else	/* HO_VMS */
	return(!((had_warnings() && flagseen['Z'])
		 || had_errors() > 0));			/* WIN */
#endif	/* HO_VMS */

} /* main() */


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
static void perform_an_assembly_pass(argc, argv)
int argc;
char **argv;
{
	int saw_a_file = 0;
	need_pass_2		= 0;

#ifdef MANY_SEGMENTS
	unsigned int i;

	for (i= SEG_E0; i < SEG_UNKNOWN; i++)
	    {
		    segment_info[i].fix_root = 0;
	    }
	/* Create the three fixed ones */
	subseg_new (SEG_E0, 0);
	subseg_new (SEG_E1, 0);
	subseg_new (SEG_E2, 0);
	strcpy(segment_info[SEG_E0].scnhdr.s_name,".text");
	strcpy(segment_info[SEG_E1].scnhdr.s_name,".data");
	strcpy(segment_info[SEG_E2].scnhdr.s_name,".bss");

	subseg_new (SEG_E0, 0);
#else /* not MANY_SEGMENTS */
	text_fix_root		= NULL;
	data_fix_root		= NULL;
	bss_fix_root		= NULL;

	subseg_new (SEG_TEXT, 0);
#endif /* not MANY_SEGMENTS */

	argv++; /* skip argv[0] */
	argc--; /* skip argv[0] */
	while (argc--) {
		if (*argv) { /* Is it a file-name argument? */
			saw_a_file++;
			/* argv->"" if stdin desired, else->filename */
			read_a_source_file(*argv);
		}
		argv++; /* completed that argv */
	}
	if (!saw_a_file)
	    read_a_source_file("");
} /* perform_an_assembly_pass() */

/*
 *			stralloc()
 *
 * Allocate memory for a new copy of a string. Copy the string.
 * Return the address of the new string. Die if there is any error.
 */

static char *
    stralloc (str)
char *	str;
{
	register char *	retval;
	register long len;

	len = strlen (str) + 1;
	retval = xmalloc (len);
	(void) strcpy(retval, str);
	return(retval);
}

#ifdef comment
static void lose() {
	as_fatal("%s: 2nd pass not implemented - get your code from random(3)", myname);
	return;
} /* lose() */
#endif /* comment */

static SIGTY
    got_sig(sig)
int sig;
{
	static here_before = 0;

	as_bad("Interrupted by signal %d", sig);
	if (here_before++)
	    exit(1);
	return((SIGTY) 0);
}

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of as.c */
