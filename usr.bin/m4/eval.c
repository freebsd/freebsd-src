/*	$OpenBSD: eval.c,v 1.43 2002/02/16 21:27:48 millert Exp $	*/
/*	$NetBSD: eval.c,v 1.7 1996/11/10 21:21:29 pk Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
#if 0
static char sccsid[] = "@(#)eval.c	8.2 (Berkeley) 4/27/95";
#else
static char rcsid[] = "$OpenBSD: eval.c,v 1.43 2002/02/16 21:27:48 millert Exp $";
#endif
#endif /* not lint */

/*
 * eval.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"
#include "pathnames.h"

#define BUILTIN_MARKER	"__builtin_"

static void	dodefn(const char *);
static void	dopushdef(const char *, const char *);
static void	dodump(const char *[], int);
static void	dotrace(const char *[], int, int);
static void	doifelse(const char *[], int);
static int	doincl(const char *);
static int	dopaste(const char *);
static void	gnu_dochq(const char *[], int);
static void	dochq(const char *[], int);
static void	gnu_dochc(const char *[], int);
static void	dochc(const char *[], int);
static void	dodiv(int);
static void	doundiv(const char *[], int);
static void	dosub(const char *[], int);
static void	map(char *, const char *, const char *, const char *);
static const char *handledash(char *, char *, const char *);
static void	expand_builtin(const char *[], int, int);
static void	expand_macro(const char *[], int);
static void	dump_one_def(ndptr);

unsigned long	expansion_id;

/*
 * eval - eval all macros and builtins calls
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
 * A call in the form of macro-or-builtin() will result in:
 *			argv[0] = nullstr
 *			argv[1] = macro-or-builtin
 *			argv[2] = nullstr
 *
 * argc is 3 for macro-or-builtin() and 2 for macro-or-builtin
 */
void
eval(argv, argc, td)
	const char *argv[];
	int argc;
	int td;
{
	ssize_t mark = -1;

	expansion_id++;
	if (td & RECDEF) 
		errx(1, "%s at line %lu: expanding recursive definition for %s",
			CURRENT_NAME, CURRENT_LINE, argv[1]);
	if (traced_macros && is_traced(argv[1]))
		mark = trace(argv, argc, infile+ilevel);
	if (td == MACRTYPE)
		expand_macro(argv, argc);
	else
		expand_builtin(argv, argc, td);
    	if (mark != -1)
		finish_trace(mark);
}

/*
 * expand_builtin - evaluate built-in macros.
 */
void
expand_builtin(argv, argc, td)
	const char *argv[];
	int argc;
	int td;
{
	int c, n;
	int ac;
	static int sysval = 0;

#ifdef DEBUG
	printf("argc = %d\n", argc);
	for (n = 0; n < argc; n++)
		printf("argv[%d] = %s\n", n, argv[n]);
	fflush(stdout);
#endif

 /*
  * if argc == 3 and argv[2] is null, then we
  * have macro-or-builtin() type call. We adjust
  * argc to avoid further checking..
  */
  	ac = argc;

	if (argc == 3 && !*(argv[2]))
		argc--;

	switch (td & TYPEMASK) {

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

	case TRACEONTYPE:
		dotrace(argv, argc, 1);
		break;

	case TRACEOFFTYPE:
		dotrace(argv, argc, 0);
		break;

	case EXPRTYPE:
	/*
	 * doexpr - evaluate arithmetic
	 * expression
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
	 * doifdef - select one of two
	 * alternatives based on the existence of
	 * another definition
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
	 * dolen - find the length of the
	 * argument
	 */
		pbnum((argc > 2) ? strlen(argv[2]) : 0);
		break;

	case INCRTYPE:
	/*
	 * doincr - increment the value of the
	 * argument
	 */
		if (argc > 2)
			pbnum(atoi(argv[2]) + 1);
		break;

	case DECRTYPE:
	/*
	 * dodecr - decrement the value of the
	 * argument
	 */
		if (argc > 2)
			pbnum(atoi(argv[2]) - 1);
		break;

	case SYSCTYPE:
	/*
	 * dosys - execute system command
	 */
		if (argc > 2)
			sysval = system(argv[2]);
		break;

	case SYSVTYPE:
	/*
	 * dosysval - return value of the last
	 * system call.
	 * 
	 */
		pbnum(sysval);
		break;

	case ESYSCMDTYPE:
		if (argc > 2)
			doesyscmd(argv[2]);
	    	break;
	case INCLTYPE:
		if (argc > 2)
			if (!doincl(argv[2]))
				err(1, "%s at line %lu: include(%s)",
				    CURRENT_NAME, CURRENT_LINE, argv[2]);
		break;

	case SINCTYPE:
		if (argc > 2)
			(void) doincl(argv[2]);
		break;
#ifdef EXTENDED
	case PASTTYPE:
		if (argc > 2)
			if (!dopaste(argv[2]))
				err(1, "%s at line %lu: paste(%s)", 
				    CURRENT_NAME, CURRENT_LINE, argv[2]);
		break;

	case SPASTYPE:
		if (argc > 2)
			(void) dopaste(argv[2]);
		break;
#endif
	case CHNQTYPE:
		if (mimic_gnu)
			gnu_dochq(argv, ac);
		else
			dochq(argv, argc);
		break;

	case CHNCTYPE:
		if (mimic_gnu)
			gnu_dochc(argv, ac);
		else
			dochc(argv, argc);
		break;

	case SUBSTYPE:
	/*
	 * dosub - select substring
	 * 
	 */
		if (argc > 3)
			dosub(argv, argc);
		break;

	case SHIFTYPE:
	/*
	 * doshift - push back all arguments
	 * except the first one (i.e. skip
	 * argv[2])
	 */
		if (argc > 3) {
			for (n = argc - 1; n > 3; n--) {
				pbstr(rquote);
				pbstr(argv[n]);
				pbstr(lquote);
				putback(COMMA);
			}
			pbstr(rquote);
			pbstr(argv[3]);
			pbstr(lquote);
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
	 * dodivnum - return the number of
	 * current output diversion
	 */
		pbnum(oindex);
		break;

	case UNDFTYPE:
	/*
	 * doundefine - undefine a previously
	 * defined macro(s) or m4 keyword(s).
	 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], ALL);
		break;

	case POPDTYPE:
	/*
	 * dopopdef - remove the topmost
	 * definitions of macro(s) or m4
	 * keyword(s).
	 */
		if (argc > 2)
			for (n = 2; n < argc; n++)
				remhash(argv[n], TOP);
		break;

	case MKTMTYPE:
	/*
	 * dotemp - create a temporary file
	 */
		if (argc > 2) {
			int fd;
			char *temp;

			temp = xstrdup(argv[2]);
			
			fd = mkstemp(temp);
			if (fd == -1)
				err(1, 
	    "%s at line %lu: couldn't make temp file %s", 
	    CURRENT_NAME, CURRENT_LINE, argv[2]);
			close(fd);
			pbstr(temp);
			free(temp);
		}
		break;

	case TRNLTYPE:
	/*
	 * dotranslit - replace all characters in
	 * the source string that appears in the
	 * "from" string with the corresponding
	 * characters in the "to" string.
	 */
		if (argc > 3) {
			char *temp;

			temp = xalloc(strlen(argv[2])+1);
			if (argc > 4)
				map(temp, argv[2], argv[3], argv[4]);
			else
				map(temp, argv[2], argv[3], null);
			pbstr(temp);
			free(temp);
		} else if (argc > 2)
			pbstr(argv[2]);
		break;

	case INDXTYPE:
	/*
	 * doindex - find the index of the second
	 * argument string in the first argument
	 * string. -1 if not present.
	 */
		pbnum((argc > 3) ? indx(argv[2], argv[3]) : -1);
		break;

	case ERRPTYPE:
	/*
	 * doerrp - print the arguments to stderr
	 * file
	 */
		if (argc > 2) {
			for (n = 2; n < argc; n++)
				fprintf(stderr, "%s ", argv[n]);
			fprintf(stderr, "\n");
		}
		break;

	case DNLNTYPE:
	/*
	 * dodnl - eat-up-to and including
	 * newline
	 */
		while ((c = gpbc()) != '\n' && c != EOF)
			;
		break;

	case M4WRTYPE:
	/*
	 * dom4wrap - set up for
	 * wrap-up/wind-down activity
	 */
		m4wraps = (argc > 2) ? xstrdup(argv[2]) : null;
		break;

	case EXITTYPE:
	/*
	 * doexit - immediate exit from m4.
	 */
		killdiv();
		exit((argc > 2) ? atoi(argv[2]) : 0);
		break;

	case DEFNTYPE:
		if (argc > 2)
			for (n = 2; n < argc; n++)
				dodefn(argv[n]);
		break;

	case INDIRTYPE:	/* Indirect call */
		if (argc > 2)
			doindir(argv, argc);
		break;
	
	case BUILTINTYPE: /* Builtins only */
		if (argc > 2)
			dobuiltin(argv, argc);
		break;

	case PATSTYPE:
		if (argc > 2)
			dopatsubst(argv, argc);
		break;
	case REGEXPTYPE:
		if (argc > 2)
			doregexp(argv, argc);
		break;
	case LINETYPE:
		doprintlineno(infile+ilevel);
		break;
	case FILENAMETYPE:
		doprintfilename(infile+ilevel);
		break;
	case SELFTYPE:
		pbstr(rquote);
		pbstr(argv[1]);
		pbstr(lquote);
		break;
	default:
		errx(1, "%s at line %lu: eval: major botch.",
			CURRENT_NAME, CURRENT_LINE);
		break;
	}
}

/*
 * expand_macro - user-defined macro expansion
 */
void
expand_macro(argv, argc)
	const char *argv[];
	int argc;
{
	const char *t;
	const char *p;
	int n;
	int argno;

	t = argv[0];		       /* defn string as a whole */
	p = t;
	while (*p)
		p++;
	p--;			       /* last character of defn */
	while (p > t) {
		if (*(p - 1) != ARGFLAG)
			PUTBACK(*p);
		else {
			switch (*p) {

			case '#':
				pbnum(argc - 2);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if ((argno = *p - '0') < argc - 1)
					pbstr(argv[argno + 1]);
				break;
			case '*':
				if (argc > 2) {
					for (n = argc - 1; n > 2; n--) {
						pbstr(argv[n]);
						putback(COMMA);
					}
					pbstr(argv[2]);
			    	}
				break;
                        case '@':
				if (argc > 2) {
					for (n = argc - 1; n > 2; n--) {
						pbstr(rquote);
						pbstr(argv[n]);
						pbstr(lquote);
						putback(COMMA);
					}
					pbstr(rquote);
					pbstr(argv[2]);
					pbstr(lquote);
				}
                                break;
			default:
				PUTBACK(*p);
				PUTBACK('$');
				break;
			}
			p--;
		}
		p--;
	}
	if (p == t)		       /* do last character */
		PUTBACK(*p);
}

/*
 * dodefine - install definition in the table
 */
void
dodefine(name, defn)
	const char *name;
	const char *defn;
{
	ndptr p;
	int n;

	if (!*name)
		errx(1, "%s at line %lu: null definition.", CURRENT_NAME,
		    CURRENT_LINE);
	if ((p = lookup(name)) == nil)
		p = addent(name);
	else if (p->defn != null)
		free((char *) p->defn);
	if (strncmp(defn, BUILTIN_MARKER, sizeof(BUILTIN_MARKER)-1) == 0) {
		n = builtin_type(defn+sizeof(BUILTIN_MARKER)-1);
		if (n != -1) {
			p->type = n & TYPEMASK;
			if ((n & NOARGS) == 0)
				p->type |= NEEDARGS;
			p->defn = null;
			return;
		}
	}
	if (!*defn)
		p->defn = null;
	else
		p->defn = xstrdup(defn);
	p->type = MACRTYPE;
	if (STREQ(name, defn))
		p->type |= RECDEF;
}

/*
 * dodefn - push back a quoted definition of
 *      the given name.
 */
static void
dodefn(name)
	const char *name;
{
	ndptr p;
	char *real;

	if ((p = lookup(name)) != nil) {
		if (p->defn != null) {
			pbstr(rquote);
			pbstr(p->defn);
			pbstr(lquote);
		} else if ((real = builtin_realname(p->type)) != NULL) {
			pbstr(real);
			pbstr(BUILTIN_MARKER);
		}
	}
}

/*
 * dopushdef - install a definition in the hash table
 *      without removing a previous definition. Since
 *      each new entry is entered in *front* of the
 *      hash bucket, it hides a previous definition from
 *      lookup.
 */
static void
dopushdef(name, defn)
	const char *name;
	const char *defn;
{
	ndptr p;

	if (!*name)
		errx(1, "%s at line %lu: null definition", CURRENT_NAME,
		    CURRENT_LINE);
	p = addent(name);
	if (!*defn)
		p->defn = null;
	else
		p->defn = xstrdup(defn);
	p->type = MACRTYPE;
	if (STREQ(name, defn))
		p->type |= RECDEF;
}

/*
 * dump_one_def - dump the specified definition.
 */
static void
dump_one_def(p)
	ndptr p;
{
	char *real;

	if (mimic_gnu) {
		if ((p->type & TYPEMASK) == MACRTYPE)
			fprintf(traceout, "%s:\t%s\n", p->name, p->defn);
		else {
			real = builtin_realname(p->type);
			if (real == NULL)
				real = null;
			fprintf(traceout, "%s:\t<%s>\n", p->name, real);
	    	}
	} else
		fprintf(traceout, "`%s'\t`%s'\n", p->name, p->defn);
}

/*
 * dodumpdef - dump the specified definitions in the hash
 *      table to stderr. If nothing is specified, the entire
 *      hash table is dumped.
 */
static void
dodump(argv, argc)
	const char *argv[];
	int argc;
{
	int n;
	ndptr p;

	if (argc > 2) {
		for (n = 2; n < argc; n++)
			if ((p = lookup(argv[n])) != nil)
				dump_one_def(p);
	} else {
		for (n = 0; n < HASHSIZE; n++)
			for (p = hashtab[n]; p != nil; p = p->nxtptr)
				dump_one_def(p);
	}
}

/*
 * dotrace - mark some macros as traced/untraced depending upon on.
 */
static void
dotrace(argv, argc, on)
	const char *argv[];
	int argc;
	int on;
{
	int n;

	if (argc > 2) {
		for (n = 2; n < argc; n++)
			mark_traced(argv[n], on);
	} else
		mark_traced(NULL, on);
}

/*
 * doifelse - select one of two alternatives - loop.
 */
static void
doifelse(argv, argc)
	const char *argv[];
	int argc;
{
	cycle {
		if (STREQ(argv[2], argv[3]))
			pbstr(argv[4]);
		else if (argc == 6)
			pbstr(argv[5]);
		else if (argc > 6) {
			argv += 3;
			argc -= 3;
			continue;
		}
		break;
	}
}

/*
 * doinclude - include a given file.
 */
static int
doincl(ifile)
	const char *ifile;
{
	if (ilevel + 1 == MAXINP)
		errx(1, "%s at line %lu: too many include files.",
		    CURRENT_NAME, CURRENT_LINE);
	if (fopen_trypath(infile+ilevel+1, ifile) != NULL) {
		ilevel++;
		bbase[ilevel] = bufbase = bp;
		return (1);
	} else
		return (0);
}

#ifdef EXTENDED
/*
 * dopaste - include a given file without any
 *           macro processing.
 */
static int
dopaste(pfile)
	const char *pfile;
{
	FILE *pf;
	int c;

	if ((pf = fopen(pfile, "r")) != NULL) {
		while ((c = getc(pf)) != EOF)
			putc(c, active);
		(void) fclose(pf);
		return (1);
	} else
		return (0);
}
#endif

static void
gnu_dochq(argv, ac)
	const char *argv[];
	int ac;
{
	/* In gnu-m4 mode, the only way to restore quotes is to have no
	 * arguments at all. */
	if (ac == 2) {
		lquote[0] = LQUOTE, lquote[1] = EOS;
		rquote[0] = RQUOTE, rquote[1] = EOS;
	} else {
		strlcpy(lquote, argv[2], sizeof(lquote));
		if(ac > 3)
			strlcpy(rquote, argv[3], sizeof(rquote));
		else
			rquote[0] = EOS;
	}
}

/*
 * dochq - change quote characters
 */
static void
dochq(argv, argc)
	const char *argv[];
	int argc;
{
	if (argc > 2) {
		if (*argv[2])
			strlcpy(lquote, argv[2], sizeof(lquote));
		else {
			lquote[0] = LQUOTE;
			lquote[1] = EOS;
		}
		if (argc > 3) {
			if (*argv[3])
				strlcpy(rquote, argv[3], sizeof(rquote));
		} else
			strcpy(rquote, lquote);
	} else {
		lquote[0] = LQUOTE, lquote[1] = EOS;
		rquote[0] = RQUOTE, rquote[1] = EOS;
	}
}

static void
gnu_dochc(argv, ac)
	const char *argv[];
	int ac;
{
	/* In gnu-m4 mode, no arguments mean no comment
	 * arguments at all. */
	if (ac == 2) {
		scommt[0] = EOS;
		ecommt[0] = EOS;
	} else {
		if (*argv[2])
			strlcpy(scommt, argv[2], sizeof(scommt));
		else
			scommt[0] = SCOMMT, scommt[1] = EOS;
		if(ac > 3 && *argv[3])
			strlcpy(ecommt, argv[3], sizeof(ecommt));
		else
			ecommt[0] = ECOMMT, ecommt[1] = EOS;
	}
}
/*
 * dochc - change comment characters
 */
static void
dochc(argv, argc)
	const char *argv[];
	int argc;
{
	if (argc > 2) {
		if (*argv[2])
			strlcpy(scommt, argv[2], sizeof(scommt));
		if (argc > 3) {
			if (*argv[3])
				strlcpy(ecommt, argv[3], sizeof(ecommt));
		}
		else
			ecommt[0] = ECOMMT, ecommt[1] = EOS;
	}
	else {
		scommt[0] = SCOMMT, scommt[1] = EOS;
		ecommt[0] = ECOMMT, ecommt[1] = EOS;
	}
}

/*
 * dodivert - divert the output to a temporary file
 */
static void
dodiv(n)
	int n;
{
	int fd;

	oindex = n;
	if (n >= maxout) {
		if (mimic_gnu)
			resizedivs(n + 10);
		else
			n = 0;		/* bitbucket */
    	}

	if (n < 0)
		n = 0;		       /* bitbucket */
	if (outfile[n] == NULL) {
		char fname[] = _PATH_DIVNAME;

		if ((fd = mkstemp(fname)) < 0 || 
			(outfile[n] = fdopen(fd, "w+")) == NULL)
				err(1, "%s: cannot divert", fname);
		if (unlink(fname) == -1)
			err(1, "%s: cannot unlink", fname);
	}
	active = outfile[n];
}

/*
 * doundivert - undivert a specified output, or all
 *              other outputs, in numerical order.
 */
static void
doundiv(argv, argc)
	const char *argv[];
	int argc;
{
	int ind;
	int n;

	if (argc > 2) {
		for (ind = 2; ind < argc; ind++) {
			n = atoi(argv[ind]);
			if (n > 0 && n < maxout && outfile[n] != NULL)
				getdiv(n);

		}
	}
	else
		for (n = 1; n < maxout; n++)
			if (outfile[n] != NULL)
				getdiv(n);
}

/*
 * dosub - select substring
 */
static void
dosub(argv, argc)
	const char *argv[];
	int argc;
{
	const char *ap, *fc, *k;
	int nc;

	ap = argv[2];		       /* target string */
#ifdef EXPR
	fc = ap + expr(argv[3]);       /* first char */
#else
	fc = ap + atoi(argv[3]);       /* first char */
#endif
	nc = strlen(fc);
	if (argc >= 5)
#ifdef EXPR
		nc = min(nc, expr(argv[4]));
#else
		nc = min(nc, atoi(argv[4]));
#endif
	if (fc >= ap && fc < ap + strlen(ap))
		for (k = fc + nc - 1; k >= fc; k--)
			putback(*k);
}

/*
 * map:
 * map every character of s1 that is specified in from
 * into s3 and replace in s. (source s1 remains untouched)
 *
 * This is a standard implementation of map(s,from,to) function of ICON
 * language. Within mapvec, we replace every character of "from" with
 * the corresponding character in "to". If "to" is shorter than "from",
 * than the corresponding entries are null, which means that those
 * characters dissapear altogether. Furthermore, imagine
 * map(dest, "sourcestring", "srtin", "rn..*") type call. In this case,
 * `s' maps to `r', `r' maps to `n' and `n' maps to `*'. Thus, `s'
 * ultimately maps to `*'. In order to achieve this effect in an efficient
 * manner (i.e. without multiple passes over the destination string), we
 * loop over mapvec, starting with the initial source character. if the
 * character value (dch) in this location is different than the source
 * character (sch), sch becomes dch, once again to index into mapvec, until
 * the character value stabilizes (i.e. sch = dch, in other words
 * mapvec[n] == n). Even if the entry in the mapvec is null for an ordinary
 * character, it will stabilize, since mapvec[0] == 0 at all times. At the
 * end, we restore mapvec* back to normal where mapvec[n] == n for
 * 0 <= n <= 127. This strategy, along with the restoration of mapvec, is
 * about 5 times faster than any algorithm that makes multiple passes over
 * destination string.
 */
static void
map(dest, src, from, to)
	char *dest;
	const char *src;
	const char *from;
	const char *to;
{
	const char *tmp;
	unsigned char sch, dch;
	static char frombis[257];
	static char tobis[257];
	static unsigned char mapvec[256] = {
	    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	    19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
	    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
	    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
	    70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
	    87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102,
	    103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
	    116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
	    129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141,
	    142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154,
	    155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167,
	    168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180,
	    181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193,
	    194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
	    207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219,
	    220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
	    233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245,
	    246, 247, 248, 249, 250, 251, 252, 253, 254, 255
	};

	if (*src) {
		if (mimic_gnu) {
			/*
			 * expand character ranges on the fly
			 */
			from = handledash(frombis, frombis + 256, from);
			to = handledash(tobis, tobis + 256, to);
		}
		tmp = from;
	/*
	 * create a mapping between "from" and
	 * "to"
	 */
		while (*from)
			mapvec[(unsigned char)(*from++)] = (*to) ? 
				(unsigned char)(*to++) : 0;

		while (*src) {
			sch = (unsigned char)(*src++);
			dch = mapvec[sch];
			while (dch != sch) {
				sch = dch;
				dch = mapvec[sch];
			}
			if ((*dest = (char)dch))
				dest++;
		}
	/*
	 * restore all the changed characters
	 */
		while (*tmp) {
			mapvec[(unsigned char)(*tmp)] = (unsigned char)(*tmp);
			tmp++;
		}
	}
	*dest = '\0';
}


/*
 * handledash:
 *  use buffer to copy the src string, expanding character ranges
 * on the way.
 */
static const char *
handledash(buffer, end, src)
	char *buffer;
	char *end;
	const char *src;
{
	char *p;
	
	p = buffer;
	while(*src) {
		if (src[1] == '-' && src[2]) {
			unsigned char i;
			for (i = (unsigned char)src[0]; 
			    i <= (unsigned char)src[2]; i++) {
				*p++ = i;
				if (p == end) {
					*p = '\0';
					return buffer;
				}
			}
			src += 3;
		} else
			*p++ = *src++;
		if (p == end)
			break;
	}
	*p = '\0';
	return buffer;
}
			    
