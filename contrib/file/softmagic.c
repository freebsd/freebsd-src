/*
 * softmagic - interpret variable magic from MAGIC
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

#include "file.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <regex.h>


#ifndef	lint
FILE_RCSID("@(#)$Id: softmagic.c,v 1.54 2003/02/25 13:04:32 christos Exp $")
#endif	/* lint */

static int match(struct magic *, uint32_t, unsigned char *, int);
static int mget(union VALUETYPE *, unsigned char *, struct magic *, int);
static int mcheck(union VALUETYPE *, struct magic *);
static int32_t mprint(union VALUETYPE *, struct magic *);
static void mdebug(int32_t, char *, int);
static int mconvert(union VALUETYPE *, struct magic *);

extern int kflag;

/*
 * softmagic - lookup one file in database 
 * (already read from MAGIC by apprentice.c).
 * Passed the name and FILE * of one file to be typed.
 */
/*ARGSUSED1*/		/* nbytes passed for regularity, maybe need later */
int
softmagic(unsigned char *buf, int nbytes)
{
	struct mlist *ml;

	for (ml = mlist.next; ml != &mlist; ml = ml->next)
		if (match(ml->magic, ml->nmagic, buf, nbytes))
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
match(struct magic *magic, uint32_t nmagic, unsigned char *s, int nbytes)
{
	int magindex = 0;
	int cont_level = 0;
	int need_separator = 0;
	union VALUETYPE p;
	static int32_t *tmpoff = NULL;
	static size_t tmplen = 0;
	int32_t oldoff = 0;
	int returnval = 0; /* if a match is found it is set to 1*/
	int firstline = 1; /* a flag to print X\n  X\n- X */

	if (tmpoff == NULL)
		if ((tmpoff = (int32_t *) malloc(
		    (tmplen = 20) * sizeof(*tmpoff))) == NULL)
			error("out of memory\n");

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

		if (! firstline) { /* we found another match */
			/* put a newline and '-' to do some simple formatting*/
			printf("\n- ");
		}

		tmpoff[cont_level] = mprint(&p, &magic[magindex]);
		/*
		 * If we printed something, we'll need to print
		 * a blank before we print something else.
		 */
		if (magic[magindex].desc[0])
			need_separator = 1;
		/* and any continuations that match */
		if (++cont_level >= tmplen)
			if ((tmpoff = (int32_t *) realloc(tmpoff,
			    (tmplen += 20) * sizeof(*tmpoff))) == NULL)
				error("out of memory\n");
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
				if (magic[magindex].flag & OFFADD) {
					oldoff=magic[magindex].offset;
					magic[magindex].offset +=
					    tmpoff[cont_level-1];
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
					tmpoff[cont_level] =
					    mprint(&p, &magic[magindex]);
					if (magic[magindex].desc[0])
						need_separator = 1;

					/*
					 * If we see any continuations
					 * at a higher level,
					 * process them.
					 */
					if (++cont_level >= tmplen)
						if ((tmpoff = 
						    (int32_t *) realloc(tmpoff,
						    (tmplen += 20) 
						    * sizeof(*tmpoff))) == NULL)
							error("out of memory\n");
				}
				if (magic[magindex].flag & OFFADD) {
					 magic[magindex].offset = oldoff;
				}
			}
		}
		firstline = 0;
		returnval = 1;
		if (!kflag) {
			return 1; /* don't keep searching */
		}			
	}
	return returnval;  /* This is hit if -k is set or there is no match */
}

static int32_t
mprint(union VALUETYPE *p, struct magic *m)
{
	uint32_t v;
	int32_t t=0 ;


  	switch (m->type) {
  	case BYTE:
		v = signextend(m, p->b);
		(void) printf(m->desc, (unsigned char) v);
		t = m->offset + sizeof(char);
		break;

  	case SHORT:
  	case BESHORT:
  	case LESHORT:
		v = signextend(m, p->h);
		(void) printf(m->desc, (unsigned short) v);
		t = m->offset + sizeof(short);
		break;

  	case LONG:
  	case BELONG:
  	case LELONG:
		v = signextend(m, p->l);
		(void) printf(m->desc, (uint32_t) v);
		t = m->offset + sizeof(int32_t);
  		break;

  	case STRING:
  	case PSTRING:
		if (m->reln == '=') {
			(void) printf(m->desc, m->value.s);
			t = m->offset + strlen(m->value.s);
		}
		else {
			if (*m->value.s == '\0') {
				char *cp = strchr(p->s,'\n');
				if (cp)
					*cp = '\0';
			}
			(void) printf(m->desc, p->s);
			t = m->offset + strlen(p->s);
		}
		break;

	case DATE:
	case BEDATE:
	case LEDATE:
		(void) printf(m->desc, fmttime(p->l, 1));
		t = m->offset + sizeof(time_t);
		break;

	case LDATE:
	case BELDATE:
	case LELDATE:
		(void) printf(m->desc, fmttime(p->l, 0));
		t = m->offset + sizeof(time_t);
		break;
	case REGEX:
	  	(void) printf(m->desc, p->s);
		t = m->offset + strlen(p->s);
		break;

	default:
		error("invalid m->type (%d) in mprint().\n", m->type);
		/*NOTREACHED*/
	}
	return(t);
}

/*
 * Convert the byte order of the data we are looking at
 * While we're here, let's apply the mask operation
 * (unless you have a better idea)
 */
static int
mconvert(union VALUETYPE *p, struct magic *m)
{
	switch (m->type) {
	case BYTE:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->b &= m->mask;
				break;
			case OPOR:
				p->b |= m->mask;
				break;
			case OPXOR:
				p->b ^= m->mask;
				break;
			case OPADD:
				p->b += m->mask;
				break;
			case OPMINUS:
				p->b -= m->mask;
				break;
			case OPMULTIPLY:
				p->b *= m->mask;
				break;
			case OPDIVIDE:
				p->b /= m->mask;
				break;
			case OPMODULO:
				p->b %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->b = ~p->b;
		return 1;
	case SHORT:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->h &= m->mask;
				break;
			case OPOR:
				p->h |= m->mask;
				break;
			case OPXOR:
				p->h ^= m->mask;
				break;
			case OPADD:
				p->h += m->mask;
				break;
			case OPMINUS:
				p->h -= m->mask;
				break;
			case OPMULTIPLY:
				p->h *= m->mask;
				break;
			case OPDIVIDE:
				p->h /= m->mask;
				break;
			case OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->h = ~p->h;
		return 1;
	case LONG:
	case DATE:
	case LDATE:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->l &= m->mask;
				break;
			case OPOR:
				p->l |= m->mask;
				break;
			case OPXOR:
				p->l ^= m->mask;
				break;
			case OPADD:
				p->l += m->mask;
				break;
			case OPMINUS:
				p->l -= m->mask;
				break;
			case OPMULTIPLY:
				p->l *= m->mask;
				break;
			case OPDIVIDE:
				p->l /= m->mask;
				break;
			case OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->l = ~p->l;
		return 1;
	case STRING:
		{
			int n;

			/* Null terminate and eat *trailing* return */
			p->s[sizeof(p->s) - 1] = '\0';
			n = strlen(p->s) - 1;
			if (p->s[n] == '\n')
				p->s[n] = '\0';
			return 1;
		}
	case PSTRING:
		{
			char *ptr1 = p->s, *ptr2 = ptr1 + 1;
			int n = *p->s;
			if (n >= sizeof(p->s))
				n = sizeof(p->s) - 1;
			while (n--)
				*ptr1++ = *ptr2++;
			*ptr1 = '\0';
			n = strlen(p->s) - 1;
			if (p->s[n] == '\n')
				p->s[n] = '\0';
			return 1;
		}
	case BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->h &= m->mask;
				break;
			case OPOR:
				p->h |= m->mask;
				break;
			case OPXOR:
				p->h ^= m->mask;
				break;
			case OPADD:
				p->h += m->mask;
				break;
			case OPMINUS:
				p->h -= m->mask;
				break;
			case OPMULTIPLY:
				p->h *= m->mask;
				break;
			case OPDIVIDE:
				p->h /= m->mask;
				break;
			case OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->h = ~p->h;
		return 1;
	case BELONG:
	case BEDATE:
	case BELDATE:
		p->l = (int32_t)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->l &= m->mask;
				break;
			case OPOR:
				p->l |= m->mask;
				break;
			case OPXOR:
				p->l ^= m->mask;
				break;
			case OPADD:
				p->l += m->mask;
				break;
			case OPMINUS:
				p->l -= m->mask;
				break;
			case OPMULTIPLY:
				p->l *= m->mask;
				break;
			case OPDIVIDE:
				p->l /= m->mask;
				break;
			case OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->l = ~p->l;
		return 1;
	case LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->h &= m->mask;
				break;
			case OPOR:
				p->h |= m->mask;
				break;
			case OPXOR:
				p->h ^= m->mask;
				break;
			case OPADD:
				p->h += m->mask;
				break;
			case OPMINUS:
				p->h -= m->mask;
				break;
			case OPMULTIPLY:
				p->h *= m->mask;
				break;
			case OPDIVIDE:
				p->h /= m->mask;
				break;
			case OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->h = ~p->h;
		return 1;
	case LELONG:
	case LEDATE:
	case LELDATE:
		p->l = (int32_t)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case OPAND:
				p->l &= m->mask;
				break;
			case OPOR:
				p->l |= m->mask;
				break;
			case OPXOR:
				p->l ^= m->mask;
				break;
			case OPADD:
				p->l += m->mask;
				break;
			case OPMINUS:
				p->l -= m->mask;
				break;
			case OPMULTIPLY:
				p->l *= m->mask;
				break;
			case OPDIVIDE:
				p->l /= m->mask;
				break;
			case OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & OPINVERSE)
			p->l = ~p->l;
		return 1;
	case REGEX:
		return 1;
	default:
		error("invalid type %d in mconvert().\n", m->type);
		return 0;
	}
}


static void
mdebug(int32_t offset, char *str, int len)
{
	(void) fprintf(stderr, "mget @%d: ", offset);
	showstr(stderr, (char *) str, len);
	(void) fputc('\n', stderr);
	(void) fputc('\n', stderr);
}

static int
mget(union VALUETYPE *p, unsigned char *s, struct magic *m, int nbytes)
{
	int32_t offset = m->offset;

	if (m->type == REGEX) {
	      /*
	       * offset is interpreted as last line to search,
	       * (starting at 1), not as bytes-from start-of-file
	       */
	      unsigned char *last = NULL;
	      p->buf = (char *)s;
	      for (; offset && (s = (unsigned char *)strchr(s, '\n')) != NULL;
		  offset--, s++)
		    last = s;
	      if (last != NULL)
		*last = '\0';
	} else if (offset + sizeof(union VALUETYPE) <= nbytes)
		memcpy(p, s + offset, sizeof(union VALUETYPE));
	else {
		/*
		 * the usefulness of padding with zeroes eludes me, it
		 * might even cause problems
		 */
		int32_t have = nbytes - offset;
		memset(p, 0, sizeof(union VALUETYPE));
		if (have > 0)
			memcpy(p, s + offset, have);
	}

	if (debug) {
		mdebug(offset, (char *) p, sizeof(union VALUETYPE));
		mdump(m);
	}

	if (m->flag & INDIR) {
		switch (m->in_type) {
		case BYTE:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = p->b & m->in_offset;
					break;
				case OPOR:
					offset = p->b | m->in_offset;
					break;
				case OPXOR:
					offset = p->b ^ m->in_offset;
					break;
				case OPADD:
					offset = p->b + m->in_offset;
					break;
				case OPMINUS:
					offset = p->b - m->in_offset;
					break;
				case OPMULTIPLY:
					offset = p->b * m->in_offset;
					break;
				case OPDIVIDE:
					offset = p->b / m->in_offset;
					break;
				case OPMODULO:
					offset = p->b % m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case BESHORT:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) &
						 m->in_offset;
					break;
				case OPOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) |
						 m->in_offset;
					break;
				case OPXOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) ^
						 m->in_offset;
					break;
				case OPADD:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) +
						 m->in_offset;
					break;
				case OPMINUS:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) -
						 m->in_offset;
					break;
				case OPMULTIPLY:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) *
						 m->in_offset;
					break;
				case OPDIVIDE:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) /
						 m->in_offset;
					break;
				case OPMODULO:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) %
						 m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case LESHORT:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) &
						 m->in_offset;
					break;
				case OPOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) |
						 m->in_offset;
					break;
				case OPXOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) ^
						 m->in_offset;
					break;
				case OPADD:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) +
						 m->in_offset;
					break;
				case OPMINUS:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) -
						 m->in_offset;
					break;
				case OPMULTIPLY:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) *
						 m->in_offset;
					break;
				case OPDIVIDE:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) /
						 m->in_offset;
					break;
				case OPMODULO:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) %
						 m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case SHORT:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = p->h & m->in_offset;
					break;
				case OPOR:
					offset = p->h | m->in_offset;
					break;
				case OPXOR:
					offset = p->h ^ m->in_offset;
					break;
				case OPADD:
					offset = p->h + m->in_offset;
					break;
				case OPMINUS:
					offset = p->h - m->in_offset;
					break;
				case OPMULTIPLY:
					offset = p->h * m->in_offset;
					break;
				case OPDIVIDE:
					offset = p->h / m->in_offset;
					break;
				case OPMODULO:
					offset = p->h % m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case BELONG:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) &
						 m->in_offset;
					break;
				case OPOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) |
						 m->in_offset;
					break;
				case OPXOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) ^
						 m->in_offset;
					break;
				case OPADD:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) +
						 m->in_offset;
					break;
				case OPMINUS:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) -
						 m->in_offset;
					break;
				case OPMULTIPLY:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) *
						 m->in_offset;
					break;
				case OPDIVIDE:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) /
						 m->in_offset;
					break;
				case OPMODULO:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) %
						 m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case LELONG:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) &
						 m->in_offset;
					break;
				case OPOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) |
						 m->in_offset;
					break;
				case OPXOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) ^
						 m->in_offset;
					break;
				case OPADD:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) +
						 m->in_offset;
					break;
				case OPMINUS:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) -
						 m->in_offset;
					break;
				case OPMULTIPLY:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) *
						 m->in_offset;
					break;
				case OPDIVIDE:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) /
						 m->in_offset;
					break;
				case OPMODULO:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) %
						 m->in_offset;
					break;
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		case LONG:
			if (m->in_offset)
				switch (m->in_op&0x7F) {
				case OPAND:
					offset = p->l & m->in_offset;
					break;
				case OPOR:
					offset = p->l | m->in_offset;
					break;
				case OPXOR:
					offset = p->l ^ m->in_offset;
					break;
				case OPADD:
					offset = p->l + m->in_offset;
					break;
				case OPMINUS:
					offset = p->l - m->in_offset;
					break;
				case OPMULTIPLY:
					offset = p->l * m->in_offset;
					break;
				case OPDIVIDE:
					offset = p->l / m->in_offset;
					break;
				case OPMODULO:
					offset = p->l % m->in_offset;
					break;
			/*	case TOOMANYSWITCHBLOCKS:
			 *		ugh = p->eye % m->strain;
			 *		rub;
			 *	case BEER:
			 *		off = p->tab & m->in_gest;
			 *		sleep;
			 */
				}
			if (m->in_op & OPINVERSE)
				offset = ~offset;
			break;
		}

		if (offset + sizeof(union VALUETYPE) > nbytes)
			return 0;

		memcpy(p, s + offset, sizeof(union VALUETYPE));

		if (debug) {
			mdebug(offset, (char *) p, sizeof(union VALUETYPE));
			mdump(m);
		}
	}
	if (!mconvert(p, m))
	  return 0;
	return 1;
}

static int
mcheck(union VALUETYPE *p, struct magic *m)
{
	uint32_t l = m->value.l;
	uint32_t v;
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
	case LDATE:
	case BELDATE:
	case LELDATE:
		v = p->l;
		break;

	case STRING:
	case PSTRING:
	{
		/*
		 * What we want here is:
		 * v = strncmp(m->value.s, p->s, m->vallen);
		 * but ignoring any nulls.  bcmp doesn't give -/+/0
		 * and isn't universally available anyway.
		 */
		unsigned char *a = (unsigned char*)m->value.s;
		unsigned char *b = (unsigned char*)p->s;
		int len = m->vallen;
		l = 0;
		v = 0;
		if (0L == m->mask) { /* normal string: do it fast */
			while (--len >= 0)
				if ((v = *b++ - *a++) != '\0')
					break; 
		} else { /* combine the others */
			while (--len >= 0) {
				if ((m->mask & STRING_IGNORE_LOWERCASE) &&
				    islower(*a)) {
					if ((v = tolower(*b++) - *a++) != '\0')
						break;
				} else if ((m->mask & STRING_COMPACT_BLANK) && 
				    isspace(*a)) { 
					a++;
					if (isspace(*b++)) {
						while (isspace(*b))
							b++;
					} else {
						v = 1;
						break;
					}
				} else if (isspace(*a) &&
				    (m->mask & STRING_COMPACT_OPTIONAL_BLANK)) {
					a++;
					while (isspace(*b))
						b++;
				} else {
					if ((v = *b++ - *a++) != '\0')
						break;
				}
			}
		}
		break;
	}
	case REGEX:
	{
		int rc;
		regex_t rx;
		char errmsg[512];

		rc = regcomp(&rx, m->value.s, REG_EXTENDED|REG_NOSUB);
		if (rc) {
			regerror(rc, &rx, errmsg, sizeof(errmsg));
			error("regex error %d, (%s)\n", rc, errmsg);
		} else {
			rc = regexec(&rx, p->buf, 0, 0, 0);
			return !rc;
		}
	}
	default:
		error("invalid type %d in mcheck().\n", m->type);
		return 0;/*NOTREACHED*/
	}

	if(m->type != STRING && m->type != PSTRING)
		v = signextend(m, v);

	switch (m->reln) {
	case 'x':
		if (debug)
			(void) fprintf(stderr, "%u == *any* = 1\n", v);
		matched = 1;
		break;

	case '!':
		matched = v != l;
		if (debug)
			(void) fprintf(stderr, "%u != %u = %d\n",
				       v, l, matched);
		break;

	case '=':
		matched = v == l;
		if (debug)
			(void) fprintf(stderr, "%u == %u = %d\n",
				       v, l, matched);
		break;

	case '>':
		if (m->flag & UNSIGNED) {
			matched = v > l;
			if (debug)
				(void) fprintf(stderr, "%u > %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v > (int32_t) l;
			if (debug)
				(void) fprintf(stderr, "%d > %d = %d\n",
					       v, l, matched);
		}
		break;

	case '<':
		if (m->flag & UNSIGNED) {
			matched = v < l;
			if (debug)
				(void) fprintf(stderr, "%u < %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v < (int32_t) l;
			if (debug)
				(void) fprintf(stderr, "%d < %d = %d\n",
					       v, l, matched);
		}
		break;

	case '&':
		matched = (v & l) == l;
		if (debug)
			(void) fprintf(stderr, "((%x & %x) == %x) = %d\n",
				       v, l, l, matched);
		break;

	case '^':
		matched = (v & l) != l;
		if (debug)
			(void) fprintf(stderr, "((%x & %x) != %x) = %d\n",
				       v, l, l, matched);
		break;

	default:
		matched = 0;
		error("mcheck: can't happen: invalid relation %d.\n", m->reln);
		break;/*NOTREACHED*/
	}

	return matched;
}
