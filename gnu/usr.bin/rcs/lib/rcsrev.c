/* Handle RCS revision numbers.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 * $Log: rcsrev.c,v $
 * Revision 5.10  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.9  1995/06/01 16:23:43  eggert
 * (cmpdate, normalizeyear): New functions work around MKS RCS incompatibility.
 * (cmpnum, compartial): s[d] -> *(s+d) to work around Cray compiler bug.
 * (genrevs, genbranch): cmpnum -> cmpdate
 *
 * Revision 5.8  1994/03/17 14:05:48  eggert
 * Remove lint.
 *
 * Revision 5.7  1993/11/09 17:40:15  eggert
 * Fix format string typos.
 *
 * Revision 5.6  1993/11/03 17:42:27  eggert
 * Revision number `.N' now stands for `D.N', where D is the default branch.
 * Add -z.  Improve quality of diagnostics.  Add `namedrev' for Name support.
 *
 * Revision 5.5  1992/07/28  16:12:44  eggert
 * Identifiers may now start with a digit.  Avoid `unsigned'.
 *
 * Revision 5.4  1992/01/06  02:42:34  eggert
 * while (E) ; -> while (E) continue;
 *
 * Revision 5.3  1991/08/19  03:13:55  eggert
 * Add `-r$', `-rB.'.  Remove botches like `<now>' from messages.  Tune.
 *
 * Revision 5.2  1991/04/21  11:58:28  eggert
 * Add tiprev().
 *
 * Revision 5.1  1991/02/25  07:12:43  eggert
 * Avoid overflow when comparing revision numbers.
 *
 * Revision 5.0  1990/08/22  08:13:43  eggert
 * Remove compile-time limits; use malloc instead.
 * Ansify and Posixate.  Tune.
 * Remove possibility of an internal error.  Remove lint.
 *
 * Revision 4.5  89/05/01  15:13:22  narten
 * changed copyright header to reflect current distribution rules
 *
 * Revision 4.4  87/12/18  11:45:22  narten
 * more lint cleanups. Also, the NOTREACHED comment is no longer necessary,
 * since there's now a return value there with a value. (Guy Harris)
 *
 * Revision 4.3  87/10/18  10:38:42  narten
 * Updating version numbers. Changes relative to version 1.1 actually
 * relative to 4.1
 *
 * Revision 1.3  87/09/24  14:00:37  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf
 * warnings)
 *
 * Revision 1.2  87/03/27  14:22:37  jenkins
 * Port to suns
 *
 * Revision 4.1  83/03/25  21:10:45  wft
 * Only changed $Header to $Id.
 *
 * Revision 3.4  82/12/04  13:24:08  wft
 * Replaced getdelta() with gettree().
 *
 * Revision 3.3  82/11/28  21:33:15  wft
 * fixed compartial() and compnum() for nil-parameters; fixed nils
 * in error messages. Testprogram output shortenend.
 *
 * Revision 3.2  82/10/18  21:19:47  wft
 * renamed compnum->cmpnum, compnumfld->cmpnumfld,
 * numericrevno->numricrevno.
 *
 * Revision 3.1  82/10/11  19:46:09  wft
 * changed expandsym() to check for source==nil; returns zero length string
 * in that case.
 */

#include "rcsbase.h"

libId(revId, "$Id: rcsrev.c,v 5.10 1995/06/16 06:19:24 eggert Exp $")

static char const *branchtip P((char const*));
static char const *lookupsym P((char const*));
static char const *normalizeyear P((char const*,char[5]));
static struct hshentry *genbranch P((struct hshentry const*,char const*,int,char const*,char const*,char const*,struct hshentries**));
static void absent P((char const*,int));
static void cantfindbranch P((char const*,char const[datesize],char const*,char const*));
static void store1 P((struct hshentries***,struct hshentry*));



	int
countnumflds(s)
	char const *s;
/* Given a pointer s to a dotted number (date or revision number),
 * countnumflds returns the number of digitfields in s.
 */
{
	register char const *sp;
	register int count;
	if (!(sp=s) || !*sp)
		return 0;
        count = 1;
	do {
                if (*sp++ == '.') count++;
	} while (*sp);
        return(count);
}

	void
getbranchno(revno,branchno)
	char const *revno;
	struct buf *branchno;
/* Given a revision number revno, getbranchno copies the number of the branch
 * on which revno is into branchno. If revno itself is a branch number,
 * it is copied unchanged.
 */
{
	register int numflds;
	register char *tp;

	bufscpy(branchno, revno);
        numflds=countnumflds(revno);
	if (!(numflds & 1)) {
		tp = branchno->string;
		while (--numflds)
			while (*tp++ != '.')
				continue;
                *(tp-1)='\0';
        }
}



int cmpnum(num1, num2)
	char const *num1, *num2;
/* compares the two dotted numbers num1 and num2 lexicographically
 * by field. Individual fields are compared numerically.
 * returns <0, 0, >0 if num1<num2, num1==num2, and num1>num2, resp.
 * omitted fields are assumed to be higher than the existing ones.
*/
{
	register char const *s1, *s2;
	register size_t d1, d2;
	register int r;

	s1 = num1 ? num1 : "";
	s2 = num2 ? num2 : "";

	for (;;) {
		/* Give precedence to shorter one.  */
		if (!*s1)
			return (unsigned char)*s2;
		if (!*s2)
			return -1;

		/* Strip leading zeros, then find number of digits.  */
		while (*s1=='0') ++s1;
		while (*s2=='0') ++s2;
		for (d1=0; isdigit(*(s1+d1)); d1++) continue;
		for (d2=0; isdigit(*(s2+d2)); d2++) continue;

		/* Do not convert to integer; it might overflow!  */
		if (d1 != d2)
			return d1<d2 ? -1 : 1;
		if ((r = memcmp(s1, s2, d1)))
			return r;
		s1 += d1;
		s2 += d1;

                /* skip '.' */
		if (*s1) s1++;
		if (*s2) s2++;
	}
}



int cmpnumfld(num1, num2, fld)
	char const *num1, *num2;
	int fld;
/* Compare the two dotted numbers at field fld.
 * num1 and num2 must have at least fld fields.
 * fld must be positive.
*/
{
	register char const *s1, *s2;
	register size_t d1, d2;

	s1 = num1;
	s2 = num2;
        /* skip fld-1 fields */
	while (--fld) {
		while (*s1++ != '.')
			continue;
		while (*s2++ != '.')
			continue;
	}
        /* Now s1 and s2 point to the beginning of the respective fields */
	while (*s1=='0') ++s1;  for (d1=0; isdigit(*(s1+d1)); d1++) continue;
	while (*s2=='0') ++s2;  for (d2=0; isdigit(*(s2+d2)); d2++) continue;

	return d1<d2 ? -1 : d1==d2 ? memcmp(s1,s2,d1) : 1;
}


	int
cmpdate(d1, d2)
	char const *d1, *d2;
/*
* Compare the two dates.  This is just like cmpnum,
* except that for compatibility with old versions of RCS,
* 1900 is added to dates with two-digit years.
*/
{
	char year1[5], year2[5];
	int r = cmpnumfld(normalizeyear(d1,year1), normalizeyear(d2,year2), 1);

	if (r)
		return r;
	else {
		while (isdigit(*d1)) d1++;  d1 += *d1=='.';
		while (isdigit(*d2)) d2++;  d2 += *d2=='.';
		return cmpnum(d1, d2);
	}
}

	static char const *
normalizeyear(date, year)
	char const *date;
	char year[5];
{
	if (isdigit(date[0]) && isdigit(date[1]) && !isdigit(date[2])) {
		year[0] = '1';
		year[1] = '9';
		year[2] = date[0];
		year[3] = date[1];
		year[4] = 0;
		return year;
	} else
		return date;
}


	static void
cantfindbranch(revno, date, author, state)
	char const *revno, date[datesize], *author, *state;
{
	char datebuf[datesize + zonelenmax];

	rcserror("No revision on branch %s has%s%s%s%s%s%s.",
		revno,
		date ? " a date before " : "",
		date ? date2str(date,datebuf) : "",
		author ? " and author "+(date?0:4) : "",
		author ? author : "",
		state ? " and state "+(date||author?0:4) : "",
		state ? state : ""
	);
}

	static void
absent(revno, field)
	char const *revno;
	int field;
{
	struct buf t;
	bufautobegin(&t);
	rcserror("%s %s absent", field&1?"revision":"branch",
		partialno(&t,revno,field)
	);
	bufautoend(&t);
}


	int
compartial(num1, num2, length)
	char const *num1, *num2;
	int length;

/*   compare the first "length" fields of two dot numbers;
     the omitted field is considered to be larger than any number  */
/*   restriction:  at least one number has length or more fields   */

{
	register char const *s1, *s2;
	register size_t d1, d2;
	register int r;

        s1 = num1;      s2 = num2;
	if (!s1) return 1;
	if (!s2) return -1;

	for (;;) {
	    if (!*s1) return 1;
	    if (!*s2) return -1;

	    while (*s1=='0') ++s1; for (d1=0; isdigit(*(s1+d1)); d1++) continue;
	    while (*s2=='0') ++s2; for (d2=0; isdigit(*(s2+d2)); d2++) continue;

	    if (d1 != d2)
		    return d1<d2 ? -1 : 1;
	    if ((r = memcmp(s1, s2, d1)))
		    return r;
	    if (!--length)
		    return 0;

	    s1 += d1;
	    s2 += d1;

	    if (*s1 == '.') s1++;
            if (*s2 == '.') s2++;
	}
}


char * partialno(rev1,rev2,length)
	struct buf *rev1;
	char const *rev2;
	register int length;
/* Function: Copies length fields of revision number rev2 into rev1.
 * Return rev1's string.
 */
{
	register char *r1;

	bufscpy(rev1, rev2);
	r1 = rev1->string;
        while (length) {
		while (*r1!='.' && *r1)
			++r1;
		++r1;
                length--;
        }
        /* eliminate last '.'*/
        *(r1-1)='\0';
	return rev1->string;
}




	static void
store1(store, next)
	struct hshentries ***store;
	struct hshentry *next;
/*
 * Allocate a new list node that addresses NEXT.
 * Append it to the list that **STORE is the end pointer of.
 */
{
	register struct hshentries *p;

	p = ftalloc(struct hshentries);
	p->first = next;
	**store = p;
	*store = &p->rest;
}

struct hshentry * genrevs(revno,date,author,state,store)
	char const *revno, *date, *author, *state;
	struct hshentries **store;
/* Function: finds the deltas needed for reconstructing the
 * revision given by revno, date, author, and state, and stores pointers
 * to these deltas into a list whose starting address is given by store.
 * The last delta (target delta) is returned.
 * If the proper delta could not be found, 0 is returned.
 */
{
	int length;
        register struct hshentry * next;
        int result;
	char const *branchnum;
	struct buf t;
	char datebuf[datesize + zonelenmax];

	bufautobegin(&t);

	if (!(next = Head)) {
		rcserror("RCS file empty");
		goto norev;
        }

        length = countnumflds(revno);

        if (length >= 1) {
                /* at least one field; find branch exactly */
		while ((result=cmpnumfld(revno,next->num,1)) < 0) {
			store1(&store, next);
                        next = next->next;
			if (!next) {
			    rcserror("branch number %s too low", partialno(&t,revno,1));
			    goto norev;
			}
                }

		if (result>0) {
			absent(revno, 1);
			goto norev;
		}
        }
        if (length<=1){
                /* pick latest one on given branch */
                branchnum = next->num; /* works even for empty revno*/
		while (next &&
		       cmpnumfld(branchnum,next->num,1) == 0 &&
		       (
			(date && cmpdate(date,next->date) < 0) ||
			(author && strcmp(author,next->author) != 0) ||
			(state && strcmp(state,next->state) != 0)
		       )
		      )
		{
			store1(&store, next);
                        next=next->next;
                }
		if (!next ||
                    (cmpnumfld(branchnum,next->num,1)!=0))/*overshot*/ {
			cantfindbranch(
				length ? revno : partialno(&t,branchnum,1),
				date, author, state
			);
			goto norev;
                } else {
			store1(&store, next);
                }
		*store = 0;
                return next;
        }

        /* length >=2 */
        /* find revision; may go low if length==2*/
	while ((result=cmpnumfld(revno,next->num,2)) < 0  &&
               (cmpnumfld(revno,next->num,1)==0) ) {
		store1(&store, next);
                next = next->next;
		if (!next)
			break;
        }

	if (!next || cmpnumfld(revno,next->num,1) != 0) {
		rcserror("revision number %s too low", partialno(&t,revno,2));
		goto norev;
        }
        if ((length>2) && (result!=0)) {
		absent(revno, 2);
		goto norev;
        }

        /* print last one */
	store1(&store, next);

        if (length>2)
                return genbranch(next,revno,length,date,author,state,store);
        else { /* length == 2*/
		if (date && cmpdate(date,next->date)<0) {
			rcserror("Revision %s has date %s.",
				next->num,
				date2str(next->date, datebuf)
			);
			return 0;
		}
		if (author && strcmp(author,next->author)!=0) {
			rcserror("Revision %s has author %s.",
				next->num, next->author
			);
			return 0;
                }
		if (state && strcmp(state,next->state)!=0) {
			rcserror("Revision %s has state %s.",
				next->num,
				next->state ? next->state : "<empty>"
			);
			return 0;
                }
		*store = 0;
                return next;
        }

    norev:
	bufautoend(&t);
	return 0;
}




	static struct hshentry *
genbranch(bpoint, revno, length, date, author, state, store)
	struct hshentry const *bpoint;
	char const *revno;
	int length;
	char const *date, *author, *state;
	struct hshentries **store;
/* Function: given a branchpoint, a revision number, date, author, and state,
 * genbranch finds the deltas necessary to reconstruct the given revision
 * from the branch point on.
 * Pointers to the found deltas are stored in a list beginning with store.
 * revno must be on a side branch.
 * Return 0 on error.
 */
{
	int field;
        register struct hshentry * next, * trail;
	register struct branchhead const *bhead;
        int result;
	struct buf t;
	char datebuf[datesize + zonelenmax];

	field = 3;
        bhead = bpoint->branches;

	do {
		if (!bhead) {
			bufautobegin(&t);
			rcserror("no side branches present for %s",
				partialno(&t,revno,field-1)
			);
			bufautoend(&t);
			return 0;
		}

                /*find branch head*/
                /*branches are arranged in increasing order*/
		while (0 < (result=cmpnumfld(revno,bhead->hsh->num,field))) {
                        bhead = bhead->nextbranch;
			if (!bhead) {
			    bufautobegin(&t);
			    rcserror("branch number %s too high",
				partialno(&t,revno,field)
			    );
			    bufautoend(&t);
			    return 0;
			}
                }

		if (result<0) {
		    absent(revno, field);
		    return 0;
		}

                next = bhead->hsh;
                if (length==field) {
                        /* pick latest one on that branch */
			trail = 0;
			do { if ((!date || cmpdate(date,next->date)>=0) &&
				 (!author || strcmp(author,next->author)==0) &&
				 (!state || strcmp(state,next->state)==0)
                             ) trail = next;
                             next=next->next;
			} while (next);

			if (!trail) {
			     cantfindbranch(revno, date, author, state);
			     return 0;
                        } else { /* print up to last one suitable */
                             next = bhead->hsh;
                             while (next!=trail) {
				  store1(&store, next);
                                  next=next->next;
                             }
			     store1(&store, next);
                        }
			*store = 0;
                        return next;
                }

                /* length > field */
                /* find revision */
                /* check low */
                if (cmpnumfld(revno,next->num,field+1)<0) {
			bufautobegin(&t);
			rcserror("revision number %s too low",
				partialno(&t,revno,field+1)
			);
			bufautoend(&t);
			return 0;
                }
		do {
			store1(&store, next);
                        trail = next;
                        next = next->next;
		} while (next && cmpnumfld(revno,next->num,field+1)>=0);

                if ((length>field+1) &&  /*need exact hit */
                    (cmpnumfld(revno,trail->num,field+1) !=0)){
			absent(revno, field+1);
			return 0;
                }
                if (length == field+1) {
			if (date && cmpdate(date,trail->date)<0) {
				rcserror("Revision %s has date %s.",
					trail->num,
					date2str(trail->date, datebuf)
				);
				return 0;
                        }
			if (author && strcmp(author,trail->author)!=0) {
				rcserror("Revision %s has author %s.",
					trail->num, trail->author
				);
				return 0;
                        }
			if (state && strcmp(state,trail->state)!=0) {
				rcserror("Revision %s has state %s.",
					trail->num,
					trail->state ? trail->state : "<empty>"
				);
				return 0;
                        }
                }
                bhead = trail->branches;

	} while ((field+=2) <= length);
	*store = 0;
        return trail;
}


	static char const *
lookupsym(id)
	char const *id;
/* Function: looks up id in the list of symbolic names starting
 * with pointer SYMBOLS, and returns a pointer to the corresponding
 * revision number.  Return 0 if not present.
 */
{
	register struct assoc const *next;
	for (next = Symbols;  next;  next = next->nextassoc)
                if (strcmp(id, next->symbol)==0)
			return next->num;
	return 0;
}

int expandsym(source, target)
	char const *source;
	struct buf *target;
/* Function: Source points to a revision number. Expandsym copies
 * the number to target, but replaces all symbolic fields in the
 * source number with their numeric values.
 * Expand a branch followed by `.' to the latest revision on that branch.
 * Ignore `.' after a revision.  Remove leading zeros.
 * returns false on error;
 */
{
	return fexpandsym(source, target, (RILE*)0);
}

	int
fexpandsym(source, target, fp)
	char const *source;
	struct buf *target;
	RILE *fp;
/* Same as expandsym, except if FP is nonzero, it is used to expand KDELIM.  */
{
	register char const *sp, *bp;
	register char *tp;
	char const *tlim;
	int dots;

	sp = source;
	bufalloc(target, 1);
	tp = target->string;
	if (!sp || !*sp) { /* Accept 0 pointer as a legal value.  */
                *tp='\0';
                return true;
        }
	if (sp[0] == KDELIM  &&  !sp[1]) {
		if (!getoldkeys(fp))
			return false;
		if (!*prevrev.string) {
			workerror("working file lacks revision number");
			return false;
		}
		bufscpy(target, prevrev.string);
		return true;
	}
	tlim = tp + target->size;
	dots = 0;

	for (;;) {
		register char *p = tp;
		size_t s = tp - target->string;
		int id = false;
		for (;;) {
		    switch (ctab[(unsigned char)*sp]) {
			case IDCHAR:
			case LETTER:
			case Letter:
			    id = true;
			    /* fall into */
			case DIGIT:
			    if (tlim <= p)
				    p = bufenlarge(target, &tlim);
			    *p++ = *sp++;
			    continue;

			default:
			    break;
		    }
		    break;
		}
		if (tlim <= p)
			p = bufenlarge(target, &tlim);
		*p = 0;
		tp = target->string + s;

		if (id) {
			bp = lookupsym(tp);
			if (!bp) {
				rcserror("Symbolic name `%s' is undefined.",tp);
                                return false;
                        }
		} else {
			/* skip leading zeros */
			for (bp = tp;  *bp=='0' && isdigit(bp[1]);  bp++)
				continue;

			if (!*bp)
			    if (s || *sp!='.')
				break;
			    else {
				/* Insert default branch before initial `.'.  */
				char const *b;
				if (Dbranch)
				    b = Dbranch;
				else if (Head)
				    b = Head->num;
				else
				    break;
				getbranchno(b, target);
				bp = tp = target->string;
				tlim = tp + target->size;
			    }
		}

		while ((*tp++ = *bp++))
			if (tlim <= tp)
				tp = bufenlarge(target, &tlim);

		switch (*sp++) {
		    case '\0':
			return true;

		    case '.':
			if (!*sp) {
				if (dots & 1)
					break;
				if (!(bp = branchtip(target->string)))
					return false;
				bufscpy(target, bp);
				return true;
			}
			++dots;
			tp[-1] = '.';
			continue;
		}
		break;
        }

	rcserror("improper revision number: %s", source);
	return false;
}

	char const *
namedrev(name, delta)
	char const *name;
	struct hshentry *delta;
/* Yield NAME if it names DELTA, 0 otherwise.  */
{
	if (name) {
		char const *id = 0, *p, *val;
		for (p = name;  ;  p++)
			switch (ctab[(unsigned char)*p]) {
				case IDCHAR:
				case LETTER:
				case Letter:
					id = name;
					break;

				case DIGIT:
					break;

				case UNKN:
					if (!*p && id &&
						(val = lookupsym(id)) &&
						strcmp(val, delta->num) == 0
					)
						return id;
					/* fall into */
				default:
					return 0;
			}
	}
	return 0;
}

	static char const *
branchtip(branch)
	char const *branch;
{
	struct hshentry *h;
	struct hshentries *hs;

	h  =  genrevs(branch, (char*)0, (char*)0, (char*)0, &hs);
	return h ? h->num : (char const*)0;
}

	char const *
tiprev()
{
	return Dbranch ? branchtip(Dbranch) : Head ? Head->num : (char const*)0;
}



#ifdef REVTEST

/*
* Test the routines that generate a sequence of delta numbers
* needed to regenerate a given delta.
*/

char const cmdid[] = "revtest";

	int
main(argc,argv)
int argc; char * argv[];
{
	static struct buf numricrevno;
	char symrevno[100];       /* used for input of revision numbers */
        char author[20];
        char state[20];
        char date[20];
	struct hshentries *gendeltas;
        struct hshentry * target;
        int i;

        if (argc<2) {
		aputs("No input file\n",stderr);
		exitmain(EXIT_FAILURE);
        }
	if (!(finptr=Iopen(argv[1], FOPEN_R, (struct stat*)0))) {
		faterror("can't open input file %s", argv[1]);
        }
        Lexinit();
        getadmin();

        gettree();

        getdesc(false);

        do {
                /* all output goes to stderr, to have diagnostics and       */
                /* errors in sequence.                                      */
		aputs("\nEnter revision number or <return> or '.': ",stderr);
		if (!gets(symrevno)) break;
                if (*symrevno == '.') break;
		aprintf(stderr,"%s;\n",symrevno);
		expandsym(symrevno,&numricrevno);
		aprintf(stderr,"expanded number: %s; ",numricrevno.string);
		aprintf(stderr,"Date: ");
		gets(date); aprintf(stderr,"%s; ",date);
		aprintf(stderr,"Author: ");
		gets(author); aprintf(stderr,"%s; ",author);
		aprintf(stderr,"State: ");
		gets(state); aprintf(stderr, "%s;\n", state);
		target = genrevs(numricrevno.string, *date?date:(char *)0, *author?author:(char *)0,
				 *state?state:(char*)0, &gendeltas);
		if (target) {
			while (gendeltas) {
				aprintf(stderr,"%s\n",gendeltas->first->num);
				gendeltas = gendeltas->next;
                        }
                }
        } while (true);
	aprintf(stderr,"done\n");
	exitmain(EXIT_SUCCESS);
}

void exiterr() { _exit(EXIT_FAILURE); }

#endif
