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
	"@(#)softmagic.c,v 1.2 1993/06/10 00:38:18 jtc Exp";
#endif	/* lint */

static int match	__P((unsigned char *));
static int mcheck	__P((unsigned char	*, struct magic *));
static void mprint	__P((struct magic *, unsigned char *));

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
	if (match(buf))
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
match(s)
unsigned char	*s;
{
	int magindex = 0;
	int cont_level = 0;
	int need_separator = 0;

	while (magindex < nmagic) {
		/* if main entry matches, print it... */
		if (mcheck(s, &magic[magindex])) {
			mprint(&magic[magindex],s);
			/*
			 * If we printed something, we'll need to print
			 * a blank before we print something else.
			 */
			if (magic[magindex].desc[0])
				need_separator = 1;
			/* and any continuations that match */
			cont_level++;
			while (magic[magindex+1].cont_level != 0 &&
				magindex < nmagic) {
				++magindex;
				if (cont_level >=
				    magic[magindex].cont_level) {
					if (cont_level >
					    magic[magindex].cont_level) {
						/*
						 * We're at the end of the
						 * level-"cont_level"
						 * continuations.
						 */
						cont_level = 
						  magic[magindex].cont_level;
					}
					if (mcheck(s, &magic[magindex])) {
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
						mprint(&magic[magindex],s);
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
		} else {
			/* main entry didn't match, flush its continuation */
			while (magic[magindex+1].cont_level != 0 &&
				magindex < nmagic) {
				++magindex;
			}
		}
		++magindex;			/* on to the next */
	}
	return 0;				/* no match at all */
}

static void
mprint(m, s)
struct magic *m;
unsigned char *s;
{
	register union VALUETYPE *p = (union VALUETYPE *)(s+m->offset);
	char *pp, *rt;

	/* correct byte order dependancies */
	switch (m->type) {
	case BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		break;
	case BELONG:
	case BEDATE:
		p->l = (long)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		break;
	case LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		break;
	case LELONG:
	case LEDATE:
		p->l = (long)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		break;
	}

  	switch (m->type) {
  	case BYTE:
 		(void) printf(m->desc,
 			      (m->reln & MASK) ? p->b & m->mask : p->b);
  		break;
  	case SHORT:
  	case BESHORT:
  	case LESHORT:
 		(void) printf(m->desc,
 			      (m->reln & MASK) ? p->h & m->mask : p->h);
  		break;
  	case LONG:
  	case BELONG:
  	case LELONG:
 		(void) printf(m->desc,
 			      (m->reln & MASK) ? p->l & m->mask : p->l);
  		break;
  	case STRING:
		if ((rt=strchr(p->s, '\n')) != NULL)
			*rt = '\0';
		(void) printf(m->desc, p->s);
		if (rt)
			*rt = '\n';
		break;
	case DATE:
	case BEDATE:
	case LEDATE:
		pp = ctime((time_t*) &p->l);
		if ((rt = strchr(pp, '\n')) != NULL)
			*rt = '\0';
		(void) printf(m->desc, pp);
		if (rt)
			*rt = '\n';
		break;
	default:
		error("invalid m->type (%d) in mprint().\n", m->type);
		/*NOTREACHED*/
	}
}

static int
mcheck(s, m)
unsigned char	*s;
struct magic *m;
{
	register union VALUETYPE *p = (union VALUETYPE *)(s+m->offset);
	register long l = m->value.l;
	register long mask = m->mask;
	register long v;

	if (debug) {
		(void) printf("mcheck: %10.10s ", s);
		mdump(m);
	}

	if ( (m->value.s[0] == 'x') && (m->value.s[1] == '\0') ) {
		printf("BOINK");
		return 1;
	}

	switch (m->type) {
	case BYTE:
		v = p->b; break;
	case SHORT:
		v = p->h; break;
	case LONG:
	case DATE:
		v = p->l; break;
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
	case BESHORT:
		v = (short)((p->hs[0]<<8)|(p->hs[1]));
		break;
	case BELONG:
	case BEDATE:
		v = (long)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		break;
	case LESHORT:
		v = (short)((p->hs[1]<<8)|(p->hs[0]));
		break;
	case LELONG:
	case LEDATE:
		v = (long)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		break;
	default:
		error("invalid type %d in mcheck().\n", m->type);
		return -1;/*NOTREACHED*/
	}

	if (m->mask != 0L)
		v &= m->mask;

	switch (m->reln) {
	case 'x':
		return 1;
	case '!':
		return v != l;
	case '=':
		return v == l;
	case '>':
		return v > l;
	case '<':
		return v < l;
	case '&':
		return (v & l) == l;
	case '^':
		return (v & l) != l;
	case MASK | '=':
		return (v & mask) == l;
	case MASK | '>':
		return (v & mask) > l;
	case MASK | '<':
		return (v & mask) < l;
	default:
		error("mcheck: can't happen: invalid relation %d.\n", m->reln);
		return -1;/*NOTREACHED*/
	}
}
