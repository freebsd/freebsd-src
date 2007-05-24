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
FILE_RCSID("@(#)$Id: softmagic.c,v 1.87 2006/12/11 21:48:49 christos Exp $")
#endif	/* lint */

private int match(struct magic_set *, struct magic *, uint32_t,
    const unsigned char *, size_t);
private int mget(struct magic_set *, union VALUETYPE *, const unsigned char *,
    struct magic *, size_t, unsigned int);
private int magiccheck(struct magic_set *, union VALUETYPE *, struct magic *);
private int32_t mprint(struct magic_set *, union VALUETYPE *, struct magic *);
private void mdebug(uint32_t, const char *, size_t);
private int mcopy(struct magic_set *, union VALUETYPE *, int, int,
    const unsigned char *, uint32_t, size_t);
private int mconvert(struct magic_set *, union VALUETYPE *, struct magic *);
private int check_mem(struct magic_set *, unsigned int);
private int print_sep(struct magic_set *, int);
private void cvt_8(union VALUETYPE *, const struct magic *);
private void cvt_16(union VALUETYPE *, const struct magic *);
private void cvt_32(union VALUETYPE *, const struct magic *);
private void cvt_64(union VALUETYPE *, const struct magic *);

/*
 * softmagic - lookup one file in parsed, in-memory copy of database
 * Passed the name and FILE * of one file to be typed.
 */
/*ARGSUSED1*/		/* nbytes passed for regularity, maybe need later */
protected int
file_softmagic(struct magic_set *ms, const unsigned char *buf, size_t nbytes)
{
	struct mlist *ml;
	int rv;
	for (ml = ms->mlist->next; ml != ms->mlist; ml = ml->next)
		if ((rv = match(ms, ml->magic, ml->nmagic, buf, nbytes)) != 0)
			return rv;

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
	int printed_something = 0;

	if (check_mem(ms, cont_level) == -1)
		return -1;

	for (magindex = 0; magindex < nmagic; magindex++) {
		/* if main entry matches, print it... */
		ms->offset = magic[magindex].offset;
		int flush = !mget(ms, &p, s, &magic[magindex], nbytes,
		    cont_level);
		if (flush) {
			if (magic[magindex].reln == '!')
				flush = 0;
		} else {	
			switch (magiccheck(ms, &p, &magic[magindex])) {
			case -1:
				return -1;
			case 0:
				flush++;
				break;
			default:
				break;
			}
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

		/*
		 * If we are going to print something, we'll need to print
		 * a blank before we print something else.
		 */
		if (magic[magindex].desc[0]) {
			need_separator = 1;
			printed_something = 1;
			if (print_sep(ms, firstline) == -1)
				return -1;
		}

		if ((ms->c.off[cont_level] = mprint(ms, &p, &magic[magindex]))
		    == -1)
			return -1;

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
			ms->offset = magic[magindex].offset;
			if (magic[magindex].flag & OFFADD) {
				ms->offset +=
				    ms->c.off[cont_level - 1];
			}

			flush = !mget(ms, &p, s, &magic[magindex], nbytes,
			    cont_level);
			if (flush && magic[magindex].reln != '!')
				continue;
				
			switch (flush ? 1 : magiccheck(ms, &p, &magic[magindex])) {
			case -1:
				return -1;
			case 0:
				break;
			default:
				/*
				 * If we are going to print something,
				 * make sure that we have a separator first.
				 */
				if (magic[magindex].desc[0]) {
					printed_something = 1;
					if (print_sep(ms, firstline) == -1)
						return -1;
				}
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
		}
		firstline = 0;
		if (printed_something)
			returnval = 1;
		if ((ms->flags & MAGIC_CONTINUE) == 0 && printed_something) {
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
	file_oomem(ms, len);
	return -1;
}

private int
check_fmt(struct magic_set *ms, struct magic *m)
{
	regex_t rx;
	int rc;

	if (strchr(m->desc, '%') == NULL)
		return 0;

	rc = regcomp(&rx, "%[-0-9\\.]*s", REG_EXTENDED|REG_NOSUB);
	if (rc) {
		char errmsg[512];
		regerror(rc, &rx, errmsg, sizeof(errmsg));
		file_error(ms, 0, "regex error %d, (%s)", rc, errmsg);
		return -1;
	} else {
		rc = regexec(&rx, m->desc, 0, 0, 0);
		regfree(&rx);
		return !rc;
	}
}

private int32_t
mprint(struct magic_set *ms, union VALUETYPE *p, struct magic *m)
{
	uint64_t v;
	int32_t t = 0;
 	char buf[512];


  	switch (m->type) {
  	case FILE_BYTE:
		v = file_signextend(ms, m, (uint64_t)p->b);
		switch (check_fmt(ms, m)) {
		case -1:
			return -1;
		case 1:
			if (snprintf(buf, sizeof(buf), "%c",
			    (unsigned char)v) < 0)
				return -1;
			if (file_printf(ms, m->desc, buf) == -1)
				return -1;
			break;
		default:
			if (file_printf(ms, m->desc, (unsigned char) v) == -1)
				return -1;
			break;
		}
		t = ms->offset + sizeof(char);
		break;

  	case FILE_SHORT:
  	case FILE_BESHORT:
  	case FILE_LESHORT:
		v = file_signextend(ms, m, (uint64_t)p->h);
		switch (check_fmt(ms, m)) {
		case -1:
			return -1;
		case 1:
			if (snprintf(buf, sizeof(buf), "%hu",
			    (unsigned short)v) < 0)
				return -1;
			if (file_printf(ms, m->desc, buf) == -1)
				return -1;
			break;
		default:
			if (file_printf(ms, m->desc, (unsigned short) v) == -1)
				return -1;
			break;
		}
		t = ms->offset + sizeof(short);
		break;

  	case FILE_LONG:
  	case FILE_BELONG:
  	case FILE_LELONG:
  	case FILE_MELONG:
		v = file_signextend(ms, m, (uint64_t)p->l);
		switch (check_fmt(ms, m)) {
		case -1:
			return -1;
		case 1:
			if (snprintf(buf, sizeof(buf), "%u", (uint32_t)v) < 0)
				return -1;
			if (file_printf(ms, m->desc, buf) == -1)
				return -1;
			break;
		default:
			if (file_printf(ms, m->desc, (uint32_t) v) == -1)
				return -1;
			break;
		}
		t = ms->offset + sizeof(int32_t);
  		break;

  	case FILE_QUAD:
  	case FILE_BEQUAD:
  	case FILE_LEQUAD:
		v = file_signextend(ms, m, p->q);
		if (file_printf(ms, m->desc, (uint64_t) v) == -1)
			return -1;
		t = ms->offset + sizeof(int64_t);
  		break;
  	case FILE_STRING:
  	case FILE_PSTRING:
  	case FILE_BESTRING16:
  	case FILE_LESTRING16:
		if (m->reln == '=' || m->reln == '!') {
			if (file_printf(ms, m->desc, m->value.s) == -1)
				return -1;
			t = ms->offset + m->vallen;
		}
		else {
			if (*m->value.s == '\0') {
				char *cp = strchr(p->s,'\n');
				if (cp)
					*cp = '\0';
			}
			if (file_printf(ms, m->desc, p->s) == -1)
				return -1;
			t = ms->offset + strlen(p->s);
		}
		break;

	case FILE_DATE:
	case FILE_BEDATE:
	case FILE_LEDATE:
	case FILE_MEDATE:
		if (file_printf(ms, m->desc, file_fmttime(p->l, 1)) == -1)
			return -1;
		t = ms->offset + sizeof(time_t);
		break;

	case FILE_LDATE:
	case FILE_BELDATE:
	case FILE_LELDATE:
	case FILE_MELDATE:
		if (file_printf(ms, m->desc, file_fmttime(p->l, 0)) == -1)
			return -1;
		t = ms->offset + sizeof(time_t);
		break;

	case FILE_QDATE:
	case FILE_BEQDATE:
	case FILE_LEQDATE:
		if (file_printf(ms, m->desc, file_fmttime((uint32_t)p->q, 1))
		    == -1)
			return -1;
		t = ms->offset + sizeof(uint64_t);
		break;

	case FILE_QLDATE:
	case FILE_BEQLDATE:
	case FILE_LEQLDATE:
		if (file_printf(ms, m->desc, file_fmttime((uint32_t)p->q, 0))
		    == -1)
			return -1;
		t = ms->offset + sizeof(uint64_t);
		break;

	case FILE_REGEX:
	  	if (file_printf(ms, m->desc, p->s) == -1)
			return -1;
		t = ms->offset + strlen(p->s);
		break;

	case FILE_SEARCH:
	  	if (file_printf(ms, m->desc, m->value.s) == -1)
			return -1;
		t = ms->offset + m->vallen;
		break;

	default:
		file_error(ms, 0, "invalid m->type (%d) in mprint()", m->type);
		return -1;
	}
	return(t);
}


#define DO_CVT(fld, cast) \
	if (m->mask) \
		switch (m->mask_op & 0x7F) { \
		case FILE_OPAND: \
			p->fld &= cast m->mask; \
			break; \
		case FILE_OPOR: \
			p->fld |= cast m->mask; \
			break; \
		case FILE_OPXOR: \
			p->fld ^= cast m->mask; \
			break; \
		case FILE_OPADD: \
			p->fld += cast m->mask; \
			break; \
		case FILE_OPMINUS: \
			p->fld -= cast m->mask; \
			break; \
		case FILE_OPMULTIPLY: \
			p->fld *= cast m->mask; \
			break; \
		case FILE_OPDIVIDE: \
			p->fld /= cast m->mask; \
			break; \
		case FILE_OPMODULO: \
			p->fld %= cast m->mask; \
			break; \
		} \
	if (m->mask_op & FILE_OPINVERSE) \
		p->fld = ~p->fld \

private void
cvt_8(union VALUETYPE *p, const struct magic *m)
{
	DO_CVT(b, (uint8_t));
}

private void
cvt_16(union VALUETYPE *p, const struct magic *m)
{
	DO_CVT(h, (uint16_t));
}

private void
cvt_32(union VALUETYPE *p, const struct magic *m)
{
	DO_CVT(l, (uint32_t));
}

private void
cvt_64(union VALUETYPE *p, const struct magic *m)
{
	DO_CVT(q, (uint64_t));
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
		cvt_8(p, m);
		return 1;
	case FILE_SHORT:
		cvt_16(p, m);
		return 1;
	case FILE_LONG:
	case FILE_DATE:
	case FILE_LDATE:
		cvt_32(p, m);
		return 1;
	case FILE_QUAD:
	case FILE_QDATE:
	case FILE_QLDATE:
		cvt_64(p, m);
		return 1;
	case FILE_STRING:
	case FILE_BESTRING16:
	case FILE_LESTRING16:
		{
			size_t len;

			/* Null terminate and eat *trailing* return */
			p->s[sizeof(p->s) - 1] = '\0';
			len = strlen(p->s);
			if (len-- && p->s[len] == '\n')
				p->s[len] = '\0';
			return 1;
		}
	case FILE_PSTRING:
		{
			char *ptr1 = p->s, *ptr2 = ptr1 + 1;
			size_t len = *p->s;
			if (len >= sizeof(p->s))
				len = sizeof(p->s) - 1;
			while (len--)
				*ptr1++ = *ptr2++;
			*ptr1 = '\0';
			len = strlen(p->s);
			if (len-- && p->s[len] == '\n')
				p->s[len] = '\0';
			return 1;
		}
	case FILE_BESHORT:
		p->h = (short)((p->hs[0]<<8)|(p->hs[1]));
		cvt_16(p, m);
		return 1;
	case FILE_BELONG:
	case FILE_BEDATE:
	case FILE_BELDATE:
		p->l = (int32_t)
		    ((p->hl[0]<<24)|(p->hl[1]<<16)|(p->hl[2]<<8)|(p->hl[3]));
		cvt_32(p, m);
		return 1;
	case FILE_BEQUAD:
	case FILE_BEQDATE:
	case FILE_BEQLDATE:
		p->q = (int64_t)
		    (((int64_t)p->hq[0]<<56)|((int64_t)p->hq[1]<<48)|
		     ((int64_t)p->hq[2]<<40)|((int64_t)p->hq[3]<<32)|
		     (p->hq[4]<<24)|(p->hq[5]<<16)|(p->hq[6]<<8)|(p->hq[7]));
		cvt_64(p, m);
		return 1;
	case FILE_LESHORT:
		p->h = (short)((p->hs[1]<<8)|(p->hs[0]));
		cvt_16(p, m);
		return 1;
	case FILE_LELONG:
	case FILE_LEDATE:
	case FILE_LELDATE:
		p->l = (int32_t)
		    ((p->hl[3]<<24)|(p->hl[2]<<16)|(p->hl[1]<<8)|(p->hl[0]));
		cvt_32(p, m);
		return 1;
	case FILE_LEQUAD:
	case FILE_LEQDATE:
	case FILE_LEQLDATE:
		p->q = (int64_t)
		    (((int64_t)p->hq[7]<<56)|((int64_t)p->hq[6]<<48)|
		     ((int64_t)p->hq[5]<<40)|((int64_t)p->hq[4]<<32)|
		     (p->hq[3]<<24)|(p->hq[2]<<16)|(p->hq[1]<<8)|(p->hq[0]));
		cvt_64(p, m);
		return 1;
	case FILE_MELONG:
	case FILE_MEDATE:
	case FILE_MELDATE:
		p->l = (int32_t)
		    ((p->hl[1]<<24)|(p->hl[0]<<16)|(p->hl[3]<<8)|(p->hl[2]));
		cvt_32(p, m);
		return 1;
	case FILE_REGEX:
	case FILE_SEARCH:
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
mcopy(struct magic_set *ms, union VALUETYPE *p, int type, int indir,
    const unsigned char *s, uint32_t offset, size_t nbytes)
{
	if (type == FILE_REGEX && indir == 0) {
		/*
		 * offset is interpreted as last line to search,
		 * (starting at 1), not as bytes-from start-of-file
		 */
		char *b, *c, *last = NULL;
		if (s == NULL) {
			p->search.buflen = 0;
			p->search.buf = NULL;
			return 0;
		}
		if ((p->search.buf = strdup((const char *)s)) == NULL) {
			file_oomem(ms, strlen((const char *)s));
			return -1;
		}
		for (b = p->search.buf; offset && 
		    ((b = strchr(c = b, '\n')) || (b = strchr(c, '\r')));
		    offset--, b++) {
			last = b;
			if (b[0] == '\r' && b[1] == '\n') b++;
		}
		if (last != NULL)
			*last = '\0';
		p->search.buflen = last - p->search.buf;
		return 0;
	}

	if (indir == 0 && (type == FILE_BESTRING16 || type == FILE_LESTRING16))
	{
		const unsigned char *src = s + offset;
		const unsigned char *esrc = s + nbytes;
		char *dst = p->s, *edst = &p->s[sizeof(p->s) - 1];

		if (type == FILE_BESTRING16)
			src++;

		/* check for pointer overflow */
		if (src < s) {
			file_error(ms, 0, "invalid offset %zu in mcopy()",
			    offset);
			return -1;
		}

		for (;src < esrc; src++, dst++) {
			if (dst < edst)
				*dst = *src++;
			else
				break;
			if (*dst == '\0')
				*dst = ' ';
		}
		*edst = '\0';
		return 0;
	}

	if (offset >= nbytes) {
		(void)memset(p, '\0', sizeof(*p));
		return 0;
	}
	if (nbytes - offset < sizeof(*p))
		nbytes = nbytes - offset;
	else
		nbytes = sizeof(*p);

	(void)memcpy(p, s + offset, nbytes);

	/*
	 * the usefulness of padding with zeroes eludes me, it
	 * might even cause problems
	 */
	if (nbytes < sizeof(*p))
		(void)memset(((char *)(void *)p) + nbytes, '\0',
		    sizeof(*p) - nbytes);
	return 0;
}

private int
mget(struct magic_set *ms, union VALUETYPE *p, const unsigned char *s,
    struct magic *m, size_t nbytes, unsigned int cont_level)
{
	uint32_t offset = ms->offset;

	if (mcopy(ms, p, m->type, m->flag & INDIR, s, offset, nbytes) == -1)
		return -1;

	if ((ms->flags & MAGIC_DEBUG) != 0) {
		mdebug(offset, (char *)(void *)p, sizeof(union VALUETYPE));
		file_mdump(m);
	}

	if (m->flag & INDIR) {
		int off = m->in_offset;
		if (m->in_op & FILE_OPINDIRECT) {
			const union VALUETYPE *q =
			    ((const void *)(s + offset + off));
			switch (m->in_type) {
			case FILE_BYTE:
				off = q->b;
				break;
			case FILE_SHORT:
				off = q->h;
				break;
			case FILE_BESHORT:
				off = (short)((q->hs[0]<<8)|(q->hs[1]));
				break;
			case FILE_LESHORT:
				off = (short)((q->hs[1]<<8)|(q->hs[0]));
				break;
			case FILE_LONG:
				off = q->l;
				break;
			case FILE_BELONG:
				off = (int32_t)((q->hl[0]<<24)|(q->hl[1]<<16)|
						 (q->hl[2]<<8)|(q->hl[3]));
				break;
			case FILE_LELONG:
				off = (int32_t)((q->hl[3]<<24)|(q->hl[2]<<16)|
						 (q->hl[1]<<8)|(q->hl[0]));
				break;
			case FILE_MELONG:
				off = (int32_t)((q->hl[1]<<24)|(q->hl[0]<<16)|
						 (q->hl[3]<<8)|(q->hl[2]));
				break;
			}
		}
		switch (m->in_type) {
		case FILE_BYTE:
			if (nbytes < (offset + 1)) return 0;
			if (off) {
				switch (m->in_op & 0x3F) {
				case FILE_OPAND:
					offset = p->b & off;
					break;
				case FILE_OPOR:
					offset = p->b | off;
					break;
				case FILE_OPXOR:
					offset = p->b ^ off;
					break;
				case FILE_OPADD:
					offset = p->b + off;
					break;
				case FILE_OPMINUS:
					offset = p->b - off;
					break;
				case FILE_OPMULTIPLY:
					offset = p->b * off;
					break;
				case FILE_OPDIVIDE:
					offset = p->b / off;
					break;
				case FILE_OPMODULO:
					offset = p->b % off;
					break;
				}
			} else
				offset = p->b;
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_BESHORT:
			if (nbytes < (offset + 2))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) &
						 off;
					break;
				case FILE_OPOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) |
						 off;
					break;
				case FILE_OPXOR:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) ^
						 off;
					break;
				case FILE_OPADD:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) +
						 off;
					break;
				case FILE_OPMINUS:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) -
						 off;
					break;
				case FILE_OPMULTIPLY:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) *
						 off;
					break;
				case FILE_OPDIVIDE:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) /
						 off;
					break;
				case FILE_OPMODULO:
					offset = (short)((p->hs[0]<<8)|
							 (p->hs[1])) %
						 off;
					break;
				}
			} else
				offset = (short)((p->hs[0]<<8)|
						 (p->hs[1]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_LESHORT:
			if (nbytes < (offset + 2))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) &
						 off;
					break;
				case FILE_OPOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) |
						 off;
					break;
				case FILE_OPXOR:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) ^
						 off;
					break;
				case FILE_OPADD:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) +
						 off;
					break;
				case FILE_OPMINUS:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) -
						 off;
					break;
				case FILE_OPMULTIPLY:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) *
						 off;
					break;
				case FILE_OPDIVIDE:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) /
						 off;
					break;
				case FILE_OPMODULO:
					offset = (short)((p->hs[1]<<8)|
							 (p->hs[0])) %
						 off;
					break;
				}
			} else
				offset = (short)((p->hs[1]<<8)|
						 (p->hs[0]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_SHORT:
			if (nbytes < (offset + 2))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = p->h & off;
					break;
				case FILE_OPOR:
					offset = p->h | off;
					break;
				case FILE_OPXOR:
					offset = p->h ^ off;
					break;
				case FILE_OPADD:
					offset = p->h + off;
					break;
				case FILE_OPMINUS:
					offset = p->h - off;
					break;
				case FILE_OPMULTIPLY:
					offset = p->h * off;
					break;
				case FILE_OPDIVIDE:
					offset = p->h / off;
					break;
				case FILE_OPMODULO:
					offset = p->h % off;
					break;
				}
			}
			else
				offset = p->h;
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_BELONG:
			if (nbytes < (offset + 4))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) &
						 off;
					break;
				case FILE_OPOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) |
						 off;
					break;
				case FILE_OPXOR:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) ^
						 off;
					break;
				case FILE_OPADD:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) +
						 off;
					break;
				case FILE_OPMINUS:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) -
						 off;
					break;
				case FILE_OPMULTIPLY:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) *
						 off;
					break;
				case FILE_OPDIVIDE:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) /
						 off;
					break;
				case FILE_OPMODULO:
					offset = (int32_t)((p->hl[0]<<24)|
							 (p->hl[1]<<16)|
							 (p->hl[2]<<8)|
							 (p->hl[3])) %
						 off;
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
			if (nbytes < (offset + 4))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) &
						 off;
					break;
				case FILE_OPOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) |
						 off;
					break;
				case FILE_OPXOR:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) ^
						 off;
					break;
				case FILE_OPADD:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) +
						 off;
					break;
				case FILE_OPMINUS:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) -
						 off;
					break;
				case FILE_OPMULTIPLY:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) *
						 off;
					break;
				case FILE_OPDIVIDE:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) /
						 off;
					break;
				case FILE_OPMODULO:
					offset = (int32_t)((p->hl[3]<<24)|
							 (p->hl[2]<<16)|
							 (p->hl[1]<<8)|
							 (p->hl[0])) %
						 off;
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
		case FILE_MELONG:
			if (nbytes < (offset + 4))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) &
						 off;
					break;
				case FILE_OPOR:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) |
						 off;
					break;
				case FILE_OPXOR:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) ^
						 off;
					break;
				case FILE_OPADD:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) +
						 off;
					break;
				case FILE_OPMINUS:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) -
						 off;
					break;
				case FILE_OPMULTIPLY:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) *
						 off;
					break;
				case FILE_OPDIVIDE:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) /
						 off;
					break;
				case FILE_OPMODULO:
					offset = (int32_t)((p->hl[1]<<24)|
							 (p->hl[0]<<16)|
							 (p->hl[3]<<8)|
							 (p->hl[2])) %
						 off;
					break;
				}
			} else
				offset = (int32_t)((p->hl[1]<<24)|
						 (p->hl[0]<<16)|
						 (p->hl[3]<<8)|
						 (p->hl[2]));
			if (m->in_op & FILE_OPINVERSE)
				offset = ~offset;
			break;
		case FILE_LONG:
			if (nbytes < (offset + 4))
				return 0;
			if (off) {
				switch (m->in_op & 0x7F) {
				case FILE_OPAND:
					offset = p->l & off;
					break;
				case FILE_OPOR:
					offset = p->l | off;
					break;
				case FILE_OPXOR:
					offset = p->l ^ off;
					break;
				case FILE_OPADD:
					offset = p->l + off;
					break;
				case FILE_OPMINUS:
					offset = p->l - off;
					break;
				case FILE_OPMULTIPLY:
					offset = p->l * off;
					break;
				case FILE_OPDIVIDE:
					offset = p->l / off;
					break;
				case FILE_OPMODULO:
					offset = p->l % off;
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

		if (m->flag & INDIROFFADD) offset += ms->c.off[cont_level-1];
		if (mcopy(ms, p, m->type, 0, s, offset, nbytes) == -1)
			return -1;
		ms->offset = offset;

		if ((ms->flags & MAGIC_DEBUG) != 0) {
			mdebug(offset, (char *)(void *)p,
			    sizeof(union VALUETYPE));
			file_mdump(m);
		}
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
		case FILE_MELONG:
		case FILE_DATE:
		case FILE_BEDATE:
		case FILE_LEDATE:
		case FILE_MEDATE:
		case FILE_LDATE:
		case FILE_BELDATE:
		case FILE_LELDATE:
		case FILE_MELDATE:
			if (nbytes < (offset + 4))
				return 0;
			break;

		case FILE_STRING:
		case FILE_PSTRING:
		case FILE_SEARCH:
			if (nbytes < (offset + m->vallen))
				return 0;
			break;
		default: break;
	}

	if (m->type == FILE_SEARCH) {
		size_t mlen = (size_t)(m->mask + m->vallen);
		size_t flen = nbytes - offset;
		if (flen < mlen)
			mlen = flen;
		p->search.buflen = mlen;
		p->search.buf = malloc(mlen + 1);
		if (p->search.buf == NULL) {
			file_error(ms, errno, "Cannot allocate search buffer");
			return 0;
		}
		(void)memcpy(p->search.buf, s + offset, mlen);
		p->search.buf[mlen] = '\0';
	}
	if (!mconvert(ms, p, m))
		return 0;
	return 1;
}

private int
magiccheck(struct magic_set *ms, union VALUETYPE *p, struct magic *m)
{
	uint64_t l = m->value.q;
	uint64_t v;
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
	case FILE_MELONG:
	case FILE_DATE:
	case FILE_BEDATE:
	case FILE_LEDATE:
	case FILE_MEDATE:
	case FILE_LDATE:
	case FILE_BELDATE:
	case FILE_LELDATE:
	case FILE_MELDATE:
		v = p->l;
		break;

	case FILE_QUAD:
	case FILE_LEQUAD:
	case FILE_BEQUAD:
	case FILE_QDATE:
	case FILE_BEQDATE:
	case FILE_LEQDATE:
	case FILE_QLDATE:
	case FILE_BEQLDATE:
	case FILE_LEQLDATE:
		v = p->q;
		break;

	case FILE_STRING:
	case FILE_BESTRING16:
	case FILE_LESTRING16:
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

		if (p->search.buf == NULL)
			return 0;

		rc = regcomp(&rx, m->value.s,
		    REG_EXTENDED|REG_NOSUB|REG_NEWLINE|
		    ((m->mask & STRING_IGNORE_LOWERCASE) ? REG_ICASE : 0));
		if (rc) {
			free(p->search.buf);
			p->search.buf = NULL;
			regerror(rc, &rx, errmsg, sizeof(errmsg));
			file_error(ms, 0, "regex error %d, (%s)", rc, errmsg);
			return -1;
		} else {
			rc = regexec(&rx, p->search.buf, 0, 0, 0);
			regfree(&rx);
			free(p->search.buf);
			p->search.buf = NULL;
			return !rc;
		}
	}
	case FILE_SEARCH:
	{
		/*
		 * search for a string in a certain range
		 */
		unsigned char *a = (unsigned char*)m->value.s;
		unsigned char *b = (unsigned char*)p->search.buf;
		size_t len, slen = m->vallen;
		size_t range = 0;
		if (slen > sizeof(m->value.s))
			slen = sizeof(m->value.s);
		l = 0;
		v = 0;
		if (b == NULL)
			return 0;
		len = slen;
		while (++range <= m->mask) {
			while (len-- > 0 && (v = *b++ - *a++) == 0)
				continue;
			if (!v) {
				ms->offset += range - 1;
				break;
			}
			if (range + slen >= p->search.buflen) {
				v = 1;
				break;
			}
			len = slen;
			a = (unsigned char*)m->value.s;
			b = (unsigned char*)p->search.buf + range;
		}
		free(p->search.buf);
		p->search.buf = NULL;
		if (v)
			return 0;
		break;
	}
	default:
		file_error(ms, 0, "invalid type %d in magiccheck()", m->type);
		return -1;
	}

	if (m->type != FILE_STRING && m->type != FILE_PSTRING)
		v = file_signextend(ms, m, v);

	switch (m->reln) {
	case 'x':
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%llu == *any* = 1\n",
			    (unsigned long long)v);
		matched = 1;
		break;

	case '!':
		matched = v != l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%llu != %llu = %d\n",
			    (unsigned long long)v, (unsigned long long)l,
			    matched);
		break;

	case '=':
		matched = v == l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "%llu == %llu = %d\n",
			    (unsigned long long)v, (unsigned long long)l,
			    matched);
		break;

	case '>':
		if (m->flag & UNSIGNED) {
			matched = v > l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%llu > %llu = %d\n",
				    (unsigned long long)v,
				    (unsigned long long)l, matched);
		}
		else {
			matched = (int32_t) v > (int32_t) l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%lld > %lld = %d\n",
				    (long long)v, (long long)l, matched);
		}
		break;

	case '<':
		if (m->flag & UNSIGNED) {
			matched = v < l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%llu < %llu = %d\n",
				    (unsigned long long)v,
				    (unsigned long long)l, matched);
		}
		else {
			matched = (int32_t) v < (int32_t) l;
			if ((ms->flags & MAGIC_DEBUG) != 0)
				(void) fprintf(stderr, "%lld < %lld = %d\n",
				       (long long)v, (long long)l, matched);
		}
		break;

	case '&':
		matched = (v & l) == l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "((%llx & %llx) == %llx) = %d\n",
			    (unsigned long long)v, (unsigned long long)l,
			    (unsigned long long)l, matched);
		break;

	case '^':
		matched = (v & l) != l;
		if ((ms->flags & MAGIC_DEBUG) != 0)
			(void) fprintf(stderr, "((%llx & %llx) != %llx) = %d\n",
			    (unsigned long long)v, (unsigned long long)l,
			    (unsigned long long)l, matched);
		break;

	default:
		matched = 0;
		file_error(ms, 0, "cannot happen: invalid relation `%c'",
		    m->reln);
		return -1;
	}

	return matched;
}

private int
print_sep(struct magic_set *ms, int firstline)
{
	if (firstline)
		return 0;
	/*
	 * we found another match 
	 * put a newline and '-' to do some simple formatting
	 */
	return file_printf(ms, "\n- ");
}
