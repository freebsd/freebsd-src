/*
 * field.c - routines for dealing with fields and record parsing
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-1999 the Free Software Foundation, Inc.
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
 * $FreeBSD: src/contrib/awk/field.c,v 1.4 1999/09/27 08:56:57 sheldonh Exp $
 */

#include "awk.h"

typedef void (* Setfunc) P((long, char *, long, NODE *));

static long (*parse_field) P((long, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static void rebuild_record P((void));
static long re_parse_field P((long, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static long def_parse_field P((long, char **, int, NODE *,
			      Regexp *, Setfunc, NODE *));
static long posix_def_parse_field P((long, char **, int, NODE *,
			      Regexp *, Setfunc, NODE *));
static long null_parse_field P((long, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static long sc_parse_field P((long, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static long fw_parse_field P((long, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static void set_element P((long num, char * str, long len, NODE *arr));
static void grow_fields_arr P((long num));
static void set_field P((long num, char *str, long len, NODE *dummy));


static char *parse_extent;	/* marks where to restart parse of record */
static long parse_high_water = 0; /* field number that we have parsed so far */
static long nf_high_water = 0;	/* size of fields_arr */
static int resave_fs;
static NODE *save_FS;		/* save current value of FS when line is read,
				 * to be used in deferred parsing
				 */
static int *FIELDWIDTHS = NULL;

NODE **fields_arr;		/* array of pointers to the field nodes */
int field0_valid;		/* $(>0) has not been changed yet */
int default_FS;			/* TRUE when FS == " " */
Regexp *FS_regexp = NULL;
static NODE *Null_field = NULL;

/* using_FIELDWIDTHS --- static function, macro to avoid overhead */
#define using_FIELDWIDTHS()	(parse_field == fw_parse_field)

/* init_fields --- set up the fields array to start with */

void
init_fields()
{
	NODE *n;

	emalloc(fields_arr, NODE **, sizeof(NODE *), "init_fields");
	getnode(n);
	*n = *Nnull_string;
	n->flags |= (SCALAR|FIELD);
	n->flags &= ~PERM;
	fields_arr[0] = n;
	parse_extent = fields_arr[0]->stptr;
	save_FS = dupnode(FS_node->var_value);
	getnode(Null_field);
	*Null_field = *Nnull_string;
	Null_field->flags |= (SCALAR|FIELD);
	Null_field->flags &= ~(NUM|NUMBER|MAYBE_NUM|PERM);
	field0_valid = TRUE;
}

/* grow_fields --- acquire new fields as needed */

static void
grow_fields_arr(num)
long num;
{
	register int t;
	register NODE *n;

	erealloc(fields_arr, NODE **, (num + 1) * sizeof(NODE *), "grow_fields_arr");
	for (t = nf_high_water + 1; t <= num; t++) {
		getnode(n);
		*n = *Null_field;
		fields_arr[t] = n;
	}
	nf_high_water = num;
}

/* set_field --- set the value of a particular field */

/*ARGSUSED*/
static void
set_field(num, str, len, dummy)
long num;
char *str;
long len;
NODE *dummy;	/* not used -- just to make interface same as set_element */
{
	register NODE *n;

	if (num > nf_high_water)
		grow_fields_arr(num);
	n = fields_arr[num];
	n->stptr = str;
	n->stlen = len;
	n->flags = (STR|STRING|MAYBE_NUM|SCALAR|FIELD);
}

/* rebuild_record --- Someone assigned a value to $(something).
			Fix up $0 to be right */

static void
rebuild_record()
{
	/*
	 * use explicit unsigned longs for lengths, in case
	 * a size_t isn't big enough.
	 */
	register unsigned long tlen;
	register unsigned long ofslen;
	register NODE *tmp;
	NODE *ofs;
	char *ops;
	register char *cops;
	long i;

	assert(NF != -1);

	tlen = 0;
	ofs = force_string(OFS_node->var_value);
	ofslen = ofs->stlen;
	for (i = NF; i > 0; i--) {
		tmp = fields_arr[i];
		tmp = force_string(tmp);
		tlen += tmp->stlen;
	}
	tlen += (NF - 1) * ofslen;
	if ((long) tlen < 0)
		tlen = 0;
	emalloc(ops, char *, tlen + 2, "rebuild_record");
	cops = ops;
	ops[0] = '\0';
	for (i = 1;  i <= NF; i++) {
		tmp = fields_arr[i];
		/* copy field */
		if (tmp->stlen == 1)
			*cops++ = tmp->stptr[0];
		else if (tmp->stlen != 0) {
			memcpy(cops, tmp->stptr, tmp->stlen);
			cops += tmp->stlen;
		}
		/* copy OFS */
		if (i != NF) {
			if (ofslen == 1)
				*cops++ = ofs->stptr[0];
			else if (ofslen != 0) {
				memcpy(cops, ofs->stptr, ofslen);
				cops += ofslen;
			}
		}
	}
	tmp = make_str_node(ops, tlen, ALREADY_MALLOCED);

	/*
	 * Since we are about to unref fields_arr[0], we want to find
	 * any fields that still point into it, and have them point
	 * into the new field zero.
	 */
	for (cops = ops, i = 1; i <= NF; i++) {
		if (fields_arr[i]->stlen > 0) {
 		        NODE *n;
		        getnode(n);

			if ((fields_arr[i]->flags & FIELD) == 0) {
				*n = *Null_field;
				n->stlen = fields_arr[i]->stlen;
				if ((fields_arr[i]->flags & (NUM|NUMBER)) != 0) {
					n->flags |= (fields_arr[i]->flags & (NUM|NUMBER));
					n->numbr = fields_arr[i]->numbr;
				}
			} else {
				*n = *(fields_arr[i]);
				n->flags &= ~(MALLOC|TEMP|PERM|STRING);
			}

			n->stptr = cops;
			unref(fields_arr[i]);
			fields_arr[i] = n;
		}
		cops += fields_arr[i]->stlen + ofslen;
	}

	unref(fields_arr[0]);

	fields_arr[0] = tmp;
	field0_valid = TRUE;
}

/*
 * set_record:
 * setup $0, but defer parsing rest of line until reference is made to $(>0)
 * or to NF.  At that point, parse only as much as necessary.
 */
void
set_record(buf, cnt, freeold)
char *buf;		/* ignored if ! freeold */
int cnt;		/* ignored if ! freeold */
int freeold;
{
	register int i;
	NODE *n;

	NF = -1;
	for (i = 1; i <= parse_high_water; i++) {
		unref(fields_arr[i]);
		getnode(n);
		*n = *Null_field;
		fields_arr[i] = n;
	}

	parse_high_water = 0;
	/*
	 * $0 = $0 should resplit using the current value of FS, thus,
	 * this is executed orthogonally to the value of freeold.
	 */
	if (resave_fs) {
		resave_fs = FALSE;
		unref(save_FS);
		save_FS = dupnode(FS_node->var_value);
	}
	if (freeold) {
		unref(fields_arr[0]);
		getnode(n);
		n->stptr = buf;
		n->stlen = cnt;
		n->stref = 1;
		n->type = Node_val;
		n->stfmt = -1;
		n->flags = (STRING|STR|MAYBE_NUM|SCALAR|FIELD);
		fields_arr[0] = n;
	}
	fields_arr[0]->flags |= MAYBE_NUM;
	field0_valid = TRUE;
}

/* reset_record --- start over again with current $0 */

void
reset_record()
{
	(void) force_string(fields_arr[0]);
	set_record(fields_arr[0]->stptr, fields_arr[0]->stlen, FALSE);
}

/* set_NF --- handle what happens to $0 and fields when NF is changed */

void
set_NF()
{
	register int i;
	NODE *n;

	assert(NF != -1);

	NF = (long) force_number(NF_node->var_value);
	if (NF > nf_high_water)
		grow_fields_arr(NF);
	if (parse_high_water < NF) {
		for (i = parse_high_water + 1; i <= NF; i++) {
			unref(fields_arr[i]);
			getnode(n);
			*n = *Null_field;
			fields_arr[i] = n;
		}
	} else if (parse_high_water > 0) {
		for (i = NF + 1; i <= parse_high_water; i++) {
			unref(fields_arr[i]);
			getnode(n);
			*n = *Null_field;
			fields_arr[i] = n;
		}
		parse_high_water = NF;
	}
	field0_valid = FALSE;
}

/*
 * re_parse_field --- parse fields using a regexp.
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a regular
 * expression -- either user-defined or because RS=="" and FS==" "
 */
static long
re_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register long nf = parse_high_water;
	register char *field;
	register char *end = scan + len;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	if (RS_is_null && default_FS)
		while (scan < end && (*scan == ' ' || *scan == '\t' || *scan == '\n'))
			scan++;
	field = scan;
	while (scan < end
	       && research(rp, scan, 0, (end - scan), TRUE) != -1
	       && nf < up_to) {
		if (REEND(rp, scan) == RESTART(rp, scan)) {   /* null match */
			scan++;
			if (scan == end) {
				(*set)(++nf, field, (long)(scan - field), n);
				up_to = nf;
				break;
			}
			continue;
		}
		(*set)(++nf, field,
		       (long)(scan + RESTART(rp, scan) - field), n);
		scan += REEND(rp, scan);
		field = scan;
		if (scan == end)	/* FS at end of record */
			(*set)(++nf, field, 0L, n);
	}
	if (nf != up_to && scan < end) {
		(*set)(++nf, scan, (long)(end - scan), n);
		scan = end;
	}
	*buf = scan;
	return (nf);
}

/*
 * def_parse_field --- default field parsing.
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a single space
 * character.
 */

static long
def_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register long nf = parse_high_water;
	register char *field;
	register char *end = scan + len;
	char sav;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	/*
	 * Nasty special case. If FS set to "", return whole record
	 * as first field. This is not worth a separate function.
	 */
	if (fs->stlen == 0) {
		(*set)(++nf, *buf, len, n);
		*buf += len;
		return nf;
	}

	/* before doing anything save the char at *end */
	sav = *end;
	/* because it will be destroyed now: */

	*end = ' ';	/* sentinel character */
	for (; nf < up_to; scan++) {
		/*
		 * special case:  fs is single space, strip leading whitespace 
		 */
		while (scan < end && (*scan == ' ' || *scan == '\t' || *scan == '\n'))
			scan++;
		if (scan >= end)
			break;
		field = scan;
		while (*scan != ' ' && *scan != '\t' && *scan != '\n')
			scan++;
		(*set)(++nf, field, (long)(scan - field), n);
		if (scan == end)
			break;
	}

	/* everything done, restore original char at *end */
	*end = sav;

	*buf = scan;
	return nf;
}

/*
 * posix_def_parse_field --- default field parsing.
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a single space
 * character.  The only difference between this and def_parse_field()
 * is that this one does not allow newlines to separate fields.
 */

static long
posix_def_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register long nf = parse_high_water;
	register char *field;
	register char *end = scan + len;
	char sav;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	/*
	 * Nasty special case. If FS set to "", return whole record
	 * as first field. This is not worth a separate function.
	 */
	if (fs->stlen == 0) {
		(*set)(++nf, *buf, len, n);
		*buf += len;
		return nf;
	}

	/* before doing anything save the char at *end */
	sav = *end;
	/* because it will be destroyed now: */

	*end = ' ';	/* sentinel character */
	for (; nf < up_to; scan++) {
		/*
		 * special case:  fs is single space, strip leading whitespace 
		 */
		while (scan < end && (*scan == ' ' || *scan == '\t'))
			scan++;
		if (scan >= end)
			break;
		field = scan;
		while (*scan != ' ' && *scan != '\t')
			scan++;
		(*set)(++nf, field, (long)(scan - field), n);
		if (scan == end)
			break;
	}

	/* everything done, restore original char at *end */
	*end = sav;

	*buf = scan;
	return nf;
}

/*
 * null_parse_field --- each character is a separate field
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is the null string.
 */
static long
null_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register long nf = parse_high_water;
	register char *end = scan + len;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	for (; nf < up_to && scan < end; scan++)
		(*set)(++nf, scan, 1L, n);

	*buf = scan;
	return nf;
}

/*
 * sc_parse_field --- single character field separator
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a single character
 * other than space.
 */
static long
sc_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register char fschar;
	register long nf = parse_high_water;
	register char *field;
	register char *end = scan + len;
	int onecase;
	char sav;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	if (RS_is_null && fs->stlen == 0)
		fschar = '\n';
	else
		fschar = fs->stptr[0];

	onecase = (IGNORECASE && isalpha(fschar));
	if (onecase)
		fschar = casetable[(int) fschar];

	/* before doing anything save the char at *end */
	sav = *end;
	/* because it will be destroyed now: */
	*end = fschar;	/* sentinel character */

	for (; nf < up_to;) {
		field = scan;
		if (onecase) {
			while (casetable[(int) *scan] != fschar)
				scan++;
		} else {
			while (*scan != fschar)
				scan++;
		}
		(*set)(++nf, field, (long)(scan - field), n);
		if (scan == end)
			break;
		scan++;
		if (scan == end) {	/* FS at end of record */
			(*set)(++nf, field, 0L, n);
			break;
		}
	}

	/* everything done, restore original char at *end */
	*end = sav;

	*buf = scan;
	return nf;
}

/*
 * fw_parse_field --- field parsing using FIELDWIDTHS spec
 *
 * This is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for fields are fixed widths.
 */
static long
fw_parse_field(up_to, buf, len, fs, rp, set, n)
long up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register long nf = parse_high_water;
	register char *end = scan + len;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;
	for (; nf < up_to && (len = FIELDWIDTHS[nf+1]) != -1; ) {
		if (len > end - scan)
			len = end - scan;
		(*set)(++nf, scan, (long) len, n);
		scan += len;
	}
	if (len == -1)
		*buf = end;
	else
		*buf = scan;
	return nf;
}

/* get_field --- return a particular $n */

NODE **
get_field(requested, assign)
register long requested;
Func_ptr *assign;	/* this field is on the LHS of an assign */
{
	/*
	 * if requesting whole line but some other field has been altered,
	 * then the whole line must be rebuilt
	 */
	if (requested == 0) {
		if (! field0_valid) {
			/* first, parse remainder of input record */
			if (NF == -1) {
				NF = (*parse_field)(HUGE-1, &parse_extent,
		    			fields_arr[0]->stlen -
					(parse_extent - fields_arr[0]->stptr),
		    			save_FS, FS_regexp, set_field,
					(NODE *) NULL);
				parse_high_water = NF;
			}
			rebuild_record();
		}
		if (assign != NULL)
			*assign = reset_record;
		return &fields_arr[0];
	}

	/* assert(requested > 0); */

	if (assign != NULL)
		field0_valid = FALSE;		/* $0 needs reconstruction */

	if (requested <= parse_high_water)	/* already parsed this field */
		return &fields_arr[requested];

	if (NF == -1) {	/* have not yet parsed to end of record */
		/*
		 * parse up to requested fields, calling set_field() for each,
		 * saving in parse_extent the point where the parse left off
		 */
		if (parse_high_water == 0)	/* starting at the beginning */
			parse_extent = fields_arr[0]->stptr;
		parse_high_water = (*parse_field)(requested, &parse_extent,
		     fields_arr[0]->stlen - (parse_extent - fields_arr[0]->stptr),
		     save_FS, FS_regexp, set_field, (NODE *) NULL);

		/*
		 * if we reached the end of the record, set NF to the number of
		 * fields so far.  Note that requested might actually refer to
		 * a field that is beyond the end of the record, but we won't
		 * set NF to that value at this point, since this is only a
		 * reference to the field and NF only gets set if the field
		 * is assigned to -- this case is handled below
		 */
		if (parse_extent == fields_arr[0]->stptr + fields_arr[0]->stlen)
			NF = parse_high_water;
		if (requested == HUGE-1)	/* HUGE-1 means set NF */
			requested = parse_high_water;
	}
	if (parse_high_water < requested) { /* requested beyond end of record */
		if (assign != NULL) {	/* expand record */
			if (requested > nf_high_water)
				grow_fields_arr(requested);

			NF = requested;
			parse_high_water = requested;
		} else
			return &Null_field;
	}

	return &fields_arr[requested];
}

/* set_element --- set an array element, used by do_split() */

static void
set_element(num, s, len, n)
long num;
char *s;
long len;
NODE *n;
{
	register NODE *it;

	it = make_string(s, len);
	it->flags |= MAYBE_NUM;
	*assoc_lookup(n, tmp_number((AWKNUM) (num))) = it;
}

/* do_split --- implement split(), semantics are same as for field splitting */

NODE *
do_split(tree)
NODE *tree;
{
	NODE *src, *arr, *sep, *tmp;
	NODE *fs;
	char *s;
	long (*parseit) P((long, char **, int, NODE *,
			 Regexp *, Setfunc, NODE *));
	Regexp *rp = NULL;

	/*
	 * do dupnode(), to avoid problems like
	 *	x = split(a[1], a, "blah")
	 * since we assoc_clear the array. gack.
	 * this also gives us complete call by value semantics.
	 */
	tmp = tree_eval(tree->lnode);
	src = dupnode(tmp);
	free_temp(tmp);

	arr = tree->rnode->lnode;
	if (tree->rnode->rnode != NULL)
		sep = tree->rnode->rnode->lnode;	/* 3rd arg */
	else
		sep = NULL;

	(void) force_string(src);

	if (arr->type == Node_param_list)
		arr = stack_ptr[arr->param_cnt];
	if (arr->type != Node_var && arr->type != Node_var_array)
		fatal("second argument of split is not an array");
	arr->type = Node_var_array;
	assoc_clear(arr);

	if ((sep->re_flags & FS_DFLT) != 0 && ! using_FIELDWIDTHS()) {
		parseit = parse_field;
		fs = force_string(FS_node->var_value);
		rp = FS_regexp;
	} else {
		tmp = force_string(tree_eval(sep->re_exp));
		if (tmp->stlen == 0)
			parseit = null_parse_field;
		else if (tmp->stlen == 1 && (sep->re_flags & CONST) == 0) {
			if (tmp->stptr[0] == ' ') {
				if (do_posix)
					parseit = posix_def_parse_field;
				else
					parseit = def_parse_field;
			} else
				parseit = sc_parse_field;
		} else {
			parseit = re_parse_field;
			rp = re_update(sep);
		}
		fs = tmp;
	}

	s = src->stptr;
	tmp = tmp_number((AWKNUM) (*parseit)(HUGE, &s, (int) src->stlen,
					     fs, rp, set_element, arr));
	unref(src);
	free_temp(sep);
	return tmp;
}

/* set_FIELDWIDTHS --- handle an assignment to FIELDWIDTHS */

void
set_FIELDWIDTHS()
{
	register char *scan;
	char *end;
	register int i;
	static int fw_alloc = 1;
	static int warned = FALSE;
	extern double strtod();

	if (do_lint && ! warned) {
		warned = TRUE;
		warning("use of FIELDWIDTHS is a gawk extension");
	}
	if (do_traditional)	/* quick and dirty, does the trick */
		return;

	/*
	 * If changing the way fields are split, obey least-suprise
	 * semantics, and force $0 to be split totally.
	 */
	if (fields_arr != NULL)
		(void) get_field(HUGE - 1, 0);

	parse_field = fw_parse_field;
	scan = force_string(FIELDWIDTHS_node->var_value)->stptr;
	end = scan + 1;
	if (FIELDWIDTHS == NULL)
		emalloc(FIELDWIDTHS, int *, fw_alloc * sizeof(int), "set_FIELDWIDTHS");
	FIELDWIDTHS[0] = 0;
	for (i = 1; ; i++) {
		if (i >= fw_alloc) {
			fw_alloc *= 2;
			erealloc(FIELDWIDTHS, int *, fw_alloc * sizeof(int), "set_FIELDWIDTHS");
		}
		FIELDWIDTHS[i] = (int) strtod(scan, &end);
		if (end == scan)
			break;
		scan = end;
	}
	FIELDWIDTHS[i] = -1;
}

void
set_FS_if_not_FIELDWIDTHS()
{
	if (parse_field != fw_parse_field)
		set_FS();
}

/* set_FS --- handle things when FS is assigned to */

void
set_FS()
{
	char buf[10];
	NODE *fs;
	static NODE *save_fs = NULL;
	static NODE *save_rs = NULL;

	/*
	 * If changing the way fields are split, obey least-suprise
	 * semantics, and force $0 to be split totally.
	 */
	if (fields_arr != NULL)
		(void) get_field(HUGE - 1, 0);

	if (! (save_fs && cmp_nodes(FS_node->var_value, save_fs) == 0
		&& save_rs && cmp_nodes(RS_node->var_value, save_rs) == 0)) {
		unref(save_fs);
		save_fs = dupnode(FS_node->var_value);
		unref(save_rs);
		save_rs = dupnode(RS_node->var_value);
		resave_fs = TRUE;
		if (FS_regexp) {
			refree(FS_regexp);
			FS_regexp = NULL;
		}
	}
	buf[0] = '\0';
	default_FS = FALSE;
	fs = force_string(FS_node->var_value);
	if (! do_traditional && fs->stlen == 0)
		parse_field = null_parse_field;
	else if (fs->stlen > 1)
		parse_field = re_parse_field;
	else if (RS_is_null) {
		parse_field = sc_parse_field;
		if (fs->stlen == 1) {
			if (fs->stptr[0] == ' ') {
				default_FS = TRUE;
				strcpy(buf, "[ \t\n]+");
			} else if (fs->stptr[0] != '\n')
				sprintf(buf, "[%c\n]", fs->stptr[0]);
		}
	} else {
		if (do_posix)
			parse_field = posix_def_parse_field;
		else
			parse_field = def_parse_field;
		if (fs->stptr[0] == ' ' && fs->stlen == 1)
			default_FS = TRUE;
		else if (fs->stptr[0] != ' ' && fs->stlen == 1) {
			if (! IGNORECASE || ! isalpha(fs->stptr[0]))
				parse_field = sc_parse_field;
			else if (fs->stptr[0] == '\\')
				/* yet another special case */
				strcpy(buf, "[\\\\]");
			else
				sprintf(buf, "[%c]", fs->stptr[0]);
		}
	}
	if (buf[0] != '\0') {
		FS_regexp = make_regexp(buf, strlen(buf), IGNORECASE, TRUE);
		parse_field = re_parse_field;
	} else if (parse_field == re_parse_field) {
		FS_regexp = make_regexp(fs->stptr, fs->stlen, IGNORECASE, TRUE);
	} else
		FS_regexp = NULL;
}

/* using_fieldwidths --- is FS or FIELDWIDTHS in use? */

int
using_fieldwidths()
{
	return using_FIELDWIDTHS();
}
