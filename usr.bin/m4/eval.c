/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit.
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
static char sccsid[] = "@(#)eval.c	5.4 (Berkeley) 2/26/91";
#endif /* not lint */

/*
 * eval.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdef.h"
#include "extr.h"

extern ndptr lookup();

/*
 * eval - evaluate built-in macros.
 *	  argc - number of elements in argv.
 *	  argv - element vector :
 *			argv[0] = definition of a user
 *				  macro or nil if built-in.
 *			argv[1] = name of the macro or
 *				  built-in.
 *			argv[2] = parameters to user-defined
 *			   .	  macro or built-in.
 *			   .
 *
 * Note that the minimum value for argc is 3. A call in the form
 * of macro-or-builtin() will result in:
 *			argv[0] = nullstr
 *			argv[1] = macro-or-builtin
 *			argv[2] = nullstr
 *
 */

eval (argv, argc, td)
register char *argv[];
register int argc;
register int  td;
{
	register int c, n;
	static int sysval;

#ifdef DEBUG
	printf("argc = %d\n", argc);
	for (n = 0; n < argc; n++)
		printf("argv[%d] = %s\n", n, argv[n]);
#endif
	/*
	 * if argc == 3 and argv[2] is null,
	 * then we have macro-or-builtin() type call.
	 * We adjust argc to avoid further checking..
	 *
	 */
	if (argc == 3 && !*(argv[2]))
		argc--;

	switch (td & ~STATIC) {

	case DEFITYPE:
		if (argc > 2)
			dodefine(argv[2], (argc > 3) ? argv[3] : null);
		break;

	case PUSDTYPE:
		if (argc > 2)
			dopushdef(argv[2], (argc > 3) ? argv[3] : null);
		break;

	case DUMPTYPE:
		dodump(argv, argc);
		break;

	case EXPRTYPE:
		/*
		 * doexpr - evaluate arithmetic expression
		 *
		 */
		if (argc > 2)
			pbnum(expr(argv[2]));
		break;

	case IFELTYPE:
		if (argc > 4)
			doifelse(argv, argc);
		break;

	case IFDFTYPE:
		/*
		 * doifdef - select one of two alternatives based
		 *	     on the existence of another definition
		 */
		if (argc > 3) {
			if (lookup(argv[2]) != nil)
				pbstr(argv[3]);
			else if (argc > 4)
				pbstr(argv[4]);
		}
		break;

	case LENGTYPE:
		/*
		 * dolen - find the length of the argument
		 *
		 */
		if (argc > 2)
			pbnum((argc > 2) ? strlen(argv[2]) : 0);
		break;

	case INCRTYPE:
		/*
		 * doincr - increment the value of the argument
		 *
		 */
		if (argc > 2)
			pbnum(atoi(argv[2]) + 1);
		break;

	case DECRTYPE:
		/*
		 * dodecr - decrement the value of the argument
		 *
		 */
		if (argc > 2)
			pbnum(atoi(argv[2]) - 1);
		break;

	case SYSCTYPE:
		/*
		 * dosys - execute system command
		 *
		 */
		if (argc > 2)
			sysval = system(argv[2]);
		break;

	case SYSVTYPE:
		/*
		 * dosysval - return value of the last system call.
		 *
		 */
		pbnum(sysval);
		break;

	case INCLTYPE:
		if (argc > 2)
			if (!doincl(argv[2])) {
				fprintf(stderr,"m4: %s: ",argv[2]);
				error("cannot open for read.");
			}
		break;

	case SINCTYPE:
		if (argc > 2)
			(void) doincl(argv[2]);
		break;
#ifdef EXTENDED
	case PASTTYPE:
		if (argc > 2)
			if (!dopaste(argv[2])) {
				fprintf(stderr,"m4: %s: ",argv[2]);
				error("cannot open for read.");
			}
		break;

	case SPASTYPE:
		if (argc > 2)
			(void) dopaste(argv[2]);
		break;
#endif
	case CHNQTYPE:
		dochq(argv, argc);
		break;

	case CHNCTYPE:
		dochc(argv, argc);
		break;

	case SUBSTYPE:
		/*
		 * dosub - select substring
		 *
		 */
		if (argc > 3)
			dosub(argv,argc);
		break;

	case SHIFTYPE:
		/*
		 * doshift - push back all arguments except the
		 *	     first one (i.e. skip argv[2])
		 */
		if (argc > 3) {
			for (n = argc-1; n > 3; n--) {
				putback(rquote);
				pbstr(argv[n]);
				putback(lquote);
				putback(',');
			}
			putback(rquote);
			pbstr(argv[3]);
			putback(lquote);
		}
		break;

	case DIVRTYPE:
		if (argc > 2 && (n = atoi(argv[2])) != 0)
			dodiv(n);
		else {
			active = stdout;
			oindex = 0;
		}
		break;

	case UNDVTYPE:
		doundiv(argv, argc);
		break;

	case DIVNTYPE:
		/*
		 * dodivnum - return the number of current
		 * output diversion
		 *
		 */
		pbnum(oindex);
		break;

	case UNDFTYPE:
		/*
		 * doundefine - undefine a previously defined
		 *		macro(s) or m4 keyword(s).
		 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], ALL);
		break;

	case POPDTYPE:
		/*
		 * dopopdef - remove the topmost definitions of
		 *	      macro(s) or m4 keyword(s).
		 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], TOP);
		break;

	case MKTMTYPE:
		/*
		 * dotemp - create a temporary file
		 *
		 */
		if (argc > 2)
			pbstr(mktemp(argv[2]));
		break;

	case TRNLTYPE:
		/*
		 * dotranslit - replace all characters in the
		 *		source string that appears in
		 *		the "from" string with the corresponding
		 *		characters in the "to" string.
		 *
		 */
		if (argc > 3) {
			char temp[MAXTOK];
			if (argc > 4)
				map(temp, argv[2], argv[3], argv[4]);
			else
				map(temp, argv[2], argv[3], null);
			pbstr(temp);
		}
		else
		    if (argc > 2)
			pbstr(argv[2]);
		break;

	case INDXTYPE:
		/*
		 * doindex - find the index of the second argument
		 *	     string in the first argument string.
		 *	     -1 if not present.
		 */
		pbnum((argc > 3) ? indx(argv[2], argv[3]) : -1);
		break;

	case ERRPTYPE:
		/*
		 * doerrp - print the arguments to stderr file
		 *
		 */
		if (argc > 2) {
			for (n = 2; n < argc; n++)
				fprintf(stderr,"%s ", argv[n]);
			fprintf(stderr, "\n");
		}
		break;

	case DNLNTYPE:
		/*
		 * dodnl - eat-up-to and including newline
		 *
		 */
		while ((c = gpbc()) != '\n' && c != EOF)
			;
		break;

	case M4WRTYPE:
		/*
		 * dom4wrap - set up for wrap-up/wind-down activity
		 *
		 */
		m4wraps = (argc > 2) ? strdup(argv[2]) : null;
		break;

	case EXITTYPE:
		/*
		 * doexit - immediate exit from m4.
		 *
		 */
		exit((argc > 2) ? atoi(argv[2]) : 0);
		break;

	case DEFNTYPE:
		if (argc > 2)
			for (n = 2; n < argc; n++)
				dodefn(argv[n]);
		break;

	default:
		error("m4: major botch in eval.");
		break;
	}
}
