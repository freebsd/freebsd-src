/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Asa Romberger and Jerry Berkman.
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
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)fsplit.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 *	usage:		fsplit [-e efile] ... [file]
 *
 *	split single file containing source for several fortran programs
 *		and/or subprograms into files each containing one
 *		subprogram unit.
 *	each separate file will be named using the corresponding subroutine,
 *		function, block data or program name if one is found; otherwise
 *		the name will be of the form mainNNN.f or blkdtaNNN.f .
 *		If a file of that name exists, it is saved in a name of the
 *		form zzz000.f .
 *	If -e option is used, then only those subprograms named in the -e
 *		option are split off; e.g.:
 *			fsplit -esub1 -e sub2 prog.f
 *		isolates sub1 and sub2 in sub1.f and sub2.f.  The space 
 *		after -e is optional.
 *
 *	Modified Feb., 1983 by Jerry Berkman, Computing Services, U.C. Berkeley.
 *		- added comments
 *		- more function types: double complex, character*(*), etc.
 *		- fixed minor bugs
 *		- instead of all unnamed going into zNNN.f, put mains in
 *		  mainNNN.f, block datas in blkdtaNNN.f, dups in zzzNNN.f .
 */

#define BSZ 512
char buf[BSZ];
FILE *ifp;
char 	x[]="zzz000.f",
	mainp[]="main000.f",
	blkp[]="blkdta000.f";
char *look(), *skiplab(), *functs();

#define TRUE 1
#define FALSE 0
int	extr = FALSE,
	extrknt = -1,
	extrfnd[100];
char	extrbuf[1000],
	*extrnames[100];
struct stat sbuf;

#define trim(p)	while (*p == ' ' || *p == '\t') p++

main(argc, argv)
char **argv;
{
	register FILE *ofp;	/* output file */
	register rv;		/* 1 if got card in output file, 0 otherwise */
	register char *ptr;
	int nflag,		/* 1 if got name of subprog., 0 otherwise */
		retval,
		i;
	char name[20],
		*extrptr = extrbuf;

	/*  scan -e options */
	while ( argc > 1  && argv[1][0] == '-' && argv[1][1] == 'e') {
		extr = TRUE;
		ptr = argv[1] + 2;
		if(!*ptr) {
			argc--;
			argv++;
			if(argc <= 1) badparms();
			ptr = argv[1];
		}
		extrknt = extrknt + 1;
		extrnames[extrknt] = extrptr;
		extrfnd[extrknt] = FALSE;
		while(*ptr) *extrptr++ = *ptr++;
		*extrptr++ = 0;
		argc--;
		argv++;
	}

	if (argc > 2)
		badparms();
	else if (argc == 2) {
		if ((ifp = fopen(argv[1], "r")) == NULL) {
			fprintf(stderr, "fsplit: cannot open %s\n", argv[1]);
			exit(1);
		}
	}
	else
		ifp = stdin;
    for(;;) {
	/* look for a temp file that doesn't correspond to an existing file */
	get_name(x, 3);
	ofp = fopen(x, "w");
	nflag = 0;
	rv = 0;
	while (getline() > 0) {
		rv = 1;
		fprintf(ofp, "%s", buf);
		if (lend())		/* look for an 'end' statement */
			break;
		if (nflag == 0)		/* if no name yet, try and find one */
			nflag = lname(name);
	}
	fclose(ofp);
	if (rv == 0) {			/* no lines in file, forget the file */
		unlink(x);
		retval = 0;
		for ( i = 0; i <= extrknt; i++ )
			if(!extrfnd[i]) {
				retval = 1;
				fprintf( stderr, "fsplit: %s not found\n",
					extrnames[i]);
			}
		exit( retval );
	}
	if (nflag) {			/* rename the file */
		if(saveit(name)) {
			if (stat(name, &sbuf) < 0 ) {
				link(x, name);
				unlink(x);
				printf("%s\n", name);
				continue;
			} else if (strcmp(name, x) == 0) {
				printf("%s\n", x);
				continue;
			}
			printf("%s already exists, put in %s\n", name, x);
			continue;
		} else
			unlink(x);
			continue;
	}
	if(!extr)
		printf("%s\n", x);
	else
		unlink(x);
    }
}

badparms()
{
	fprintf(stderr, "fsplit: usage:  fsplit [-e efile] ... [file] \n");
	exit(1);
}

saveit(name)
char *name;
{
	int i;
	char	fname[50],
		*fptr = fname;

	if(!extr) return(1);
	while(*name) *fptr++ = *name++;
	*--fptr = 0;
	*--fptr = 0;
	for ( i=0 ; i<=extrknt; i++ ) 
		if( strcmp(fname, extrnames[i]) == 0 ) {
			extrfnd[i] = TRUE;
			return(1);
		}
	return(0);
}

get_name(name, letters)
char *name;
int letters;
{
	register char *ptr;

	while (stat(name, &sbuf) >= 0) {
		for (ptr = name + letters + 2; ptr >= name + letters; ptr--) {
			(*ptr)++;
			if (*ptr <= '9')
				break;
			*ptr = '0';
		}
		if(ptr < name + letters) {
			fprintf( stderr, "fsplit: ran out of file names\n");
			exit(1);
		}
	}
}

getline()
{
	register char *ptr;

	for (ptr = buf; ptr < &buf[BSZ]; ) {
		*ptr = getc(ifp);
		if (feof(ifp))
			return (-1);
		if (*ptr++ == '\n') {
			*ptr = 0;
			return (1);
		}
	}
	while (getc(ifp) != '\n' && feof(ifp) == 0) ;
	fprintf(stderr, "line truncated to %d characters\n", BSZ);
	return (1);
}

/* return 1 for 'end' alone on card (up to col. 72),  0 otherwise */
lend()
{
	register char *p;

	if ((p = skiplab(buf)) == 0)
		return (0);
	trim(p);
	if (*p != 'e' && *p != 'E') return(0);
	p++;
	trim(p);
	if (*p != 'n' && *p != 'N') return(0);
	p++;
	trim(p);
	if (*p != 'd' && *p != 'D') return(0);
	p++;
	trim(p);
	if (p - buf >= 72 || *p == '\n')
		return (1);
	return (0);
}

/*		check for keywords for subprograms	
		return 0 if comment card, 1 if found
		name and put in arg string. invent name for unnamed
		block datas and main programs.		*/
lname(s)
char *s;
{
#	define LINESIZE 80 
	register char *ptr, *p, *sptr;
	char	line[LINESIZE], *iptr = line;

	/* first check for comment cards */
	if(buf[0] == 'c' || buf[0] == 'C' || buf[0] == '*') return(0);
	ptr = buf;
	while (*ptr == ' ' || *ptr == '\t') ptr++;
	if(*ptr == '\n') return(0);


	ptr = skiplab(buf);
	if (ptr == 0)
		return (0);


	/*  copy to buffer and converting to lower case */
	p = ptr;
	while (*p && p <= &buf[71] ) {
	   *iptr = isupper(*p) ? tolower(*p) : *p;
	   iptr++;
	   p++;
	}
	*iptr = '\n';

	if ((ptr = look(line, "subroutine")) != 0 ||
	    (ptr = look(line, "function")) != 0 ||
	    (ptr = functs(line)) != 0) {
		if(scan_name(s, ptr)) return(1);
		strcpy( s, x);
	} else if((ptr = look(line, "program")) != 0) {
		if(scan_name(s, ptr)) return(1);
		get_name( mainp, 4);
		strcpy( s, mainp);
	} else if((ptr = look(line, "blockdata")) != 0) {
		if(scan_name(s, ptr)) return(1);
		get_name( blkp, 6);
		strcpy( s, blkp);
	} else if((ptr = functs(line)) != 0) {
		if(scan_name(s, ptr)) return(1);
		strcpy( s, x);
	} else {
		get_name( mainp, 4);
		strcpy( s, mainp);
	}
	return(1);
}

scan_name(s, ptr)
char *s, *ptr;
{
	char *sptr;

	/* scan off the name */
	trim(ptr);
	sptr = s;
	while (*ptr != '(' && *ptr != '\n') {
		if (*ptr != ' ' && *ptr != '\t')
			*sptr++ = *ptr;
		ptr++;
	}

	if (sptr == s) return(0);

	*sptr++ = '.';
	*sptr++ = 'f';
	*sptr++ = 0;
	return(1);
}

char *functs(p)
char *p;
{
        register char *ptr;

/*      look for typed functions such as: real*8 function,
                character*16 function, character*(*) function  */

        if((ptr = look(p,"character")) != 0 ||
           (ptr = look(p,"logical")) != 0 ||
           (ptr = look(p,"real")) != 0 ||
           (ptr = look(p,"integer")) != 0 ||
           (ptr = look(p,"doubleprecision")) != 0 ||
           (ptr = look(p,"complex")) != 0 ||
           (ptr = look(p,"doublecomplex")) != 0 ) {
                while ( *ptr == ' ' || *ptr == '\t' || *ptr == '*'
			|| (*ptr >= '0' && *ptr <= '9')
			|| *ptr == '(' || *ptr == ')') ptr++;
		ptr = look(ptr,"function");
		return(ptr);
	}
        else
                return(0);
}

/* 	if first 6 col. blank, return ptr to col. 7,
	if blanks and then tab, return ptr after tab,
	else return 0 (labelled statement, comment or continuation */
char *skiplab(p)
char *p;
{
	register char *ptr;

	for (ptr = p; ptr < &p[6]; ptr++) {
		if (*ptr == ' ')
			continue;
		if (*ptr == '\t') {
			ptr++;
			break;
		}
		return (0);
	}
	return (ptr);
}

/* 	return 0 if m doesn't match initial part of s;
	otherwise return ptr to next char after m in s */
char *look(s, m)
char *s, *m;
{
	register char *sp, *mp;

	sp = s; mp = m;
	while (*mp) {
		trim(sp);
		if (*sp++ != *mp++)
			return (0);
	}
	return (sp);
}
