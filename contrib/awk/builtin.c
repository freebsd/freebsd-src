/*
 * builtin.c - Builtin functions and various utility procedures 
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-2001 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $FreeBSD$
 */


#include "awk.h"
#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif
#undef HUGE
#undef CHARBITS
#undef INTBITS
#include <math.h>
#ifndef __FreeBSD__
#include "random.h"

/* can declare these, since we always use the random shipped with gawk */
extern char *initstate P((unsigned long seed, char *state, long n));
extern char *setstate P((char *state));
extern long random P((void));
extern void srandom P((unsigned long seed));
#endif

extern NODE **fields_arr;
extern int output_is_tty;

static NODE *sub_common P((NODE *tree, int how_many, int backdigs));

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
 * Since we supply the version of random(), we know what
 * value to use here.
 */
#define GAWK_RANDOM_MAX 0x7fffffffL

static void efwrite P((const void *ptr, size_t size, size_t count, FILE *fp,
		       const char *from, struct redirect *rp, int flush));

/* efwrite --- like fwrite, but with error checking */

static void
efwrite(const void *ptr,
	size_t size,
	size_t count,
	FILE *fp,
	const char *from,
	struct redirect *rp,
	int flush)
{
	errno = 0;
	if (fwrite(ptr, size, count, fp) != count)
		goto wrerror;
	if (flush
	  && ((fp == stdout && output_is_tty)
	   || (rp != NULL && (rp->flag & RED_NOBUF)))) {
		fflush(fp);
		if (ferror(fp))
			goto wrerror;
	}
	return;

wrerror:
	fatal(_("%s to \"%s\" failed (%s)"), from,
		rp ? rp->value : _("standard output"),
		errno ? strerror(errno) : _("reason unknown"));
}

/* do_exp --- exponential function */

NODE *
do_exp(NODE *tree)
{
	NODE *tmp;
	double d, res;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("exp: received non-numeric argument"));
	d = force_number(tmp);
	free_temp(tmp);
	errno = 0;
	res = exp(d);
	if (errno == ERANGE)
		warning(_("exp: argument %g is out of range"), d);
	return tmp_number((AWKNUM) res);
}

/* stdfile --- return fp for a standard file */

/*
 * This function allows `fflush("/dev/stdout")' to work.
 * The other files will be available via getredirect().
 * /dev/stdin is not included, since fflush is only for output.
 */

static FILE *
stdfile(char *name, size_t len)
{
	if (len == 11) {
		if (STREQN(name, "/dev/stderr", 11))
			return stderr;
		else if (STREQN(name, "/dev/stdout", 11))
			return stdout;
	}

	return NULL;
}

/* do_fflush --- flush output, either named file or pipe or everything */

NODE *
do_fflush(NODE *tree)
{
	struct redirect *rp;
	NODE *tmp;
	FILE *fp;
	int status = 0;
	char *file;

	/* fflush() --- flush stdout */
	if (tree == NULL) {
		status = fflush(stdout);
		return tmp_number((AWKNUM) status);
	}

	tmp = tree_eval(tree->lnode);
	tmp = force_string(tmp);
	file = tmp->stptr;

	/* fflush("") --- flush all */
	if (tmp->stlen == 0) {
		status = flush_io();
		free_temp(tmp);
		return tmp_number((AWKNUM) status);
	}

	rp = getredirect(tmp->stptr, tmp->stlen);
	status = -1;
	if (rp != NULL) {
		if ((rp->flag & (RED_WRITE|RED_APPEND)) == 0) {
			if (rp->flag & RED_PIPE)
				warning(_("fflush: cannot flush: pipe `%s' opened for reading, not writing"),
					file);
			else
				warning(_("fflush: cannot flush: file `%s' opened for reading, not writing"),
					file);
			free_temp(tmp);
			return tmp_number((AWKNUM) status);
		}
		fp = rp->fp;
		if (fp != NULL)
			status = fflush(fp);
	} else if ((fp = stdfile(tmp->stptr, tmp->stlen)) != NULL) {
		status = fflush(fp);
	} else {
		status = -1;
		warning(_("fflush: `%s' is not an open file, pipe or co-process"), file);
	}
	free_temp(tmp);
	return tmp_number((AWKNUM) status);
}

/* do_index --- find index of a string */

NODE *
do_index(NODE *tree)
{
	NODE *s1, *s2;
	register char *p1, *p2;
	register size_t l1, l2;
	long ret;


	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	if (do_lint) {
		if ((s1->flags & (STRING|STR)) == 0)
			lintwarn(_("index: received non-string first argument"));
		if ((s2->flags & (STRING|STR)) == 0)
			lintwarn(_("index: received non-string second argument"));
	}
	force_string(s1);
	force_string(s2);
	p1 = s1->stptr;
	p2 = s2->stptr;
	l1 = s1->stlen;
	l2 = s2->stlen;
	ret = 0;

	/* IGNORECASE will already be false if posix */
	if (IGNORECASE) {
		while (l1 > 0) {
			if (l2 > l1)
				break;
			if (casetable[(unsigned char)*p1] == casetable[(unsigned char)*p2]
			    && (l2 == 1 || strncasecmp(p1, p2, l2) == 0)) {
				ret = 1 + s1->stlen - l1;
				break;
			}
			l1--;
			p1++;
		}
	} else {
		while (l1 > 0) {
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

/* double_to_int --- convert double to int, used several places */

double
double_to_int(double d)
{
	if (d >= 0)
		d = Floor(d);
	else
		d = Ceil(d);
	return d;
}

/* do_int --- convert double to int for awk */

NODE *
do_int(NODE *tree)
{
	NODE *tmp;
	double d;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("int: received non-numeric argument"));
	d = force_number(tmp);
	d = double_to_int(d);
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

/* do_length --- length of a string or $0 */

NODE *
do_length(NODE *tree)
{
	NODE *tmp;
	size_t len;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (STRING|STR)) == 0)
		lintwarn(_("length: received non-string argument"));
	len = force_string(tmp)->stlen;
	free_temp(tmp);
	return tmp_number((AWKNUM) len);
}

/* do_log --- the log function */

NODE *
do_log(NODE *tree)
{
	NODE *tmp;
	double d, arg;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("log: received non-numeric argument"));
	arg = (double) force_number(tmp);
	if (arg < 0.0)
		warning(_("log: received negative argument %g"), arg);
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
format_tree(
	const char *fmt_string,
	int n0,
	register NODE *carg,
	int num_args)
{
/* copy 'l' bytes from 's' to 'obufout' checking for space in the process */
/* difference of pointers should be of ptrdiff_t type, but let us be kind */
#define bchunk(s, l) if (l) { \
	while ((l) > ofre) { \
		long olen = obufout - obuf; \
		erealloc(obuf, char *, osiz * 2, "format_tree"); \
		ofre += osiz; \
		osiz *= 2; \
		obufout = obuf + olen; \
	} \
	memcpy(obufout, s, (size_t) (l)); \
	obufout += (l); \
	ofre -= (l); \
}

/* copy one byte from 's' to 'obufout' checking for space in the process */
#define bchunk_one(s) { \
	if (ofre <= 0) { \
		long olen = obufout - obuf; \
		erealloc(obuf, char *, osiz * 2, "format_tree"); \
		ofre += osiz; \
		osiz *= 2; \
		obufout = obuf + olen; \
	} \
	*obufout++ = *s; \
	--ofre; \
}

/* Is there space for something L big in the buffer? */
#define chksize(l)  if ((l) > ofre) { \
	long olen = obufout - obuf; \
	erealloc(obuf, char *, osiz * 2, "format_tree"); \
	obufout = obuf + olen; \
	ofre += osiz; \
	osiz *= 2; \
}

	static NODE **the_args = 0;
	static size_t args_size = 0;
	size_t cur_arg = 0;

	auto NODE **save_args = 0;
	auto size_t save_args_size = 0;
	static int call_level = 0;

	NODE *r;
	int i;
	int toofew = FALSE;
	char *obuf, *obufout;
	size_t osiz, ofre;
	char *chbuf;
	const char *s0, *s1;
	int cs1;
	NODE *arg;
	long fw, prec, argnum;
	int used_dollar;
	int lj, alt, big, bigbig, small, have_prec, need_format;
	long *cur = NULL;
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
	char signchar = FALSE;
	size_t len;
	int zero_flag = FALSE;
	static char sp[] = " ";
	static char zero_string[] = "0";
	static char lchbuf[] = "0123456789abcdef";
	static char Uchbuf[] = "0123456789ABCDEF";

#define INITIAL_OUT_SIZE	512
	emalloc(obuf, char *, INITIAL_OUT_SIZE, "format_tree");
	obufout = obuf;
	osiz = INITIAL_OUT_SIZE;
	ofre = osiz - 1;

	/*
	 * Icky problem.  If the args make a nested call to printf/sprintf,
	 * we end up clobbering the static variable `the_args'.  Not good.
	 * We don't just malloc and free the_args each time, since most of the
	 * time there aren't nested calls.  But if this is a nested call,
	 * save the memory pointed to by the_args and allocate a fresh
	 * array.  Then free it on end.
	 */
	if (++call_level > 1) {	/* nested */
		save_args = the_args;
		save_args_size = args_size;

		args_size = 0;	/* force fresh allocation */
	}

	if (args_size == 0) {
		/* allocate array */
		emalloc(the_args, NODE **, (num_args+1) * sizeof(NODE *), "format_tree");
		args_size = num_args + 1;
	} else if (num_args + 1 > args_size) {
		/* grow it */
		erealloc(the_args, NODE **, (num_args+1) * sizeof(NODE *), "format_tree");
		args_size = num_args + 1;
	}


	/* fill it in */
	/*
	 * We ignore the_args[0] since format strings use
	 * 1-based numbers to indicate the arguments.  It's
	 * easiest to just convert to int and index, without
	 * having to remember to subtract 1.
	 */
	memset(the_args, '\0', num_args * sizeof(NODE *));
	for (i = 1; carg != NULL; i++, carg = carg->rnode) {
		NODE *tmp;

		/* Here lies the wumpus's other brother. R.I.P. */
		tmp = tree_eval(carg->lnode);
		the_args[i] = dupnode(tmp);
		free_temp(tmp);
	}
	assert(i == num_args);
	cur_arg = 1;

	/*
	 * Check first for use of `count$'.
	 * If plain argument retrieval was used earlier, choke.
	 *	Otherwise, return the requested argument.
	 * If not `count$' now, but it was used earlier, choke.
	 * If this format is more than total number of args, choke.
	 * Otherwise, return the current argument.
	 */
#define parse_next_arg() { \
	if (argnum > 0) { \
		if (cur_arg > 1) \
			fatal(_("must use `count$' on all formats or none")); \
		arg = the_args[argnum]; \
	} else if (used_dollar) { \
		fatal(_("must use `count$' on all formats or none")); \
		arg = 0; /* shutup the compiler */ \
	} else if (cur_arg >= num_args) { \
		arg = 0; /* shutup the compiler */ \
		toofew = TRUE; \
		break; \
	} else { \
		arg = the_args[cur_arg]; \
		cur_arg++; \
	} \
}

	need_format = FALSE;
	used_dollar = FALSE;

	s0 = s1 = fmt_string;
	while (n0-- > 0) {
		if (*s1 != '%') {
			s1++;
			continue;
		}
		need_format = TRUE;
		bchunk(s0, s1 - s0);
		s0 = s1;
		cur = &fw;
		fw = 0;
		prec = 0;
		argnum = 0;
		have_prec = FALSE;
		signchar = FALSE;
		zero_flag = FALSE;
		lj = alt = big = bigbig = small = FALSE;
		fill = sp;
		cp = cend;
		chbuf = lchbuf;
		s1++;

retry:
		if (n0-- <= 0)	/* ran out early! */
			break;

		switch (cs1 = *s1++) {
		case (-1):	/* dummy case to allow for checking */
check_pos:
			if (cur != &fw)
				break;		/* reject as a valid format */
			goto retry;
		case '%':
			need_format = FALSE;
			bchunk_one("%");
			s0 = s1;
			break;

		case '0':
			/*
			 * Only turn on zero_flag if we haven't seen
			 * the field width or precision yet.  Otherwise,
			 * screws up floating point formatting.
			 */
			if (cur == & fw)
				zero_flag = TRUE;
			if (lj)
				goto retry;
			/* FALL through */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (cur == NULL)
				break;
			if (prec >= 0)
				*cur = cs1 - '0';
			/*
			 * with a negative precision *cur is already set
			 * to -1, so it will remain negative, but we have
			 * to "eat" precision digits in any case
			 */
			while (n0 > 0 && *s1 >= '0' && *s1 <= '9') {
				--n0;
				*cur = *cur * 10 + *s1++ - '0';
			}
			if (prec < 0) 	/* negative precision is discarded */
				have_prec = FALSE;
			if (cur == &prec)
				cur = NULL;
			if (n0 == 0)	/* badly formatted control string */
				continue;
			goto retry;
		case '$':
			if (do_traditional)
				fatal(_("`$' is not permitted in awk formats"));
			if (cur == &fw) {
				argnum = fw;
				fw = 0;
				used_dollar = TRUE;
				if (argnum <= 0)
					fatal(_("arg count with `$' must be > 0"));
				if (argnum >= num_args)
					fatal(_("arg count %d greater than total number of supplied arguments"), argnum);
			} else
				fatal(_("`$' not permitted after period in format"));
			goto retry;
		case '*':
			if (cur == NULL)
				break;
			if (! do_traditional && ISDIGIT(*s1)) {
				int val = 0;

				for (; n0 > 0 && *s1 && ISDIGIT(*s1); s1++, n0--) {
					val *= 10;
					val += *s1 - '0';
				}
				if (*s1 != '$') {
					fatal(_("no `$' supplied for positional field width or precision"));
				} else {
					s1++;
					n0--;
				}

				arg = the_args[val];
			} else {
				parse_next_arg();
			}
			*cur = force_number(arg);
			if (*cur < 0 && cur == &fw) {
				*cur = -*cur;
				lj++;
			}
			if (cur == &prec) {
				if (*cur >= 0)
					have_prec = TRUE;
				else
					have_prec = FALSE;
				cur = NULL;
			}
			goto retry;
		case ' ':		/* print ' ' or '-' */
					/* 'space' flag is ignored */
					/* if '+' already present  */
			if (signchar != FALSE) 
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
			have_prec = TRUE;
			goto retry;
		case '#':
			alt = TRUE;
			goto check_pos;
		case 'l':
			if (big)
				break;
			else {
				static int warned = FALSE;
				
				if (do_lint && ! warned) {
					lintwarn(_("`l' is meaningless in awk formats; ignored"));
					warned = TRUE;
				}
				if (do_posix)
					fatal(_("`l' is not permitted in POSIX awk formats"));
			}
			big = TRUE;
			goto retry;
		case 'L':
			if (bigbig)
				break;
			else {
				static int warned = FALSE;
				
				if (do_lint && ! warned) {
					lintwarn(_("`L' is meaningless in awk formats; ignored"));
					warned = TRUE;
				}
				if (do_posix)
					fatal(_("`L' is not permitted in POSIX awk formats"));
			}
			bigbig = TRUE;
			goto retry;
		case 'h':
			if (small)
				break;
			else {
				static int warned = FALSE;
				
				if (do_lint && ! warned) {
					lintwarn(_("`h' is meaningless in awk formats; ignored"));
					warned = TRUE;
				}
				if (do_posix)
					fatal(_("`h' is not permitted in POSIX awk formats"));
			}
			small = TRUE;
			goto retry;
		case 'c':
			need_format = FALSE;
			if (zero_flag && ! lj)
				fill = zero_string;
			parse_next_arg();
			/* user input that looks numeric is numeric */
			if ((arg->flags & (MAYBE_NUM|NUMBER)) == MAYBE_NUM)
				(void) force_number(arg);
			if (arg->flags & NUMBER) {
#ifdef sun386
				tmp_uval = arg->numbr; 
				uval = (unsigned long) tmp_uval;
#else
				uval = (unsigned long) arg->numbr;
#endif
				cpbuf[0] = uval;
				prec = 1;
				cp = cpbuf;
				goto pr_tail;
			}
			if (have_prec == FALSE)
				prec = 1;
			else if (prec > arg->stlen)
				prec = arg->stlen;
			cp = arg->stptr;
			goto pr_tail;
		case 's':
			need_format = FALSE;
			if (zero_flag && ! lj)
				fill = zero_string;
			parse_next_arg();
			arg = force_string(arg);
			if (! have_prec || prec > arg->stlen)
				prec = arg->stlen;
			cp = arg->stptr;
			goto pr_tail;
		case 'd':
		case 'i':
			need_format = FALSE;
			parse_next_arg();
			tmpval = force_number(arg);

			/*
			 * ``The result of converting a zero value with a
			 * precision of zero is no characters.''
			 */
			if (have_prec && prec == 0 && tmpval == 0)
				goto pr_tail;

			if (tmpval < 0) {
				if (tmpval < LONG_MIN)
					goto out_of_range;
				sgn = TRUE;
				uval = - (unsigned long) (long) tmpval;
			} else {
				/* Use !, so that NaNs are out of range.
				   The cast avoids a SunOS 4.1.x cc bug.  */
				if (! (tmpval <= (unsigned long) ULONG_MAX))
					goto out_of_range;
				sgn = FALSE;
				uval = (unsigned long) tmpval;
			}
			do {
				*--cp = (char) ('0' + uval % 10);
				uval /= 10;
			} while (uval > 0);

			/* add more output digits to match the precision */
			if (have_prec) {
				while (cend - cp < prec)
					*--cp = '0';
			}

			if (sgn)
				*--cp = '-';
			else if (signchar)
				*--cp = signchar;
			/*
			 * When to fill with zeroes is of course not simple.
			 * First: No zero fill if left-justifying.
			 * Next: There seem to be two cases:
			 * 	A '0' without a precision, e.g. %06d
			 * 	A precision with no field width, e.g. %.10d
			 * Any other case, we don't want to fill with zeroes.
			 */
			if (! lj
			    && ((zero_flag && ! have_prec)
				 || (fw == 0 && have_prec)))
				fill = zero_string;
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
			need_format = FALSE;
			parse_next_arg();
			tmpval = force_number(arg);

			/*
			 * ``The result of converting a zero value with a
			 * precision of zero is no characters.''
			 *
			 * If I remember the ANSI C standard, though,
			 * it says that for octal conversions
			 * the precision is artificially increased
			 * to add an extra 0 if # is supplied.
			 * Indeed, in C,
			 * 	printf("%#.0o\n", 0);
			 * prints a single 0.
			 */
			if (! alt && have_prec && prec == 0 && tmpval == 0)
				goto pr_tail;

			if (tmpval < 0) {
				if (tmpval < LONG_MIN)
					goto out_of_range;
				uval = (unsigned long) (long) tmpval;
			} else {
				/* Use !, so that NaNs are out of range.
				   The cast avoids a SunOS 4.1.x cc bug.  */
				if (! (tmpval <= (unsigned long) ULONG_MAX))
					goto out_of_range;
				uval = (unsigned long) tmpval;
			}
			/*
			 * When to fill with zeroes is of course not simple.
			 * First: No zero fill if left-justifying.
			 * Next: There seem to be two cases:
			 * 	A '0' without a precision, e.g. %06d
			 * 	A precision with no field width, e.g. %.10d
			 * Any other case, we don't want to fill with zeroes.
			 */
			if (! lj
			    && ((zero_flag && ! have_prec)
				 || (fw == 0 && have_prec)))
				fill = zero_string;
			do {
				*--cp = chbuf[uval % base];
				uval /= base;
			} while (uval > 0);

			/* add more output digits to match the precision */
			if (have_prec) {
				while (cend - cp < prec)
					*--cp = '0';
			}

			if (alt && tmpval != 0) {
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
			if (prec > fw)
				fw = prec;
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
			break;

     out_of_range:
			/* out of range - emergency use of %g format */
			cs1 = 'g';
			goto format_float;

		case 'g':
		case 'G':
		case 'e':
		case 'f':
		case 'E':
			need_format = FALSE;
			parse_next_arg();
			tmpval = force_number(arg);
     format_float:
			if (! have_prec)
				prec = DEFAULT_G_PRECISION;
			chksize(fw + prec + 9);	/* 9 == slop */

			cp = cpbuf;
			*cp++ = '%';
			if (lj)
				*cp++ = '-';
			if (signchar)
				*cp++ = signchar;
			if (alt)
				*cp++ = '#';
			if (zero_flag)
				*cp++ = '0';
			strcpy(cp, "*.*");
			cp += 3;
			*cp++ = cs1;
			*cp = '\0';
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
			fatal("%s\n\t`%s'\n\t%*s%s",
			      _("not enough arguments to satisfy format string"),
			      fmt_string, s1 - fmt_string - 2, "",
			      _("^ ran out for this one"));
	}
	if (do_lint) {
		if (need_format)
			lintwarn(
			_("[s]printf: format specifier does not have control letter"));
		if (carg != NULL)
			lintwarn(
			_("too many arguments supplied for format string"));
	}
	bchunk(s0, s1 - s0);
	r = make_str_node(obuf, obufout - obuf, ALREADY_MALLOCED);
	r->flags |= TEMP;

	for (i = 1; i < num_args; i++) {
		unref(the_args[i]);
	}

	if (call_level-- > 1) {
		free(the_args);
		the_args = save_args;
		args_size = save_args_size;
	}

	return r;
}

/* do_sprintf --- perform sprintf */

NODE *
do_sprintf(NODE *tree)
{
	NODE *r;
	NODE *sfmt = force_string(tree_eval(tree->lnode));

	r = format_tree(sfmt->stptr, sfmt->stlen, tree->rnode, tree->printf_count);
	free_temp(sfmt);
	return r;
}

/* do_printf --- perform printf, including redirection */

void
do_printf(NODE *tree)
{
	struct redirect *rp = NULL;
	register FILE *fp;

	if (tree->lnode == NULL) {
		if (do_traditional) {
			if (do_lint)
				lintwarn(_("printf: no arguments"));
			return;	/* bwk accepts it silently */
		}
		fatal(_("printf: no arguments"));
	}

	if (tree->rnode != NULL) {
		int errflg;	/* not used, sigh */

		rp = redirect(tree->rnode, &errflg);
		if (rp != NULL) {
			fp = rp->fp;
			if (fp == NULL)
				return;
		} else
			return;
	} else
		fp = stdout;
	tree->lnode->printf_count = tree->printf_count;
	tree = do_sprintf(tree->lnode);
	efwrite(tree->stptr, sizeof(char), tree->stlen, fp, "printf", rp, TRUE);
	if (rp != NULL && (rp->flag & RED_TWOWAY) != 0)
		fflush(rp->fp);
	free_temp(tree);
}

/* do_sqrt --- do the sqrt function */

NODE *
do_sqrt(NODE *tree)
{
	NODE *tmp;
	double arg;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("sqrt: received non-numeric argument"));
	arg = (double) force_number(tmp);
	free_temp(tmp);
	if (arg < 0.0)
		warning(_("sqrt: called with negative argument %g"), arg);
	return tmp_number((AWKNUM) sqrt(arg));
}

/* do_substr --- do the substr function */

NODE *
do_substr(NODE *tree)
{
	NODE *t1, *t2, *t3;
	NODE *r;
	register size_t indx;
	size_t length;
	double d_index, d_length;

	t1 = force_string(tree_eval(tree->lnode));
	t2 = tree_eval(tree->rnode->lnode);
	d_index = force_number(t2);
	free_temp(t2);

	if (d_index < 1.0) {
		if (do_lint)
			lintwarn(_("substr: start index %g is invalid, using 1"),
				 d_index);
		d_index = 1;
	}
	if (do_lint && double_to_int(d_index) != d_index)
		lintwarn(_("substr: non-integer start index %g will be truncated"),
			 d_index);

	indx = d_index - 1;	/* awk indices are from 1, C's are from 0 */

	if (tree->rnode->rnode == NULL) {	/* third arg. missing */
		/* use remainder of string */
		length = t1->stlen - indx;
	} else {
		t3 = tree_eval(tree->rnode->rnode->lnode);
		d_length = force_number(t3);
		free_temp(t3);
		if (d_length <= 0.0) {
			if (do_lint)
				lintwarn(_("substr: length %g is <= 0"), d_length);
			free_temp(t1);
			return Nnull_string;
		}
		if (do_lint && double_to_int(d_length) != d_length)
			lintwarn(
		_("substr: non-integer length %g will be truncated"),
				d_length);
		length = d_length;
	}

	if (t1->stlen == 0) {
		if (do_lint)
			lintwarn(_("substr: source string is zero length"));
		free_temp(t1);
		return Nnull_string;
	}
	if ((indx + length) > t1->stlen) {
		if (do_lint)
			lintwarn(
	_("substr: length %d at start index %d exceeds length of first argument (%d)"),
			length, indx+1, t1->stlen);
		length = t1->stlen - indx;
	}
	if (indx >= t1->stlen) {
		if (do_lint)
			lintwarn(_("substr: start index %d is past end of string"),
				indx+1);
		free_temp(t1);
		return Nnull_string;
	}
	r = tmp_string(t1->stptr + indx, length);
	free_temp(t1);
	return r;
}

/* do_strftime --- format a time stamp */

NODE *
do_strftime(NODE *tree)
{
	NODE *t1, *t2, *ret;
	struct tm *tm;
	time_t fclock;
	char *bufp;
	size_t buflen, bufsize;
	char buf[BUFSIZ];
	/* FIXME: One day make %d be %e, after C 99 is common. */
	static char def_format[] = "%a %b %d %H:%M:%S %Z %Y";
	char *format;
	int formatlen;

	/* set defaults first */
	format = def_format;	/* traditional date format */
	formatlen = strlen(format);
	(void) time(&fclock);	/* current time of day */

	t1 = t2 = NULL;
	if (tree != NULL) {	/* have args */
		if (tree->lnode != NULL) {
			NODE *tmp = tree_eval(tree->lnode);
			if (do_lint && (tmp->flags & (STRING|STR)) == 0)
				lintwarn(_("strftime: recieved non-string first argument"));
			t1 = force_string(tmp);
			format = t1->stptr;
			formatlen = t1->stlen;
			if (formatlen == 0) {
				if (do_lint)
					lintwarn(_("strftime: received empty format string"));
				free_temp(t1);
				return tmp_string("", 0);
			}
		}
	
		if (tree->rnode != NULL) {
			t2 = tree_eval(tree->rnode->lnode);
			if (do_lint && (t2->flags & (NUM|NUMBER)) == 0)
				lintwarn(_("strftime: recieved non-numeric second argument"));
			fclock = (time_t) force_number(t2);
			free_temp(t2);
		}
	}

	tm = localtime(&fclock);

	bufp = buf;
	bufsize = sizeof(buf);
	for (;;) {
		*bufp = '\0';
		buflen = strftime(bufp, bufsize, format, tm);
		/*
		 * buflen can be zero EITHER because there's not enough
		 * room in the string, or because the control command
		 * goes to the empty string. Make a reasonable guess that
		 * if the buffer is 1024 times bigger than the length of the
		 * format string, it's not failing for lack of room.
		 * Thanks to Paul Eggert for pointing out this issue.
		 */
		if (buflen > 0 || bufsize >= 1024 * formatlen)
			break;
		bufsize *= 2;
		if (bufp == buf)
			emalloc(bufp, char *, bufsize, "do_strftime");
		else
			erealloc(bufp, char *, bufsize, "do_strftime");
	}
	ret = tmp_string(bufp, buflen);
	if (bufp != buf)
		free(bufp);
	if (t1)
		free_temp(t1);
	return ret;
}

/* do_systime --- get the time of day */

NODE *
do_systime(NODE *tree)
{
	time_t lclock;

	(void) time(&lclock);
	return tmp_number((AWKNUM) lclock);
}

/* do_mktime --- turn a time string into a timestamp */

NODE *
do_mktime(NODE *tree)
{
	NODE *t1;
	struct tm then;
	long year;
	int month, day, hour, minute, second, count;
	int dst = -1; /* default is unknown */
	time_t then_stamp;
	char save;

	t1 = tree_eval(tree->lnode);
	if (do_lint && (t1->flags & (STRING|STR)) == 0)
		lintwarn(_("mktime: received non-string argument"));
	t1 = force_string(t1);

	save = t1->stptr[t1->stlen];
	t1->stptr[t1->stlen] = '\0';

	count = sscanf(t1->stptr, "%ld %d %d %d %d %d %d",
		        & year, & month, & day,
			& hour, & minute, & second,
		        & dst);

	t1->stptr[t1->stlen] = save;
	free_temp(t1);

	if (count < 6
	    || month < month - 1
	    || year < year - 1900 || year - 1900 != (int) (year - 1900))
		return tmp_number((AWKNUM) -1);

	memset(& then, '\0', sizeof(then));
	then.tm_sec = second;
	then.tm_min = minute;
	then.tm_hour = hour;
	then.tm_mday = day;
	then.tm_mon = month - 1;
	then.tm_year = year - 1900;
	then.tm_isdst = dst;

	then_stamp = mktime(& then);
	return tmp_number((AWKNUM) then_stamp);
}

/* do_system --- run an external command */

NODE *
do_system(NODE *tree)
{
	NODE *tmp;
	int ret = 0;
	char *cmd;
	char save;

	(void) flush_io();     /* so output is synchronous with gawk's */
	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (STRING|STR)) == 0)
		lintwarn(_("system: recieved non-string argument"));
	cmd = force_string(tmp)->stptr;

	if (cmd && *cmd) {
		/* insure arg to system is zero-terminated */

		/*
		 * From: David Trueman <david@cs.dal.ca>
		 * To: arnold@cc.gatech.edu (Arnold Robbins)
		 * Date: Wed, 3 Nov 1993 12:49:41 -0400
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

		os_restore_mode(fileno(stdin));
		ret = system(cmd);
		ret = (ret >> 8) & 0xff;
		if ((BINMODE & 1) != 0)
			os_setbinmode(fileno(stdin), O_BINARY);

		cmd[tmp->stlen] = save;
	}
	free_temp(tmp);
	return tmp_number((AWKNUM) ret);
}

extern NODE **fmt_list;  /* declared in eval.c */

/* do_print --- print items, separated by OFS, terminated with ORS */

void 
do_print(register NODE *tree)
{
	register NODE **t;
	struct redirect *rp = NULL;
	register FILE *fp;
	int numnodes, i;
	NODE *save;
	NODE *tval;

	if (tree->rnode) {
		int errflg;		/* not used, sigh */

		rp = redirect(tree->rnode, &errflg);
		if (rp != NULL) {
			fp = rp->fp;
			if (fp == NULL)
				return;
		} else
			return;
	} else
		fp = stdout;

	/*
	 * General idea is to evaluate all the expressions first and
	 * then print them, otherwise you get suprising behavior.
	 * See test/prtoeval.awk for an example program.
	 */
	save = tree = tree->lnode;
	for (numnodes = 0; tree != NULL; tree = tree->rnode)
		numnodes++;
	emalloc(t, NODE **, numnodes * sizeof(NODE *), "do_print");

	tree = save;
	for (i = 0; tree != NULL; i++, tree = tree->rnode) {
		NODE *n;

		/* Here lies the wumpus. R.I.P. */
		n = tree_eval(tree->lnode);
		t[i] = dupnode(n);
		free_temp(n);

		if ((t[i]->flags & (NUMBER|STRING)) == NUMBER) {
			if (OFMTidx == CONVFMTidx)
				(void) force_string(t[i]);
			else {
				tval = tmp_number(t[i]->numbr);
				unref(t[i]);
				t[i] = format_val(OFMT, OFMTidx, tval);
			}
		}
	}

	for (i = 0; i < numnodes; i++) {
		efwrite(t[i]->stptr, sizeof(char), t[i]->stlen, fp, "print", rp, FALSE);
		unref(t[i]);

		if (i != numnodes - 1 && OFSlen > 0)
			efwrite(OFS, sizeof(char), (size_t) OFSlen,
				fp, "print", rp, FALSE);

	}
	if (ORSlen > 0)
		efwrite(ORS, sizeof(char), (size_t) ORSlen, fp, "print", rp, TRUE);

	if (rp != NULL && (rp->flag & RED_TWOWAY) != 0)
		fflush(rp->fp);

	free(t);
}

/* do_tolower --- lower case a string */

NODE *
do_tolower(NODE *tree)
{
	NODE *t1, *t2;
	register unsigned char *cp, *cp2;

	t1 = tree_eval(tree->lnode);
	if (do_lint && (t1->flags & (STRING|STR)) == 0)
		lintwarn(_("tolower: recieved non-string argument"));
	t1 = force_string(t1);
	t2 = tmp_string(t1->stptr, t1->stlen);
	for (cp = (unsigned char *)t2->stptr,
	     cp2 = (unsigned char *)(t2->stptr + t2->stlen); cp < cp2; cp++)
		if (ISUPPER(*cp))
			*cp = TOLOWER(*cp);
	free_temp(t1);
	return t2;
}

/* do_toupper --- upper case a string */

NODE *
do_toupper(NODE *tree)
{
	NODE *t1, *t2;
	register unsigned char *cp, *cp2;

	t1 = tree_eval(tree->lnode);
	if (do_lint && (t1->flags & (STRING|STR)) == 0)
		lintwarn(_("toupper: recieved non-string argument"));
	t1 = force_string(t1);
	t2 = tmp_string(t1->stptr, t1->stlen);
	for (cp = (unsigned char *)t2->stptr,
	     cp2 = (unsigned char *)(t2->stptr + t2->stlen); cp < cp2; cp++)
		if (ISLOWER(*cp))
			*cp = TOUPPER(*cp);
	free_temp(t1);
	return t2;
}

/* do_atan2 --- do the atan2 function */

NODE *
do_atan2(NODE *tree)
{
	NODE *t1, *t2;
	double d1, d2;

	t1 = tree_eval(tree->lnode);
	t2 = tree_eval(tree->rnode->lnode);
	if (do_lint) {
		if ((t1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("atan2: received non-numeric first argument"));
		if ((t2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("atan2: received non-numeric second argument"));
	}
	d1 = force_number(t1);
	d2 = force_number(t2);
	free_temp(t1);
	free_temp(t2);
	return tmp_number((AWKNUM) atan2(d1, d2));
}

/* do_sin --- do the sin function */

NODE *
do_sin(NODE *tree)
{
	NODE *tmp;
	double d;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("sin: received non-numeric argument"));
	d = sin((double) force_number(tmp));
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

/* do_cos --- do the cos function */

NODE *
do_cos(NODE *tree)
{
	NODE *tmp;
	double d;

	tmp = tree_eval(tree->lnode);
	if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
		lintwarn(_("cos: received non-numeric argument"));
	d = cos((double) force_number(tmp));
	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

/* do_rand --- do the rand function */

static int firstrand = TRUE;
static char state[512];

/* ARGSUSED */
NODE *
do_rand(NODE *tree)
{
	if (firstrand) {
		(void) initstate((unsigned) 1, state, sizeof state);
		srandom(1);
		firstrand = FALSE;
	}
	return tmp_number((AWKNUM) random() / GAWK_RANDOM_MAX);
}

/* do_srand --- seed the random number generator */

NODE *
do_srand(NODE *tree)
{
	NODE *tmp;
	static long save_seed = 1;
	long ret = save_seed;	/* SVR4 awk srand returns previous seed */

	if (firstrand) {
		(void) initstate((unsigned) 1, state, sizeof state);
		/* don't need to srandom(1), we're changing the seed below */
		firstrand = FALSE;
	} else
		(void) setstate(state);

	if (tree == NULL)
#ifdef __FreeBSD__
		srandom((unsigned int) (save_seed = (long) time((time_t *) 0) ^ getpid()));
#else
		srandom((unsigned int) (save_seed = (long) time((time_t *) 0)));
#endif
	else {
		tmp = tree_eval(tree->lnode);
		if (do_lint && (tmp->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("srand: received non-numeric argument"));
		srandom((unsigned int) (save_seed = (long) force_number(tmp)));
		free_temp(tmp);
	}
	return tmp_number((AWKNUM) ret);
}

/* do_match --- match a regexp, set RSTART and RLENGTH,
 * 	optional third arg is array filled with text of
 * 	subpatterns enclosed in parens.
 */

NODE *
do_match(NODE *tree)
{
	NODE *t1, *dest, *it;
	int rstart, len, ii;
	AWKNUM rlength;
	Regexp *rp;
	regoff_t s;
	char *start;

	t1 = force_string(tree_eval(tree->lnode));
	tree = tree->rnode;
	rp = re_update(tree->lnode);

	dest = NULL;
	if (tree->rnode != NULL) {  /* 3rd optional arg for the subpatterns */
		dest = tree->rnode->lnode;
		if (dest->type == Node_param_list)
			dest = stack_ptr[dest->param_cnt];
		if (dest->type == Node_array_ref)
			dest = dest->orig_array;
		if (dest->type != Node_var && dest->type != Node_var_array)
			fatal(_("match: third argument is not an array"));
		dest->type = Node_var_array;
		assoc_clear(dest);
	}
	
	rstart = research(rp, t1->stptr, 0, t1->stlen, TRUE);
	if (rstart >= 0) {	/* match succeded */
		rstart++;	/* 1-based indexing */
		rlength = REEND(rp, t1->stptr) - RESTART(rp, t1->stptr);
	
		/* Build the array only if the caller wants the optional subpatterns */
		if (dest != NULL) {
			for (ii = 0; (s = SUBPATSTART(rp, t1->stptr, ii)) != -1; ii++) {
				start = t1->stptr + s;
				len = SUBPATEND(rp, t1->stptr, ii) - s;

				it = make_string(start, len);
				it->flags |= MAYBE_NUM;
				*assoc_lookup(dest, tmp_number((AWKNUM) (ii)), FALSE) = it;
			}
		}
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

/* sub_common --- the common code (does the work) for sub, gsub, and gensub */

/*
 * Gsub can be tricksy; particularly when handling the case of null strings.
 * The following awk code was useful in debugging problems.  It is too bad
 * that it does not readily translate directly into the C code, below.
 * 
 * #! /usr/local/bin/mawk -f
 * 
 * BEGIN {
 * 	TRUE = 1; FALSE = 0
 * 	print "--->", mygsub("abc", "b+", "FOO")
 * 	print "--->", mygsub("abc", "x*", "X")
 * 	print "--->", mygsub("abc", "b*", "X")
 * 	print "--->", mygsub("abc", "c", "X")
 * 	print "--->", mygsub("abc", "c+", "X")
 * 	print "--->", mygsub("abc", "x*$", "X")
 * }
 * 
 * function mygsub(str, regex, replace,	origstr, newstr, eosflag, nonzeroflag)
 * {
 * 	origstr = str;
 * 	eosflag = nonzeroflag = FALSE
 * 	while (match(str, regex)) {
 * 		if (RLENGTH > 0) {	# easy case
 * 			nonzeroflag = TRUE
 * 			if (RSTART == 1) {	# match at front of string
 * 				newstr = newstr replace
 * 			} else {
 * 				newstr = newstr substr(str, 1, RSTART-1) replace
 * 			}
 * 			str = substr(str, RSTART+RLENGTH)
 * 		} else if (nonzeroflag) {
 * 			# last match was non-zero in length, and at the
 * 			# current character, we get a zero length match,
 * 			# which we don't really want, so skip over it
 * 			newstr = newstr substr(str, 1, 1)
 * 			str = substr(str, 2)
 * 			nonzeroflag = FALSE
 * 		} else {
 * 			# 0-length match
 * 			if (RSTART == 1) {
 * 				newstr = newstr replace substr(str, 1, 1)
 * 				str = substr(str, 2)
 * 			} else {
 * 				return newstr str replace
 * 			}
 * 		}
 * 		if (length(str) == 0)
 * 			if (eosflag)
 * 				break;
 * 			else
 * 				eosflag = TRUE
 * 	}
 * 	if (length(str) > 0)
 * 		newstr = newstr str	# rest of string
 * 
 * 	return newstr
 * }
 */

/*
 * NB: `howmany' conflicts with a SunOS 4.x macro in <sys/param.h>.
 */

static NODE *
sub_common(NODE *tree, int how_many, int backdigs)
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
	int priv = FALSE;
	Func_ptr after_assign = NULL;

	int global = (how_many == -1);
	long current;
	int lastmatchnonzero;

	tmp = tree->lnode;
	rp = re_update(tmp);

	tree = tree->rnode;
	s = tree->lnode;

	tree = tree->rnode;
	tmp = tree->lnode;
	t = force_string(tree_eval(tmp));

	/* do the search early to avoid work on non-match */
	if (research(rp, t->stptr, 0, t->stlen, TRUE) == -1 ||
	    RESTART(rp, t->stptr) > t->stlen) {
		free_temp(t);
		return tmp_number((AWKNUM) 0.0);
	}

	if (tmp->type == Node_val)
		lhs = NULL;
	else
		lhs = get_lhs(tmp, &after_assign, FALSE);
	t->flags |= STRING;
	/*
	 * create a private copy of the string
	 */
	if (t->stref > 1 || (t->flags & (PERM|FIELD)) != 0) {
		tmp = copynode(t);
		t = tmp;
		priv = TRUE;
	}
	text = t->stptr;
	textlen = t->stlen;
	buflen = textlen + 2;

	s = force_string(tree_eval(s));
	repl = s->stptr;
	replend = repl + s->stlen;
	repllen = replend - repl;
	emalloc(buf, char *, buflen + 2, "sub_common");
	buf[buflen] = '\0';
	buf[buflen + 1] = '\0';
	ampersands = 0;
	for (scan = repl; scan < replend; scan++) {
		if (*scan == '&') {
			repllen--;
			ampersands++;
		} else if (*scan == '\\') {
			if (backdigs) {	/* gensub, behave sanely */
				if (ISDIGIT(scan[1])) {
					ampersands++;
					scan++;
				} else {	/* \q for any q --> q */
					repllen--;
					scan++;
				}
			} else {	/* (proposed) posix '96 mode */
				if (strncmp(scan, "\\\\\\&", 4) == 0) {
					/* \\\& --> \& */
					repllen -= 2;
					scan += 3;
				} else if (strncmp(scan, "\\\\&", 3) == 0) {
					/* \\& --> \<string> */
					ampersands++;
					repllen--;
					scan += 2;
				} else if (scan[1] == '&') {
					/* \& --> & */
					repllen--;
					scan++;
				} /* else
					leave alone, it goes into the output */
			}
		}
	}

	lastmatchnonzero = FALSE;
	bp = buf;
	for (current = 1;; current++) {
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
		while (buflen < (sofar + len + 1)) {
			buflen *= 2;
			erealloc(buf, char *, buflen, "sub_common");
			bp = buf + sofar;
		}
		for (scan = text; scan < matchstart; scan++)
			*bp++ = *scan;
		if (global || current == how_many) {
			/*
			 * If the current match matched the null string,
			 * and the last match didn't and did a replacement,
			 * then skip this one.
			 */
			if (lastmatchnonzero && matchstart == matchend) {
				lastmatchnonzero = FALSE;
				matches--;
				goto empty;
			}
			/*
			 * If replacing all occurrences, or this is the
			 * match we want, copy in the replacement text,
			 * making substitutions as we go.
			 */
			for (scan = repl; scan < replend; scan++)
				if (*scan == '&')
					for (cp = matchstart; cp < matchend; cp++)
						*bp++ = *cp;
				else if (*scan == '\\') {
					if (backdigs) {	/* gensub, behave sanely */
						if (ISDIGIT(scan[1])) {
							int dig = scan[1] - '0';
							char *start, *end;
		
							start = t->stptr
							      + SUBPATSTART(rp, t->stptr, dig);
							end = t->stptr
							      + SUBPATEND(rp, t->stptr, dig);
		
							for (cp = start; cp < end; cp++)
								*bp++ = *cp;
							scan++;
						} else	/* \q for any q --> q */
							*bp++ = *++scan;
					} else {	/* posix '96 mode, bleah */
						if (strncmp(scan, "\\\\\\&", 4) == 0) {
							/* \\\& --> \& */
							*bp++ = '\\';
							*bp++ = '&';
							scan += 3;
						} else if (strncmp(scan, "\\\\&", 3) == 0) {
							/* \\& --> \<string> */
							*bp++ = '\\';
							for (cp = matchstart; cp < matchend; cp++)
								*bp++ = *cp;
							scan += 2;
						} else if (scan[1] == '&') {
							/* \& --> & */
							*bp++ = '&';
							scan++;
						} else
							*bp++ = *scan;
					}
				} else
					*bp++ = *scan;
			if (matchstart != matchend)
				lastmatchnonzero = TRUE;
		} else {
			/*
			 * don't want this match, skip over it by copying
			 * in current text.
			 */
			for (cp = matchstart; cp < matchend; cp++)
				*bp++ = *cp;
		}
	empty:
		/* catch the case of gsub(//, "blah", whatever), i.e. empty regexp */
		if (matchstart == matchend && matchend < text + textlen) {
			*bp++ = *matchend;
			matchend++;
		}
		textlen = text + textlen - matchend;
		text = matchend;

		if ((current >= how_many && !global)
		    || ((long) textlen <= 0 && matchstart == matchend)
		    || research(rp, t->stptr, text - t->stptr, textlen, TRUE) == -1)
			break;

	}
	sofar = bp - buf;
	if (buflen - sofar - textlen - 1) {
		buflen = sofar + textlen + 2;
		erealloc(buf, char *, buflen, "sub_common");
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
		if (after_assign != NULL)
			(*after_assign)();
		t->flags &= ~(NUM|NUMBER);
	}
	return tmp_number((AWKNUM) matches);
}

/* do_gsub --- global substitution */

NODE *
do_gsub(NODE *tree)
{
	return sub_common(tree, -1, FALSE);
}

/* do_sub --- single substitution */

NODE *
do_sub(NODE *tree)
{
	return sub_common(tree, 1, FALSE);
}

/* do_gensub --- fix up the tree for sub_common for the gensub function */

NODE *
do_gensub(NODE *tree)
{
	NODE n1, n2, n3, *t, *tmp, *target, *ret;
	long how_many = 1;	/* default is one substitution */
	double d;

	/*
	 * We have to pull out the value of the global flag, and
	 * build up a tree without the flag in it, turning it into the
	 * kind of tree that sub_common() expects.  It helps to draw
	 * a picture of this ...
	 */
	n1 = *tree;
	n2 = *(tree->rnode);
	n1.rnode = & n2;

	t = tree_eval(n2.rnode->lnode);	/* value of global flag */

	tmp = force_string(tree_eval(n2.rnode->rnode->lnode));	/* target */

	/*
	 * We make copy of the original target string, and pass that
	 * in to sub_common() as the target to make the substitution in.
	 * We will then return the result string as the return value of
	 * this function.
	 */
	target = make_string(tmp->stptr, tmp->stlen);
	free_temp(tmp);

	n3 = *(n2.rnode->rnode);
	n3.lnode = target;
	n2.rnode = & n3;

	if ((t->flags & (STR|STRING)) != 0) {
		if (t->stlen > 0 && (t->stptr[0] == 'g' || t->stptr[0] == 'G'))
			how_many = -1;
		else
			how_many = 1;
	} else {
		d = force_number(t);
		if (d > 0)
			how_many = d;
		else
			how_many = 1;
		if (d == 0)
			warning(_("gensub: 3rd argument of 0 treated as 1"));
	}

	free_temp(t);

	ret = sub_common(&n1, how_many, TRUE);
	free_temp(ret);

	/*
	 * Note that we don't care what sub_common() returns, since the
	 * easiest thing for the programmer is to return the string, even
	 * if no substitutions were done.
	 */
	target->flags |= TEMP;
	return target;
}

#ifdef GFMT_WORKAROUND
/*
 * printf's %g format [can't rely on gcvt()]
 *	caveat: don't use as argument to *printf()!
 * 'format' string HAS to be of "<flags>*.*g" kind, or we bomb!
 */
static void
sgfmt(char *buf,	/* return buffer; assumed big enough to hold result */
	const char *format,
	int alt,	/* use alternate form flag */
	int fwidth,	/* field width in a format */
	int prec,	/* indicates desired significant digits, not decimal places */
	double g)	/* value to format */
{
	char dform[40];
	register char *gpos;
	register char *d, *e, *p;
	int again = FALSE;

	strncpy(dform, format, sizeof dform - 1);
	dform[sizeof dform - 1] = '\0';
	gpos = strrchr(dform, '.');

	if (g == 0.0 && ! alt) {	/* easy special case */
		*gpos++ = 'd';
		*gpos = '\0';
		(void) sprintf(buf, dform, fwidth, 0);
		return;
	}

	/* advance to location of 'g' in the format */
	while (*gpos && *gpos != 'g' && *gpos != 'G')
		gpos++;

	if (prec <= 0)	      /* negative precision is ignored */
		prec = (prec < 0 ?  DEFAULT_G_PRECISION : 1);

	if (*gpos == 'G')
		again = TRUE;
	/* start with 'e' format (it'll provide nice exponent) */
	*gpos = 'e';
	prec--;
	(void) sprintf(buf, dform, fwidth, prec, g);
	if ((e = strrchr(buf, 'e')) != NULL) {	/* find exponent  */
		int expn = atoi(e+1);		/* fetch exponent */
		if (expn >= -4 && expn <= prec) {	/* per K&R2, B1.2 */
			/* switch to 'f' format and re-do */
			*gpos = 'f';
			prec -= expn;		/* decimal precision */
			(void) sprintf(buf, dform, fwidth, prec, g);
			e = buf + strlen(buf);
			while (*--e == ' ')
				continue;
			e++;
		}
		else if (again)
			*gpos = 'E';

		/* if 'alt' in force, then trailing zeros are not removed */
		if (! alt && (d = strrchr(buf, '.')) != NULL) {
			/* throw away an excess of precision */
			for (p = e; p > d && *--p == '0'; )
				prec--;
			if (d == p)
				prec--;
			if (prec < 0)
				prec = 0;
			/* and do that once again */
			again = TRUE;
		}
		if (again)
			(void) sprintf(buf, dform, fwidth, prec, g);
	}
}
#endif	/* GFMT_WORKAROUND */

#define BITS_PER_BYTE	8	/* if not true, you lose. too bad. */

/* do_lshift --- perform a << operation */

NODE *
do_lshift(NODE *tree)
{
	NODE *s1, *s2;
	unsigned long uval, ushift, res;
	AWKNUM val, shift;

	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	val = force_number(s1);
	shift = force_number(s2);

	if (do_lint) {
		if ((s1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("lshift: received non-numeric first argument"));
		if ((s2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("lshift: received non-numeric first argument"));
		if (val < 0 || shift < 0)
			lintwarn(_("lshift(%lf, %lf): negative values will give strange results"), val, shift);
		if (double_to_int(val) != val || double_to_int(shift) != shift)
			lintwarn(_("lshift(%lf, %lf): fractional values will be truncated"), val, shift);
		if (shift > (sizeof(unsigned long) * BITS_PER_BYTE))
			lintwarn(_("lshift(%lf, %lf): too large shift value will give strange results"), val, shift);
	}

	free_temp(s1);
	free_temp(s2);

	uval = (unsigned long) val;
	ushift = (unsigned long) shift;

	res = uval << ushift;
	return tmp_number((AWKNUM) res);
}

/* do_rshift --- perform a >> operation */

NODE *
do_rshift(NODE *tree)
{
	NODE *s1, *s2;
	unsigned long uval, ushift, res;
	AWKNUM val, shift;

	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	val = force_number(s1);
	shift = force_number(s2);

	if (do_lint) {
		if ((s1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("rshift: received non-numeric first argument"));
		if ((s2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("rshift: received non-numeric first argument"));
		if (val < 0 || shift < 0)
			lintwarn(_("rshift(%lf, %lf): negative values will give strange results"), val, shift);
		if (double_to_int(val) != val || double_to_int(shift) != shift)
			lintwarn(_("rshift(%lf, %lf): fractional values will be truncated"), val, shift);
		if (shift > (sizeof(unsigned long) * BITS_PER_BYTE))
			lintwarn(_("rshift(%lf, %lf): too large shift value will give strange results"), val, shift);
	}

	free_temp(s1);
	free_temp(s2);

	uval = (unsigned long) val;
	ushift = (unsigned long) shift;

	res = uval >> ushift;
	return tmp_number((AWKNUM) res);
}

/* do_and --- perform an & operation */

NODE *
do_and(NODE *tree)
{
	NODE *s1, *s2;
	unsigned long uleft, uright, res;
	AWKNUM left, right;

	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	left = force_number(s1);
	right = force_number(s2);

	if (do_lint) {
		if ((s1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("and: received non-numeric first argument"));
		if ((s2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("and: received non-numeric first argument"));
		if (left < 0 || right < 0)
			lintwarn(_("and(%lf, %lf): negative values will give strange results"), left, right);
		if (double_to_int(left) != left || double_to_int(right) != right)
			lintwarn(_("and(%lf, %lf): fractional values will be truncated"), left, right);
	}

	free_temp(s1);
	free_temp(s2);

	uleft = (unsigned long) left;
	uright = (unsigned long) right;

	res = uleft & uright;
	return tmp_number((AWKNUM) res);
}

/* do_or --- perform an | operation */

NODE *
do_or(NODE *tree)
{
	NODE *s1, *s2;
	unsigned long uleft, uright, res;
	AWKNUM left, right;

	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	left = force_number(s1);
	right = force_number(s2);

	if (do_lint) {
		if ((s1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("or: received non-numeric first argument"));
		if ((s2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("or: received non-numeric first argument"));
		if (left < 0 || right < 0)
			lintwarn(_("or(%lf, %lf): negative values will give strange results"), left, right);
		if (double_to_int(left) != left || double_to_int(right) != right)
			lintwarn(_("or(%lf, %lf): fractional values will be truncated"), left, right);
	}

	free_temp(s1);
	free_temp(s2);

	uleft = (unsigned long) left;
	uright = (unsigned long) right;

	res = uleft | uright;
	return tmp_number((AWKNUM) res);
}

/* do_xor --- perform an ^ operation */

NODE *
do_xor(NODE *tree)
{
	NODE *s1, *s2;
	unsigned long uleft, uright, res;
	AWKNUM left, right;

	s1 = tree_eval(tree->lnode);
	s2 = tree_eval(tree->rnode->lnode);
	left = force_number(s1);
	right = force_number(s2);

	if (do_lint) {
		if ((s1->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("xor: received non-numeric first argument"));
		if ((s2->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("xor: received non-numeric first argument"));
		if (left < 0 || right < 0)
			lintwarn(_("xor(%lf, %lf): negative values will give strange results"), left, right);
		if (double_to_int(left) != left || double_to_int(right) != right)
			lintwarn(_("xor(%lf, %lf): fractional values will be truncated"), left, right);
	}

	free_temp(s1);
	free_temp(s2);

	uleft = (unsigned long) left;
	uright = (unsigned long) right;

	res = uleft ^ uright;
	return tmp_number((AWKNUM) res);
}

/* do_compl --- perform a ~ operation */

NODE *
do_compl(NODE *tree)
{
	NODE *tmp;
	double d;
	unsigned long uval;

	tmp = tree_eval(tree->lnode);
	d = force_number(tmp);
	free_temp(tmp);

	if (do_lint) {
		if ((tmp->flags & (NUM|NUMBER)) == 0)
			lintwarn(_("compl: received non-numeric argument"));
		if (d < 0)
			lintwarn(_("compl(%lf): negative value will give strange results"), d);
		if (double_to_int(d) != d)
			lintwarn(_("compl(%lf): fractional value will be truncated"), d);
	}

	uval = (unsigned long) d;
	uval = ~ uval;
	return tmp_number((AWKNUM) uval);
}

/* do_strtonum --- the strtonum function */

NODE *
do_strtonum(NODE *tree)
{
	NODE *tmp;
	double d;

	tmp = tree_eval(tree->lnode);

	if ((tmp->flags & (NUM|NUMBER)) != 0)
		d = (double) force_number(tmp);
	else if (isnondecimal(tmp->stptr))
		d = nondec2awknum(tmp->stptr, tmp->stlen);
	else
		d = (double) force_number(tmp);

	free_temp(tmp);
	return tmp_number((AWKNUM) d);
}

/* nondec2awknum --- convert octal or hex value to double */

/*
 * Because of awk's concatenation rules and the way awk.y:yylex()
 * collects a number, this routine has to be willing to stop on the
 * first invalid character.
 */

AWKNUM
nondec2awknum(char *str, size_t len)
{
	AWKNUM retval = 0.0;
	char save;
	short val;
	char *start = str;

	if (*str == '0' && (str[1] == 'x' || str[1] == 'X')) {
		assert(len > 2);

		for (str += 2, len -= 2; len > 0; len--, str++) {
			switch (*str) {
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
				val = *str - '0';
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				val = *str - 'a' + 10;
				break;
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
				val = *str - 'A' + 10;
				break;
			default:
				goto done;
			}
			retval = (retval * 16) + val;
		}
	} else if (*str == '0') {
		if (strchr(str, '8') != NULL || strchr(str, '9') != NULL)
			goto decimal;
		for (; len > 0; len--) {
			if (! ISDIGIT(*str))
				goto done;
			else if (*str == '8' || *str == '9') {
				str = start;
				goto decimal;
			}
			retval = (retval * 8) + (*str - '0');
			str++;
		}
	} else {
decimal:
		save = str[len];
		retval = strtod(str, NULL);
		str[len] = save;
	}
done:
	return retval;
}

/* do_dcgettext --- handle i18n translations */

/*
 * awk usage is
 *
 * 	str = dcgettext(string [, domain [, category]])
 *
 * Default domain is TEXTDOMAIN, default category is LC_MESSAGES.
 */

NODE *
do_dcgettext(NODE *tree)
{
	NODE *tmp, *t1, *t2;
	char *string;
	char *the_result;
#if ENABLE_NLS && HAVE_LC_MESSAGES && HAVE_DCGETTEXT
	int lc_cat = -1;
	char *category, *domain;
	static struct category_table {
		int val;
		char *name;
	} cat_tab[] = {
#ifdef LC_ALL
		{ LC_ALL,	"LC_ALL" },
#endif /* LC_ALL */
#ifdef LC_COLLATE
		{ LC_COLLATE,	"LC_COLLATE" },
#endif /* LC_COLLATE */
#ifdef LC_CTYPE
		{ LC_CTYPE,	"LC_CTYPE" },
#endif /* LC_CTYPE */
#ifdef LC_MESSAGES
		{ LC_MESSAGES,	"LC_MESSAGES" },
#endif /* LC_MESSAGES */
#ifdef LC_MONETARY
		{ LC_MONETARY,	"LC_MONETARY" },
#endif /* LC_MONETARY */
#ifdef LC_NUMERIC
		{ LC_NUMERIC,	"LC_NUMERIC" },
#endif /* LC_NUMERIC */
#ifdef LC_RESPONSE
		{ LC_RESPONSE,	"LC_RESPONSE" },
#endif /* LC_RESPONSE */
#ifdef LC_TIME
		{ LC_TIME,	"LC_TIME" },
#endif /* LC_TIME */
	};
#endif /* ENABLE_NLS */

	tmp = tree->lnode;	/* first argument */
	t1 = force_string(tree_eval(tmp));
	string = t1->stptr;

	t2 = NULL;
#if ENABLE_NLS && HAVE_LC_MESSAGES && HAVE_DCGETTEXT
	tree = tree->rnode;	/* second argument */
	if (tree != NULL) {
		tmp = tree->lnode;
		t2 = force_string(tree_eval(tmp));
		domain = t2->stptr;
	} else
		domain = TEXTDOMAIN;

	if (tree != NULL && tree->rnode != NULL) {	/* third argument */
		int low, high, i, mid;
		NODE *t;

		tree = tree->rnode;
		tmp = tree->lnode;
		t = force_string(tree_eval(tmp));
		category = t->stptr;

		/* binary search the table */
		low = 0;
		high = (sizeof(cat_tab) / sizeof(cat_tab[0])) - 1;
		while (low <= high) {
			mid = (low + high) / 2;
			i = strcmp(category, cat_tab[mid].name);

			if (i < 0)		/* category < mid */
				high = mid - 1;
			else if (i > 0)		/* category > mid */
				low = mid + 1;
			else {
				lc_cat = cat_tab[mid].val;
				break;
			}
		}
		if (lc_cat == -1)	/* not there */
			fatal(_("dcgettext: `%s' is not a valid locale category"), category);

		free_temp(t);
	} else
		lc_cat = LC_MESSAGES;

	the_result = dcgettext(domain, string, lc_cat);
#else
	the_result = string;
#endif
	free_temp(t1);
	if (t2 != NULL)
		free_temp(t2);

	return tmp_string(the_result, strlen(the_result));
}

/* do_bindtextdomain --- set the directory for a text domain */

/*
 * awk usage is
 *
 * 	binding = bindtextdomain(dir [, domain])
 *
 * If dir is "", pass NULL to C version.
 * Default domain is TEXTDOMAIN.
 */

NODE *
do_bindtextdomain(NODE *tree)
{
	NODE *tmp, *t1, *t2;
	char *directory, *domain;
	char *the_result;

	t1 = t2 = NULL;
	/* set defaults */
	directory = NULL;
	domain = TEXTDOMAIN;

	tmp = tree->lnode;	/* first argument */
	t1 = force_string(tree_eval(tmp));
	if (t1->stlen > 0)
		directory = t1->stptr;

	tree = tree->rnode;	/* second argument */
	if (tree != NULL) {
		tmp = tree->lnode;
		t2 = force_string(tree_eval(tmp));
		domain = t2->stptr;
	}

	the_result = bindtextdomain(domain, directory);

	free_temp(t1);
	if (t2 != NULL)
		free_temp(t1);

	return tmp_string(the_result, strlen(the_result));
}
