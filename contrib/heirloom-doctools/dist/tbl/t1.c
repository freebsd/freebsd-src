/*	Co/pyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved. The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */
     
/*
 * Copyright (c) 1983, 1984 1985, 1986, 1987, 1988, Sun Microsystems, Inc.
 * All Rights Reserved.
 */
  
/*	from OpenSolaris "t1.c	1.9	05/06/02 SMI"	 SVr4.0 1.1		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)t1.c	1.12 (gritter) 9/8/06
 */

 /* t1.c: main control and input switching */
#
# include "t..c"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <locale.h>
# ifdef gcos
/* required by GCOS because file is passed to "tbl" by troff preprocessor */
# define _f1 _f
extern FILE *_f[];
# endif

# ifndef gcos
# define MACROS "/usr/doctools/tmac/tmac.s"
# define MACROSS MACDIR "/s"
# define PYMACS "/usr/doctools/tmac/tmac.m"
# define PYMACSS MACDIR "/m"
# define MEMACSS MACDIR "/e"
# endif

# ifdef gcos
# define MACROS "cc/troff/smac"
# define PYMACS "cc/troff/mmac"
# endif

# define ever (;;)

int 
main(int argc, char *argv[])
{
# ifndef gcos
void badsig(int);
# endif
	progname = basename(argv[0]);
# ifndef gcos
signal(SIGPIPE, badsig);
# endif
# ifdef gcos
if(!intss()) tabout = fopen("qq", "w"); /* default media code is type 5 */
# endif
exit(tbl(argc,argv));
}


int 
tbl(int argc, char *argv[])
{
char *line = NULL;
size_t linesize = 0;
/* required by GCOS because "stdout" is set by troff preprocessor */
tabin=stdin; tabout=stdout;
setinp(argc,argv);
while (gets1(&line, &line, &linesize))
	{
	fprintf(tabout, "%s\n",line);
	if (cprefix("TS", line))
		tableput();
	}
fclose(tabin);
free(line);
return(0);
}
int sargc;
char **sargv;
void 
setinp(int argc, char **argv)
{
	sargc = argc;
	sargv = argv;
	sargc--; sargv++;
	if (sargc>0)
		swapin();
	if (pr1403 || utf8 || tlp) nflm = 1;
}
int 
swapin(void)
{
	char *optarg;
	while (sargc>0 && **sargv=='-') /* Mem fault if no test on sargc */
	{
		if (sargc<=0) return(0);
		if (match("-me", *sargv))
			{
			*sargv = MEMACSS;
			break;
			}
		if (match("-ms", *sargv))
			{
			*sargv = MACROSS;
			break;
			}
		if (match("-mm", *sargv))
			{
			*sargv = PYMACSS;
			break;
			}
		if ((*sargv)[1] == 'T') {
			optarg = *sargv + 2;
			if (!*optarg) {
				sargc--; sargv++;
				if (!sargc || **sargv == '-') {
					fprintf(stderr, "%s: Argument expected"
					    " after option -T\n", progname);
					exit(1);
				}
				optarg = *sargv;
			}
			if (*optarg == 'X' && !optarg[1]) {
				pr1403=1;
			} else if (!strcmp(optarg, "lp")) {
				tlp = 1;
				utf8 = 0;
				Graphics = 0;
			} else if (!strcmp(optarg, "locale")) {
				Graphics = 0;
				if (strstr(setlocale(LC_ALL, ""), "UTF-8")) {
					utf8 = 1;
					tlp = 0;
				} else {
					tlp = 1;
				}
			}
		}
		else if (match("-g", *sargv))
		{
			Graphics=1;
			utf8 = 0;
			tlp = 0;
		}
		else {
			(void) fprintf(stderr, "%s: Invalid option "
			    "(%s).\n", progname, *sargv);
			(void) fprintf(stderr, "Usage: %s [ -me ] "
			    "[ -mm ] [ -ms ] [ filename ... ]\n", progname);
			exit(1);
		}
		sargc--; sargv++;
	}
	if (sargc<=0) return(0);
# ifndef gcos
/* file closing is done by GCOS troff preprocessor */
	if (tabin!=stdin) fclose(tabin);
# endif
	tabin = fopen(ifile= *sargv, "r");
	iline=1;
# ifndef gcos
/* file names are all put into f. by the GCOS troff preprocessor */
	fprintf(tabout, ".ds f. %s\n",ifile);
# endif
	fprintf(tabout, ".lf 1 %s\n", ifile);
	if (tabin==NULL)
		return error("Can't open file");
	sargc--;
	sargv++;
	return(1);
}
# ifndef gcos
void 
badsig(int unused)
{
signal(SIGPIPE, SIG_IGN);
 exit(0);
}
# endif
