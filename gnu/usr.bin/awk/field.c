/*
 * field.c - routines for dealing with fields and record parsing
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

typedef void (* Setfunc) P((int, char*, int, NODE *));

static long (*parse_field) P((int, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static void rebuild_record P((void));
static long re_parse_field P((int, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static long def_parse_field P((int, char **, int, NODE *,
			      Regexp *, Setfunc, NODE *));
static long sc_parse_field P((int, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static long fw_parse_field P((int, char **, int, NODE *,
			     Regexp *, Setfunc, NODE *));
static void set_element P((int, char *, int, NODE *));
static void grow_fields_arr P((long num));
static void set_field P((int num, char *str, int len, NODE *dummy));


static Regexp *FS_regexp = NULL;
static char *parse_extent;	/* marks where to restart parse of record */
static long parse_high_water=0;	/* field number that we have parsed so far */
static long nf_high_water = 0;	/* size of fields_arr */
static int resave_fs;
static NODE *save_FS;		/* save current value of FS when line is read,
				 * to be used in deferred parsing
				 */

NODE **fields_arr;		/* array of pointers to the field nodes */
int field0_valid;		/* $(>0) has not been changed yet */
int default_FS;
static NODE **nodes;		/* permanent repository of field nodes */
static int *FIELDWIDTHS = NULL;

void
init_fields()
{
	NODE *n;

	emalloc(fields_arr, NODE **, sizeof(NODE *), "init_fields");
	emalloc(nodes, NODE **, sizeof(NODE *), "init_fields");
	getnode(n);
	*n = *Nnull_string;
	fields_arr[0] = nodes[0] = n;
	parse_extent = fields_arr[0]->stptr;
	save_FS = dupnode(FS_node->var_value);
	field0_valid = 1;
}


static void
grow_fields_arr(num)
long num;
{
	register int t;
	register NODE *n;

	erealloc(fields_arr, NODE **, (num + 1) * sizeof(NODE *), "set_field");
	erealloc(nodes, NODE **, (num+1) * sizeof(NODE *), "set_field");
	for (t = nf_high_water+1; t <= num; t++) {
		getnode(n);
		*n = *Nnull_string;
		fields_arr[t] = nodes[t] = n;
	}
	nf_high_water = num;
}

/*ARGSUSED*/
static void
set_field(num, str, len, dummy)
int num;
char *str;
int len;
NODE *dummy;	/* not used -- just to make interface same as set_element */
{
	register NODE *n;

	if (num > nf_high_water)
		grow_fields_arr(num);
	n = nodes[num];
	n->stptr = str;
	n->stlen = len;
	n->flags = (PERM|STR|STRING|MAYBE_NUM);
	fields_arr[num] = n;
}

/* Someone assigned a value to $(something).  Fix up $0 to be right */
static void
rebuild_record()
{
	register size_t tlen;
	register NODE *tmp;
	NODE *ofs;
	char *ops;
	register char *cops;
	register NODE **ptr;
	register size_t ofslen;

	tlen = 0;
	ofs = force_string(OFS_node->var_value);
	ofslen = ofs->stlen;
	ptr = &fields_arr[NF];
	while (ptr > &fields_arr[0]) {
		tmp = force_string(*ptr);
		tlen += tmp->stlen;
		ptr--;
	}
	tlen += (NF - 1) * ofslen;
	if ((long)tlen < 0)
	    tlen = 0;
	emalloc(ops, char *, tlen + 2, "rebuild_record");
	cops = ops;
	ops[0] = '\0';
	for (ptr = &fields_arr[1]; ptr <= &fields_arr[NF]; ptr++) {
		tmp = *ptr;
		if (tmp->stlen == 1)
			*cops++ = tmp->stptr[0];
		else if (tmp->stlen != 0) {
			memcpy(cops, tmp->stptr, tmp->stlen);
			cops += tmp->stlen;
		}
		if (ptr != &fields_arr[NF]) {
			if (ofslen == 1)
				*cops++ = ofs->stptr[0];
			else if (ofslen != 0) {
				memcpy(cops, ofs->stptr, ofslen);
				cops += ofslen;
			}
		}
	}
	tmp = make_str_node(ops, tlen, ALREADY_MALLOCED);
	unref(fields_arr[0]);
	fields_arr[0] = tmp;
	field0_valid = 1;
}

/*
 * setup $0, but defer parsing rest of line until reference is made to $(>0)
 * or to NF.  At that point, parse only as much as necessary.
 */
void
set_record(buf, cnt, freeold)
char *buf;
int cnt;
int freeold;
{
	register int i;

	NF = -1;
	for (i = 1; i <= parse_high_water; i++) {
		unref(fields_arr[i]);
	}
	parse_high_water = 0;
	if (freeold) {
		unref(fields_arr[0]);
		if (resave_fs) {
			resave_fs = 0;
			unref(save_FS);
			save_FS = dupnode(FS_node->var_value);
		}
		nodes[0]->stptr = buf;
		nodes[0]->stlen = cnt;
		nodes[0]->stref = 1;
		nodes[0]->flags = (STRING|STR|PERM|MAYBE_NUM);
		fields_arr[0] = nodes[0];
	}
	fields_arr[0]->flags |= MAYBE_NUM;
	field0_valid = 1;
}

void
reset_record()
{
	(void) force_string(fields_arr[0]);
	set_record(fields_arr[0]->stptr, fields_arr[0]->stlen, 0);
}

void
set_NF()
{
	register int i;

	NF = (long) force_number(NF_node->var_value);
	if (NF > nf_high_water)
		grow_fields_arr(NF);
	for (i = parse_high_water + 1; i <= NF; i++) {
		unref(fields_arr[i]);
		fields_arr[i] = Nnull_string;
	}
	field0_valid = 0;
}

/*
 * this is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a regular
 * expression -- either user-defined or because RS=="" and FS==" "
 */
static long
re_parse_field(up_to, buf, len, fs, rp, set, n)
int up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register int nf = parse_high_water;
	register char *field;
	register char *end = scan + len;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	if (*RS == 0 && default_FS)
		while (scan < end && (*scan == ' ' || *scan == '\t' || *scan == '\n'))
			scan++;
	field = scan;
	while (scan < end
	       && research(rp, scan, 0, (end - scan), 1) != -1
	       && nf < up_to) {
		if (REEND(rp, scan) == RESTART(rp, scan)) {   /* null match */
			scan++;
			if (scan == end) {
				(*set)(++nf, field, (int)(scan - field), n);
				up_to = nf;
				break;
			}
			continue;
		}
		(*set)(++nf, field,
		       (int)(scan + RESTART(rp, scan) - field), n);
		scan += REEND(rp, scan);
		field = scan;
		if (scan == end)	/* FS at end of record */
			(*set)(++nf, field, 0, n);
	}
	if (nf != up_to && scan < end) {
		(*set)(++nf, scan, (int)(end - scan), n);
		scan = end;
	}
	*buf = scan;
	return (nf);
}

/*
 * this is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a single space
 * character.
 */
static long
def_parse_field(up_to, buf, len, fs, rp, set, n)
int up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register int nf = parse_high_water;
	register char *field;
	register char *end = scan + len;
	char sav;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

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
		(*set)(++nf, field, (int)(scan - field), n);
		if (scan == end)
			break;
	}

	/* everything done, restore original char at *end */
	*end = sav;

	*buf = scan;
	return nf;
}

/*
 * this is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for when FS is a single character
 * other than space.
 */
static long
sc_parse_field(up_to, buf, len, fs, rp, set, n)
int up_to;	/* parse only up to this field number */
char **buf;	/* on input: string to parse; on output: point to start next */
int len;
NODE *fs;
Regexp *rp;
Setfunc set;	/* routine to set the value of the parsed field */
NODE *n;
{
	register char *scan = *buf;
	register char fschar;
	register int nf = parse_high_water;
	register char *field;
	register char *end = scan + len;
	char sav;

	if (up_to == HUGE)
		nf = 0;
	if (len == 0)
		return nf;

	if (*RS == 0 && fs->stlen == 0)
		fschar = '\n';
	else
		fschar = fs->stptr[0];

	/* before doing anything save the char at *end */
	sav = *end;
	/* because it will be destroyed now: */
	*end = fschar;	/* sentinel character */

	for (; nf < up_to;) {
		field = scan;
		while (*scan != fschar)
			scan++;
		(*set)(++nf, field, (int)(scan - field), n);
		if (scan == end)
			break;
		scan++;
		if (scan == end) {	/* FS at end of record */
			(*set)(++nf, field, 0, n);
			break;
		}
	}

	/* everything done, restore original char at *end */
	*end = sav;

	*buf = scan;
	return nf;
}

/*
 * this is called both from get_field() and from do_split()
 * via (*parse_field)().  This variation is for fields are fixed widths.
 */
static long
fw_parse_field(up_to, buf, len, fs, rp, set, n)
int up_to;	/* parse only up to this field number */
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
		(*set)(++nf, scan, len, n);
		scan += len;
	}
	if (len == -1)
		*buf = end;
	else
		*buf = scan;
	return nf;
}

NODE **
get_field(requested, assign)
register int requested;
Func_ptr *assign;	/* this field is on the LHS of an assign */
{
	/*
	 * if requesting whole line but some other field has been altered,
	 * then the whole line must be rebuilt
	 */
	if (requested == 0) {
		if (!field0_valid) {
			/* first, parse remainder of input record */
			if (NF == -1) {
				NF = (*parse_field)(HUGE-1, &parse_extent,
		    			fields_arr[0]->stlen -
					(parse_extent - fields_arr[0]->stptr),
		    			save_FS, FS_regexp, set_field,
					(NODE *)NULL);
				parse_high_water = NF;
			}
			rebuild_record();
		}
		if (assign)
			*assign = reset_record;
		return &fields_arr[0];
	}

	/* assert(requested > 0); */

	if (assign)
		field0_valid = 0;		/* $0 needs reconstruction */

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
		     fields_arr[0]->stlen - (parse_extent-fields_arr[0]->stptr),
		     save_FS, FS_regexp, set_field, (NODE *)NULL);

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
		if (assign) {	/* expand record */
			register int i;

			if (requested > nf_high_water)
				grow_fields_arr(requested);

			/* fill in fields that don't exist */
			for (i = parse_high_water + 1; i <= requested; i++)
				fields_arr[i] = Nnull_string;

			NF = requested;
			parse_high_water = requested;
		} else
			return &Nnull_string;
	}

	return &fields_arr[requested];
}

static void
set_element(num, s, len, n)
int num;
char *s;
int len;
NODE *n;
{
	register NODE *it;

	it = make_string(s, len);
	it->flags |= MAYBE_NUM;
	*assoc_lookup(n, tmp_number((AWKNUM) (num))) = it;
}

NODE *
do_split(tree)
NODE *tree;
{
	NODE *t1, *t2, *t3, *tmp;
	NODE *fs;
	char *s;
	long (*parseit)P((int, char **, int, NODE *,
			 Regexp *, Setfunc, NODE *));
	Regexp *rp = NULL;


	/*
	 * do dupnode(), to avoid problems like
	 *	x = split(a[1], a, "blah")
	 * since we assoc_clear the array. gack.
	 * this also gives up complete call by value semantics.
	 */
	tmp = tree_eval(tree->lnode);
	t1 = dupnode(tmp);
	free_temp(tmp);

	t2 = tree->rnode->lnode;
	t3 = tree->rnode->rnode->lnode;

	(void) force_string(t1);

	if (t2->type == Node_param_list)
		t2 = stack_ptr[t2->param_cnt];
	if (t2->type != Node_var && t2->type != Node_var_array)
		fatal("second argument of split is not a variable");
	assoc_clear(t2);

	if (t3->re_flags & FS_DFLT) {
		parseit = parse_field;
		fs = force_string(FS_node->var_value);
		rp = FS_regexp;
	} else {
		tmp = force_string(tree_eval(t3->re_exp));
		if (tmp->stlen == 1) {
			if (tmp->stptr[0] == ' ')
				parseit = def_parse_field;
			else
				parseit = sc_parse_field;
		} else {
			parseit = re_parse_field;
			rp = re_update(t3);
		}
		fs = tmp;
	}

	s = t1->stptr;
	tmp = tmp_number((AWKNUM) (*parseit)(HUGE, &s, (int)t1->stlen,
					     fs, rp, set_element, t2));
	unref(t1);
	free_temp(t3);
	return tmp;
}

void
set_FS()
{
	char buf[10];
	NODE *fs;

	/*
	 * If changing the way fields are split, obey least-suprise
	 * semantics, and force $0 to be split totally.
	 */
	if (fields_arr != NULL)
		(void) get_field(HUGE - 1, 0);

	buf[0] = '\0';
	default_FS = 0;
	if (FS_regexp) {
		refree(FS_regexp);
		FS_regexp = NULL;
	}
	fs = force_string(FS_node->var_value);
	if (fs->stlen > 1)
		parse_field = re_parse_field;
	else if (*RS == 0) {
		parse_field = sc_parse_field;
		if (fs->stlen == 1) {
			if (fs->stptr[0] == ' ') {
				default_FS = 1;
				strcpy(buf, "[ \t\n]+");
			} else if (fs->stptr[0] != '\n')
				sprintf(buf, "[%c\n]", fs->stptr[0]);
		}
	} else {
		parse_field = def_parse_field;
		if (fs->stptr[0] == ' ' && fs->stlen == 1)
			default_FS = 1;
		else if (fs->stptr[0] != ' ' && fs->stlen == 1) {
			if (IGNORECASE == 0)
				parse_field = sc_parse_field;
			else if (fs->stptr[0] == '\\')
				/* yet another special case */
				strcpy(buf, "[\\\\]");
			else
				sprintf(buf, "[%c]", fs->stptr[0]);
		}
	}
	if (buf[0]) {
		FS_regexp = make_regexp(buf, strlen(buf), IGNORECASE, 1);
		parse_field = re_parse_field;
	} else if (parse_field == re_parse_field) {
		FS_regexp = make_regexp(fs->stptr, fs->stlen, IGNORECASE, 1);
	} else
		FS_regexp = NULL;
	resave_fs = 1;
}

void
set_RS()
{
	(void) force_string(RS_node->var_value);
	RS = RS_node->var_value->stptr;
	set_FS();
}

void
set_FIELDWIDTHS()
{
	register char *scan;
	char *end;
	register int i;
	static int fw_alloc = 1;
	static int warned = 0;
	extern double strtod();

	if (do_lint && ! warned) {
		warned = 1;
		warning("use of FIELDWIDTHS is a gawk extension");
	}
	if (do_unix)	/* quick and dirty, does the trick */
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
