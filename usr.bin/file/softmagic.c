/*
 * softmagic - interpret variable magic from /etc/magic
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "file.h"

#ifndef	lint
static char *moduleid =
	"@(#)$Id: softmagic.c,v 1.3 1995/05/30 06:30:09 rgrimes Exp $";
#endif	/* lint */

static int match	__P((unsigned char *, int));
static int mget		__P((union VALUETYPE *,
			     unsigned char *, struct magic *, int));
static int mcheck	__P((union VALUETYPE *, struct magic *));
static void mprint	__P((union VALUETYPE *, struct magic *));
static void mdebug	__P((long, char *, int));
static int mconvert	__P((union VALUETYPE *, struct magic *));

/*
 * softmagic - lookup one file in database
 * (already read from /etc/magic by apprentice.c).
 * Passed the name and FILE * of one file to be typed.
 */
/*ARGSUSED1*/		/* nbytes passed for regularity, maybe need later */
int
softmagic(buf, nbytes)
unsigned char *buf;
int nbytes;
{
	if (match(buf, nbytes))
		return 1;

	return 0;
}

/*
 * Go through the whole list, stopping if you find a match.  Process all
 * the continuations of that match before returning.
 *
 * We support multi-level continuations:
 *
 *	At any time when processing a successful top-level match, there is a
 *	current continuation level; it represents the level of the last
 *	successfully matched continuation.
 *
 *	Continuations above that level are skipped as, if we see one, it
 *	means that the continuation that controls them - i.e, the
 *	lower-level continuation preceding them - failed to match.
 *
 *	Continuations below that level are processed as, if we see one,
 *	it means we've finished processing or skipping higher-level
 *	continuations under the control of a successful or unsuccessful
 *	lower-level continuation, and are now seeing the next lower-level
 *	continuation and should process it.  The current continuation
 *	level reverts to the level of the one we're seeing.
 *
 *	Continuations at the current level are processed as, if we see
 *	one, there's no lower-level continuation that may have failed.
 *
 *	If a continuation matches, we bump the current continuation level
 *	so that higher-level continuations are processed.
 */
static int
match(s, nbytes)
unsigned char	*s;
int nbytes;
{
	int magindex = 0;
	int cont_level = 0;
	int need_separator = 0;
	union VALUETYPE p;

	for (magindex = 0; magindex < nmagic; magindex++) {
		/* if main entry matches, print it... */
		if (!mget(&p, s, &magic[magindex], nbytes) ||
		    !mcheck(&p, &magic[magindex])) {
			    /*
			     * main entry didn't match,
			     * flush its continuations
			     */
			    while (magindex < nmagic &&
			    	   magic[magindex + 1].cont_level != 0)
			    	   magindex++;
			    continue;
		}

		mprint(&p, &magic[magindex]);
		/*
		 * If we printed something, we'll need to print
		 * a blank before we print something else.
		 */
		if (magic[magindex].desc[0])
			need_separator = 1;
		/* and any continuations that match */
		cont_level++;
		while (magic[magindex+1].cont_level != 0 &&
		       ++magindex < nmagic) {
			if (cont_level >= magic[magindex].cont_level) {
				if (cont_level > magic[magindex].cont_level) {
					/*
					 * We're at the end of the level
					 * "cont_level" continuations.
					 */
					cont_level = magic[magindex].cont_level;
				}
				if (mget(&p, s, &magic[magindex], nbytes) &&
				    mcheck(&p, &magic[magindex])) {
					/*
					 * This continuation matched.
					 * Print its message, with
					 * a blank before it if
					 * the previous item printed
					 * and this item isn't empty.
					 */
					/* space if previous printed */
					if (need_separator
					   && (magic[magindex].nospflag == 0)
					   && (magic[magindex].desc[0] != '\0')
					   ) {
						(void) putchar(' ');
						need_separator = 0;
					}
					mprint(&p, &magic[magindex]);
					if (magic[magindex].desc[0])
						need_separator = 1;

					/*
					 * If we see any continuations
					 * at a higher level,
					 * process them.
					 */
					cont_level++;
				}
			}
		}
		return 1;		/* all through */
	}
	return 0;			/* no match at all */
}

static void
mprint(p, m)
union VALUETYPE *p;
struct magic *m;
{
	char *pp, *rt;
	unsigned long v;


  	switch (m->type) {
  	case BYTE:
		v = p->b;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (unsigned char) v);
		break;

  	case SHORT:
  	case BESHORT:
  	case LESHORT:
		v = p->h;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (unsigned short) v);
		break;

  	case LONG:
  	case BELONG:
  	case LELONG:
		v = p->l;
		v = signextend(m, v) & m->mask;
		(void) printf(m->desc, (unsigned long) v);
  		break;

  	case STRING:
		if (m->reln == '=') {
			(void) printf(m->desc, m->value.s);
		}
		else {
			(void) printf(m->desc, p->s);
		}
		return;

	case DATE:
	case BEDATE:
	case LEDATE:
		pp = ctime((time_t*) &p->l);
		if ((rt = strchr(pp, '\n')) != NULL)
			*rt = '\0';
		(void) printf(m->desc, pp);
		return;
	default:
		error("invalid m->type (%d) in mprint().\n", m->type);
		/*NOTREACHED*/
	}
}

/*
 * Convert the byte order of the data we are looking at
 */
static int
mconvert(p, m)
union VALUETYPE *p;
struct magic *m;
{
	char *rt;

	switch (m->type) {
	case BYTE:
	case SHORT:
	case LONG:
	case DATE:
		return 1;
	case STRING:
		/* Null terminate and eat the return */
		p->s[sizeof(p->s) - 1] = '\0';
		if ((rt = strchr(p->s, '\n')) != NULL)
			*rt = '\0';
		return 1;
	case BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		return 1;
	case BELONG:
	case BEDATE:
		p->l = (long)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		return 1;
	case LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		return 1;
	case LELONG:
	case LEDATE:
		p->l = (long)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		return 1;
	default:
		error("invalid type %d in mconvert().\n", m->type);
		return 0;
	}
}


static void
mdebug(offset, str, len)
long offset;
char *str;
int len;
{
	(void) fprintf(stderr, "mget @%ld: ", offset);
	showstr(stderr, (char *) str, len);
	(void) fputc('\n', stderr);
	(void) fputc('\n', stderr);
}

static int
mget(p, s, m, nbytes)
union VALUETYPE* p;
unsigned char	*s;
struct magic *m;
int nbytes;
{
	long offset = m->offset;

	if (offset + sizeof(union VALUETYPE) <= nbytes)
		memcpy(p, s + offset, sizeof(union VALUETYPE));
	else {
		/*
		 * the usefulness of padding with zeroes eludes me, it
		 * might even cause problems
		 */
		long have = nbytes - offset;
		memset(p, 0, sizeof(union VALUETYPE));
		if (have > 0)
			memcpy(p, s + offset, have);
	}


	if (debug) {
		mdebug(offset, (char *) p, sizeof(union VALUETYPE));
		mdump(m);
	}

	if (!mconvert(p, m))
		return 0;

	if (m->flag & INDIR) {

		switch (m->in.type) {
		case BYTE:
			offset = p->b + m->in.offset;
			break;
		case SHORT:
			offset = p->h + m->in.offset;
			break;
		case LONG:
			offset = p->l + m->in.offset;
			break;
		}

		if (offset + sizeof(union VALUETYPE) > nbytes)
			return 0;

		memcpy(p, s + offset, sizeof(union VALUETYPE));

		if (debug) {
			mdebug(offset, (char *) p, sizeof(union VALUETYPE));
			mdump(m);
		}

		if (!mconvert(p, m))
			return 0;
	}
	return 1;
}

static int
mcheck(p, m)
union VALUETYPE* p;
struct magic *m;
{
	register unsigned long l = m->value.l;
	register unsigned long v;
	int matched;

	if ( (m->value.s[0] == 'x') && (m->value.s[1] == '\0') ) {
		fprintf(stderr, "BOINK");
		return 1;
	}


	switch (m->type) {
	case BYTE:
		v = p->b;
		break;

	case SHORT:
	case BESHORT:
	case LESHORT:
		v = p->h;
		break;

	case LONG:
	case BELONG:
	case LELONG:
	case DATE:
	case BEDATE:
	case LEDATE:
		v = p->l;
		break;

	case STRING:
		l = 0;
		/* What we want here is:
		 * v = strncmp(m->value.s, p->s, m->vallen);
		 * but ignoring any nulls.  bcmp doesn't give -/+/0
		 * and isn't universally available anyway.
		 */
		v = 0;
		{
			register unsigned char *a = (unsigned char*)m->value.s;
			register unsigned char *b = (unsigned char*)p->s;
			register int len = m->vallen;

			while (--len >= 0)
				if ((v = *b++ - *a++) != 0)
					break;
		}
		break;
	default:
		error("invalid type %d in mcheck().\n", m->type);
		return 0;/*NOTREACHED*/
	}

	v = signextend(m, v) & m->mask;

	switch (m->reln) {
	case 'x':
		if (debug)
			(void) fprintf(stderr, "%lu == *any* = 1\n", v);
		matched = 1;
		break;

	case '!':
		matched = v != l;
		if (debug)
			(void) fprintf(stderr, "%lu != %lu = %d\n",
				       v, l, matched);
		break;

	case '=':
		matched = v == l;
		if (debug)
			(void) fprintf(stderr, "%lu == %lu = %d\n",
				       v, l, matched);
		break;

	case '>':
		if (m->flag & UNSIGNED) {
			matched = v > l;
			if (debug)
				(void) fprintf(stderr, "%lu > %lu = %d\n",
					       v, l, matched);
		}
		else {
			matched = (long) v > (long) l;
			if (debug)
				(void) fprintf(stderr, "%ld > %ld = %d\n",
					       v, l, matched);
		}
		break;

	case '<':
		if (m->flag & UNSIGNED) {
			matched = v < l;
			if (debug)
				(void) fprintf(stderr, "%lu < %lu = %d\n",
					       v, l, matched);
		}
		else {
			matched = (long) v < (long) l;
			if (debug)
				(void) fprintf(stderr, "%ld < %ld = %d\n",
					       v, l, matched);
		}
		break;

	case '&':
		matched = (v & l) == l;
		if (debug)
			(void) fprintf(stderr, "((%lx & %lx) == %lx) = %d\n",
				       v, l, l, matched);
		break;

	case '^':
		matched = (v & l) != l;
		if (debug)
			(void) fprintf(stderr, "((%lx & %lx) != %lx) = %d\n",
				       v, l, l, matched);
		break;

	default:
		matched = 0;
		error("mcheck: can't happen: invalid relation %d.\n", m->reln);
		break;/*NOTREACHED*/
	}

	return matched;
}
