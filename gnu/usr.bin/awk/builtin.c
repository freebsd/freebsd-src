/*
 * builtin.c - Builtin functions and various utility procedures 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "awk.h"

#ifndef SRANDOM_PROTO
extern void srandom P((unsigned int seed));
#endif
#ifndef linux
extern char *initstate P((unsigned seed, char *state, int n));
extern char *setstate P((char *state));
extern long random P((void));
#endif

extern NODE **fields_arr;
extern int output_is_tty;

static NODE *sub_common P((NODE *tree, int global));
NODE *format_tree P((const char *, int, NODE *));

#ifdef _CRAY
/* Work around a problem in conversion of doubles to exact integers. */
#include <float.h>
#define Floor(n) floor((n) * (1.0 + DBL_EPSILON))
#define Ceil(n) ceil((n) * (1.0 + DBL_EPSILON))

/* Force the standard C compiler to use the library math functions. */
extern double exp(double);
double (*Exp)() = exp;
#define exp(x) (*Exp)(x)
extern double log(double);
double (*Log)() = log;
#define log(x) (*Log)(x)
#else
#define Floor(n) floor(n)
#define Ceil(n) ceil(n)
#endif

#define DEFAULT_G_PRECISION 6

#ifdef GFMT_WORKAROUND
/* semi-temporary hack, mostly to gracefully handle VMS */
static void sgfmt P((char *buf, const char *format, int alt,
		     int fwidth, int precision, double value));
#endif /* GFMT_WORKAROUND */

/*
 * On the alpha, LONG_MAX is too big for doing rand().
 * On the Cray (Y-MP, anyway), ints and longs are 64 bits, but
 * random() does things in terms of 32 bits. So we have to chop
 * LONG_MAX down.
 */
#if (defined(__alpha) && defined(__osf__)) || defined(_CRAY)
#define GAWK_RANDOM_MAX (LONG_MAX & 0x7fffffff)
#else
#define GAWK_RANDOM_MAX LONG_MAX
#endif

static void efwrite P((const void *ptr, size_t size, size_t count, FILE *fp,
		       const char *from, struct redirect *rp,int flush));

static void
efwrite(ptr, size, count, fp, from, rp, flush)
const void *ptr;
size_t size, count;
FILE *fp;
const char *from;
struct redirect *rp;
int flush;
{
	errno = 0;
	if (fwrite(ptr, size, count, fp) != count)
		goto wrerror;
	if (flush
	  && ((fp == stdout && output_is_tty)
	   || (rp && (rp->flag & RED_NOBUF)))) {
		fflush(fp);
		if (ferror(fp))
			goto wrerror;
	}
	return;

  wrerror:
	fatal("%s to \"%s\" failed (%s)", from,
		rp ? rp->value : "standard output",
		errno ? strerror(errno) : "reason unknown");
}

/* Builtin functions */
NODE *
do_exp(tree)
NODE *tree;
{
	NODE *tmp;
	double d, res;
#ifndef exp
	double exp P((double));
#endif

	tmp= tree_eval(tree->lnode);
	d = force_number(tmp);
	free_temp(tmp);
	errno = 0;
	res = exp(d);
	if (errno == ERANGE)
		warning("exp argument %g is out of range", d);
	return tmp_number((AWKNUM) res);
}

NODE *
do_index(tree)
NODE *tree;
{
	NODE *s1, *s2;
	register char *p1, *p2;
	register size_t l1, l2;
	long ret;


	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	force_string(s1);
	force_string(s2);
	p1 = s1->stptr;
	p2 = s2->stptr;
	l1 = s1->stlen;
	l2 = s2->stlen;
	ret = 0;
	if (IGNORECASE) {
		while (l1) {
			if (l2 > l1)
				break;
			if (casetable[(int)*p1] == casetable[(int)*p2]
			    && (l2 == 1 || strncasecmp(p1, p2, l2) == 0)) {
				ret = 1 + s1->stlen - l1;
				break;
			}
			l1--;
			p1++;
		}
	} else {
		while (l1) {
			if (l2 > l1)
				break;
			if (*p1 == *p2
			    && (l2 == 1 || STREQN(p1, p2, l2))) {
				ret = 1 + s1->stlen - l1;
				break;
			}
			l1--;
			p1++;
		}
	}
	free_temp(s1);
	free_temp(s2);
	return tmp_number((AWKNUM) ret);
}

double
double_to_int(d)
double d;
{
	double floor P((double));
	double ceil P((double));

	if (d >= 0)
		d = Floor(d);
	else
		d = Ceil(d);
	return d;
}

NODE *
do_int(tree)
NODE *tree;
{
	NODE *tmp;
	double d;

	tmp = tree_eval(tree->lnode);
	d = force_number(tmp);
	d = double_to_int(d);
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

NODE *
do_length(tree)
NODE *tree;
{
	NODE *tmp;
	size_t len;

	tmp = tree_eval(tree->lnode);
	len = force_string(tmp)->stlen;
	free_temp(tmp);
	return tmp_number((AWKNUM) len);
}

NODE *
do_log(tree)
NODE *tree;
{
	NODE *tmp;
#ifndef log
	double log P((double));
#endif
	double d, arg;

	tmp = tree_eval(tree->lnode);
	arg = (double) force_number(tmp);
	if (arg < 0.0)
		warning("log called with negative argument %g", arg);
	d = log(arg);
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

/*
 * format_tree() formats nodes of a tree, starting with a left node,
 * and accordingly to a fmt_string providing a format like in
 * printf family from C library.  Returns a string node which value
 * is a formatted string.  Called by  sprintf function.
 *
 * It is one of the uglier parts of gawk.  Thanks to Michal Jaegermann
 * for taming this beast and making it compatible with ANSI C.
 */

NODE *
format_tree(fmt_string, n0, carg)
const char *fmt_string;
int n0;
register NODE *carg;
{
/* copy 'l' bytes from 's' to 'obufout' checking for space in the process */
/* difference of pointers should be of ptrdiff_t type, but let us be kind */
#define bchunk(s,l) if(l) {\
    while((l)>ofre) {\
      long olen = obufout - obuf;\
      erealloc(obuf, char *, osiz*2, "format_tree");\
      ofre+=osiz;\
      osiz*=2;\
      obufout = obuf + olen;\
    }\
    memcpy(obufout,s,(size_t)(l));\
    obufout+=(l);\
    ofre-=(l);\
  }
/* copy one byte from 's' to 'obufout' checking for space in the process */
#define bchunk_one(s) {\
    if(ofre <= 0) {\
      long olen = obufout - obuf;\
      erealloc(obuf, char *, osiz*2, "format_tree");\
      ofre+=osiz;\
      osiz*=2;\
      obufout = obuf + olen;\
    }\
    *obufout++ = *s;\
    --ofre;\
  }

	/* Is there space for something L big in the buffer? */
#define chksize(l)  if((l)>ofre) {\
    long olen = obufout - obuf;\
    erealloc(obuf, char *, osiz*2, "format_tree");\
    obufout = obuf + olen;\
    ofre+=osiz;\
    osiz*=2;\
  }

	/*
	 * Get the next arg to be formatted.  If we've run out of args,
	 * return "" (Null string) 
	 */
#define parse_next_arg() {\
  if(!carg) { toofew = 1; break; }\
  else {\
	arg=tree_eval(carg->lnode);\
	carg=carg->rnode;\
  }\
 }

	NODE *r;
	int toofew = 0;
	char *obuf, *obufout;
	size_t osiz, ofre;
	char *chbuf;
	const char *s0, *s1;
	int cs1;
	NODE *arg;
	long fw, prec;
	int lj, alt, big, have_prec;
	long *cur;
	long val;
#ifdef sun386		/* Can't cast unsigned (int/long) from ptr->value */
	long tmp_uval;	/* on 386i 4.0.1 C compiler -- it just hangs */
#endif
	unsigned long uval;
	int sgn;
	int base = 0;
	char cpbuf[30];		/* if we have numbers bigger than 30 */
	char *cend = &cpbuf[30];/* chars, we lose, but seems unlikely */
	char *cp;
	char *fill;
	double tmpval;
	char signchar = 0;
	size_t len;
	static char sp[] = " ";
	static char zero_string[] = "0";
	static char lchbuf[] = "0123456789abcdef";
	static char Uchbuf[] = "0123456789ABCDEF";

	emalloc(obuf, char *, 120, "format_tree");
	obufout = obuf;
	osiz = 120;
	ofre = osiz - 1;

	s0 = s1 = fmt_string;
	while (n0-- > 0) {
		if (*s1 != '%') {
			s1++;
			continue;
		}
		bchunk(s0, s1 - s0);
		s0 = s1;
		cur = &fw;
		fw = 0;
		prec = 0;
		have_prec = 0;
		lj = alt = big = 0;
		fill = sp;
		cp = cend;
		chbuf = lchbuf;
		s1++;

retry:
		--n0;
		switch (cs1 = *s1++) {
		case (-1):	/* dummy case to allow for checking */
check_pos:
			if (cur != &fw)
				break;		/* reject as a valid format */
			goto retry;
		case '%':
			bchunk_one("%");
			s0 = s1;
			break;

		case '0':
			if (lj)
				goto retry;
			if (cur == &fw)
				fill = zero_string;	/* FALL through */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (cur == 0)
				/* goto lose; */
				break;
			if (prec >= 0)
				*cur = cs1 - '0';
			/* with a negative precision *cur is already set  */
			/* to -1, so it will remain negative, but we have */
			/* to "eat" precision digits in any case          */
			while (n0 > 0 && *s1 >= '0' && *s1 <= '9') {
				--n0;
				*cur = *cur * 10 + *s1++ - '0';
			}
			if (prec < 0) 	/* negative precision is discarded */
				have_prec = 0;
			if (cur == &prec)
				cur = 0;
			goto retry;
		case '*':
			if (cur == 0)
				/* goto lose; */
				break;
			parse_next_arg();
			*cur = force_number(arg);
			free_temp(arg);
			if (cur == &prec)
				cur = 0;
			goto retry;
		case ' ':		/* print ' ' or '-' */
					/* 'space' flag is ignored */
					/* if '+' already present  */
			if (signchar != 0) 
				goto check_pos;
			/* FALL THROUGH */
		case '+':		/* print '+' or '-' */
			signchar = cs1;
			goto check_pos;
		case '-':
			if (prec < 0)
				break;
			if (cur == &prec) {
				prec = -1;
				goto retry;
			}
			fill = sp;      /* if left justified then other */
			lj++; 		/* filling is ignored */
			goto check_pos;
		case '.':
			if (cur != &fw)
				break;
			cur = &prec;
			have_prec++;
			goto retry;
		case '#':
			alt++;
			goto check_pos;
		case 'l':
			if (big)
				break;
			big++;
			goto check_pos;
		case 'c':
			parse_next_arg();
			if (arg->flags & NUMBER) {
#ifdef sun386
				tmp_uval = arg->numbr; 
				uval= (unsigned long) tmp_uval;
#else
				uval = (unsigned long) arg->numbr;
#endif
				cpbuf[0] = uval;
				prec = 1;
				cp = cpbuf;
				goto pr_tail;
			}
			if (have_prec == 0)
				prec = 1;
			else if (prec > arg->stlen)
				prec = arg->stlen;
			cp = arg->stptr;
			goto pr_tail;
		case 's':
			parse_next_arg();
			arg = force_string(arg);
			if (have_prec == 0 || prec > arg->stlen)
				prec = arg->stlen;
			cp = arg->stptr;
			goto pr_tail;
		case 'd':
		case 'i':
			parse_next_arg();
			tmpval = force_number(arg);
			if (tmpval > LONG_MAX || tmpval < LONG_MIN) {
				/* out of range - emergency use of %g format */
				cs1 = 'g';
				goto format_float;
			}
			val = (long) tmpval;

			if (val < 0) {
				sgn = 1;
				if (val > LONG_MIN)
					uval = (unsigned long) -val;
				else
					uval = (unsigned long)(-(LONG_MIN + 1))
					       + (unsigned long)1;
			} else {
				sgn = 0;
				uval = (unsigned long) val;
			}
			do {
				*--cp = (char) ('0' + uval % 10);
				uval /= 10;
			} while (uval);
			if (sgn)
				*--cp = '-';
			else if (signchar)
				*--cp = signchar;
			if (have_prec != 0)	/* ignore '0' flag if */
				fill = sp; 	/* precision given    */
			if (prec > fw)
				fw = prec;
			prec = cend - cp;
			if (fw > prec && ! lj && fill != sp
			    && (*cp == '-' || signchar)) {
				bchunk_one(cp);
				cp++;
				prec--;
				fw--;
			}
			goto pr_tail;
		case 'X':
			chbuf = Uchbuf;	/* FALL THROUGH */
		case 'x':
			base += 6;	/* FALL THROUGH */
		case 'u':
			base += 2;	/* FALL THROUGH */
		case 'o':
			base += 8;
			parse_next_arg();
			tmpval = force_number(arg);
			if (tmpval > ULONG_MAX || tmpval < LONG_MIN) {
				/* out of range - emergency use of %g format */
				cs1 = 'g';
				goto format_float;
			}
			uval = (unsigned long)tmpval;
			if (have_prec != 0)	/* ignore '0' flag if */
				fill = sp; 	/* precision given    */
			do {
				*--cp = chbuf[uval % base];
				uval /= base;
			} while (uval);
			if (alt) {
				if (base == 16) {
					*--cp = cs1;
					*--cp = '0';
					if (fill != sp) {
						bchunk(cp, 2);
						cp += 2;
						fw -= 2;
					}
				} else if (base == 8)
					*--cp = '0';
			}
			base = 0;
			prec = cend - cp;
	pr_tail:
			if (! lj) {
				while (fw > prec) {
			    		bchunk_one(fill);
					fw--;
				}
			}
			bchunk(cp, (int) prec);
			while (fw > prec) {
				bchunk_one(fill);
				fw--;
			}
			s0 = s1;
			free_temp(arg);
			break;
		case 'g':
		case 'G':
		case 'e':
		case 'f':
		case 'E':
			parse_next_arg();
			tmpval = force_number(arg);
     format_float:
			free_temp(arg);
			if (have_prec == 0)
				prec = DEFAULT_G_PRECISION;
			chksize(fw + prec + 9);	/* 9==slop */

			cp = cpbuf;
			*cp++ = '%';
			if (lj)
				*cp++ = '-';
			if (signchar)
				*cp++ = signchar;
			if (alt)
				*cp++ = '#';
			if (fill != sp)
				*cp++ = '0';
			cp = strcpy(cp, "*.*") + 3;
			*cp++ = cs1;
			*cp   = '\0';
#ifndef GFMT_WORKAROUND
			(void) sprintf(obufout, cpbuf,
				       (int) fw, (int) prec, (double) tmpval);
#else	/* GFMT_WORKAROUND */
			if (cs1 == 'g' || cs1 == 'G')
				sgfmt(obufout, cpbuf, (int) alt,
				       (int) fw, (int) prec, (double) tmpval);
			else
				(void) sprintf(obufout, cpbuf,
				       (int) fw, (int) prec, (double) tmpval);
#endif	/* GFMT_WORKAROUND */
			len = strlen(obufout);
			ofre -= len;
			obufout += len;
			s0 = s1;
			break;
		default:
			break;
		}
		if (toofew)
			fatal("%s\n\t%s\n\t%*s%s",
			"not enough arguments to satisfy format string",
			fmt_string, s1 - fmt_string - 2, "",
			"^ ran out for this one"
			);
	}
	if (do_lint && carg != NULL)
		warning("too many arguments supplied for format string");
	bchunk(s0, s1 - s0);
	r = make_str_node(obuf, obufout - obuf, ALREADY_MALLOCED);
	r->flags |= TEMP;
	return r;
}

NODE *
do_sprintf(tree)
NODE *tree;
{
	NODE *r;
	NODE *sfmt = force_string(tree_eval(tree->lnode));

	r = format_tree(sfmt->stptr, sfmt->stlen, tree->rnode);
	free_temp(sfmt);
	return r;
}


void
do_printf(tree)
register NODE *tree;
{
	struct redirect *rp = NULL;
	register FILE *fp;

	if (tree->rnode) {
		int errflg;	/* not used, sigh */

		rp = redirect(tree->rnode, &errflg);
		if (rp) {
			fp = rp->fp;
			if (!fp)
				return;
		} else
			return;
	} else
		fp = stdout;
	tree = do_sprintf(tree->lnode);
	efwrite(tree->stptr, sizeof(char), tree->stlen, fp, "printf", rp , 1);
	free_temp(tree);
}

NODE *
do_sqrt(tree)
NODE *tree;
{
	NODE *tmp;
	double arg;
	extern double sqrt P((double));

	tmp = tree_eval(tree->lnode);
	arg = (double) force_number(tmp);
	free_temp(tmp);
	if (arg < 0.0)
		warning("sqrt called with negative argument %g", arg);
	return tmp_number((AWKNUM) sqrt(arg));
}

NODE *
do_substr(tree)
NODE *tree;
{
	NODE *t1, *t2, *t3;
	NODE *r;
	register int indx;
	size_t length;
	int is_long;

	t1 = tree_eval(tree->lnode);
	t2 = tree_eval(tree->rnode->lnode);
	if (tree->rnode->rnode == NULL)	/* third arg. missing */
		length = t1->stlen;
	else {
		t3 = tree_eval(tree->rnode->rnode->lnode);
		length = (size_t) force_number(t3);
		free_temp(t3);
	}
	indx = (int) force_number(t2) - 1;
	free_temp(t2);
	t1 = force_string(t1);
	if (indx < 0)
		indx = 0;
	if (indx >= t1->stlen || (long) length <= 0) {
		free_temp(t1);
		return Nnull_string;
	}
	if ((is_long = (indx + length > t1->stlen)) || LONG_MAX - indx < length) {
		length = t1->stlen - indx;
		if (do_lint && is_long)
			warning("substr: length %d at position %d exceeds length of first argument",
				length, indx+1);
	}
	r =  tmp_string(t1->stptr + indx, length);
	free_temp(t1);
	return r;
}

NODE *
do_strftime(tree)
NODE *tree;
{
	NODE *t1, *t2;
	struct tm *tm;
	time_t fclock;
	char buf[100];

	t1 = force_string(tree_eval(tree->lnode));

	if (tree->rnode == NULL)	/* second arg. missing, default */
		(void) time(&fclock);
	else {
		t2 = tree_eval(tree->rnode->lnode);
		fclock = (time_t) force_number(t2);
		free_temp(t2);
	}
	tm = localtime(&fclock);

	return tmp_string(buf, strftime(buf, 100, t1->stptr, tm));
}

NODE *
do_systime(tree)
NODE *tree;
{
	time_t lclock;

	(void) time(&lclock);
	return tmp_number((AWKNUM) lclock);
}

NODE *
do_system(tree)
NODE *tree;
{
	NODE *tmp;
	int ret = 0;
	char *cmd;
	char save;

	(void) flush_io ();     /* so output is synchronous with gawk's */
	tmp = tree_eval(tree->lnode);
	cmd = force_string(tmp)->stptr;

	if (cmd && *cmd) {
		/* insure arg to system is zero-terminated */

		/*
		 * From: David Trueman <emory!cs.dal.ca!david>
		 * To: arnold@cc.gatech.edu (Arnold Robbins)
		 * Date: 	Wed, 3 Nov 1993 12:49:41 -0400
		 * 
		 * It may not be necessary to save the character, but
		 * I'm not sure.  It would normally be the field
		 * separator.  If the parse has not yet gone beyond
		 * that, it could mess up (although I doubt it).  If
		 * FIELDWIDTHS is being used, it might be the first
		 * character of the next field.  Unless someone wants
		 * to check it out exhaustively, I suggest saving it
		 * for now...
		 */
		save = cmd[tmp->stlen];
		cmd[tmp->stlen] = '\0';

		ret = system(cmd);
		ret = (ret >> 8) & 0xff;

		cmd[tmp->stlen] = save;
	}
	free_temp(tmp);
	return tmp_number((AWKNUM) ret);
}

extern NODE **fmt_list;  /* declared in eval.c */

void 
do_print(tree)
register NODE *tree;
{
	register NODE *t1;
	struct redirect *rp = NULL;
	register FILE *fp;
	register char *s;

	if (tree->rnode) {
		int errflg;		/* not used, sigh */

		rp = redirect(tree->rnode, &errflg);
		if (rp) {
			fp = rp->fp;
			if (!fp)
				return;
		} else
			return;
	} else
		fp = stdout;
	tree = tree->lnode;
	while (tree) {
		t1 = tree_eval(tree->lnode);
		if (t1->flags & NUMBER) {
			if (OFMTidx == CONVFMTidx)
				(void) force_string(t1);
			else {
#ifndef GFMT_WORKAROUND
				char buf[100];

				(void) sprintf(buf, OFMT, t1->numbr);
				free_temp(t1);
				t1 = tmp_string(buf, strlen(buf));
#else /* GFMT_WORKAROUND */
				free_temp(t1);
				t1 = format_tree(OFMT,
						 fmt_list[OFMTidx]->stlen,
						 tree);
#endif /* GFMT_WORKAROUND */
			}
		}
		efwrite(t1->stptr, sizeof(char), t1->stlen, fp, "print", rp, 0);
		free_temp(t1);
		tree = tree->rnode;
		if (tree) {
			s = OFS;
			if (OFSlen)
				efwrite(s, sizeof(char), (size_t)OFSlen,
					fp, "print", rp, 0);
		}
	}
	s = ORS;
	if (ORSlen)
		efwrite(s, sizeof(char), (size_t)ORSlen, fp, "print", rp, 1);
}

NODE *
do_tolower(tree)
NODE *tree;
{
	NODE *t1, *t2;
	register char *cp, *cp2;

	t1 = tree_eval(tree->lnode);
	t1 = force_string(t1);
	t2 = tmp_string(t1->stptr, t1->stlen);
	for (cp = t2->stptr, cp2 = t2->stptr + t2->stlen; cp < cp2; cp++)
		if (isupper(*cp))
			*cp = tolower(*cp);
	free_temp(t1);
	return t2;
}

NODE *
do_toupper(tree)
NODE *tree;
{
	NODE *t1, *t2;
	register char *cp;

	t1 = tree_eval(tree->lnode);
	t1 = force_string(t1);
	t2 = tmp_string(t1->stptr, t1->stlen);
	for (cp = t2->stptr; cp < t2->stptr + t2->stlen; cp++)
		if (islower(*cp))
			*cp = toupper(*cp);
	free_temp(t1);
	return t2;
}

NODE *
do_atan2(tree)
NODE *tree;
{
	NODE *t1, *t2;
	extern double atan2 P((double, double));
	double d1, d2;

	t1 = tree_eval(tree->lnode);
	t2 = tree_eval(tree->rnode->lnode);
	d1 = force_number(t1);
	d2 = force_number(t2);
	free_temp(t1);
	free_temp(t2);
	return tmp_number((AWKNUM) atan2(d1, d2));
}

NODE *
do_sin(tree)
NODE *tree;
{
	NODE *tmp;
	extern double sin P((double));
	double d;

	tmp = tree_eval(tree->lnode);
	d = sin((double)force_number(tmp));
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

NODE *
do_cos(tree)
NODE *tree;
{
	NODE *tmp;
	extern double cos P((double));
	double d;

	tmp = tree_eval(tree->lnode);
	d = cos((double)force_number(tmp));
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

static int firstrand = 1;
static char state[512];

/* ARGSUSED */
NODE *
do_rand(tree)
NODE *tree;
{
	if (firstrand) {
		(void) initstate((unsigned) 1, state, sizeof state);
		srandom(1);
		firstrand = 0;
	}
	return tmp_number((AWKNUM) random() / GAWK_RANDOM_MAX);
}

NODE *
do_srand(tree)
NODE *tree;
{
	NODE *tmp;
	static long save_seed = 0;
	long ret = save_seed;	/* SVR4 awk srand returns previous seed */

	if (firstrand)
		(void) initstate((unsigned) 1, state, sizeof state);
	else
		(void) setstate(state);

	if (!tree)
		srandom((unsigned int) (save_seed = (long) time((time_t *) 0)));
	else {
		tmp = tree_eval(tree->lnode);
		srandom((unsigned int) (save_seed = (long) force_number(tmp)));
		free_temp(tmp);
	}
	firstrand = 0;
	return tmp_number((AWKNUM) ret);
}

NODE *
do_match(tree)
NODE *tree;
{
	NODE *t1;
	int rstart;
	AWKNUM rlength;
	Regexp *rp;

	t1 = force_string(tree_eval(tree->lnode));
	tree = tree->rnode->lnode;
	rp = re_update(tree);
	rstart = research(rp, t1->stptr, 0, t1->stlen, 1);
	if (rstart >= 0) {	/* match succeded */
		rstart++;	/* 1-based indexing */
		rlength = REEND(rp, t1->stptr) - RESTART(rp, t1->stptr);
	} else {		/* match failed */
		rstart = 0;
		rlength = -1.0;
	}
	free_temp(t1);
	unref(RSTART_node->var_value);
	RSTART_node->var_value = make_number((AWKNUM) rstart);
	unref(RLENGTH_node->var_value);
	RLENGTH_node->var_value = make_number(rlength);
	return tmp_number((AWKNUM) rstart);
}

static NODE *
sub_common(tree, global)
NODE *tree;
int global;
{
	register char *scan;
	register char *bp, *cp;
	char *buf;
	size_t buflen;
	register char *matchend;
	register size_t len;
	char *matchstart;
	char *text;
	size_t textlen;
	char *repl;
	char *replend;
	size_t repllen;
	int sofar;
	int ampersands;
	int matches = 0;
	Regexp *rp;
	NODE *s;		/* subst. pattern */
	NODE *t;		/* string to make sub. in; $0 if none given */
	NODE *tmp;
	NODE **lhs = &tree;	/* value not used -- just different from NULL */
	int priv = 0;
	Func_ptr after_assign = NULL;

	tmp = tree->lnode;
	rp = re_update(tmp);

	tree = tree->rnode;
	s = tree->lnode;

	tree = tree->rnode;
	tmp = tree->lnode;
	t = force_string(tree_eval(tmp));

	/* do the search early to avoid work on non-match */
	if (research(rp, t->stptr, 0, t->stlen, 1) == -1 ||
	    RESTART(rp, t->stptr) > t->stlen) {
		free_temp(t);
		return tmp_number((AWKNUM) 0.0);
	}

	if (tmp->type == Node_val)
		lhs = NULL;
	else
		lhs = get_lhs(tmp, &after_assign);
	t->flags |= STRING;
	/*
	 * create a private copy of the string
	 */
	if (t->stref > 1 || (t->flags & PERM)) {
		unsigned int saveflags;

		saveflags = t->flags;
		t->flags &= ~MALLOC;
		tmp = dupnode(t);
		t->flags = saveflags;
		t = tmp;
		priv = 1;
	}
	text = t->stptr;
	textlen = t->stlen;
	buflen = textlen + 2;

	s = force_string(tree_eval(s));
	repl = s->stptr;
	replend = repl + s->stlen;
	repllen = replend - repl;
	emalloc(buf, char *, buflen + 2, "do_sub");
	buf[buflen] = '\0';
	buf[buflen + 1] = '\0';
	ampersands = 0;
	for (scan = repl; scan < replend; scan++) {
		if (*scan == '&') {
			repllen--;
			ampersands++;
		} else if (*scan == '\\' && *(scan+1) == '&') {
			repllen--;
			scan++;
		}
	}

	bp = buf;
	for (;;) {
		matches++;
		matchstart = t->stptr + RESTART(rp, t->stptr);
		matchend = t->stptr + REEND(rp, t->stptr);

		/*
		 * create the result, copying in parts of the original
		 * string 
		 */
		len = matchstart - text + repllen
		      + ampersands * (matchend - matchstart);
		sofar = bp - buf;
		while ((long)(buflen - sofar - len - 1) < 0) {
			buflen *= 2;
			erealloc(buf, char *, buflen, "do_sub");
			bp = buf + sofar;
		}
		for (scan = text; scan < matchstart; scan++)
			*bp++ = *scan;
		for (scan = repl; scan < replend; scan++)
			if (*scan == '&')
				for (cp = matchstart; cp < matchend; cp++)
					*bp++ = *cp;
			else if (*scan == '\\' && *(scan+1) == '&') {
				scan++;
				*bp++ = *scan;
			} else
				*bp++ = *scan;

		/* catch the case of gsub(//, "blah", whatever), i.e. empty regexp */
		if (global && matchstart == matchend && matchend < text + textlen) {
			*bp++ = *matchend;
			matchend++;
		}
		textlen = text + textlen - matchend;
		text = matchend;
		if (!global || (long)textlen <= 0 ||
		    research(rp, t->stptr, text-t->stptr, textlen, 1) == -1)
			break;
	}
	sofar = bp - buf;
	if (buflen - sofar - textlen - 1) {
		buflen = sofar + textlen + 2;
		erealloc(buf, char *, buflen, "do_sub");
		bp = buf + sofar;
	}
	for (scan = matchend; scan < text + textlen; scan++)
		*bp++ = *scan;
	*bp = '\0';
	textlen = bp - buf;
	free(t->stptr);
	t->stptr = buf;
	t->stlen = textlen;

	free_temp(s);
	if (matches > 0 && lhs) {
		if (priv) {
			unref(*lhs);
			*lhs = t;
		}
		if (after_assign)
			(*after_assign)();
		t->flags &= ~(NUM|NUMBER);
	}
	return tmp_number((AWKNUM) matches);
}

NODE *
do_gsub(tree)
NODE *tree;
{
	return sub_common(tree, 1);
}

NODE *
do_sub(tree)
NODE *tree;
{
	return sub_common(tree, 0);
}

#ifdef GFMT_WORKAROUND
/*
 * printf's %g format [can't rely on gcvt()]
 *	caveat: don't use as argument to *printf()!
 * 'format' string HAS to be of "<flags>*.*g" kind, or we bomb!
 */
static void
sgfmt(buf, format, alt, fwidth, prec, g)
char *buf;	/* return buffer; assumed big enough to hold result */
const char *format;
int alt;	/* use alternate form flag */
int fwidth;	/* field width in a format */
int prec;	/* indicates desired significant digits, not decimal places */
double g;	/* value to format */
{
	char dform[40];
	register char *gpos;
	register char *d, *e, *p;
	int again = 0;

	strncpy(dform, format, sizeof dform - 1);
	dform[sizeof dform - 1] = '\0';
	gpos = strrchr(dform, '.');

	if (g == 0.0 && alt == 0) {	/* easy special case */
		*gpos++ = 'd';
		*gpos = '\0';
		(void) sprintf(buf, dform, fwidth, 0);
		return;
	}
	gpos += 2;  /* advance to location of 'g' in the format */

	if (prec <= 0)	      /* negative precision is ignored */
		prec = (prec < 0 ?  DEFAULT_G_PRECISION : 1);

	if (*gpos == 'G')
		again = 1;
	/* start with 'e' format (it'll provide nice exponent) */
	*gpos = 'e';
	prec -= 1;
	(void) sprintf(buf, dform, fwidth, prec, g);
	if ((e = strrchr(buf, 'e')) != NULL) {	/* find exponent  */
		int exp = atoi(e+1);		/* fetch exponent */
		if (exp >= -4 && exp <= prec) {	/* per K&R2, B1.2 */
			/* switch to 'f' format and re-do */
			*gpos = 'f';
			prec -= exp;		/* decimal precision */
			(void) sprintf(buf, dform, fwidth, prec, g);
			e = buf + strlen(buf);
			while (*--e == ' ')
				continue;
			e += 1;
		}
		else if (again != 0)
			*gpos = 'E';

		/* if 'alt' in force, then trailing zeros are not removed */
		if (alt == 0 && (d = strrchr(buf, '.')) != NULL) {
			/* throw away an excess of precision */
			for (p = e; p > d && *--p == '0'; )
				prec -= 1;
			if (d == p)
				prec -= 1;
			if (prec < 0)
				prec = 0;
			/* and do that once again */
			again = 1;
		}
		if (again != 0)
			(void) sprintf(buf, dform, fwidth, prec, g);
	}
}
#endif	/* GFMT_WORKAROUND */
