/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "daps.c	1.6	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)daps.c	1.8 (gritter) 7/9/06
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

char *xxxvers = "troff.d/devaps/daps.c	1.2";



/****************************************************************************
 *																			*
 *		This is the post-processor for the APS-5 phototypesetter. The 		*
 *	language that is accepted by this program is produced by the new device	*
 *	independent troff, and consists of the following statements,			*
 *																			*
 *																			*
 *		sn			set the point size to n									*
 *		fn			set the typesetter font to the one in position n		*
 *		cx			output the ASCII character x 							*
 *		Cxyz		output the code for the special character xyz. This		*
 *					command is terminated by white space.					*
 *		Hn			go to absolute horizontal position n					*
 *		Vn			go to absolute vertical position n ( down is positive )	*
 *		hn			go n units horizontally from current position			*
 *		vn			go n units vertically from current position				*
 *		nnc			move right nn units, then print the character c. This	*
 *					command expects exactly two digits followed by the		*
 *					character c.											*
 *		w			paddable word space - no action needed					*
 *		nb a		end of line ( information only - no action needed )		*
 *		pn			begin page n											*
 *		Dt ...\n	draw operation 't':										*
 *																			*
 *						Dl x y		line from here to x,y					*
 *						Dc d		circle of diameter d, left side here	*
 *						De x y		ellipse of axes x,y, left side here		*
 *						Da u v x y	arc 									*
 *						D~ x y x y	wiggly line by x,y then x,y				*
 *																			*
 *		x ... \n	device control functions:								*
 *																			*
 *						x i			initialize the typesetter				*
 *						x T s		name of device is s						*
 *						x r n h v	resolution is n units per inch. h is	*
 *									min horizontal motion, v is min vert.	*
 *									motion in machine units.				*
 *						x p			pause - can restart the typesetter		*
 *						x s			stop - done forever						*
 *						x t			generate trailer - no-op for the APS	*
 *						x f n s		load font position n with tables for 	*
 *									font s. Referring to font n now means	*
 *									font s.									*
 *						x H n		set character height to n				*
 *						x S n		set character slant to n				*
 *																			*
 *						Subcommands like i are often spelled out as "init"	*
 *																			*
 *		To get the post-processor running properly on your system, you may	*
 *	have to make one or more of changes:									*
 *																			*
 *			Choose the appropriate description of your typesetter. These	*
 *			values include the type of lens and the maximum master range	*
 *			for your fonts. The values that you will need to adjust are		*
 *			macros and defined constants located at the start of the		*
 *			daps.h file.													*
 *																			*
 *			Make sure the variable 'typesetter' is properly initialized		*
 *			to the APS-5 typesetter file on your system. If you are not		*
 *			going to have daps directly drive the typesetter, you may 		*
 *			want to set it to the null file, and/or initialize 'tf' to		*
 *			be stdout. (file daps.g)										*
 *																			*
 *			Make sure that the accounting file pathname 'tracct' is the		*
 *			the accounting file that you want. If no accounting is to be	*
 *			done then initialize it to the null string. (file daps.g)		*
 *																			*
 *			Check to make sure that 'fontdir' is the directory that			*
 *			contains the devaps directory where your font tables are		*
 *			located. (file daps.g)											*
 *																			*
 *			If there are no characters on your font disk that need any		*
 *			adjustment in their vertical placement, then make sure that		*
 *			the conditional compilation flag ADJUST is undefined. I would	*
 *			recommend that you start this way to see what your font disk	*
 *			really looks like. (file daps.h)								*
 *																			*
 *																			*
 ****************************************************************************/






#include	<stdio.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<stdarg.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<pwd.h>

#include	"aps.h"					/* APS-5 opcode definitions */
#include	"dev.h"					/* font and dev structure declarations */

#include	"daps.h"				/* constant and macro definitions */
#include	"daps.g"				/* global variable definitions */





/*****************************************************************************/


int 
main (int argc, char *argv[])


{


	/********************************************************************
	 *																	*
	 *		This is the main program for the APS-5 post-processor. It 	*
	 *	is responsible for calling the sequence of routines that are	*
	 *	needed to translate troff's new typesetter independent output	*
	 *	language into a form that will be understood by the APS-5		*
	 *	phototypesetter.												*
	 *																	*
	 ********************************************************************/


	fp_debug = stderr;
	fp_error = stderr;
	fp_acct = stderr;
	tf = stdout;

	get_options(argc, argv);		/* process the option list */
	if ( tf != stdout )				/* need to open the file */
		out_file();
	init_signals();					/* set up signal handling */
	acct_file();					/* open the accounting file */
	process_input(argc, argv);		/* translate the input files */
	account();						/* make sure we charge this guy */
	done();							/* finish up this job and reset the APS */
	/*NOTREACHED*/
	return 0;

}	/* End of main */


/****************************************************************************/


void
get_options (int argc, char *argv[])


{


	int		save;					/* used to adjust arg_index */
	int		v_step = 0;				/* vertical step in -V option */


	int ch;
	int i, sharpsign = 0;
	extern char *optarg;
	extern int optind;


	/********************************************************************
	 *																	*
	 *		This is the routine that processes the command line option	*
	 *	list. The macro SET_ARGS uses the global variable arg_index to	*
	 *	properly initialize the local argc and argv values, while the	*
	 *	macro COUNT_ARGS adjusts the value of arg_index to account for	*
	 *	the number of arguments that were just processed.				*
	 *																	*
	 *		The options that are currently available in this driver 	*
	 *	are,															*
	 *																	*
	 *				-f dir	- 	use dir as the font directory			*
	 *				-F dir	-	same									*
	 *																	*
	 *				-t		-	use standard output						*
	 *																	*
	 *				-r		-	report the number of pages				*
	 *																	*
	 *				-A		-	do accounting even if there is no real	*
	 *							accounting file. If tracct is NULL then	*
	 *							the accounting information is written	*
	 *							to stderr. This is the way things are	*
	 *							done on our APS.						*
	 *																	*
	 *				-b		-	report whether typesetter is busy or	*
	 *							not. Nothing is printed.				*
	 *																	*
	 *				-w		-	wait until typesetter is available,		*
	 *							then procees the job.					*
	 *																	*
	 *				-o[str]	-	process only this list of pages. The	*
	 *							list may contain single pages or page	*
	 *							ranges, where the latter consists of 	*
	 *							a pair of pages separated by -.			*
	 *																	*
	 *				-s[num]	-	stop processing every num pages, and	*
	 *							HALT the typesetter.					*
	 *																	*
	 *				-v[num] -	use num as the maximum vertical step	*
	 *							size up or down the page. The argument	*
	 *							num is interpreted as 10ths of a point.	*
	 *																	*
	 *				-h[str]	-	use str as the string to be printed		*
	 *							in the header.							*
	 *																	*
	 *				-H[str]	-	use str as the pathname of the file		*
	 *							whose first line contains the string	*
	 *							to be printed in the header.			*
	 *																	*
	 *				-d[str] -	toggle the debug flags for each number	*
	 *							contained in the string str. If str		*
	 *							contains the character '*' then all of	*
	 *							the debug flags will be toggled.		*
	 *																	*
	 *				-D[str] -	dump all of the debug information into	*
	 *							file str. If this option is not used	*
	 *							then the debugging stuff is written to	*
	 *							stderr.									*
	 *																	*
	 *				-L[str] -	use the file str as the log file for	*
	 *							all error messages. If this option is	*
	 *							not used then all error messages will	*
	 *							be written to stderr.					*
	 *																	*
	 *				-I		-	ignore all FATAL errors. This option is	*
	 *							only to be used for debugging - it may	*
	 *							cause a core file to be written.		*
	 *																	*
	 ********************************************************************/



	SET_ARGS(save);					/* MACRO - adjust internal argc and argv */

	while ((ch = getopt(argc, argv, "f:F:trAbwo:s:h:H:d:D:L:Iv:c:#"))
		!= EOF) {				/*read options list*/

		switch ( ch )  {					/* check option */

			case 'f':								/* font directory */
			case 'F':
						fontdir = optarg;
						break;

			case 't':								/* use standard output */
						tf = stdout;
						break;

			case 'r':								/* print page report */
						report = YES;
						break;

			case 'A':								/* do accounting! */
						if ( privelege == ON )
							x_stat |= DO_ACCT;
						break;

			case 'b':								/* check if busy or not */
						busyflag = ON;
						break;

			case 'w':								/* wait til APS is free */
						waitflag = ON;
						break;

			case 'o':								/* process page list */
						outlist(optarg);
						break;

			case 's':								/* stop every spage(s) */
						spage = atoi(optarg);
						if ( spage <= 0 )			/* illegal page number */
							spage = 9999;
						break;

			case 'h':								/* banner is in argument */
						banner = optarg;
						print_banner = YES;
						break;

			case 'H':								/* banner is in file */
						ban_file(optarg);
						print_banner = YES;
						break;

			case 'd':								/* selective debug */
						debug_select(optarg);
						break;

			case 'D':								/* set up debug file */
						debug_file(optarg);
						break;

			case 'L':								/* set up log file */
						log_file(optarg);
						break;

			case 'I':								/* ignore fatal errors */
						if ( privelege == ON )
							ignore = YES;
						break;

			case 'v':								/* set max vertical step */
						v_step = atoi(optarg);
						if ( v_step != 0 )
							vert_step = ( v_step > 0 ) ? v_step
													   : -v_step;
						if ( vert_step > MAX_INT )	/* its too big */
							vert_step = MAX_INT;
						break;

			case 'c':								/* set beam cutoff */
						if ( (cutoff = atof(optarg)) <= 0 )
							cutoff = CUTOFF;
						break;

		case '#':
			sharpsign = 1;
			break;
			default:								/* didn't find it */
						error(NON_FATAL, "illegal option %c", argv[1][1]);
						break;

		}	/* End of switch */

	}	/* End while */

	argc -= optind - 1;
	COUNT_ARGS(save);				/* MACRO - adjust arg_index */
	if (sharpsign == 1) {
		fprintf(stderr, "report = %d, x_stat = %o\n",
			report, x_stat);
		fprintf(stderr, "busyflag = %d, waitflag = %d, ignore = %d\n",
			busyflag, waitflag, ignore);
		fprintf(stderr, "fontdir = %s, spage = %d, banner = %s\n",
			fontdir, spage, banner);
		fprintf(stderr, "v_step = %d, cutoff = %g\n",
			v_step, cutoff);
		for (i=0; i<nolist; i+=2)
			fprintf (stderr, "olist[%d] is %d; olist[%d] is %d.\n",
				i, olist[i], i+1, olist[i+1]);
		for (i=0; i<MAX_DEBUG; i+=4)
			fprintf(stderr, "debug[%d] = %d, %d, %d, %d\n",
				i,debug[i],debug[i+1],debug[i+2],debug[i+3]);
		if (fp_debug != stderr)
			fprintf(stderr, "fp_debug is %p\n", fp_debug);
		if (fp_error != stderr)
			fprintf(stderr, "fp_error is %p\n", fp_error);
		if (tf != stdout)
			fprintf(stderr, "tf is %p\n", tf);
	}

}	/* End of get_options */


/*****************************************************************************/


void
process_input (int argc, char *argv[])


{


	FILE	*fp_in;					/* input file descriptor */
	int 	save;					/* used to adjust arg_index before exit */



	/********************************************************************
	 *																	*
	 *		This routine is called by main to handle the processing		*
	 *	of the input files from the command line. If there were no		*
	 *	files specified in the call then the post-processor will read	*
	 *	from the standard input. Otherwise it will process all of the	*
	 *	input files in the list, concatenating the output from each		*
	 *	one onto the output file. The only convention that is used for	*
	 *	input file names is that the character '-' as a file name will	*
	 *	cause the driver to read from the standard input, provided it	*
	 *	isn't the first 'file' in the list of input files. It would		*
	 *	probably be better if we chose some other character or sequence	*
	 *	of characters for this purpose.									*
	 *																	*
	 ********************************************************************/



	SET_ARGS(save);						/* MACRO - adjust argc and argv */

	if ( argc <= 1 ) conv(stdin);		/* no more args - use stdin */
	else  {								/* read input file list */
		while ( --argc > 0 )  {			/* rest of the args are input files */

			if ( strcmp(*++argv, "-") == 0 )	/* use standard input */
				fp_in = stdin;
			else if ( (fp_in = fopen(*argv, "r")) == NULL )  {		
					error(FATAL, "can't open input file %s", *argv);
					continue;			/* in case we ignore this error */
			}	/* End else */

			conv(fp_in);				/* translate the file */
			if ( fp_in != stdin )		/* probably don't need it anymore */
				fclose(fp_in);

		}	/* End of while */
	}	/* End else */

	COUNT_ARGS(save);					/* MACRO - adjust arg_index */

}	/* End of process_input */


/****************************************************************************/


void
init_signals (void)


{





	/********************************************************************
	 *																	*
	 *		This routine is called by main to set up the appropriate	*
	 *	handling of external signals for the post-processor. As 		*
	 *	currently written interrupts, quits and hangups are all either	*
	 *	ignored or processed by the routine wrap_up().					*
	 *																	*
	 ********************************************************************/



	signal(SIGFPE, float_err);			/* catch floating point errors */

	if ( signal(SIGINT, wrap_up) == SIG_IGN )  {	/* ignoring interrupts */
		signal(SIGINT, SIG_IGN);					/* so reset SIGINT */
		signal(SIGQUIT, SIG_IGN);					/* and ignore the rest */
		signal(SIGHUP, SIG_IGN);
	} else {										/* wrap_up() handles them */
		signal(SIGQUIT, wrap_up);
		signal(SIGHUP, wrap_up);
	}	/* End if */

}	/* End of init_signals */


/*****************************************************************************/


void
debug_select (
    char *str						/* string of debug flags */
)


{


	int		index;						/* single debug flag to toggle */
	int		i;							/* for loop index */



	/********************************************************************
	 *																	*
	 *		This routine is called by main() when it finds the -d		*
	 *	option.	The parameter str is a pointer to a string of comma		*
	 *	separated tokens from the command line that specify which of 	*
	 *	the debug flags is to be toggled. As currently implemented the	*
	 *	tokens in str may consist of numbers, which specify the actual	*
	 *	flag to be toggled, or the character '*', which stands for all	*
	 *	of the available debug flags.									*
	 *																	*
	 ********************************************************************/



	while ( *str )  {

		if ( isdigit(*str) )  {			/* have a single debug flag */
			STR_CONVERT(str, index);	/* MACRO - get the debug flag */
			if ( index >= 0  &&  index < MAX_DEBUG )
				TOGGLE(debug[index]);	/* MACRO - toggle it */
		} else if ( *str == '*' )		/* toggle all the debug flags */
			for ( i = 0; i < MAX_DEBUG; i++ )
				TOGGLE(debug[i]);		/* MACRO */

		if ( *str != '\0' ) str++;		/* skip the comma */

	}	/* End while */

}	/* End of debug_select */


/*****************************************************************************/


void
debug_file (
    char *str						/* debug file pathname */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called by get_options() when it finds the	*
	 *	-D option in the command line. The parameter str is a pointer	*
	 *	to the pathname of the file to be used for all of the debugging	*
	 *	output. If the -D option is not specified then by default all	*
	 *	the debug output is written to stderr.							*
	 *																	*
	 ********************************************************************/



	if ( (fp_debug = fopen(str, "w")) == NULL )  {
		fp_debug = stderr;
		error(NON_FATAL, "can't open debug file %s", str);
	}	/* End if */

}	/* End of debug_file */


/*****************************************************************************/


void
log_file (
    char *str						/* log file pathname */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to open the log file for the APS-5	*
	 *	post-processor. The pathname of the file is passed in the		*
	 *	parameter str when this routine is called from get_options().	*
	 *	If the log file isn't specified then the post-processor will	*
	 *	write all of it's error messages to stderr.						*
	 *																	*
	 ********************************************************************/



	if ( (fp_error = fopen(str, "a")) == NULL )  {
		fp_error = stderr;
		error(FATAL, "can't open log file %s", str);
	}	/* End if */

}	/* End of log_file */


/*****************************************************************************/


void
acct_file (void)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to open the accounting file whose	*
	 *	pathname is pointed to by the variable tracct. If there is no	*
	 *	pathname in tracct then nothing is done in this routine, while	*
	 *	if we are unable to open the accounting file then an error		*
	 *	message is printed out and we quit.								*
	 *																	*
	 ********************************************************************/



	if ( *tracct )  {					/* we have an accnt file pathname */

		if ( (fp_acct = fopen(tracct, "a")) == NULL )  {	/* so open it */
			fp_acct = stderr;			/* couldn't open accounting file */
			x_stat |= NO_ACCTFILE;		/* indicate this in the exit status */
			error(FATAL, "unable to open accounting file");
			exit(x_stat);				/* in case we ignore this error */
		}	/* End if */
		x_stat |= DO_ACCT;				/* accounting needs to be done */

	}	/* End if */

}	/* End of acct_file */


/*****************************************************************************/


void
ban_file (
    char *str						/* banner file pathname */
)


{


	FILE	*fp_ban;					/* banner file descriptor */



	/********************************************************************
	 *																	*
	 *		This routine is called from get_options() to read the 		*
	 *	banner string from the first line of the file whose pathname is	*
	 *	contained in the string str.									*
	 *																	*
	 ********************************************************************/



	if ( (fp_ban = fopen(str, "r")) == NULL )  {
		error(NON_FATAL, "can't open the banner file %s", str);
		return;
	}	/* End if */

	GET_LINE(fp_ban, ban_buf);			/* MACRO - banner is first line only */
	banner = ban_buf;					/* t_banner() prints string *banner */

	fclose(fp_ban);						/* shouldn't need this file again */

}	/* End of ban_file */


/*****************************************************************************/


void
out_file (void)


{



	/********************************************************************
	 *																	*
	 *		This routine is called from the main program to open the	*
	 *	file pointed to by typesetter.									*
	 *																	*
	 ********************************************************************/



	do  {
		tf = fopen(typesetter, "w");	/* typesetter output file */
		if ( busyflag == ON )  {		/* report status and then exit */
			printf(tf == NULL ? "Busy.\n" : "Available.\n");
			exit(0);
		}	/* End if */

		if ( tf == NULL )  {			/* didn't open on last try */
			if ( waitflag == OFF )  {	/* he doesn't want to wait */
				error(NON_FATAL, "can't open typesetter");
				exit(NO_OUTFILE);
			}	/* End if */
			else sleep(60);				/* try again later */
		}	/* End if */
	}  while ( tf == NULL );

}	/* End of out_file */


/*****************************************************************************/


void
outlist (
    char *str						/* string of pages to process */
)


{


	int		start, stop;				/* page range end points */
	int		i;							/* loop index - debug only */



	/********************************************************************
	 *																	*
	 *		This routine is called when the -o option is read in		*
	 *	the command line. The parameter str points to a list of page	*
	 *	numbers to be processed. This list consists single pages or 	*
	 *	page ranges, separated by commas. A page range is specified by	*
	 *	separating two page numbers by the character '-'. In this case	*
	 *	all pages in this closed interval will be processed by daps.	*
	 *																	*
	 ********************************************************************/



	while ( *str  &&  nolist < MAX_OUTLIST - 2 )  {

		start = 0;

		if ( isdigit(*str) )				/* page number should begin here */
			STR_CONVERT(str,start);			/* MACRO - get left end point */
		else start = -9999;					/* use lowest possible page */

		stop = start;						/* in case it is a single page */

		if ( *str == '-' )  {				/* have a page range */
			str++;							/* so skip the minus sign */
			if ( isdigit(*str) )			/* page number begins here */
				STR_CONVERT(str,stop);		/* MACRO - get right end point */
			else stop = 9999;				/* use largest possible page */
		}	/* End if */

		if ( start > stop )
			error(FATAL,"illegal range %d-%d",start,stop);

		olist[nolist++] = start;			/* save the page range */
		olist[nolist++] = stop;

		if ( *str != '\0' ) str++;			/* skip the comma */

	}	/* End while */

	olist[nolist] = 0;						/* terminate the page list */

	if ( *str )							/* too many pages for olist array */
		error(NON_FATAL, "skipped pages %s", str);

	if ( debug[1] )							/* dump the olist[] array */
		for ( i = 0; i < nolist; i += 2 )
			fprintf(fp_debug,"%3d  %3d\n", olist[i], olist[i+1]);

}	/* End of outlist */


/*****************************************************************************/


void
error (
    int kind,					/* kind of error ie. FATAL or NON_FATAL */
    char *str,					/* pointer to message to be printed */
    ...
)


{


	/********************************************************************
	 *																	*
	 *		This routine is called when the post-processor has found	*
	 *	an internal error. The parameter kind has the value FATAL or	*
	 *	NON_FATAL, and accordingly determines whether processing will	*
	 *	continue or not. The parameter str is a pointer to the error	*
	 *	message that is to be printed. All the remaining parameters		*
	 *	are the arguments that may be referenced in the control string	*
	 *	str.															*
	 *																	*
	 *		The global variable ignore is initialized to NO in the		*
	 *	file daps.globals, and can be set to YES by using the -I option	*
	 *	in the command line. This will allow the post-processor to		*
	 *	continue after a normally FATAL error has been encountered. 	*
	 *	This is only a debugging feature and should not generally be	*
	 *	used.															*
	 *																	*
	 ********************************************************************/


	va_list ap;

	fprintf(fp_error, "daps: ");
	if ( (kind == NON_FATAL) && (line_number > 0) )
		fprintf(fp_error, "warning - ");
	va_start(ap, str);
	vfprintf(fp_error, str, ap);
	va_end(ap);

	if ( line_number > 0 )
		fprintf(fp_error, " ( line = %ld )", line_number);
	fprintf(fp_error, "\n");

	if ( ignore == YES  &&  privelege == ON )
		return;

	if ( kind == FATAL )			/* can't ignore this error */
		wrap_up(0);					/* so quit */

}	/* End of error */


/*****************************************************************************/


int 
done (void)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to do the final processing for the	*
	 *	current job. If there is to be any accounting for this job we	*
	 *	need to be sure that the account function is called first		*
	 *	because there is nothing more we can do when we get here.		*
	 *																	*
	 ********************************************************************/



	if ( tf == NULL ) 					/* Nowhere to write */
		exit(x_stat | NO_OUTFILE);		/* so set the bit in x_stat and quit */

	t_reset('s');						/* get APS ready for the next job */
	exit(x_stat);						/* quit with status x_stat */

}	/* End of done */


/*****************************************************************************/


void 
float_err (
    int sig						/* signal number - not used */
)


{



	/********************************************************************
	 *																	*
	 *		Called when a floating point error has been detected.		*
	 *	Needed because we want to make sure we exit gracefully if a		*
	 *	users job would normally dump a core file.						*
	 *																	*
	 ********************************************************************/



	error(FATAL, "floating point exception");

}	/* End of float_err */


/*****************************************************************************/


void 
wrap_up (
    int sig						/* signal number - not used */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to make sure that all the necessary	*
	 *	stuff is done when the driver finishes its job because of some	*
	 *	external signal or because it encountered a FATAL syntax error.	*
	 *																	*
	 ********************************************************************/



	account();							/* keep some kind of record for this job */
	done();								/* get the APS ready for next job */

}	/* End of wrap_up */


/*****************************************************************************/


void
conv(


	register FILE	*fp				/* input file descriptor */
)


{


	register int	ch;					/* first character of the command */
	int		c;							/* used only as a character */
	int		n;							/* general purpose integer variable */
	char	str[100], buf[300];			/* buffers for fscanf and fgets */



	/********************************************************************
	 *																	*
	 *		This is the main interpreter for the post-processor. It is	*
	 *	called from routine process_input with the single parameter		*
	 *	fp, which is the file descriptor for the current input file.	*
	 *																	*
	 *		The global variable line_number is used to keep track of	*
	 *	the current line in the input file fp. Its value is adjusted	*
	 *	in both conv() and devcntrl() and it is used in the routine		*
	 *	error() when error messages are written to the file fp_error.	*
	 *																	*
	 *		The bits in the global variable x_stat are used to keep 	*
	 *	track of the progress of the post-processor, and when the 		*
	 *	program exits it will return x_stat as its termination status.	*
	 *																	*
	 *																	*
	 *		NOTE - In order to improve the speed of this routine we may	*
	 *	want to declare more register variables. When we do this we 	*
	 *	need to be sure that the macros that are being used will accept	*
	 *	register variables as arguments. For example if a macro takes	*
	 *	the address of one of its arguments, then we can't assign this	*
	 *	variable to a register.											*
	 *																	*
	 ********************************************************************/



	x_stat |= FILE_STARTED;					/* indicate this in x_stat */
	line_number = 1;						/* line in current input file */

	while ( (ch = getc(fp) ) != EOF )  {

		switch ( ch )  {					/* ch determines the command */

			case 'w':						/* don't do anything for these */
			case ' ':
			case  0:
						break;

			case '\n':						/* just increment line_number */
						line_number++;
						break;

			case '0':						/* two motion digits and a char */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
						GET_DIG(fp, c);		/* MACRO - get the second digit */
						hmot((ch - '0') * 10 + c - '0');

				/* Be careful - we need to fall through here */

			case 'c':						/* single ASCII character */
						GET_CHAR(fp, c);	/* MACRO - read the character */
						put1(c);			/* output c's APS-5 code */
						break;

			case 'h':						/* relative horizontal motion */
			case 'H':						/* absolute horizontal motion */
			case 'v':						/* relative vertical motion */
			case 'V':						/* absolute vertical motion */
			case 's':						/* set point size */
			case 'p':						/* start a new page */
						GET_INT(fp, n);		/* MACRO - first get an integer */
						switch( ch )  {		/* and then process the command */

							case 'h':	hmot(n);
										break;

							case 'H':	hgoto(n);
										break;

							case 'v':	vmot(n);
										break;

							case 'V':	vgoto(n);
										break;

							case 's':	setsize(t_size(n));
										break;

							case 'p':	t_page(n);
										break;

						}	/* End switch */
						break;

			case 'C':						/* process special char string */
			case 'f':						/* set font */
						GET_STR(fp, str);	/* MACRO - first get a string */
						if ( ch == 'C' )
							 put1s(str);
						else setfont(t_font(str));
						break;

			case 'x':						/* device control function */
						devcntrl(fp);
						break;

			case 'D':						/* drawing operation */
			case 't':						/* text string upto newline */
						GET_LINE(fp, buf);	/* MACRO - get rest of the line */
						if ( ch == 'D' )
							 drawfunct(buf, fp);
						else t_text(buf);
						line_number++;		/* finished with this line */
						break;

			case 'n':						/* end of line */
			case '#':						/* comment */
						SKIP_LINE(fp,c);	/* MACRO - skip rest of this line */
						if ( ch == 'n' )
							t_newline();
						line_number++;		
						break;

			default:						/* illegal command - quit */
						error(FATAL,"unknown input character 0%o %c", ch, ch);
						break;				/* in case we ignore this error */

		}	/* End switch */

	}	/* End while */

	x_stat &= ~FILE_STARTED;				/* turn off FILE_STARTED bit in x_stat */

}	/* End of conv */


/*****************************************************************************/


void
drawfunct(


	char	buf[],						/* drawing command */
	FILE	*fp
)


{


	int		n1, n2, n3, n4;				/* values are set in the MACROS */



	/********************************************************************
	 *																	*
	 *		This routine interprets the drawing functions that are 		*
	 *	provided by troff. The array buf[] has been filled in by the	*
	 *	function conv(), and it contains the drawing command line from	*
	 *	the input file. 												*
	 *																	*
	 ********************************************************************/



	switch ( buf[0] )  {					/* process the command */

		case 'l':							/* draw a line */
					SCAN2(buf+1, n1, n2);	/* MACRO - get two integers */
					drawline(n1, n2, ".");
					break;

		case 'c':							/* draw a circle */
					SCAN1(buf+1, n1);		/* MACRO - get one integer */
					drawcirc(n1);
					break;

		case 'e':							/* draw an ellipse */
					SCAN2(buf+1, n1, n2);	/* MACRO - get two integers */
					drawellip(n1, n2);
					break;

		case 'a':							/* draw an arc */
					SCAN4(buf+1,n1,n2,n3,n4);	/* MACRO - get four integers */
					drawarc(n1, n2, n3, n4);
					break;

		case '~':							/* draw spline curve */
					drawwig(buf+1);
					break;

#ifdef PLOT
		case 'p':							/* plot these points */
					plot_points(buf+1, fp);
					break;
#endif
		default:							/* don't understand the command */
					error(FATAL, "unknown drawing function %s", buf);
					break;					/* in case we ignore this error */

	}	/* End switch */

}	/* End of drawfunct */


/*****************************************************************************/


void
devcntrl(


	FILE	*fp						/* input file descriptor */
)


{


	int		c;							/* character used in SKIP_LINE */
	int		n;							/* integer used in GET_INT */
	char	str[20];					/* used to hold different strings */
	char	file[50];					/* load from this font file - maybe */
	char	buf[4096];					/* buffer used in GET_LINE etc. */



	/********************************************************************
	 *																	*
	 *		This is the interpreter for the device control language		*
	 *	that is produced by the new troff. The parameter fp is the file	*
	 *	descriptor for the current input file.							*
	 *																	*
	 ********************************************************************/



	GET_STR(fp, str);						/* read command from input file */

	switch ( str[0] )  {					/* str[0] determines the command */

		case 'i':							/* initialize the device */
					fileinit();				/* read data from DESC.out */
					t_init();				/* initialize the typesetter */
					if ( print_banner == YES )	 /* print the job's banner */
						t_banner();
					break;

		case 'T':							/* set device name */
					GET_STR(fp, devname);	/* MACRO - get device string */
					if ( strcmp(devname, "aps") != SAME_STR )
						error(FATAL, "illegal typesetter %s", devname);
					break;

		case 't':							/* trailer - do nothing on APS-5 */
					break;

		case 'p':							/* pause - we can restart */
		case 's':							/* stop - done with this job */
					t_reset(str[0]);		/* reset the typesetter */
					break;

		case 'r':							/* set resolution */
					GET_INT(fp, res);		/* MACRO - get one integer */
					hcutoff = cutoff * res;	/* beam cutoff in device units */
					break;

		case 'f':							/* load a font */
					GET_INT(fp, n);			/* MACRO - put font number in n */
					GET_STR(fp, str);		/* MACRO - put font name in str */
					GET_LINE(fp, buf);		/* MACRO - use this filename */
					ungetc('\n', fp);		/* put '\n' back for SKIP_LINE */
					file[0] = 0;			/* in case there is no file name */
					SCAN_STR(buf, file);	/* MACRO - may have a file name */
					loadfont(n, str, file);	
					break;

		case 'H':							/* set character height */
					GET_INT(fp, n);			/* MACRO - read vertical point size */
					t_charht(t_size(n));
					break;

		case 'S':							/* set character slant */
					GET_INT(fp, n);			/* MACRO - read APS slant angle */
					t_slant(n);				/* set the APS slant to n */
					last_req_slant = n;		/* and remember this angle */
					break;

		case 'X':
					GET_STR(fp, buf);
					if (strcmp(buf, "LC_CTYPE") == 0)
						break;
					/*FALLTHRU*/

		default:							/* don't understand the command */
					error(FATAL, "unknown device command %c", str[0]);
					break;					/* in case we ignore this error */

	}	/* End switch */

	SKIP_LINE(fp, c);						/* finished with this line */
	line_number++;

}	/* End of devcntrl */


/*****************************************************************************/


void
t_init (void)


{


	int		i;							/* for loop variable */



	/********************************************************************
	 *																	*
	 *		This routine is called from devcntrl() when the command		*
	 *	x init is read. It produces the instructions that initialize	*
	 *	the APS, and then it sets up the picture drawing variables		*
	 *	drawdot and drawsize.											*
	 *																	*
	 *																	*
	 *		NOTE - the opcode STRTJOB is defined to be octal 272 in the	*
	 *	file aps.h, although on our APS-5 typesetter it is one of the	*
	 *	reserved but not implemented opcodes. 							*
	 *																	*
	 ********************************************************************/



	PUTC(STRTJOB, tf);					/* MACRO - output STRTJOB opcode */
	putint(1);							/* dummy argument for STRTJOB? */
	PUTC(STRTPG, tf);					/* MACRO - start page 0 */
	putint(0);

	hpos = vpos = 0;					/* initialize page coordinates */

	for ( i = 0; i < nchtab; i++ )		/* find drawing character index */
		if ( strcmp(&chname[chtab[i]], "l.") == SAME_STR )
			break;

	if ( i < nchtab )  {				/* found it in the table */
		drawdot = i + 128;				/* so use these values for drawing */
		drawsize = 1;
	} else  {							/* didn't find it - use default */
		drawdot = '.';
		drawsize = 3;
	}	/* End if */

}	/* End of t_init */


/*****************************************************************************/


void
t_banner (void)


{


	int		i;							/* while loop counter */
	char 	*bp;						/* temp pointer to banner string */



	/********************************************************************
	 *																	*
	 *		This routine is called from devcntrl() when the device		*
	 *	initialization command is read. It is responsible for printing	*
	 *	the job's header. This includes checking and then writing out	*
	 *	the string pointed to by banner. The user may set this string	*
	 *	by using either the -h or the -H options on the command line.	*
	 *																	*
	 ********************************************************************/



	output = ON;						/* ON for routine put1() etc. */
	bp = banner;						/* point to start of banner string */
	i = 0;								/* characters looked at so far */

	while ( *bp )						/* check at most BAN_LENGTH chars */
		if ( *bp++ < ' '  ||  ++i > BAN_LENGTH )	/* unprintable or too long */
			*(--bp) = '\0';				/* so last char is as far as we go */

	setsize(t_size(BAN_SIZE));			/* use font 1 and point size BAN_SIZE */

	vmot(VSPACE0);						/* space down for banner cut marks */
	t_text(cut_marks);					/* print first set of cut marks */
	hmot(HSPACE0);						/* space right for more cut marks */
	t_text(cut_marks);					/* print second set of cut marks */

	vmot(VSPACE1);						/* skip this far before banner */
	hgoto(HSPACE1);						/* indent */
	t_text(ban_sep);					/* start of banner */

	vmot(VSPACE2);						/* space down for banner string */
	hgoto(HSPACE2);						/* indent */
	t_text(banner);						/* print the user's banner */

	vmot(VSPACE3);						/* skip down for next separator */
	hgoto(HSPACE3);						/* indent */
	t_text(ban_sep);					/* print the second separator */

	vmot(VSPACE4);						/* skip down for banner cut marks */
	hgoto(HSPACE4);						/* position for first cut marks */
	t_text(cut_marks);					/* print first set of cut marks */
	hmot(HSPACE0);						/* skip right for second cut marks */
	t_text(cut_marks);					/* print second set */
	
	hpos = vpos = 0;					/* reset these guys again */
	cur_vpos = max_vpos = 0;			/* user doesn't own the banner */
	print_banner = NO;					/* don't print it again */
	output = OFF;						/* output is controlled in t_page() */

}	/* End of t_banner */


/*****************************************************************************/


void
t_page (int n)


{


	int		i;							/* for loop index */



	/********************************************************************
	 *																	*
	 *		This routine is called from conv() to do the work that is	*
	 *	necessary when we begin a new page. The STRTPG command to the	*
	 *	APS-5 invokes the RESET command which sets the font to 0000,	*
	 *	the master range to 1, and sets the oblique mode to normal and	*
	 *	the oblique angle to 14 degrees. To reset the font and range	*
	 *	properly we call setsize(), while if we were in oblique mode	*
	 *	the variable aps_slant will be non-zero and so in this case		*
	 *	we call routine t_slant() with aps_slant as the parameter.		*
	 *																	*
	 ********************************************************************/



	if ( output == ON  &&  ++scount >= spage )  {	/* reached stop page */
		t_reset('p');					/* so reset the APS and then HALT */
		scount = 0;						/* and start counting over again */
	}	/* End if */

	vpos = 0;							/* we are at the top of new page */
	output = ON;						/* enable output in put1() etc. */
	last_slant = POS_SLANT;				/* this will be the stored angle */

	PUTC(STRTPG, tf);					/* MACRO - start page n */
	putint(n);
	++pageno;							/* update user's page number */

	setsize(size);						/* reset both the font and size */
	if ( aps_slant )					/* then the APS was in oblique mode */
		t_slant(aps_slant);				/* so go back to previous slant */

	if ( nolist == 0 ) return;			/* no -o option, so print every page */
	output = OFF;						/* otherwise -o option was specified */
	for ( i = 0; i < nolist; i += 2 )	/* so check page pairs in olist[] */
		if ( n >= olist[i]  &&  n <= olist[i+1] )  {
			output = ON;				/* enable output for this page */
			break;
		}	/* End if */

}	/* End of t_page */


/*****************************************************************************/


void
t_newline (void)


{



	/********************************************************************
	 *																	*
	 *		This routine is called from conv() when it has read the		*
	 *	start new line command. This command has the form "n a b" where	*
	 *	a and b are integers that we can safely ignore for the APS-5.	*
	 *																	*
	 ********************************************************************/



	hpos = 0;							/* return to left margin */

}	/* End of t_newline */


/*****************************************************************************/


int 
t_size (
    int n							/* convert this point size */
)


{

	int		i;							/* for loop index */



	/********************************************************************
	 *																	*
	 *		This routine is called to convert the point size n to an	*
	 *	internal size, which is defined as one plus the index of the	*
	 *	least upper bound for n in the array pstab[]. If n is larger	*
	 *	than all the entries in pstab[] then nsizes is returned.		*
	 *																	*
	 *																	*
	 *		NOTE - this routine expects the entries in pstab[] to be in	*
	 *	increasing numerical order, but it doesn't require this list to	*
	 *	be terminated by a 0 point size entry.							*
	 *																	*
	 ********************************************************************/



	if ( n >= pstab[nsizes-1] )			/* greater than all entries */
		return(nsizes);					/* so use largest internal size */

	for ( i = 0; n > pstab[i]; i++ )	/* otherwise find the LUB for n */
		;
	return(i+1);						/* internal size is i+1 */

}	/* End of t_size */


/*****************************************************************************/


void
t_charht (
    int n							/* set height to this internal size */
)


{


	int		max;						/* max internal size for current range */



	/********************************************************************
	 *																	*
	 *		This routine is called by devcntrl() to set the height of	*
	 *	the characters that are being printed to the internal size		*
	 *	specified by the parameter n. If the requested size is too 		*
	 *	large for the current range then an error message is written	*
	 *	and the requested size is set to the maximum allowed in the		*
	 *	current range. Since the APS-5 apparently allows us to decrease	*
	 *	the height as far as we want, no lower limit checks are made on	*
	 *	the requested size. The global variable range is set by the		*
	 *	routine setfont() to the current master range that is being		*
	 *	used.															*
	 *																	*
	 ********************************************************************/



	if ( range < 1  ||  range > MAX_RANGE )  {	/* something is wrong here */
		error(FATAL, "illegal master range %d", range);
		return;							/* in case this error is ignored */
	}	/* End if */

	max = upper_limit(range);			/* internal upper limit for range */

	if ( n > max )  {					/* requested size is too big */
		error(NON_FATAL, "size %d too large for range %d", pstab[n-1], range);
		n = max;						/* reset n to largest allowed size */
	}	/* End if */

	PUTC(VSIZE, tf);					/* MACRO - set character height */
	putint(10 * pstab[n-1]);			/* vertical size - in decipoints */

}	/* End of t_charht */


/*****************************************************************************/


int 
upper_limit (
    int n							/* find upper limit for this range */
)


{


	int		bsize;						/* master range base size */
	int		max;						/* maximum point size for range n */
	int		max_internal;				/* maximum internal size for range n */



	/********************************************************************
	 *																	*
	 *		This routine is called by t_charht() and possibly others to	*
	 *	find the maximum internal size that is allowed for master range	*
	 *	n. The value returned to the caller is the largest internal		*
	 *	size that is allowed in this range.								*
	 *																	*
	 ********************************************************************/



	bsize = BASE_SIZE(n);				/* base size for master range n */
	max = bsize;						/* max point size if n > 3 */
	if ( n <= 3 )						/* can scale these ranges up */
		max = SCALE_UP(bsize);			/* to this limit on our APS-5 */

	max_internal = t_size(max);			/* first try at max internal size */
	if ( pstab[max_internal -1] > max )	/* take next lower internal size */
		max_internal--;

	return(max_internal);

}	/* End of upper_limit */


/*****************************************************************************/


void
t_slant (
    int n							/* set the APS slant to this value */
)


{



	/********************************************************************
	 *																	*
	 *		Called to set the slant angle to the value of the parameter	*
	 *	n. On the APS-5 we can only set positive or negative 14 degree	*
	 *	slants, even though in TROFF any slant angle can be requested.	*
	 *	The global variable last_slant is the value of the last angle	*
	 *	that was stored in the APS-5 using either the SETOBLIQUE or		*
	 *	STRTPG commands, while aps_slant is the last angle we set in	*
	 *	this routine.													*
	 *																	*
	 *		Originally we only set the oblique angle if last_slant		*
	 *	was not equal to n. This really didn't work too well, because	*
	 *	appaerntly the APS-5 resets the oblique angle to +14 degrees	*
	 *	when it returns to normal mode.									*
	 *																	*
	 ********************************************************************/



	if ( n != 0 )  {					/* need to slant type being set */

		n = ( n > 0 ) ? POS_SLANT		/* use +14 degree slant */
					  : NEG_SLANT;		/* otherwise slant at -14 degrees */

		PUTC(SETOBLIQUE, tf);			/* MACRO - store new angle n */
		putint(10 * n);					/* APS expects 10 times the angle */
		last_slant = n;					/* remember this stored angle */

		PUTC(XOBLIQUE, tf);				/* MACRO - execute oblique mode */

	}	/* End if */
	else PUTC(XNORMAL, tf);				/* MACRO - otherwise use normal mode */

	aps_slant = n;						/* angle that type is being set at */

}	/* End of t_slant */


/*****************************************************************************/


int 
t_font (
    char *str						/* convert this string to font number */
)


{


	int		n;							/* integer value for number in str */



	/********************************************************************
	 *																	*
	 *		This routine is called from conv() to convert the ASCII		*
	 *	string *str to an integer that represents a legal font number.	*
	 *	If the resulting number is outside the allowed range for fonts	*
	 *	on this typesetter then an error message is printed out and		*
	 *	the program is aborted.											*
	 *																	*
	 ********************************************************************/



	n = atoi(str);						/* font number */
	if ( n < 0  ||  n > nfonts )  {		/* illegal font - abort */
		error(FATAL, "illegal font number %d", n);
		n = font;						/* in case we don't quit on an error */
	}	/* End if */

	return(n);							/* legal value so return it */

}	/* End of t_font */


/*****************************************************************************/


void
t_text (
    char *str						/* typeset this string of characters */
)


{


	int		ch;							/* internal character variable */
	char	buf[4];						/* buffer used for special chars */



	/********************************************************************
	 *																	*
	 *		This routine is called by conv() to process the text string	*
	 *	that is in the array str. Characters are read from str and		*
	 *	written to the output file until the end of the string is		*
	 *	reached.														*
	 *																	*
	 *		After the character has been put in the output file the		*
	 *	current horizontal position is adjusted by a call to hmot()		*
	 *	using the global variable lastw as the parameter. lastw is set	*
	 *	in put1() and represents the width of the last character that	*
	 *	was printed.													*
	 *																	*
	 ********************************************************************/



	if ( debug[2] )
		fprintf(fp_debug,"input string = %s\n", str);

	if ( output == OFF ) return;		/* not doing output on this page */

	while ( (ch = *str++) != '\n'  &&  ch != '\0' )  {

		if ( ch == '\\' )  {			/* this is a special char sequence */
			switch ( ch = *str++ )  {	/* so check the next character */

				case '(':				/* special troff character sequence */
							buf[0] = *str++;
							buf[1] = *str++;
							buf[2] = '\0';
							put1s(buf);	
							break;

				case '\\':				/* backslash character */
				case 'e':
							put1('\\');
							break;

				default:				/* illegal character sequence */
							error(FATAL,"illegal character sequence \\%c", ch);
							break;

			}	/* End switch */
		} else put1(ch);				/* otherwise it is a simple character */
		hmot(lastw);					/* beam has moved right lastw units */

		if ( debug[3] )
			fprintf(fp_debug,"char = %c  width = %d\n", ch, lastw);

	}	/* End while */

}	/* End of t_text */


/*****************************************************************************/


void
t_reset (
    int ch							/* pause or stop */
)


{


	int		n;							/* for loop variable */
	long	dist;						/* distance to end of job */
	int		opcode;						/* ENDJOB or HALT */



	/********************************************************************
	 *																	*
	 *		This routine is called to produce the typesetter commands	*
	 *	that are required at the end of a job. If the parameter ch has	*
	 *	the value 's' then we have reached the end of the current job	*
	 *	and so we output the ENDJOB opcode, otherwise ch should be		*
	 *	equal to 'p', and so we produce the HALT command. This enables	*
	 *	the operator to restart the current job by pressing the PROCEED	*
	 *	button on the front panel, while if he hits the INITIAL button	*
	 *	the current job will be terminated.								*
	 *																	*
	 *		If we have finished this job, but we are not currently at	*
	 *	position max_vpos, we step the job forward to this position so	*
	 *	that the next job begins on unexposed paper.					*
	 *																	*
	 ********************************************************************/



	output = ON;						/* probably not needed here? */

	if ( ((dist = max_vpos - cur_vpos) > 0) && (ch == 's') )  {
		dist += (pstab[nsizes-1] / 2) * 10;		/* try to get whole last line */
		while ( dist > 0 )  {			/* go to the end of the job */
			vmot(dist > MAX_INT ? MAX_INT : dist);
			dist -= (dist > MAX_INT) ? MAX_INT : dist;
		}	/* End while */
	}	/* End if */

	PUTC(STRTPG, tf);					/* MACRO - output STRTPG opcode */
	putint(9000+pageno);				/* can't possibly be a page number */
	
	for ( n = 0; n < 10; n++ )			/* flush out APS internal buffer */
		PUTC(APSNOOP, tf);				/* MACRO - put out a few no-op's */

	opcode = ( ch == 'p' ) ? HALT		/* may want to restart this job */
						   : ENDJOB;	/* done with this guy */

	PUTC(opcode, tf);					/* MACRO - stop */

	fflush(tf);							/* flush any buffered output */

}	/* End of t_reset */


/*****************************************************************************/


void
hflush (void)


{



	/********************************************************************
	 *																	*
	 *		This routine is called in put1() and t_push() to make sure 	*
	 *	that the two variables hpos and htrue aren't too different. If	*
	 *	they differ by more than the constant SLOP then a tab is set 	*
	 *	and then executed to position the beam properly.				*
	 *																	*
	 *		The variable hpos is the current horizontal position as 	*
	 *	determined by troff, while htrue is the horizontal position as	*
	 *	calculated by the post-processor.								*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output for this page */

	if ( abs(hpos - htrue) > SLOP )  {	/* positions are too different */
		PUTC(SETTAB, tf);				/* MACRO - so set a tab */
		putint(hpos);					/* to the current value of hpos */
		PUTC(XTAB, tf);					/* MACRO - then execute the tab */
		htrue = hpos;					/* the positions are the same now */
	}	/* End if */

}	/* End of hflush */


/*****************************************************************************/


void
hmot (
    int n							/* move this far from here */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called from conv() to handle a relative		*
	 *	horizontal motion of n units. If n is positive then we move to	*
	 *	the right on the current line. If the final horizontal position	*
	 *	hpos is negative then something has gone wrong and so we print	*
	 *	out an error message and if we return from error() we set hpos	*
	 *	to 0.															*
	 *																	*
	 ********************************************************************/



	if ( (hpos += n ) < 0 || hpos > hcutoff )  {		/* bad beam position */
		error(FATAL, "illegal horizontal position %d", hpos);
		hpos = 0;						/* in case we ignore this error */
	}	/* End if */

}	/* End of hmot */


/*****************************************************************************/


void
hgoto (
    int n							/* move to this horizontal position */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called by conv() to set the absolute		*
	 *	horizontal position of the beam to the position n, where n		*
	 *	must be a positive integer.										*
	 *																	*
	 ********************************************************************/



	if ( (hpos = n) < 0 || hpos > hcutoff )  {		/* bad beam position */
		error(FATAL, "illegal horizontal position %d", hpos);
		hpos = 0;						/* in case we ignore this error */
	}	/* End if */

}	/* End of hgoto */


/*****************************************************************************/


void
vgoto (
    int n							/* final absolute vert position */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called from conv() to position the beam at	*
	 *	the absolute vertical position n. The unit used in all of the	*
	 *	APS absolute spacing commands is 1/10 of a point.				*
	 *																	*
	 *																	*
	 *		NOTE - it is important to check that a job doesn't try to	*
	 *	write on a previous job by using the vertical spacing commands.	*
	 *	Currently this check is made in the routine vmot().				*
	 *																	*
	 ********************************************************************/



	vmot(n - vpos);						/* move n-vpos units from here */

}	/* End of vgoto */


/*****************************************************************************/


void
vmot (
    int n							/* move n units vertically from here */
)


{


	int		sign;						/* sign of the requested motion */
	int		dist;						/* distance left to move */


	/********************************************************************
	 *																	*
	 *		This routine is called to move the vertical position of the	*
	 *	beam n units from the current position. The global variable 	*
	 *	cur_vpos is the typesetter's current vertical position as 		*
	 *	measured from the start of the job. If the user has requested	*
	 *	a relative vertical motion that would make cur_vpos negative	*
	 *	we print an error message and then abort. The variable max_vpos	*
	 *	is the maximum vertical position that this job has reached. It 	*
	 *	is used to calculate the amount of paper that has been used.	*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output on this page */

	if ( cur_vpos + n < 0 )  {			/* trying to write on last job! */
		error(FATAL, "can't backup past start of job");
		return;							/* in case we ignore this error */
	}	/* End if */

	sign = ( n > 0 ) ? 1 : -1;			/* up or down the page */
	dist = sign * n;					/* total distance to move */

	while ( dist > 0 )  {				/* not done yet */
		PUTC(VSPABS, tf);				/* MACRO - absolute vert motion */
		putint(sign * ((dist > vert_step) ? vert_step : dist));
		dist -= ( dist > vert_step ) ? vert_step : dist ;
	}	/* End while */

	vpos += n;							/* record our new vertical position */
	cur_vpos += n;
	if ( cur_vpos > max_vpos )			/* this is the farthest we have gone */
		max_vpos = cur_vpos;

}	/* End of vmot */


/*****************************************************************************/


void
put1s (
    char *s							/* print this special character */
)


{


	int		i;							/* for loop index */



	/********************************************************************
	 *																	*
	 *		This routine is called to produce the typesetter output		*
	 *	for the special character string pointed to by s. All of the	*
	 *	special characters are listed in the charset portion of the 	*
	 *	typsetter's DESC file, and the program makedev reads this part	*
	 *	of the DESC file and produces two tables called chtab[] and		*
	 *	chname[]. The character array chname[] contains the special		*
	 *	character strings separated by '\0', while chtab[i] gives us 	*
	 *	the starting position in chname[] for the special character i.	*
	 *																	*
	 *																	*
	 *		NOTE - Since we just do a sequential search of the strings	*
	 *	in chname[] when looking for a special character, the speed of	*
	 *	the post-processor could be improved if we had the most common	*
	 *	special characters at the start of the list in the DESC file.	*
	 *	However before doing this we should find out how troff does 	*
	 *	the lookup.														*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output for this page */

	for ( i = 0; i < nchtab; i++ )		/* lookup the special character */
		if ( strcmp(&chname[chtab[i]], s) == SAME_STR )
			break;
	
	if ( i < nchtab )  {				/* found it */
#ifdef ADJUST
		t_adjust(s);					/* adjust vertical positions */
#endif
		put1(i + 128);					/* special characters start at 128 */
	}	/* End if */
	else								/* didn't find it - abort */
		if ( newfile(s, pstab[size-1]) )
			error(FATAL, "special character %s not found", s);

	if ( debug[4] )  {
		fprintf(fp_debug,"string = %s  ", s);
		fprintf(fp_debug,"index = %d\n", i);
	}	/* End if */

}	/* End of put1s */


/*****************************************************************************/


#ifdef ADJUST


void
t_adjust (
    char *s							/* look for this string */
)


{


	int		i;							/* for loop index */
	int		incr;						/* incrment v_adjust by this much */



	/********************************************************************
	 *																	*
	 *		Called from put1s() to look for the string *s in adj_tbl[]	*
	 *	and if found it is used to set the appropriate vertical for the	*
	 *	character.														*
	 *																	*
	 ********************************************************************/


	v_adjust = 0;

	for ( i = 0; adj_tbl[i] != '\0'; i++ )
		if ( strcmp(s, adj_tbl[i]) == 0 )  {
			incr = (vadjustment[i] < 0) ? -1 : 1;
			v_adjust = (vadjustment[i] * pstab[size-1]) /dev.unitwidth + incr;
			break;
		}	/* End if */

}	/* End of t_adjust */

#endif


/*****************************************************************************/


void
put1 (
    int c							/* print this character */
)


{


	register int	i = 0;					/* c's index in font data structures */
	register int	k;					/* look for c on this font */
	int		j;							/* lookup failed on this many fonts */
	int		k1;							/* if not found check this font next */
	int		code;						/* APS-5 code for this character */
	int		old_font;					/* original font number (1..nfonts) */
	int		old_range;					/* original master range setting */
	int		last_range;					/* last master range setting */
	int		old_slant;					/* original device slant angle */



	/********************************************************************
	 *																	*
	 *		This routine is responsible for producing the output codes	*
	 *	for the characters to be printed by the APS. The integer c		*
	 *	is the ASCII code for the character if c < 128. Otherwise it 	*
	 *	refers to one of troff's special character sequences. Since we	*
	 *	are not concerned with unprintable ASCII characters we subtract	*
	 *	32 from c to get the right index to use when we do the lookup	*
	 *	in array fitab[][]. If the character isn't found on the current	*
	 *	font then we search all the remaining fonts, starting with the	*
	 *	first special font. If we haven't found the character after we	*
	 *	search the last font then an error message is written out and	*
	 *	we quit. Since we can't be guarenteed that font position 0 has	*
	 *	been loaded with a valid font, we need to make sure that the	*
	 *	circular search skips this position.							*
	 *																	*
	 *		If we find the character c, but it has an APS code that is	*
	 *	larger than 128, then the makedev program has encoded some		*
	 *	extra information in this field. Therefore we call the routine	*
	 *	special_case() to decode this info, and return the correct APS	*
	 *	code for c.														*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output on this page */

	old_font = font;					/* may find c on a different font */
	old_range = last_range = range;		/* and in a different master range */
	old_slant = aps_slant;				/* and may change the slant for c */

	k = font;							/* while loop looks for c on font k */
	k1 = smnt - 1;						/* get the next font from this guy */
	j = 0;								/* c not found on this many fonts */

	c -= 32;							/* tables don't include unprintable chars */

	if ( c <= 0 )  {					/* c is a space or unprintable */
		if ( c < 0 )   					/* can't be in any of our font tables */
			error(FATAL, "non-printable character 0%o\n", c+32);

		lastw = (widthtab[font][0] & BMASK) * pstab[size-1] / dev.unitwidth;
		return;							/* lastw = space width (see t_text()) */
	}	/* End if */


	/* Look for character c in current font and then on the special fonts */


	while ( (j < nfonts + 1) && ((i = fitab[k][c] & BMASK) == 0) )  {
		k = (k1++) % nfonts + 1;		/* now check all other fonts */
		j++;
	}	/* End while */


	/* If j > nfonts or i == 0 then char c not found. no-op if code = 0 */


	if ( j > nfonts || i == 0 || (code = codetab[k][i] & BMASK) == 0 )  {
		if ( i == 0  ||  j > nfonts )	
			if ( c+32 < 128 || newfile(&chname[chtab[c+32-128]], pstab[size-1]) )
				error(FATAL, "character 0%o not found", c+32);
		return;
	}	/* End if */

	if ( aps_font != fontname[k].number )	/* probably need a new font */
		if ( (code < 129) || (fontbase[font]->spare1 != fontbase[k]->spare1) )
			CHANGE_FONT(k, last_range);

	if ( code > 128  &&  fontbase[k]->spare1 )  {	/* got some more stuff to do */
		code = special_case(i, k);		/* this is the real APS code for c */
		last_range = range;				/* in case the range was changed */
	}	/* End if */

#ifdef ADJUST

	if ( v_adjust != 0 )
		vmot(v_adjust);

#endif

	hflush();							/* get to right horizontal position */
	PUTC(code, tf);						/* MACRO - then print the character */

#ifdef ADJUST

	if ( v_adjust != 0 )  {
		vmot(-v_adjust);
		v_adjust = 0;
	}	/* End if */

#endif

	if ( (last_range != old_range) || (aps_slant != old_slant) )
		CHANGE_FONT(old_font, last_range);	/* MACRO - back to the old font */

	lastw = ((widthtab[k][i] & BMASK) * pstab[size-1] + dev.unitwidth/2) / dev.unitwidth;
	htrue += lastw;						/* approximate right side of char */

}	/* End of put1 */


/*****************************************************************************/


void
putint (
    int n							/* write out this integer */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to write the integer n out in the	*
	 *	last two bytes of a type two command on the APS-5 typesetter.	*
	 *																	*
	 ********************************************************************/



	PUTC(n >> 8, tf);					
	PUTC(n, tf);						

}	/* End of putint */


/*****************************************************************************/


void
setsize (
    int n							/* new internal size */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to set the current internal size to	*
	 *	the value n, and then output the commands needed to change the	*
	 *	APS-5 point size. The internal size n is an index into the		*
	 *	array pstab[], which contains the actual point size that is to	*
	 *	be used.														*
	 *																	*
	 *																	*
	 *		NOTE - This routine always calls setfont in case the range	*
	 *	has changed, but it may be the case that the range doesn't		*
	 *	really change even though the point size has changed. Always 	*
	 *	making this call definitely produces some unnecessary APS-5		*
	 *	code. Probably fix it in setfont routine.						*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output on this page */

	if ( n <= 0  ||  n > nsizes )  {	/* internal size out of range */
		error(FATAL, "illegal internal size %d", n);
		n = size;						/* in case we return from this error */
	}	/* End if */

	size = n;							/* must preceed call to setfont() */
	change_font(font);					/* in case the range has changed */
	PUTC(HVSIZE, tf);					/* MACRO - set the new point size */
	putint(10 * pstab[n-1]);			/* APS expects 10 times the size */

}	/* End of setsize */


/*****************************************************************************/


void
setfont (
    int n							/* new font's internal number */
)


{


	int		oldrange;					/* current master range */



	/********************************************************************
	 *																	*
	 *		This routine is called to set the current typesetter font	*
	 *	to the one that corresponds to the parameter n. These internal	*
	 *	font numbers are determined by the way the font tables are set	*
	 *	up for the typesetter in the DESC file.							*
	 *																	*
	 *		Our APS-5 typesetter typically has only master ranges 1, 2,	*
	 *	and 3. The actual font that we want to set is determined by the	*
	 *	current value of the global variable size. Remember that this	*
	 *	value is an internal size, which is an index into the array		*
	 *	pstab[]. The following table gives the point sizes that can be	*
	 *	used in the different master ranges,							*
	 *																	*
	 *																	*
	 *					MASTER RANGE 1:   0 to 12						*
	 *					MASTER RANGE 2:  12 to 24						*
	 *					MASTER RANGE 3:  24 to 48						*
	 *					MASTER RANGE 4:  48 to 96						*
	 *																	*
	 *																	*
	 *	although any range can actually print characters up to size,	*
	 *																	*
	 *																	*
	 *						1.5 * limit - 1								*
	 *																	*
	 *																	*
	 *	where the value of limit is obtained from the above table.		*
	 *																	*
	 ********************************************************************/



	if ( output == OFF ) return;		/* not doing output for this page */

	if ( n < 0  ||  n > nfonts )  {
		error(FATAL, "requested font number %d out of range", n);
		n = font;						/* in case we ignore this error */
	}	/* End if */

	oldrange = range;					/* new range may be different */
	CHANGE_FONT(n, oldrange);			/* change the APS font */
	font = n;							/* current internal font number */

}	/* End of setfont */


/*****************************************************************************/


void
change_font (
    int n							/* new font's internal number */
)


{



	int		angle;						/* slant angle - used in SETSLANT */
	int		max_range;					/* largest range allowed on font n */
	int		max_size;					/* largest size allowed in max_range */
	char	f_spare1;					/* value of font flags */



	/********************************************************************
	 *																	*
	 *		This routine is called to change the font that the APS is	*
	 *	currently setting type in. Some extra stuff is done if the font	*
	 *	has any of its special flags set.								*
	 *																	*
	 ********************************************************************/



	range = get_range(pstab[size-1]);	/* probably the new range */

	f_spare1 = fontbase[n]->spare1;		/* get special case font flags */
	if ( f_spare1 & RANGE_BIT )  {		/* have a maximum range for font n */

		max_range = DECODE(f_spare1, RANGE_VAL, TWO_BITS);	/* decode this range */
		if ( range > max_range )  {		/* current range is too big */
			max_size = upper_limit(max_range);
			if ( size > max_size )		/* can't blow max_range up to size */
				error(FATAL, "size %d too big for range %d", pstab[size-1], max_range);
			range = max_range;			/* use the biggest possible range */
		}	/* End if */

	}	/* End if */

	if ( f_spare1 & SLANT_BIT )			/* this font has a default slant */
		SETSLANT(f_spare1, angle);		/* MACRO - so use it */
	else if ( aps_slant != last_req_slant )		/* back to the last requested slant */
		t_slant(last_req_slant);


	PUTC(FONT, tf);						/* MACRO - set the new font and range */
	if ( range < 3 )					/* APS expects a positive number */
		 putint(fontname[n].number + range-1);
	else putint(-(fontname[n].number + range-1));

	aps_font = fontname[n].number;		/* font that the APS is using */

}	/* End of change_font */


/*****************************************************************************/


void
t_fp (
    int n,							/* update this font position */
    char *s,							/* font's external name (eg. R) */
    char *si						/* font number used by the APS */
)


{



	/********************************************************************
	 *																	*
	 *		This routine is called to update the data structures that	*
	 *	are maintained by the post-processor to keep track of fonts.	*
	 *	The parameter n is the font position that we are going to		*
	 *	update, while s and si are the font's external and internal		*
	 *	names.															*
	 *																	*
	 ********************************************************************/



	fontname[n].name = s;
	fontname[n].number = atoi(si);
	fontname[n].nwfont = fontbase[n]->nwfont;

}	/* End of t_fp */

/*****************************************************************************/

void
getuserid(char *buf, size_t size)
{
	struct passwd	*pwd;

	if ((pwd = getpwuid(getuid())) != NULL)
		strncpy(buf, pwd->pw_name, size)[size-1] = '\0';
}

/*****************************************************************************/


void
account (void)


{


	char	user[100];				/* user's login name */
	float	pages;						/* will charge for this many pages */



	/********************************************************************
	 *																	*
	 *		This is the accounting routine for the post-processor. It	*
	 *	may have to be changed for your particular system.				*
	 *																	*
	 *		NOTE - since the variable res can be changed we better use	*
	 *	the constant RES to determine how many pages were printed.		*
	 *																	*
	 ********************************************************************/



	paper = max_vpos;						/* used this much paper */
	pages = paper / (RES * PAGE_LENGTH);	/* pages that we think were used */

	if ( x_stat & DO_ACCT )  {				/* got accounting to do */
		getuserid(user, sizeof user);
		fprintf(fp_acct, " user = %-10s", user);
		fprintf(fp_acct, " paper = %-10.1f", pages);
		x_stat &= ~DO_ACCT;					/* done the important accounting */
		fprintf(fp_acct, "exit status = 0%-6o", x_stat);
		if ( tf == stdout )
			fprintf(fp_acct, "  ??");
		fprintf(fp_acct, "\n");
	}	/* End if */

	if ( report == YES )
		fprintf(stderr, " %-3.1f pages\n", pages);

}	/* End of account */


/*****************************************************************************/


int 
special_case (
    int index,						/* char position in tables */
    int font						/* char info is on this font */
)


{


	int		old_range;					/* saved value of current range */
	int		old_font;					/* present APS-5 font number */
	int		req_font;					/* requested APS-5 font number */
	int		max_range;					/* max allowed range for character */
	int		max_size;					/* max size allowed for max_range */
	int		angle;						/* special char angle to set */
	int		code;						/* old code - with bits set */



	/********************************************************************
	 *																	*
	 *		This routine is called by put1() to handle the special any	*
	 *	special case characters for the APS-5. 							*
	 *																	*
	 *		Taken on its own, this is a very confusing routine, but one	*
	 *	which I found from my experience with the APS-5 at Murray Hill	*
	 *	was definitely needed. I'll try to give a brief, but hopefully	*
	 *	complete explination here.										*
	 *																	*
	 *		The additions that I made here needed to be completely		*
	 *	transparent to troff, and so the only field in the font tables	*
	 *	that could be changed was the actual charater code, since troff	*
	 *	can't possibly need this stuff (it's device independent!). On	*
	 *	the APS-5, character codes lie between 1 and 128 (inclusive) -	*
	 *	anything else is an actual function code. Therefore when we are	*
	 *	ready to print a character in put1(), and we find that its code	*
	 *	is larger than 128 we know something more has to be done. This	*
	 *	routine is then called to try to make sense out of the code,	*
	 *	do any of the special stuff required for this character, and	*
	 *	finially return its real code. This extra stuff can include		*
	 *	changing the character's slant, ensuring that the current		*
	 *	master range is not larger than a given value (max_range), and	*
	 *	finially getting the actual character from a completely			*
	 *	different font (alternate_font). What we actually do for each	*
	 *	special character is determined by its code in the current		*
	 *	set of font tables. This code is produced by a special version	*
	 *	of the makedev program, which I changed to interpret some 		*
	 *	extra fields in the ASCII font tables.					 		*
	 *																	*
	 *																	*
	 *		NOTE - This extra stuff is costly, and should only be used	*
	 *	when absolutely necessary. The alternate font stuff can			*
	 *	cause many extra font changes, which in turn will force extra	*
	 *	disk accesses. We found this to be one of the main reasons for	*
	 *	slow jobs on our APS, not to mention the extra wear on the disk	*
	 *	drive.															*
	 *																	*
	 ********************************************************************/



	if ( fontname[font].number != alt_tables )	/* don't have font's table */
		load_alt(font);							/* load font's '.add' tables */

	code = codetab[font][index] & BMASK;	/* encoded character information */
	old_range = range;						/* current typesetter range */
	old_font = aps_font;					/* quick change - fix it later */
	req_font = old_font;

	if ( code & SLANT_BIT )				/* special slant for this character */
		SETSLANT(code, angle);			/* MACRO - set slant to encoded value */

	if ( code & RANGE_BIT )  {			/* maximum range specified */

		max_range = DECODE(code, RANGE_VAL, TWO_BITS);
		range = get_range(pstab[size-1]);	/* get range for current size */
		if ( range > max_range )  {		/* check current size first */
			max_size = upper_limit(max_range);
			if ( size > max_size )		/* too big for max_range - abort */
				error(FATAL, "size %d too large for range %d", pstab[size-1], max_range);
			else range = max_range;		/* set the range */
		}	/* End if */

	}	/* End if */

	if ( code & FONT_BIT )				/* alternate font specified */
		req_font = alt_font[index];		/* APS-5 alternate font number */

	if ( (old_range != range) || (req_font != old_font) )  {	

		aps_font = req_font;
		PUTC(FONT, tf);					/* set new font */
		if ( range < 3 )				/* output positive number */
			putint(req_font + range - 1);
		else putint(-(req_font + range - 1));
		if ( range != old_range )  {	/* Need to set the size */
			PUTC(HVSIZE, tf);			/* MACRO - set the point size */
			putint(10 * pstab[size-1]);
		}	/* End if */

	}	/* End if */

	return(alt_code[index] & BMASK);

}	/* End of special_case */


/*****************************************************************************/


int 
get_range (
    int n							/* find the range for this point size */
)


{


	int		val;						/* return this as the range */
	int		msize;						/* size for master range val */



	/********************************************************************
	 *																	*
	 *		This routine is called to return the range corresponding to	*
	 *	the point size n. Note that n is not an internal size for this	*
	 *	routine.														*
	 *																	*
	 ********************************************************************/



	val = 0;

	while ( ++val <= MAX_RANGE )  {
		msize = BASE_SIZE(val);			/* base size for master range val */
		if ( (val == MAX_RANGE) && (val <= 3) )		/* scale it up */
			msize = SCALE_UP(msize);
		if ( n <= msize ) return(val);	/* found the correct master range */
	}	/* End while */

	error(FATAL, "size %d too large", n);
	return(1);							/* in case we ignore this error */

}	/* End of get_range */


/*****************************************************************************/


void
fileinit (void)


{


	int		fin;						/* input file descriptor */
	int		nw;							/* number of font table entries */
	int		i;							/* for loop index */
	char	*filebase;					/* pointer to memory block */
	char	*p;							/* pointer used to set up tables */
	char	temp[60];					/* pathname of the DESC.out file */



	/********************************************************************
	 *																	*
	 *		This routine is responsible for reading the DESC.out file	*
	 *	for the APS-5 typesetter, and then initializing the appropriate	*
	 *	data structures for the device and all of the default fonts		*
	 *	that were mentioned in the typesetter description file DESC.	*
	 *																	*
	 *		First the DESC.out file is opened and the structure dev is	*
	 *	initialized from the first part of the file. This structure 	*
	 *	then contains all the information needed to enable us to finish	*
	 *	the initialization process. Next the rest of the file is read	*
	 *	in and the device tables pstab[], chtab[], and chname[] are set	*
	 *	up. After this has been done we enter a loop which handles the	*
	 *	initialization for all of the fonts that were mentioned in the	*
	 *	DESC file. Finially we allocate enough memory so that font		*
	 *	position 0 can be loaded with the tables from any valid font 	*
	 *	in the library.													*
	 *																	*
	 *																	*
	 *		NOTE - the program 'makedev' creates all of the '.out' 		*
	 *	files in the troff font library. This routine quite obviously	*
	 *	depends on how 'makedev' has written these files. In addition	*
	 *	the troff program also reads the '.out' files and it too 		*
	 *	expects the same format as we do here! To really understand		*
	 *	this routine you need to look at 'makedev.c' and the ASCII		*
	 *	tables in the troff font library.								*
	 *																	*
	 ********************************************************************/



	sprintf(temp, "%s/dev%s/DESC.out", fontdir, devname);
	if ( (fin = open(temp, O_RDONLY)) < 0 )	/* file didn't open - abort */
		error(FATAL, "can't open tables for %s", temp);

	READ(fin, &dev, sizeof(struct dev));	/* init dev structure */

	nsizes = dev.nsizes;				/* number of point sizes specified */
	nchtab = dev.nchtab;				/* number of char table entries */
	nfonts = dev.nfonts;				/* number of default fonts */

	if ( nfonts > NFONT )				/* need to redefine NFONT */
		error(FATAL, "internal error - constant NFONT too small");

	filebase = malloc(dev.filesize);	/* space for rest of the file */
	READ(fin, filebase, dev.filesize);	/* read in the rest of the file */

	pstab = (short *) filebase;			/* point size list is first */
	chtab = pstab + nsizes + 1;			/* next comes chtab list */
	chname = (char *) (chtab + dev.nchtab);	/* then the char table list */

	p = chname + dev.lchname;			/* start of font table info */

	for (i = 1; i <= nfonts; i++)  {	/* set up default font tables */

		fontbase[i] = (struct Font *) p;
		nw = *p & BMASK;				/* width count comes first */

		if (smnt == 0  &&  fontbase[i]->specfont == 1)
			smnt = i;					/* this is first special font */

		p += sizeof(struct Font);		/* font structure was written first */
		widthtab[i] = p;				/* next is this font's width table */
		codetab[i] = p + 2 * nw;		/* skip kern to get the code table */
		fitab[i] = p + 3 * nw;			/* last is the font index table */

		p += 3 * nw + dev.nchtab + 128 - 32;	/* point to next font */

		t_fp(i, fontbase[i]->namefont, fontbase[i]->intname);

		if ( debug[5] ) fontprint(i);	/* dump font tables */

	}	/* End for */

	if ( smnt == 0 ) 					/* no special fonts specified */
		smnt = nfonts + 1;				/* used in routine put1() */

	fontbase[0] = (struct Font *) malloc(3*255 + dev.nchtab + (128-32) + sizeof(struct Font));
	widthtab[0] = (char *) fontbase[0] + sizeof(struct Font);
	fontbase[0]->nwfont = 255;
	close(fin);

}	/* End of fileinit */


/*****************************************************************************/


void
fontprint (
    int i							/* index of font to print out */
)


{


	int		j;							/* for loop variable */
	int		n;							/* number of width entries */
	int		pos;						/* position of char from fitab array */
	int		count;						/* count of characters in this font */



	/********************************************************************
	 *																	*
	 *		This is a debugging routine that is used to dump all of the	*
	 *	information about font i that was loaded from the '.out' file.	*
	 *	It is called from fileinit() when the debug flag 5 is turned 	*
	 *	on, and from loadfont when flag 6 is set.						*
	 *																	*
	 ********************************************************************/



	n = fontbase[i]->nwfont & BMASK;	/* number of width entries */

	fprintf(fp_debug, "\nDUMP FOR FONT %s ", fontbase[i]->namefont);
	fprintf(fp_debug, "   FONT POSITION = %d\n\n", i);
	fprintf(fp_debug, "  font structure data:\n");
	fprintf(fp_debug, "\t\tfont.nwfont = %d\n", fontbase[i]->nwfont & BMASK);
	fprintf(fp_debug, "\t\tfont.specfont = %d\n", fontbase[i]->specfont & BMASK);
	fprintf(fp_debug, "\t\tfont.ligfont = %d\n", fontbase[i]->ligfont & BMASK);
	fprintf(fp_debug, "\t\tfont.spare1 = %d\n", fontbase[i]->spare1 & BMASK);
	fprintf(fp_debug, "\t\tfont.intname = %s\n\n", fontbase[i]->intname);

	fprintf(fp_debug, "  CHAR     WIDTH      CODE     INDEX\n");
	count = 0;

	for (j = 0; j < dev.nchtab + 128 - 32; j++)  {	 

		if ( (pos = fitab[i][j] & BMASK) != 0 )  {
			count++;
			if ( j >= 96 )					/* special chars start at 128-32 */
				 fprintf(fp_debug, "%5s", &chname[chtab[j-96]]);
			else fprintf(fp_debug, "%5c", (j + 32) & BMASK);
			fprintf(fp_debug, "%10d", widthtab[i][pos] & BMASK);
			fprintf(fp_debug, "%10d", codetab[i][pos] & BMASK);
			fprintf(fp_debug, "%10d", j);
			if ( alt_tables == fontname[i].number )		/* print extra info */
				fprintf(fp_debug, "%10d%10d", alt_code[pos] & BMASK, alt_font[pos]);
			fprintf(fp_debug,"\n");
		}	/* End if */

	}	/* End for */

	fprintf(fp_debug, "\n CHARACTER COUNT FOR THIS FONT = %d\n", count);
	fprintf(fp_debug, "\n\n");

}	/* End of fontprint */


/*****************************************************************************/


void
loadfont (
    int num,						/* font position to load */
    char *name,						/* name of the font to load */
    char *fdir						/* name of directory to load from? */
)


{


	int		fin;						/* font file descriptor */
	int		nw;							/* width entries in new font file */
	int		norig;						/* width entries in old font file */
	char	temp[60];					/* path name of font file */



	/********************************************************************
	 *																	*
	 *		This routine is called to load the font position num with	*
	 *	the data for the font name. If the string fdir is not null then	*
	 *	it is taken as the pathname of the directory that contains the	*
	 *	'.out' file for the font that we are to load, otherwise we load	*
	 *	this data from the standard font library.						*
	 *																	*
	 ********************************************************************/



	if ( num < 0  ||  num > NFONT )		/* illegal font number - abort */
		error(FATAL, "illegal fp command %d %s", num, name);

	if ( strcmp(name, fontbase[num]->namefont) == SAME_STR )	/* in already */
		return;

	if ( fdir == NULL  ||  fdir[0] == '\0' )	/* no alternate directory */
		 sprintf(temp, "%s/dev%s/%s.out", fontdir, devname, name);
	else sprintf(temp, "%s/%s.out", fdir, name);

	if ( (fin = open(temp, O_RDONLY)) < 0 )			/* open the font file */
		error(FATAL, "can't open font table %s", temp);

	norig = fontbase[num]->nwfont & BMASK;	/* 'space' available in pos num */
	READ(fin, fontbase[num], 3*norig + nchtab+128-32 + sizeof(struct Font));
	nw = fontbase[num]->nwfont & BMASK;		/* 'space' needed for new font */
	if ( nw > norig )						/* new font is too big - abort */
		error(FATAL, "font %s too big for position %d", name, num);

	close(fin);
	widthtab[num] = (char *) fontbase[num] + sizeof(struct Font);
	codetab[num] = (char *) widthtab[num] + 2 * nw;
	fitab[num] = (char *) widthtab[num] + 3 * nw;
	t_fp(num, fontbase[num]->namefont, fontbase[num]->intname);

	fontbase[num]->nwfont = norig;			/* save size of position num */

	if ( debug[6] ) fontprint(num);			/* dump font position num */

}	/* End of loadfont */


/*****************************************************************************/


void
load_alt (
    int font						/* load '.add' tables for this font */
)


{


	int		nw;							/* number of width entries for font */
	int		fin;						/* font file descriptor */
	char	cmd[60];					/* room for font's .add file */



	/********************************************************************
	 *																	*
	 *		This routine is called to read in the '.add' file for the	*
	 *	font that is loaded in the position specified by the parameter	*
	 *	font. These '.add' files are created by the special version of	*
	 *	the makedev program, which reads and interprets several extra	*
	 *	fields in the APS font tables. Included in the '.add' files are	*
	 *	the alternate font table and the APS-5 character code table.	*
	 *																	*
	 ********************************************************************/



	sprintf(cmd, "%s/dev%s/%s.add", fontdir, devname, fontname[font].name);

	if ( (fin = open(cmd, O_RDONLY)) < 0 )	/* couldn't open the file */
		error(FATAL, "can't open file %s", cmd);

	nw = fontname[font].nwfont & BMASK;			/* number of width entries */

	READ(fin, alt_font, nw * sizeof(alt_font[0]));
	READ(fin, alt_code, nw);			/* read alternate code table */

	close(fin);
	alt_tables = fontname[font].number;	/* read in tables for this font */

	if ( debug[7] )						/* print all the font information */
		fontprint(font);

}	/* End of load_alt */


/*****************************************************************************/
