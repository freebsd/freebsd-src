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
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"

int	wordc;		/* how long the current error message is */
char	**wordv;	/* the actual error message */

int	nerrors;
int	language;

Errorclass	onelong();
Errorclass	cpp();
Errorclass	pccccom();	/* Portable C Compiler C Compiler */
Errorclass	richieccom();	/* Richie Compiler for 11 */
Errorclass	lint0();
Errorclass	lint1();
Errorclass	lint2();
Errorclass	lint3();
Errorclass	make();
Errorclass	f77();
Errorclass	pi();
Errorclass	ri();
Errorclass	troff();
Errorclass	mod2();
/*
 *	Eat all of the lines in the input file, attempting to categorize
 *	them by their various flavors
 */
static	char	inbuffer[BUFSIZ];

eaterrors(r_errorc, r_errorv)
	int	*r_errorc;
	Eptr	**r_errorv;
{
	extern	boolean	piflag;
	Errorclass	errorclass = C_SYNC;

    for (;;){
	if (fgets(inbuffer, BUFSIZ, errorfile) == NULL)
		break;
	wordvbuild(inbuffer, &wordc, &wordv);
	/*
	 *	for convience, convert wordv to be 1 based, instead
	 *	of 0 based.
	 */
	wordv -= 1;
	if ( wordc > 0 &&
	   ((( errorclass = onelong() ) != C_UNKNOWN)
	   || (( errorclass = cpp() ) != C_UNKNOWN)
	   || (( errorclass = pccccom() ) != C_UNKNOWN)
	   || (( errorclass = richieccom() ) != C_UNKNOWN)
	   || (( errorclass = lint0() ) != C_UNKNOWN)
	   || (( errorclass = lint1() ) != C_UNKNOWN)
	   || (( errorclass = lint2() ) != C_UNKNOWN)
	   || (( errorclass = lint3() ) != C_UNKNOWN)
	   || (( errorclass = make() ) != C_UNKNOWN)
	   || (( errorclass = f77() ) != C_UNKNOWN)
	   || ((errorclass = pi() ) != C_UNKNOWN)
	   || (( errorclass = ri() )!= C_UNKNOWN)
	   || (( errorclass = mod2() )!= C_UNKNOWN)
	   || (( errorclass = troff() )!= C_UNKNOWN))
	) ;
	else
		errorclass = catchall();
	if (wordc)
		erroradd(wordc, wordv+1, errorclass, C_UNKNOWN);	
    }
#ifdef FULLDEBUG
    printf("%d errorentrys\n", nerrors);
#endif
    arrayify(r_errorc, r_errorv, er_head);
}

/*
 *	create a new error entry, given a zero based array and count
 */
erroradd(errorlength, errorv, errorclass, errorsubclass)
	int		errorlength;
	char		**errorv;
	Errorclass	errorclass;
	Errorclass	errorsubclass;
{
	reg	Eptr	newerror;
	reg	char	*cp;

	if (errorclass == C_TRUE){
		/* check canonicalization of the second argument*/
		for(cp = errorv[1]; *cp && isdigit(*cp); cp++)
			continue;
		errorclass = (*cp == '\0') ? C_TRUE : C_NONSPEC;
#ifdef FULLDEBUG
		if (errorclass != C_TRUE)
			printf("The 2nd word, \"%s\" is not a number.\n",
				errorv[1]);
#endif
	}
	if (errorlength > 0){
		newerror = (Eptr)Calloc(1, sizeof(Edesc));
		newerror->error_language = language; /* language is global */
		newerror->error_text = errorv;
		newerror->error_lgtext = errorlength;
		if (errorclass == C_TRUE)
			newerror->error_line = atoi(errorv[1]);
		newerror->error_e_class = errorclass;
		newerror->error_s_class = errorsubclass;
		switch(newerror->error_e_class = discardit(newerror)){
			case C_SYNC:		nsyncerrors++; break;
			case C_DISCARD: 	ndiscard++; break;
			case C_NULLED:		nnulled++; break;
			case C_NONSPEC:		nnonspec++; break;
			case C_THISFILE: 	nthisfile++; break;
			case C_TRUE:		ntrue++; break;
			case C_UNKNOWN:		nunknown++; break;
			case C_IGNORE:		nignore++; break;
		}
		newerror->error_next = er_head;
		er_head = newerror;
		newerror->error_no = nerrors++;
	}	/* length > 0 */
}

Errorclass onelong()
{
	char	**nwordv;
	if ( (wordc == 1) && (language != INLD) ){
		/*
		 *	We have either:
		 *	a)	file name from cc
		 *	b)	Assembler telling world that it is complaining
		 *	c)	Noise from make ("Stop.")
		 *	c)	Random noise
		 */
		wordc = 0;
		if (strcmp(wordv[1], "Stop.") == 0){
			language = INMAKE; return(C_SYNC);
		}
		if (strcmp(wordv[1], "Assembler:") == 0){
			/* assembler always alerts us to what happened*/
			language = INAS; return(C_SYNC);
		} else 
		if (strcmp(wordv[1], "Undefined:") == 0){
			/* loader complains about unknown symbols*/
			language = INLD; return(C_SYNC);
		}
		if (lastchar(wordv[1]) == ':'){
			/* cc tells us what file we are in */
			currentfilename = wordv[1];
			(void)substitute(currentfilename, ':', '\0');
			language = INCC; return(C_SYNC);
		}
	} else
	if ( (wordc == 1) && (language == INLD) ){
		nwordv = (char **)Calloc(4, sizeof(char *));
		nwordv[0] = "ld:";
		nwordv[1] = wordv[1];
		nwordv[2] = "is";
		nwordv[3] = "undefined.";
		wordc = 4;
		wordv = nwordv - 1;
		return(C_NONSPEC);
	} else
	if (wordc == 1){
		return(C_SYNC);
	}
	return(C_UNKNOWN);
}	/* end of one long */

Errorclass	cpp()
{
	/* 
	 *	Now attempt a cpp error message match
	 *	Examples:
	 *		./morse.h: 23: undefined control
	 *		morsesend.c: 229: MAGNIBBL: argument mismatch
	 *		morsesend.c: 237: MAGNIBBL: argument mismatch
	 *		test1.c: 6: undefined control
	 */
	if (   (language != INLD)		/* loader errors have almost same fmt*/
	    && (lastchar(wordv[1]) == ':')
	    && (isdigit(firstchar(wordv[2])))
	    && (lastchar(wordv[2]) == ':') ){
		language = INCPP;
		clob_last(wordv[1], '\0');
		clob_last(wordv[2], '\0');
		return(C_TRUE);
	}
	return(C_UNKNOWN);
}	/*end of cpp*/

Errorclass pccccom()
{
	/*
	 *	Now attempt a ccom error message match:
	 *	Examples:
	 *	  "morsesend.c", line 237: operands of & have incompatible types
	 *	  "test.c", line 7: warning: old-fashioned initialization: use =
	 *	  "subdir.d/foo2.h", line 1: illegal initialization
	 */
	if (   (firstchar(wordv[1]) == '"')
	    && (lastchar(wordv[1]) == ',')
	    && (next_lastchar(wordv[1]) == '"')
	    && (strcmp(wordv[2],"line") == 0)
	    && (isdigit(firstchar(wordv[3])))
	    && (lastchar(wordv[3]) == ':') ){
		clob_last(wordv[1], '\0');	/* drop last , */
		clob_last(wordv[1], '\0');	/* drop last " */
		wordv[1]++;			/* drop first " */
		clob_last(wordv[3], '\0');	/* drop : on line number */
		wordv[2] = wordv[1];	/* overwrite "line" */
		wordv++;		/*compensate*/
		wordc--;
		currentfilename = wordv[1];
		language = INCC;
		return(C_TRUE);
	}
	return(C_UNKNOWN);
}	/* end of ccom */
/*
 *	Do the error message from the Richie C Compiler for the PDP11,
 *	which has this source:
 *
 *	if (filename[0])
 *		fprintf(stderr, "%s:", filename);
 *	fprintf(stderr, "%d: ", line);
 *
 */
Errorclass richieccom()
{
	reg	char	*cp;
	reg	char	**nwordv;
		char	*file;

	if (lastchar(wordv[1]) == ':'){
		cp = wordv[1] + strlen(wordv[1]) - 1;
		while (isdigit(*--cp))
			continue;
		if (*cp == ':'){
			clob_last(wordv[1], '\0');	/* last : */
			*cp = '\0';			/* first : */
			file = wordv[1];
			nwordv = wordvsplice(1, wordc, wordv+1);
			nwordv[0] = file;
			nwordv[1] = cp + 1;
			wordc += 1;
			wordv = nwordv - 1;
			language = INCC;
			currentfilename = wordv[1];
			return(C_TRUE);
		}
	}
	return(C_UNKNOWN);
}

Errorclass lint0()
{
	reg	char	**nwordv;
		char	*line, *file;
	/*
	 *	Attempt a match for the new lint style normal compiler
	 *	error messages, of the form
	 *	
	 *	printf("%s(%d): %s\n", filename, linenumber, message);
	 */
	if (wordc >= 2){
		if (   (lastchar(wordv[1]) == ':')
		    && (next_lastchar(wordv[1]) == ')')
		) {
			clob_last(wordv[1], '\0'); /* colon */
			if (persperdexplode(wordv[1], &line, &file)){
				nwordv = wordvsplice(1, wordc, wordv+1);
				nwordv[0] = file;	/* file name */
				nwordv[1] = line;	/* line number */
				wordc += 1;
				wordv = nwordv - 1;
				language = INLINT;
				return(C_TRUE);
			}
			wordv[1][strlen(wordv[1])] = ':';
		}
	}
	return (C_UNKNOWN);
}

Errorclass lint1()
{
	char	*line1, *line2;
	char	*file1, *file2;
	char	**nwordv1, **nwordv2;

	/*
	 *	Now, attempt a match for the various errors that lint
	 *	can complain about.
	 *
	 *	Look first for type 1 lint errors
	 */
	if (wordc > 1 && strcmp(wordv[wordc-1], "::") == 0){
	 /*
  	  * %.7s, arg. %d used inconsistently %s(%d) :: %s(%d)
  	  * %.7s value used inconsistently %s(%d) :: %s(%d)
  	  * %.7s multiply declared %s(%d) :: %s(%d)
  	  * %.7s value declared inconsistently %s(%d) :: %s(%d)
  	  * %.7s function value type must be declared before use %s(%d) :: %s(%d)
	  */
		language = INLINT;
		if (wordc > 2
		     && (persperdexplode(wordv[wordc], &line2, &file2))
		     && (persperdexplode(wordv[wordc-2], &line1, &file1)) ){
			nwordv1 = wordvsplice(2, wordc, wordv+1);
			nwordv2 = wordvsplice(2, wordc, wordv+1);
			nwordv1[0] = file1; nwordv1[1] = line1;
			erroradd(wordc+2, nwordv1, C_TRUE, C_DUPL); /* takes 0 based*/
			nwordv2[0] = file2; nwordv2[1] = line2;
			wordc = wordc + 2;
			wordv = nwordv2 - 1;	/* 1 based */
			return(C_TRUE);
		}
	}
	return(C_UNKNOWN);
} /* end of lint 1*/

Errorclass lint2()
{
	char	*file;
	char	*line;
	char	**nwordv;
	/*
	 *	Look for type 2 lint errors
	 *
	 *	%.7s used( %s(%d) ), but not defined
	 *	%.7s defined( %s(%d) ), but never used
	 *	%.7s declared( %s(%d) ), but never used or defined
	 *
	 *	bufp defined( "./metric.h"(10) ), but never used
	 */
	if (   (lastchar(wordv[2]) == '(' /* ')' */ )	
	    && (strcmp(wordv[4], "),") == 0) ){
		language = INLINT;
		if (persperdexplode(wordv[3], &line, &file)){
			nwordv = wordvsplice(2, wordc, wordv+1);
			nwordv[0] = file; nwordv[1] = line;
			wordc = wordc + 2;
			wordv = nwordv - 1;	/* 1 based */
			return(C_TRUE);
		}
	}
	return(C_UNKNOWN);
} /* end of lint 2*/

char	*Lint31[4] = {"returns", "value", "which", "is"};
char	*Lint32[6] = {"value", "is", "used,", "but", "none", "returned"};
Errorclass lint3()
{
	if (   (wordvcmp(wordv+2, 4, Lint31) == 0)
	    || (wordvcmp(wordv+2, 6, Lint32) == 0) ){
		language = INLINT;
		return(C_NONSPEC);
	}
	return(C_UNKNOWN);
}

/*
 *	Special word vectors for use by F77 recognition
 */
char	*F77_fatal[3] = {"Compiler", "error", "line"};
char	*F77_error[3] = {"Error", "on", "line"};
char	*F77_warning[3] = {"Warning", "on", "line"};
char    *F77_no_ass[3] = {"Error.","No","assembly."};
f77()
{
	char	**nwordv;
	/*
	 *	look for f77 errors:
	 *	Error messages from /usr/src/cmd/f77/error.c, with
	 *	these printf formats:
	 *
	 *		Compiler error line %d of %s: %s
	 *		Error on line %d of %s: %s
	 *		Warning on line %d of %s: %s
	 *		Error.  No assembly.
	 */
	if (wordc == 3 && wordvcmp(wordv+1, 3, F77_no_ass) == 0) {
		wordc = 0;
		return(C_SYNC);
	}
	if (wordc < 6)
		return(C_UNKNOWN);
	if (	(lastchar(wordv[6]) == ':')
	    &&(
	       (wordvcmp(wordv+1, 3, F77_fatal) == 0)
	    || (wordvcmp(wordv+1, 3, F77_error) == 0)
	    || (wordvcmp(wordv+1, 3, F77_warning) == 0) )
	){
		language = INF77;
		nwordv = wordvsplice(2, wordc, wordv+1);
		nwordv[0] = wordv[6];
		clob_last(nwordv[0],'\0');
		nwordv[1] = wordv[4];
		wordc += 2;
		wordv = nwordv - 1;	/* 1 based */
		return(C_TRUE);
	}
	return(C_UNKNOWN);
} /* end of f77 */

char	*Make_Croak[3] = {"***", "Error", "code"};
char	*Make_NotRemade[5] = {"not", "remade", "because", "of", "errors"};
Errorclass make()
{
	if (wordvcmp(wordv+1, 3, Make_Croak) == 0){
		language = INMAKE;
		return(C_SYNC);
	}
	if  (wordvcmp(wordv+2, 5, Make_NotRemade) == 0){
		language = INMAKE;
		return(C_SYNC);
	}
	return(C_UNKNOWN);
}
Errorclass ri()
{
/*
 *	Match an error message produced by ri; here is the
 *	procedure yanked from the distributed version of ri
 *	April 24, 1980.
 *	
 *	serror(str, x1, x2, x3)
 *		char str[];
 *		char *x1, *x2, *x3;
 *	{
 *		extern int yylineno;
 *		
 *		putc('"', stdout);
 *		fputs(srcfile, stdout);
 *		putc('"', stdout);
 *		fprintf(stdout, " %d: ", yylineno);
 *		fprintf(stdout, str, x1, x2, x3);
 *		fprintf(stdout, "\n");
 *		synerrs++;
 *	}
 */
	if (  (firstchar(wordv[1]) == '"')
	    &&(lastchar(wordv[1]) == '"')
	    &&(lastchar(wordv[2]) == ':')
	    &&(isdigit(firstchar(wordv[2]))) ){
		clob_last(wordv[1], '\0');	/* drop the last " */
		wordv[1]++;	/* skip over the first " */
		clob_last(wordv[2], '\0');
		language = INRI;
		return(C_TRUE);
	}
	return(C_UNKNOWN);
}

Errorclass catchall()
{
	/*
	 *	Catches random things.
	 */
	language = INUNKNOWN;
	return(C_NONSPEC);
} /* end of catch all*/

Errorclass troff()
{
	/*
	 *	troff source error message, from eqn, bib, tbl...
	 *	Just like pcc ccom, except uses `'
	 */
	if (   (firstchar(wordv[1]) == '`')
	    && (lastchar(wordv[1]) == ',')
	    && (next_lastchar(wordv[1]) == '\'')
	    && (strcmp(wordv[2],"line") == 0)
	    && (isdigit(firstchar(wordv[3])))
	    && (lastchar(wordv[3]) == ':') ){
		clob_last(wordv[1], '\0');	/* drop last , */
		clob_last(wordv[1], '\0');	/* drop last " */
		wordv[1]++;			/* drop first " */
		clob_last(wordv[3], '\0');	/* drop : on line number */
		wordv[2] = wordv[1];	/* overwrite "line" */
		wordv++;		/*compensate*/
		currentfilename = wordv[1];
		language = INTROFF;
		return(C_TRUE);
	}
	return(C_UNKNOWN);
}
Errorclass mod2()
{
	/*
	 *	for decwrl modula2 compiler (powell)
	 */
	if (   (  (strcmp(wordv[1], "!!!") == 0)	/* early version */
	        ||(strcmp(wordv[1], "File") == 0))	/* later version */
	    && (lastchar(wordv[2]) == ',')	/* file name */
	    && (strcmp(wordv[3], "line") == 0)
	    && (isdigit(firstchar(wordv[4])))	/* line number */
	    && (lastchar(wordv[4]) == ':')	/* line number */
	){
		clob_last(wordv[2], '\0');	/* drop last , on file name */
		clob_last(wordv[4], '\0');	/* drop last : on line number */
		wordv[3] = wordv[2];		/* file name on top of "line" */
		wordv += 2;
		wordc -= 2;
		currentfilename = wordv[1];
		language = INMOD2;
		return(C_TRUE);
	}
	return(C_UNKNOWN);
}
