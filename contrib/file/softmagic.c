/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ian F. Darwin and others.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * softmagic - interpret variable magic from MAGIC
 */

#include "file.h"
#include "magic.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <regex.h>


#ifndef	lint
FILE_RCSID("@(#)$Id: softmagic.c,v 1.66 2004/07/24 20:38:56 christos Exp $")
#endif	/* lint */

private int match(struct magic_set *, struct magic *, uint32_t,
    const unsigned char *, size_t);
private int mget(struct magic_set *, union VALUETYPE *, const unsigned char *,
    struct magic *, size_t);
private int mcheck(struct magic_set *, union VALUETYPE *, struct magic *);
private int32_t mprint(struct magic_set *, union VALUETYPE *, struct magic *);
private void mdebug(uint32_t, const char *, size_t);
private int mconvert(struct magic_set *, union VALUETYPE *, struct magic *);
private int check_mem(struct magic_set *, unsigned int);

/*
 * softmagic - lookup one file in database 
 * (already read from MAGIC by apprentice.c).
 * Passed the name and FILE * of one file to be typed.
 */
/*ARGSUSED1*/		/* nbytes passed for regularity, maybe need later */
protected int
file_softmagic(struct magic_set *ms, const unsigned char *buf, size_t nbytes)
{
	struct mlist *ml;
	for (ml = ms->mlist->next; ml != ms->mlist; ml = ml->next)
		if (match(ms, ml->magic, ml->nmagic, buf, nbytes))
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
private int
match(struct magic_set *ms, struct magic *magic, uint32_t nmagic,
    const unsigned char *s, size_t nbytes)
{
	uint32_t magindex = 0;
	unsigned int cont_level = 0;
	int need_separator = 0;
	union VALUETYPE p;
	int32_t oldoff = 0;
	int returnval = 0; /* if a match is found it is set to 1*/
	int firstline = 1; /* a flag to print X\n  X\n- X */

	if (check_mem(ms, cont_level) == -1)
		return -1;

	for (magindex = 0; magindex < nmagic; magindex++) {
		/* if main entry matches, print it... */
		int flush = !mget(ms, &p, s, &magic[magindex], nbytes);
		switch (mcheck(ms, &p, &magic[magindex])) {
		case -1:
			return -1;
		case 0:
			flush++;
			break;
		default:
			break;
		}
		if (flush) {
			/* 
			 * main entry didn't match,
			 * flush its continuations
			 */
			while (magindex < nmagic - 1 &&
			       magic[magindex + 1].cont_level != 0)
			       magindex++;
			continue;
		}

		if (!firstline) { /* we found another match */
			/* put a newline and '-' to do some simple formatting*/
			if (file_printf(ms, "\n- ") == -1)
				return -1;
		}

		if ((ms->c.off[cont_level] = mprint(ms, &p, &magic[magindex]))
		    == -1)
			return -1;
		/*
		 * If we printed something, we'll need to print
		 * a blank before we print something else.
		 */
		if (magic[magindex].desc[0])
			need_separator = 1;
		/* and any continuations that match */
		if (check_mem(ms, ++cont_level) == -1)
			return -1;

		while (magic[magindex+1].cont_level != 0 && 
		       ++magindex < nmagic) {
			if (cont_level < magic[magindex].cont_level)
				continue;
			if (cont_level > magic[magindex].cont_level) {
				/*
				 * We're at the end of the level
				 * "cont_level" continuations.
				 */
				cont_level = magic[magindex].cont_level;
			}
			if (magic[magindex].flag & OFFADD) {
				oldoff=magic[magindex].offset;
				magic[magindex].offset += ms->c.off[cont_level-1];
			}
			if (!mget(ms, &p, s, &magic[magindex], nbytes))
				goto done;
				
			switch (mcheck(ms, &p, &magic[magindex])) {
			case -1:
				return -1;
			case 0:
				break;
			default:
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
				   && (magic[magindex].desc[0] != '\0')) {
					if (file_printf(ms, " ") == -1)
						return -1;
					need_separator = 0;
				}
				if ((ms->c.off[cont_level] = mprint(ms, &p,
				    &magic[magindex])) == -1)
					return -1;
				if (magic[magindex].desc[0])
					need_separator = 1;

				/*
				 * If we see any continuations
				 * at a higher level,
				 * process them.
				 */
				if (check_mem(ms, ++cont_level) == -1)
					return -1;
			}
done:
			if (magic[magindex].flag & OFFADD) {
				 magic[magindex].offset = oldoff;
			}
		}
		firstline = 0;
		returnval = 1;
		if ((ms->flags & MAGIC_CONTINUE) == 0) {
			return 1; /* don't keep searching */
		}			
	}
	return returnval;  /* This is hit if -k is set or there is no match */
}

private int
check_mem(struct magic_set *ms, unsigned int level)
{
	size_t len;

	if (level < ms->c.len)
		return 0;

	len = (ms->c.len += 20) * sizeof(*ms->c.off);
	ms->c.off = (ms->c.off == NULL) ? malloc(len) : realloc(ms->c.off, len);
	if (ms->c.off != NULL)
		return 0;
	file_oomem(ms);
	return -1;
}

private int32_t
mprint(struct magic_set *ms, union VALUETYPE *p, struct magic *m)
{
	uint32_t v;
	int32_t t=0 ;


  	switch (m->type) {
  	case FILE_BYTE:
		v = file_signextend(ms, m, (size_t)p->b);
		if (file_printf(ms, m->desc, (unsigned char) v) == -1)
			return -1;
		t = m->offset + sizeof(char);
		break;

  	case FILE_SHORT:
  	case FILE_BESHORT:
  	case FILE_LESHORT:
		v = file_signextend(ms, m, (size_t)p->h);
		if (file_printf(ms, m->desc, (unsigned short) v) == -1)
			return -1;
		t = m->offset + sizeof(short);
		break;

  	case FILE_LONG:
  	case FILE_BELONG:
  	case FILE_LELONG:
		v = file_signextend(ms, m, p->l);
		if (file_printf(ms, m->desc, (uint32_t) v) == -1)
			return -1;
		t = m->offset + sizeof(int32_t);
  		break;

  	case FILE_STRING:
  	case FILE_PSTRING:
		if (m->reln == '=') {
			if (file_printf(ms, m->desc, m->value.s) == -1)
				return -1;
			t = m->offset + strlen(m->value.s);
		}
		else {
			if (*m->value.s == '\0') {
				char *cp = strchr(p->s,'\n');
				if (cp)
					*cp = '\0';
			}
			if (file_printf(ms, m->desc, p->s) == -1)
				return -1;
			t = m->offset + strlen(p->s);
		}
		break;

	case FILE_DATE:
	case FILE_BEDATE:
	case FILE_LEDATE:
		if (file_printf(ms, m->desc, file_fmttime(p->l, 1)) == -1)
			return -1;
		t = m->offset + sizeof(time_t);
		break;

	case FILE_LDATE:
	case FILE_BELDATE:
	case FILE_LELDATE:
		if (file_printf(ms, m->desc, file_fmttime(p->l, 0)) == -1)
			return -1;
		t = m->offset + sizeof(time_t);
		break;
	case FILE_REGEX:
	  	if (file_printf(ms, m->desc, p->s) == -1)
			return -1;
		t = m->offset + strlen(p->s);
		break;

	default:
		file_error(ms, 0, "invalid m->type (%d) in mprint()", m->type);
		return -1;
	}
	return(t);
}

/*
 * Convert the byte order of the data we are looking at
 * While we're here, let's apply the mask operation
 * (unless you have a better idea)
 */
private int
mconvert(struct magic_set *ms, union VALUETYPE *p, struct magic *m)
{
	switch (m->type) {
	case FILE_BYTE:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->b &= m->mask;
				break;
			case FILE_OPOR:
				p->b |= m->mask;
				break;
			case FILE_OPXOR:
				p->b ^= m->mask;
				break;
			case FILE_OPADD:
				p->b += m->mask;
				break;
			case FILE_OPMINUS:
				p->b -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->b *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->b /= m->mask;
				break;
			case FILE_OPMODULO:
				p->b %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->b = ~p->b;
		return 1;
	case FILE_SHORT:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->h &= m->mask;
				break;
			case FILE_OPOR:
				p->h |= m->mask;
				break;
			case FILE_OPXOR:
				p->h ^= m->mask;
				break;
			case FILE_OPADD:
				p->h += m->mask;
				break;
			case FILE_OPMINUS:
				p->h -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->h *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->h /= m->mask;
				break;
			case FILE_OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->h = ~p->h;
		return 1;
	case FILE_LONG:
	case FILE_DATE:
	case FILE_LDATE:
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->l &= m->mask;
				break;
			case FILE_OPOR:
				p->l |= m->mask;
				break;
			case FILE_OPXOR:
				p->l ^= m->mask;
				break;
			case FILE_OPADD:
				p->l += m->mask;
				break;
			case FILE_OPMINUS:
				p->l -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->l *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->l /= m->mask;
				break;
			case FILE_OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->l = ~p->l;
		return 1;
	case FILE_STRING:
		{
			int n;

			/* Null terminate and eat *trailing* return */
			p->s[sizeof(p->s) - 1] = '\0';
			n = strlen(p->s) - 1;
			if (p->s[n] == '\n')
				p->s[n] = '\0';
			return 1;
		}
	case FILE_PSTRING:
		{
			char *ptr1 = p->s, *ptr2 = ptr1 + 1;
			unsigned int n = *p->s;
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
	case FILE_BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->h &= m->mask;
				break;
			case FILE_OPOR:
				p->h |= m->mask;
				break;
			case FILE_OPXOR:
				p->h ^= m->mask;
				break;
			case FILE_OPADD:
				p->h += m->mask;
				break;
			case FILE_OPMINUS:
				p->h -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->h *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->h /= m->mask;
				break;
			case FILE_OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->h = ~p->h;
		return 1;
	case FILE_BELONG:
	case FILE_BEDATE:
	case FILE_BELDATE:
		p->l = (int32_t)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->l &= m->mask;
				break;
			case FILE_OPOR:
				p->l |= m->mask;
				break;
			case FILE_OPXOR:
				p->l ^= m->mask;
				break;
			case FILE_OPADD:
				p->l += m->mask;
				break;
			case FILE_OPMINUS:
				p->l -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->l *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->l /= m->mask;
				break;
			case FILE_OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->l = ~p->l;
		return 1;
	case FILE_LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->h &= m->mask;
				break;
			case FILE_OPOR:
				p->h |= m->mask;
				break;
			case FILE_OPXOR:
				p->h ^= m->mask;
				break;
			case FILE_OPADD:
				p->h += m->mask;
				break;
			case FILE_OPMINUS:
				p->h -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->h *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->h /= m->mask;
				break;
			case FILE_OPMODULO:
				p->h %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->h = ~p->h;
		return 1;
	case FILE_LELONG:
	case FILE_LEDATE:
	case FILE_LELDATE:
		p->l = (int32_t)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		if (m->mask)
			switch (m->mask_op&0x7F) {
			case FILE_OPAND:
				p->l &= m->mask;
				break;
			case FILE_OPOR:
				p->l |= m->mask;
				break;
			case FILE_OPXOR:
				p->l ^= m->mask;
				break;
			case FILE_OPADD:
				p->l += m->mask;
				break;
			case FILE_OPMINUS:
				p->l -= m->mask;
				break;
			case FILE_OPMULTIPLY:
				p->l *= m->mask;
				break;
			case FILE_OPDIVIDE:
				p->l /= m->mask;
				break;
			case FILE_OPMODULO:
				p->l %= m->mask;
				break;
			}
		if (m->mask_op & FILE_OPINVERSE)
			p->l = ~p->l;
		return 1;
	case FILE_REGEX:
		return 1;
	default:
		file_error(ms, 0, "invalid type %d in mconvert()", m->type);
		return 0;
	}
}


private void
mdebug(uint32_t offset, const char *str, size_t len)
{
	(void) fprintf(stderr, "mget @%d: ", offset);
	file_showstr(stderr, str, len);
	(void) fputc('\n', stderr);
	(void) fputc('\n', stderr);
}

private int
mget(struct magic_set *ms, union VALUETYPE *p, const unsigned char *s,
    struct magic *m, size_t nbytes)
{
	uint32_t offset = m->offset;

	if (m->type == FILE_REGEX) {
		/*
		 * offset is interpreted as last line to search,
		 * (starting at 1), not as bytes-from start-of-file
		 */
		unsigned char *b, *last = NULL;
		if ((p->buf = strdup((const char *)s)) == NULL) {
			file_oomem(ms);
			return -1;
		}
		for (b = (unsigned char *)p->buf; offset &&
		    (b = (unsigned char *)strchr((char *)b, '\n')) != NULL;
		    offset--, s++)
			last = b;
		if (last != NULL)
			*last = '\0';
	} else if (offset + sizeof(union VALUETYPE) <= nbytes)
		memcpy(p, s + offset, sizeof(union VALUETYPE));
	else {
		/*
		 * the usefulness of padding with zeroes eludes me, it
		 * might even cause problems
		 */
		memset(p, 0, sizeof(union VALUETYPE));
		if (offset < nbytes)
			memcpy(p, s + offset, nbytes - offset);
	}

	/* Verify we have enough data to match magic type */
	switch (m->type) {
		case FILE_BYTE:
			if (nbytes < (offset + 1)) /* should alway be true */
				return 0;
			break;

		case FILE_SHORT:
		case FILE_BESHORT:
		case FILE_LESHORT:
			if (nbytes < (offset + 2))
				return 0;
			break;

		case FILE_LONG:
		case FILE_BELONG:
		case FILE_LELONG:
		case FILE_DATE:
		case FILE_BEDATE:
		case FILE_LEDATE:
		case FILE_LDATE:
		case FILE_BELDATE:
		case FILE_LELDATE:
			if (nbytes < (offset + 4))
				return 0;
			break;

		case FILE_STRING:
		case FILE_PSTRING:
			if (nbytes < (offset + m->vallen))
				return 0;
			break;
	}

	if ((ms->flags & MAGIC_DEBUG) != 0) {
		mdebug(offset, (char *)(void *)p, sizeof(union VALUETYPE));
		file_mdump(m);
	}

	if (m->flag & INDIR) {
		switch (m->in_type) {
		case FILE_BYTE:
			if (m->in_offset) {
				switch (m->in_op&0x7F) {
				case FILE_OPAND:
					offset = p->b & m->in_offset;
					break;
				case FILE_OPOR:
					offset = p->b | m->in_offset;
					break;
				case FILE_OPXOR:
					offset = p->b ^ m->in_offset;
					break;
				case FILE_OPADD:
					offset = p->b + m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = p->b - m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = p->b * m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = p->b / m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = p->b % m->in_offset;
					break;
				}
			} else
				offset = p->b;
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_BESHORT:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) &
						 m->in_offset;
					break;
				case FILE_OPOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) |
						 m->in_offset;
					break;
				case FILE_OPXOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) ^
						 m->in_offset;
					break;
				case FILE_OPADD:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) +
						 m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) -
						 m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) *
						 m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) /
						 m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) %
						 m->in_offset;
					break;
				}
			} else
				offset = (short)((p->hs[0]<<8)|
						 (p->hs[1]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_LESHORT:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) &
						 m->in_offset;
					break;
				case FILE_OPOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) |
						 m->in_offset;
					break;
				case FILE_OPXOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) ^
						 m->in_offset;
					break;
				case FILE_OPADD:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) +
						 m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) -
						 m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) *
						 m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) /
						 m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) %
						 m->in_offset;
					break;
				}
			} else
				offset = (short)((p->hs[1]<<8)|
						 (p->hs[0]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_SHORT:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = p->h & m->in_offset;
					break;
				case FILE_OPOR:
					offset = p->h | m->in_offset;
					break;
				case FILE_OPXOR:
					offset = p->h ^ m->in_offset;
					break;
				case FILE_OPADD:
					offset = p->h + m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = p->h - m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = p->h * m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = p->h / m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = p->h % m->in_offset;
					break;
				}
			}
			else
				offset = p->h;
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_BELONG:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) &
						 m->in_offset;
					break;
				case FILE_OPOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) |
						 m->in_offset;
					break;
				case FILE_OPXOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) ^
						 m->in_offset;
					break;
				case FILE_OPADD:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) +
						 m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) -
						 m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) *
						 m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) /
						 m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) %
						 m->in_offset;
					break;
				}
			} else
				offset = (int32_t)((p->hl[0]<<24)|
						 (p->hl[1]<<16)|
						 (p->hl[2]<<8)|
						 (p->hl[3]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_LELONG:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) &
						 m->in_offset;
					break;
				case FILE_OPOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) |
						 m->in_offset;
					break;
				case FILE_OPXOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) ^
						 m->in_offset;
					break;
				case FILE_OPADD:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) +
						 m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) -
						 m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) *
						 m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) /
						 m->in_offset;
					break;
				case FILE_OPMODULO:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) %
						 m->in_offset;
					break;
				}
			} else
				offset = (int32_t)((p->hl[3]<<24)|
						 (p->hl[2]<<16)|
						 (p->hl[1]<<8)|
						 (p->hl[0]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_LONG:
			if (m->in_offset) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = p->l & m->in_offset;
					break;
				case FILE_OPOR:
					offset = p->l | m->in_offset;
					break;
				case FILE_OPXOR:
					offset = p->l ^ m->in_offset;
					break;
				case FILE_OPADD:
					offset = p->l + m->in_offset;
					break;
				case FILE_OPMINUS:
					offset = p->l - m->in_offset;
					break;
				case FILE_OPMULTIPLY:
					offset = p->l * m->in_offset;
					break;
				case FILE_OPDIVIDE:
					offset = p->l / m->in_offset;
					break;
				case FILE_OPMODULO:
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
			} else
				offset = p->l;
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		}

		if (nbytes < sizeof(union VALUETYPE) ||
		    nbytes - sizeof(union VALUETYPE) < offset)
			return 0;

		memcpy(p, s + offset, sizeof(union VALUETYPE));

		if ((ms->flags & MAGIC_DEBUG) != 0) {
			mdebug(offset, (char *)(void *)p,
			    sizeof(union VALUETYPE));
			file_mdump(m);
		}
	}
	if (!mconvert(ms, p, m))
	  return 0;
	return 1;
}

private int
mcheck(struct magic_set *ms, union VALUETYPE *p, struct magic *m)
{
	uint32_t l = m->value.l;
	uint32_t v;
	int matched;

	if ( (m->value.s[0] == 'x') && (m->value.s[1] == '\0') ) {
		return 1;
	}


	switch (m->type) {
	case FILE_BYTE:
		v = p->b;
		break;

	case FILE_SHORT:
	case FILE_BESHORT:
	case FILE_LESHORT:
		v = p->h;
		break;

	case FILE_LONG:
	case FILE_BELONG:
	case FILE_LELONG:
	case FILE_DATE:
	case FILE_BEDATE:
	case FILE_LEDATE:
	case FILE_LDATE:
	case FILE_BELDATE:
	case FILE_LELDATE:
		v = p->l;
		break;

	case FILE_STRING:
	case FILE_PSTRING:
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
	case FILE_REGEX:
	{
		int rc;
		regex_t rx;
		char errmsg[512];

		rc = regcomp(&rx, m->value.s, REG_EXTENDED|REG_NOSUB);
		if (rc) {
			free(p->buf);
			regerror(rc, &rx, errmsg, sizeof(errmsg));
			file_error(ms, 0, "regex error %d, (%s)", rc, errmsg);
			return -1;
		} else {
			rc = regexec(&rx, p->buf, 0, 0, 0);
			regfree(&rx);
			free(p->buf);
			return !rc;
		}
	}
	default:
		file_error(ms, 0, "invalid type %d in mcheck()", m->type);
		return -1;
	}

	if (m->type != FILE_STRING && m->type != FILE_PSTRING)
		v = file_signextend(ms, m, v);

	switch (m->reln) {
	case 'x':
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%u == *any* = 1\n", v);
		matched = 1;
		break;

	case '!':
		matched = v != l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%u != %u = %d\n",
				       v, l, matched);
		break;

	case '=':
		matched = v == l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%u == %u = %d\n",
				       v, l, matched);
		break;

	case '>':
		if (m->flag & UNSIGNED) {
			matched = v > l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%u > %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v > (int32_t) l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%d > %d = %d\n",
					       v, l, matched);
		}
		break;

	case '<':
		if (m->flag & UNSIGNED) {
			matched = v < l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%u < %u = %d\n",
					       v, l, matched);
		}
		else {
			matched = (int32_t) v < (int32_t) l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%d < %d = %d\n",
					       v, l, matched);
		}
		break;

	case '&':
		matched = (v & l) == l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "((%x & %x) == %x) = %d\n",
				       v, l, l, matched);
		break;

	case '^':
		matched = (v & l) != l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "((%x & %x) != %x) = %d\n",
				       v, l, l, matched);
		break;

	default:
		matched = 0;
		file_error(ms, 0, "cannot happen: invalid relation `%c'",
		    m->reln);
		return -1;
	}

	return matched;
}
