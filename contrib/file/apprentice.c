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
 * apprentice - make one pass through /etc/magic, learning its secrets.
 */

#include "file.h"
#include "magic.h"
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#ifdef QUICK
#include <sys/mman.h>
#endif

#ifndef	lint
FILE_RCSID("@(#)$Id: apprentice.c,v 1.100 2006/12/11 21:48:49 christos Exp $")
#endif	/* lint */

#define	EATAB {while (isascii((unsigned char) *l) && \
		      isspace((unsigned char) *l))  ++l;}
#define LOWCASE(l) (isupper((unsigned char) (l)) ? \
			tolower((unsigned char) (l)) : (l))
/*
 * Work around a bug in headers on Digital Unix.
 * At least confirmed for: OSF1 V4.0 878
 */
#if defined(__osf__) && defined(__DECC)
#ifdef MAP_FAILED
#undef MAP_FAILED
#endif
#endif

#ifndef MAP_FAILED
#define MAP_FAILED (void *) -1
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024
#endif

#define IS_PLAINSTRING(t) ((t) == FILE_STRING || (t) == FILE_PSTRING || \
    (t) == FILE_BESTRING16 || (t) == FILE_LESTRING16)
    
#define IS_STRING(t) (IS_PLAINSTRING(t) || (t) == FILE_REGEX || \
    (t) == FILE_SEARCH)

struct magic_entry {
	struct magic *mp;	
	uint32_t cont_count;
	uint32_t max_count;
};

const int file_formats[] = { FILE_FORMAT_STRING };
const size_t file_nformats = sizeof(file_formats) / sizeof(file_formats[0]);
const char *file_names[] = { FILE_FORMAT_NAME };
const size_t file_nnames = sizeof(file_names) / sizeof(file_names[0]);

private int getvalue(struct magic_set *ms, struct magic *, const char **);
private int hextoint(int);
private const char *getstr(struct magic_set *, const char *, char *, int,
    int *);
private int parse(struct magic_set *, struct magic_entry **, uint32_t *,
    const char *, size_t, int);
private void eatsize(const char **);
private int apprentice_1(struct magic_set *, const char *, int, struct mlist *);
private size_t apprentice_magic_strength(const struct magic *);
private int apprentice_sort(const void *, const void *);
private int apprentice_file(struct magic_set *, struct magic **, uint32_t *,
    const char *, int);
private void byteswap(struct magic *, uint32_t);
private void bs1(struct magic *);
private uint16_t swap2(uint16_t);
private uint32_t swap4(uint32_t);
private uint64_t swap8(uint64_t);
private char *mkdbname(const char *, char *, size_t, int);
private int apprentice_map(struct magic_set *, struct magic **, uint32_t *,
    const char *);
private int apprentice_compile(struct magic_set *, struct magic **, uint32_t *,
    const char *);
private int check_format_type(const char *, int);
private int check_format(struct magic_set *, struct magic *);

private size_t maxmagic = 0;
private size_t magicsize = sizeof(struct magic);


#ifdef COMPILE_ONLY

int main(int, char *[]);

int
main(int argc, char *argv[])
{
	int ret;
	struct magic_set *ms;
	char *progname;

	if ((progname = strrchr(argv[0], '/')) != NULL)
		progname++;
	else
		progname = argv[0];

	if (argc != 2) {
		(void)fprintf(stderr, "Usage: %s file\n", progname);
		return 1;
	}

	if ((ms = magic_open(MAGIC_CHECK)) == NULL) {
		(void)fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		return 1;
	}
	ret = magic_compile(ms, argv[1]) == -1 ? 1 : 0;
	if (ret == 1)
		(void)fprintf(stderr, "%s: %s\n", progname, magic_error(ms));
	magic_close(ms);
	return ret;
}
#endif /* COMPILE_ONLY */


/*
 * Handle one file.
 */
private int
apprentice_1(struct magic_set *ms, const char *fn, int action,
    struct mlist *mlist)
{
	struct magic *magic = NULL;
	uint32_t nmagic = 0;
	struct mlist *ml;
	int rv = -1;
	int mapped;

	if (magicsize != FILE_MAGICSIZE) {
		file_error(ms, 0, "magic element size %lu != %lu",
		    (unsigned long)sizeof(*magic),
		    (unsigned long)FILE_MAGICSIZE);
		return -1;
	}

	if (action == FILE_COMPILE) {
		rv = apprentice_file(ms, &magic, &nmagic, fn, action);
		if (rv != 0)
			return -1;
		rv = apprentice_compile(ms, &magic, &nmagic, fn);
		free(magic);
		return rv;
	}

#ifndef COMPILE_ONLY
	if ((rv = apprentice_map(ms, &magic, &nmagic, fn)) == -1) {
		if (ms->flags & MAGIC_CHECK)
			file_magwarn(ms, "using regular magic file `%s'", fn);
		rv = apprentice_file(ms, &magic, &nmagic, fn, action);
		if (rv != 0)
			return -1;
		mapped = 0;
	}

	mapped = rv;
	     
	if (magic == NULL || nmagic == 0) {
		file_delmagic(magic, mapped, nmagic);
		return -1;
	}

	if ((ml = malloc(sizeof(*ml))) == NULL) {
		file_delmagic(magic, mapped, nmagic);
		file_oomem(ms, sizeof(*ml));
		return -1;
	}

	ml->magic = magic;
	ml->nmagic = nmagic;
	ml->mapped = mapped;

	mlist->prev->next = ml;
	ml->prev = mlist->prev;
	ml->next = mlist;
	mlist->prev = ml;

	return 0;
#endif /* COMPILE_ONLY */
}

protected void
file_delmagic(struct magic *p, int type, size_t entries)
{
	if (p == NULL)
		return;
	switch (type) {
	case 2:
		p--;
		(void)munmap((void *)p, sizeof(*p) * (entries + 1));
		break;
	case 1:
		p--;
		/*FALLTHROUGH*/
	case 0:
		free(p);
		break;
	default:
		abort();
	}
}


/* const char *fn: list of magic files */
protected struct mlist *
file_apprentice(struct magic_set *ms, const char *fn, int action)
{
	char *p, *mfn, *afn = NULL;
	int file_err, errs = -1;
	struct mlist *mlist;
	static const char mime[] = ".mime";

	if (fn == NULL)
		fn = getenv("MAGIC");
	if (fn == NULL)
		fn = MAGIC;

	if ((fn = mfn = strdup(fn)) == NULL) {
		file_oomem(ms, strlen(fn));
		return NULL;
	}

	if ((mlist = malloc(sizeof(*mlist))) == NULL) {
		free(mfn);
		file_oomem(ms, sizeof(*mlist));
		return NULL;
	}
	mlist->next = mlist->prev = mlist;

	while (fn) {
		p = strchr(fn, PATHSEP);
		if (p)
			*p++ = '\0';
		if (*fn == '\0')
			break;
		if (ms->flags & MAGIC_MIME) {
			size_t len = strlen(fn) + sizeof(mime);
			if ((afn = malloc(len)) == NULL) {
				free(mfn);
				free(mlist);
				file_oomem(ms, len);
				return NULL;
			}
			(void)strcpy(afn, fn);
			(void)strcat(afn, mime);
			fn = afn;
		}
		file_err = apprentice_1(ms, fn, action, mlist);
		if (file_err > errs)
			errs = file_err;
		if (afn) {
			free(afn);
			afn = NULL;
		}
		fn = p;
	}
	if (errs == -1) {
		free(mfn);
		free(mlist);
		mlist = NULL;
		file_error(ms, 0, "could not find any magic files!");
		return NULL;
	}
	free(mfn);
	return mlist;
}

/*
 * Get weight of this magic entry, for sorting purposes.
 */
private size_t
apprentice_magic_strength(const struct magic *m)
{
#define MULT 10
	size_t val = 2 * MULT;	/* baseline strength */

	switch (m->type) {
	case FILE_BYTE:
		val += 1 * MULT;
		break;

	case FILE_SHORT:
	case FILE_LESHORT:
	case FILE_BESHORT:
		val += 2 * MULT;
		break;

	case FILE_LONG:
	case FILE_LELONG:
	case FILE_BELONG:
	case FILE_MELONG:
		val += 4 * MULT;
		break;

	case FILE_PSTRING:
	case FILE_STRING:
		val += m->vallen * MULT;
		break;

	case FILE_BESTRING16:
	case FILE_LESTRING16:
		val += m->vallen * MULT / 2;
		break;

	case FILE_SEARCH:
	case FILE_REGEX:
		val += m->vallen;
		break;

	case FILE_DATE:
	case FILE_LEDATE:
	case FILE_BEDATE:
	case FILE_MEDATE:
	case FILE_LDATE:
	case FILE_LELDATE:
	case FILE_BELDATE:
	case FILE_MELDATE:
		val += 4 * MULT;
		break;

	case FILE_QUAD:
	case FILE_BEQUAD:
	case FILE_LEQUAD:
	case FILE_QDATE:
	case FILE_LEQDATE:
	case FILE_BEQDATE:
	case FILE_QLDATE:
	case FILE_LEQLDATE:
	case FILE_BEQLDATE:
		val += 8 * MULT;
		break;

	default:
		val = 0;
		(void)fprintf(stderr, "Bad type %d\n", m->type);
		abort();
	}

	switch (m->reln) {
	case 'x':	/* matches anything penalize */
		val = 0;
		break;

	case '!':
	case '=':	/* Exact match, prefer */
		val += MULT;
		break;

	case '>':
	case '<':	/* comparison match reduce strength */
		val -= 2 * MULT;
		break;

	case '^':
	case '&':	/* masking bits, we could count them too */
		val -= MULT;
		break;

	default:
		(void)fprintf(stderr, "Bad relation %c\n", m->reln);
		abort();
	}
	return val;
}

/*  
 * Sort callback for sorting entries by "strength" (basically length)
 */
private int
apprentice_sort(const void *a, const void *b)
{
	const struct magic_entry *ma = a;
	const struct magic_entry *mb = b;
	size_t sa = apprentice_magic_strength(ma->mp);
	size_t sb = apprentice_magic_strength(mb->mp);
	if (sa == sb)
		return 0;
	else if (sa > sb)
		return -1;
	else
		return 1;
}

/*
 * parse from a file
 * const char *fn: name of magic file
 */
private int
apprentice_file(struct magic_set *ms, struct magic **magicp, uint32_t *nmagicp,
    const char *fn, int action)
{
	private const char hdr[] =
		"cont\toffset\ttype\topcode\tmask\tvalue\tdesc";
	FILE *f;
	char line[BUFSIZ+1];
	int errs = 0;
	struct magic_entry *marray;
	uint32_t marraycount, i, mentrycount = 0;
	size_t lineno = 0;

	ms->flags |= MAGIC_CHECK;	/* Enable checks for parsed files */

	f = fopen(ms->file = fn, "r");
	if (f == NULL) {
		if (errno != ENOENT)
			file_error(ms, errno, "cannot read magic file `%s'",
			    fn);
		return -1;
	}

        maxmagic = MAXMAGIS;
	if ((marray = calloc(maxmagic, sizeof(*marray))) == NULL) {
		(void)fclose(f);
		file_oomem(ms, maxmagic * sizeof(*marray));
		return -1;
	}
	marraycount = 0;

	/* print silly verbose header for USG compat. */
	if (action == FILE_CHECK)
		(void)fprintf(stderr, "%s\n", hdr);

	/* read and parse this file */
	for (ms->line = 1; fgets(line, BUFSIZ, f) != NULL; ms->line++) {
		size_t len;
		len = strlen(line);
		if (len == 0) /* null line, garbage, etc */
			continue;
		if (line[len - 1] == '\n') {
			lineno++;
			line[len - 1] = '\0'; /* delete newline */
		}
		if (line[0] == '\0')	/* empty, do not parse */
			continue;
		if (line[0] == '#')	/* comment, do not parse */
			continue;
		if (parse(ms, &marray, &marraycount, line, lineno, action) != 0)
			errs++;
	}

	(void)fclose(f);
	if (errs)
		goto out;

#ifndef NOORDER
	qsort(marray, marraycount, sizeof(*marray), apprentice_sort);
#endif

	for (i = 0; i < marraycount; i++)
		mentrycount += marray[i].cont_count;

	if ((*magicp = malloc(sizeof(**magicp) * mentrycount)) == NULL) {
		file_oomem(ms, sizeof(**magicp) * mentrycount);
		errs++;
		goto out;
	}

	mentrycount = 0;
	for (i = 0; i < marraycount; i++) {
		(void)memcpy(*magicp + mentrycount, marray[i].mp,
		    marray[i].cont_count * sizeof(**magicp));
		mentrycount += marray[i].cont_count;
	}
out:
	for (i = 0; i < marraycount; i++)
		free(marray[i].mp);
	free(marray);
	if (errs) {
		*magicp = NULL;
		*nmagicp = 0;
		return errs;
	} else {
		*nmagicp = mentrycount;
		return 0;
	}

}

/*
 * extend the sign bit if the comparison is to be signed
 */
protected uint64_t
file_signextend(struct magic_set *ms, struct magic *m, uint64_t v)
{
	if (!(m->flag & UNSIGNED))
		switch(m->type) {
		/*
		 * Do not remove the casts below.  They are
		 * vital.  When later compared with the data,
		 * the sign extension must have happened.
		 */
		case FILE_BYTE:
			v = (char) v;
			break;
		case FILE_SHORT:
		case FILE_BESHORT:
		case FILE_LESHORT:
			v = (short) v;
			break;
		case FILE_DATE:
		case FILE_BEDATE:
		case FILE_LEDATE:
		case FILE_MEDATE:
		case FILE_LDATE:
		case FILE_BELDATE:
		case FILE_LELDATE:
		case FILE_MELDATE:
		case FILE_LONG:
		case FILE_BELONG:
		case FILE_LELONG:
		case FILE_MELONG:
			v = (int32_t) v;
			break;
		case FILE_QUAD:
		case FILE_BEQUAD:
		case FILE_LEQUAD:
		case FILE_QDATE:
		case FILE_QLDATE:
		case FILE_BEQDATE:
		case FILE_BEQLDATE:
		case FILE_LEQDATE:
		case FILE_LEQLDATE:
			v = (int64_t) v;
			break;
		case FILE_STRING:
		case FILE_PSTRING:
		case FILE_BESTRING16:
		case FILE_LESTRING16:
		case FILE_REGEX:
		case FILE_SEARCH:
			break;
		default:
			if (ms->flags & MAGIC_CHECK)
			    file_magwarn(ms, "cannot happen: m->type=%d\n",
				    m->type);
			return ~0U;
		}
	return v;
}

/*
 * parse one line from magic file, put into magic[index++] if valid
 */
private int
parse(struct magic_set *ms, struct magic_entry **mentryp, uint32_t *nmentryp, 
    const char *line, size_t lineno, int action)
{
	size_t i;
	struct magic_entry *me;
	struct magic *m;
	const char *l = line;
	char *t;
	private const char *fops = FILE_OPS;
	uint64_t val;
	uint32_t cont_level;

	cont_level = 0;

	while (*l == '>') {
		++l;		/* step over */
		cont_level++; 
	}

#define ALLOC_CHUNK	(size_t)10
#define ALLOC_INCR	(size_t)200

	if (cont_level != 0) {
		if (*nmentryp == 0) {
			file_error(ms, 0, "No current entry for continuation");
			return -1;
		}
		me = &(*mentryp)[*nmentryp - 1];
		if (me->cont_count == me->max_count) {
			struct magic *nm;
			size_t cnt = me->max_count + ALLOC_CHUNK;
			if ((nm = realloc(me->mp, sizeof(*nm) * cnt)) == NULL) {
				file_oomem(ms, sizeof(*nm) * cnt);
				return -1;
			}
			me->mp = m = nm;
			me->max_count = cnt;
		}
		m = &me->mp[me->cont_count++];
		memset(m, 0, sizeof(*m));
		m->cont_level = cont_level;
	} else {
		if (*nmentryp == maxmagic) {
			struct magic_entry *mp;

			maxmagic += ALLOC_INCR;
			if ((mp = realloc(*mentryp, sizeof(*mp) * maxmagic)) ==
			    NULL) {
				file_oomem(ms, sizeof(*mp) * maxmagic);
				return -1;
			}
			(void)memset(&mp[*nmentryp], 0, sizeof(*mp) *
			    ALLOC_INCR);
			*mentryp = mp;
		}
		me = &(*mentryp)[*nmentryp];
		if (me->mp == NULL) {
			if ((m = malloc(sizeof(*m) * ALLOC_CHUNK)) == NULL) {
				file_oomem(ms, sizeof(*m) * ALLOC_CHUNK);
				return -1;
			}
			me->mp = m;
			me->max_count = ALLOC_CHUNK;
		} else
			m = me->mp;
		memset(m, 0, sizeof(*m));
		m->cont_level = 0;
		me->cont_count = 1;
	}
	m->lineno = lineno;

	if (m->cont_level != 0 && *l == '&') {
                ++l;            /* step over */
                m->flag |= OFFADD;
        }
	if (m->cont_level != 0 && *l == '(') {
		++l;		/* step over */
		m->flag |= INDIR;
		if (m->flag & OFFADD)
			m->flag = (m->flag & ~OFFADD) | INDIROFFADD;
	}
	if (m->cont_level != 0 && *l == '&') {
                ++l;            /* step over */
                m->flag |= OFFADD;
        }

	/* get offset, then skip over it */
	m->offset = (uint32_t)strtoul(l, &t, 0);
        if (l == t)
		if (ms->flags & MAGIC_CHECK)
			file_magwarn(ms, "offset `%s' invalid", l);
        l = t;

	if (m->flag & INDIR) {
		m->in_type = FILE_LONG;
		m->in_offset = 0;
		/*
		 * read [.lbs][+-]nnnnn)
		 */
		if (*l == '.') {
			l++;
			switch (*l) {
			case 'l':
				m->in_type = FILE_LELONG;
				break;
			case 'L':
				m->in_type = FILE_BELONG;
				break;
			case 'm':
				m->in_type = FILE_MELONG;
				break;
			case 'h':
			case 's':
				m->in_type = FILE_LESHORT;
				break;
			case 'H':
			case 'S':
				m->in_type = FILE_BESHORT;
				break;
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				m->in_type = FILE_BYTE;
				break;
			default:
				if (ms->flags & MAGIC_CHECK)
					file_magwarn(ms,
					    "indirect offset type `%c' invalid",
					    *l);
				break;
			}
			l++;
		}
		if (*l == '~') {
			m->in_op |= FILE_OPINVERSE;
			l++;
		}
		switch (*l) {
		case '&':
			m->in_op |= FILE_OPAND;
			l++;
			break;
		case '|':
			m->in_op |= FILE_OPOR;
			l++;
			break;
		case '^':
			m->in_op |= FILE_OPXOR;
			l++;
			break;
		case '+':
			m->in_op |= FILE_OPADD;
			l++;
			break;
		case '-':
			m->in_op |= FILE_OPMINUS;
			l++;
			break;
		case '*':
			m->in_op |= FILE_OPMULTIPLY;
			l++;
			break;
		case '/':
			m->in_op |= FILE_OPDIVIDE;
			l++;
			break;
		case '%':
			m->in_op |= FILE_OPMODULO;
			l++;
			break;
		}
		if (*l == '(') {
			m->in_op |= FILE_OPINDIRECT;
			l++;
		}
		if (isdigit((unsigned char)*l) || *l == '-') {
			m->in_offset = (int32_t)strtol(l, &t, 0);
			l = t;
		}
		if (*l++ != ')' || 
		    ((m->in_op & FILE_OPINDIRECT) && *l++ != ')'))
			if (ms->flags & MAGIC_CHECK)
				file_magwarn(ms,
				    "missing ')' in indirect offset");
	}


	while (isascii((unsigned char)*l) && isdigit((unsigned char)*l))
		++l;
	EATAB;

	if (*l == 'u') {
		++l;
		m->flag |= UNSIGNED;
	}

	/* get type, skip it */
	for (i = 0; i < file_nnames; i++) {
		size_t len = strlen(file_names[i]);
		if (strncmp(l, file_names[i], len) == 0) {
			m->type = i;
			l+= len;
			break;
		}
	}
	if (i == file_nnames) {
		if (ms->flags & MAGIC_CHECK)
			file_magwarn(ms, "type `%s' invalid", l);
		return -1;
	}
	/* New-style anding: "0 byte&0x80 =0x80 dynamically linked" */
	/* New and improved: ~ & | ^ + - * / % -- exciting, isn't it? */
	if (*l == '~') {
		if (!IS_STRING(m->type))
			m->mask_op |= FILE_OPINVERSE;
		++l;
	}
	if ((t = strchr(fops,  *l)) != NULL) {
		uint32_t op = (uint32_t)(t - fops);
		if (op != FILE_OPDIVIDE || !IS_PLAINSTRING(m->type)) {
			++l;
			m->mask_op |= op;
			val = (uint64_t)strtoull(l, &t, 0);
			l = t;
			m->mask = file_signextend(ms, m, val);
			eatsize(&l);
		} else {
			m->mask = 0L;
			while (!isspace((unsigned char)*++l)) {
				switch (*l) {
				case CHAR_IGNORE_LOWERCASE:
					m->mask |= STRING_IGNORE_LOWERCASE;
					break;
				case CHAR_COMPACT_BLANK:
					m->mask |= STRING_COMPACT_BLANK;
					break;
				case CHAR_COMPACT_OPTIONAL_BLANK:
					m->mask |=
					    STRING_COMPACT_OPTIONAL_BLANK;
					break;
				default:
					if (ms->flags & MAGIC_CHECK)
						file_magwarn(ms,
						"string extension `%c' invalid",
						*l);
					return -1;
				}
			}
			++l;
		}
	}
	/*
	 * We used to set mask to all 1's here, instead let's just not do
	 * anything if mask = 0 (unless you have a better idea)
	 */
	EATAB;
  
	switch (*l) {
	case '>':
	case '<':
	/* Old-style anding: "0 byte &0x80 dynamically linked" */
	case '&':
	case '^':
	case '=':
  		m->reln = *l;
  		++l;
		if (*l == '=') {
		   /* HP compat: ignore &= etc. */
		   ++l;
		}
		break;
	case '!':
		m->reln = *l;
		++l;
		break;
	default:
		if (*l == 'x' && ((isascii((unsigned char)l[1]) && 
		    isspace((unsigned char)l[1])) || !l[1])) {
			m->reln = *l;
			++l;
			goto GetDesc;	/* Bill The Cat */
		}
  		m->reln = '=';
		break;
	}
  	EATAB;
  
	if (getvalue(ms, m, &l))
		return -1;
	/*
	 * TODO finish this macro and start using it!
	 * #define offsetcheck {if (offset > HOWMANY-1) 
	 *	magwarn("offset too big"); }
	 */

	/*
	 * now get last part - the description
	 */
GetDesc:
	EATAB;
	if (l[0] == '\b') {
		++l;
		m->nospflag = 1;
	} else if ((l[0] == '\\') && (l[1] == 'b')) {
		++l;
		++l;
		m->nospflag = 1;
	} else
		m->nospflag = 0;
	for (i = 0; (m->desc[i++] = *l++) != '\0' && i < sizeof(m->desc); )
		continue;
	if (i == sizeof(m->desc)) {
		m->desc[sizeof(m->desc) - 1] = '\0';
		if (ms->flags & MAGIC_CHECK)
			file_magwarn(ms, "description `%s' truncated", m->desc);
	}

        /*
	 * We only do this check while compiling, or if any of the magic
	 * files were not compiled.
         */
        if (ms->flags & MAGIC_CHECK) {
		if (check_format(ms, m) == -1)
			return -1;
	}
#ifndef COMPILE_ONLY
	if (action == FILE_CHECK) {
		file_mdump(m);
	}
#endif
	if (m->cont_level == 0)
		++(*nmentryp);		/* make room for next */
	return 0;
}

private int
check_format_type(const char *ptr, int type)
{
	int quad = 0;
	if (*ptr == '\0') {
		/* Missing format string; bad */
		return -1;
	}

	switch (type) {
	case FILE_FMT_QUAD:
		quad = 1;
		/*FALLTHROUGH*/
	case FILE_FMT_NUM:
		if (*ptr == '-')
			ptr++;
		if (*ptr == '.')
			ptr++;
		while (isdigit((unsigned char)*ptr)) ptr++;
		if (*ptr == '.')
			ptr++;
		while (isdigit((unsigned char)*ptr)) ptr++;
		if (quad) {
			if (*ptr++ != 'l')
				return -1;
			if (*ptr++ != 'l')
				return -1;
		}
	
		switch (*ptr++) {
		case 'l':
			switch (*ptr++) {
			case 'i':
			case 'd':
			case 'u':
			case 'x':
			case 'X':
				return 0;
			default:
				return -1;
			}
		
		case 'h':
			switch (*ptr++) {
			case 'h':
				switch (*ptr++) {
				case 'i':
				case 'd':
				case 'u':
				case 'x':
				case 'X':
					return 0;
				default:
					return -1;
				}
			case 'd':
				return 0;
			default:
				return -1;
			}

		case 'i':
		case 'c':
		case 'd':
		case 'u':
		case 'x':
		case 'X':
			return 0;
			
		default:
			return -1;
		}
		
	case FILE_FMT_STR:
		if (*ptr == '-')
			ptr++;
		while (isdigit((unsigned char )*ptr))
			ptr++;
		if (*ptr == '.') {
			ptr++;
			while (isdigit((unsigned char )*ptr))
				ptr++;
		}
		
		switch (*ptr++) {
		case 's':
			return 0;
		default:
			return -1;
		}
		
	default:
		/* internal error */
		abort();
	}
	/*NOTREACHED*/
	return -1;
}
	
/*
 * Check that the optional printf format in description matches
 * the type of the magic.
 */
private int
check_format(struct magic_set *ms, struct magic *m)
{
	char *ptr;

	for (ptr = m->desc; *ptr; ptr++)
		if (*ptr == '%')
			break;
	if (*ptr == '\0') {
		/* No format string; ok */
		return 1;
	}

	assert(file_nformats == file_nnames);

	if (m->type >= file_nformats) {
		file_error(ms, 0, "Internal error inconsistency between "
		    "m->type and format strings");		
		return -1;
	}
	if (file_formats[m->type] == FILE_FMT_NONE) {
		file_error(ms, 0, "No format string for `%s' with description "
		    "`%s'", m->desc, file_names[m->type]);
		return -1;
	}

	ptr++;
	if (check_format_type(ptr, file_formats[m->type]) == -1) {
		/*
		 * TODO: this error message is unhelpful if the format
		 * string is not one character long
		 */
		file_error(ms, 0, "Printf format `%c' is not valid for type "
		    " `%s' in description `%s'", *ptr,
		    file_names[m->type], m->desc);
		return -1;
	}
	
	for (; *ptr; ptr++) {
		if (*ptr == '%') {
			file_error(ms, 0,
			    "Too many format strings (should have at most one) "
			    "for `%s' with description `%s'",
			    file_names[m->type], m->desc);
			return -1;
		}
	}
	return 0;
}

/* 
 * Read a numeric value from a pointer, into the value union of a magic 
 * pointer, according to the magic type.  Update the string pointer to point 
 * just after the number read.  Return 0 for success, non-zero for failure.
 */
private int
getvalue(struct magic_set *ms, struct magic *m, const char **p)
{
	int slen;

	switch (m->type) {
	case FILE_BESTRING16:
	case FILE_LESTRING16:
	case FILE_STRING:
	case FILE_PSTRING:
	case FILE_REGEX:
	case FILE_SEARCH:
		*p = getstr(ms, *p, m->value.s, sizeof(m->value.s), &slen);
		if (*p == NULL) {
			if (ms->flags & MAGIC_CHECK)
				file_magwarn(ms, "cannot get string from `%s'",
				    m->value.s);
			return -1;
		}
		m->vallen = slen;
		return 0;
	default:
		if (m->reln != 'x') {
			char *ep;
			m->value.q = file_signextend(ms, m,
			    (uint64_t)strtoull(*p, &ep, 0));
			*p = ep;
			eatsize(p);
		}
		return 0;
	}
}

/*
 * Convert a string containing C character escapes.  Stop at an unescaped
 * space or tab.
 * Copy the converted version to "p", returning its length in *slen.
 * Return updated scan pointer as function result.
 */
private const char *
getstr(struct magic_set *ms, const char *s, char *p, int plen, int *slen)
{
	const char *origs = s;
	char 	*origp = p;
	char	*pmax = p + plen - 1;
	int	c;
	int	val;

	while ((c = *s++) != '\0') {
		if (isspace((unsigned char) c))
			break;
		if (p >= pmax) {
			file_error(ms, 0, "string too long: `%s'", origs);
			return NULL;
		}
		if(c == '\\') {
			switch(c = *s++) {

			case '\0':
				goto out;

			default:
				*p++ = (char) c;
				break;

			case 'n':
				*p++ = '\n';
				break;

			case 'r':
				*p++ = '\r';
				break;

			case 'b':
				*p++ = '\b';
				break;

			case 't':
				*p++ = '\t';
				break;

			case 'f':
				*p++ = '\f';
				break;

			case 'v':
				*p++ = '\v';
				break;

			/* \ and up to 3 octal digits */
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				val = c - '0';
				c = *s++;  /* try for 2 */
				if(c >= '0' && c <= '7') {
					val = (val<<3) | (c - '0');
					c = *s++;  /* try for 3 */
					if(c >= '0' && c <= '7')
						val = (val<<3) | (c-'0');
					else
						--s;
				}
				else
					--s;
				*p++ = (char)val;
				break;

			/* \x and up to 2 hex digits */
			case 'x':
				val = 'x';	/* Default if no digits */
				c = hextoint(*s++);	/* Get next char */
				if (c >= 0) {
					val = c;
					c = hextoint(*s++);
					if (c >= 0)
						val = (val << 4) + c;
					else
						--s;
				} else
					--s;
				*p++ = (char)val;
				break;
			}
		} else
			*p++ = (char)c;
	}
out:
	*p = '\0';
	*slen = p - origp;
	return s;
}


/* Single hex char to int; -1 if not a hex char. */
private int
hextoint(int c)
{
	if (!isascii((unsigned char) c))
		return -1;
	if (isdigit((unsigned char) c))
		return c - '0';
	if ((c >= 'a')&&(c <= 'f'))
		return c + 10 - 'a';
	if (( c>= 'A')&&(c <= 'F'))
		return c + 10 - 'A';
	return -1;
}


/*
 * Print a string containing C character escapes.
 */
protected void
file_showstr(FILE *fp, const char *s, size_t len)
{
	char	c;

	for (;;) {
		c = *s++;
		if (len == ~0U) {
			if (c == '\0')
				break;
		}
		else  {
			if (len-- == 0)
				break;
		}
		if(c >= 040 && c <= 0176)	/* TODO isprint && !iscntrl */
			(void) fputc(c, fp);
		else {
			(void) fputc('\\', fp);
			switch (c) {
			
			case '\n':
				(void) fputc('n', fp);
				break;

			case '\r':
				(void) fputc('r', fp);
				break;

			case '\b':
				(void) fputc('b', fp);
				break;

			case '\t':
				(void) fputc('t', fp);
				break;

			case '\f':
				(void) fputc('f', fp);
				break;

			case '\v':
				(void) fputc('v', fp);
				break;

			default:
				(void) fprintf(fp, "%.3o", c & 0377);
				break;
			}
		}
	}
}

/*
 * eatsize(): Eat the size spec from a number [eg. 10UL]
 */
private void
eatsize(const char **p)
{
	const char *l = *p;

	if (LOWCASE(*l) == 'u') 
		l++;

	switch (LOWCASE(*l)) {
	case 'l':    /* long */
	case 's':    /* short */
	case 'h':    /* short */
	case 'b':    /* char/byte */
	case 'c':    /* char/byte */
		l++;
		/*FALLTHROUGH*/
	default:
		break;
	}

	*p = l;
}

/*
 * handle a compiled file.
 */
private int
apprentice_map(struct magic_set *ms, struct magic **magicp, uint32_t *nmagicp,
    const char *fn)
{
	int fd;
	struct stat st;
	uint32_t *ptr;
	uint32_t version;
	int needsbyteswap;
	char buf[MAXPATHLEN];
	char *dbname = mkdbname(fn, buf, sizeof(buf), 0);
	void *mm = NULL;

	if (dbname == NULL)
		return -1;

	if ((fd = open(dbname, O_RDONLY|O_BINARY)) == -1)
		return -1;

	if (fstat(fd, &st) == -1) {
		file_error(ms, errno, "cannot stat `%s'", dbname);
		goto error;
	}
	if (st.st_size < 16) {
		file_error(ms, 0, "file `%s' is too small", dbname);
		goto error;
	}

#ifdef QUICK
	if ((mm = mmap(0, (size_t)st.st_size, PROT_READ|PROT_WRITE,
	    MAP_PRIVATE|MAP_FILE, fd, (off_t)0)) == MAP_FAILED) {
		file_error(ms, errno, "cannot map `%s'", dbname);
		goto error;
	}
#define RET	2
#else
	if ((mm = malloc((size_t)st.st_size)) == NULL) {
		file_oomem(ms, (size_t)st.st_size);
		goto error;
	}
	if (read(fd, mm, (size_t)st.st_size) != (size_t)st.st_size) {
		file_badread(ms);
		goto error;
	}
#define RET	1
#endif
	*magicp = mm;
	(void)close(fd);
	fd = -1;
	ptr = (uint32_t *)(void *)*magicp;
	if (*ptr != MAGICNO) {
		if (swap4(*ptr) != MAGICNO) {
			file_error(ms, 0, "bad magic in `%s'");
			goto error;
		}
		needsbyteswap = 1;
	} else
		needsbyteswap = 0;
	if (needsbyteswap)
		version = swap4(ptr[1]);
	else
		version = ptr[1];
	if (version != VERSIONNO) {
		file_error(ms, 0, "version mismatch (%d != %d) in `%s'",
		    version, VERSIONNO, dbname);
		goto error;
	}
	*nmagicp = (uint32_t)(st.st_size / sizeof(struct magic)) - 1;
	(*magicp)++;
	if (needsbyteswap)
		byteswap(*magicp, *nmagicp);
	return RET;

error:
	if (fd != -1)
		(void)close(fd);
	if (mm) {
#ifdef QUICK
		(void)munmap((void *)mm, (size_t)st.st_size);
#else
		free(mm);
#endif
	} else {
		*magicp = NULL;
		*nmagicp = 0;
	}
	return -1;
}

private const uint32_t ar[] = {
    MAGICNO, VERSIONNO
};
/*
 * handle an mmaped file.
 */
private int
apprentice_compile(struct magic_set *ms, struct magic **magicp,
    uint32_t *nmagicp, const char *fn)
{
	int fd;
	char buf[MAXPATHLEN];
	char *dbname = mkdbname(fn, buf, sizeof(buf), 1);

	if (dbname == NULL) 
		return -1;

	if ((fd = open(dbname, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644)) == -1) {
		file_error(ms, errno, "cannot open `%s'", dbname);
		return -1;
	}

	if (write(fd, ar, sizeof(ar)) != (ssize_t)sizeof(ar)) {
		file_error(ms, errno, "error writing `%s'", dbname);
		return -1;
	}

	if (lseek(fd, (off_t)sizeof(struct magic), SEEK_SET)
	    != sizeof(struct magic)) {
		file_error(ms, errno, "error seeking `%s'", dbname);
		return -1;
	}

	if (write(fd, *magicp, (sizeof(struct magic) * *nmagicp)) 
	    != (ssize_t)(sizeof(struct magic) * *nmagicp)) {
		file_error(ms, errno, "error writing `%s'", dbname);
		return -1;
	}

	(void)close(fd);
	return 0;
}

private const char ext[] = ".mgc";
/*
 * make a dbname
 */
private char *
mkdbname(const char *fn, char *buf, size_t bufsiz, int strip)
{
	if (strip) {
		const char *p;
		if ((p = strrchr(fn, '/')) != NULL)
			fn = ++p;
	}

	(void)snprintf(buf, bufsiz, "%s%s", fn, ext);
	return buf;
}

/*
 * Byteswap an mmap'ed file if needed
 */
private void
byteswap(struct magic *magic, uint32_t nmagic)
{
	uint32_t i;
	for (i = 0; i < nmagic; i++)
		bs1(&magic[i]);
}

/*
 * swap a short
 */
private uint16_t
swap2(uint16_t sv)
{
	uint16_t rv;
	uint8_t *s = (uint8_t *)(void *)&sv; 
	uint8_t *d = (uint8_t *)(void *)&rv; 
	d[0] = s[1];
	d[1] = s[0];
	return rv;
}

/*
 * swap an int
 */
private uint32_t
swap4(uint32_t sv)
{
	uint32_t rv;
	uint8_t *s = (uint8_t *)(void *)&sv; 
	uint8_t *d = (uint8_t *)(void *)&rv; 
	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];
	return rv;
}

/*
 * swap a quad
 */
private uint64_t
swap8(uint64_t sv)
{
	uint32_t rv;
	uint8_t *s = (uint8_t *)(void *)&sv; 
	uint8_t *d = (uint8_t *)(void *)&rv; 
	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];
	d[4] = s[7];
	d[5] = s[6];
	d[6] = s[5];
	d[7] = s[4];
	return rv;
}

/*
 * byteswap a single magic entry
 */
private void
bs1(struct magic *m)
{
	m->cont_level = swap2(m->cont_level);
	m->offset = swap4((uint32_t)m->offset);
	m->in_offset = swap4((uint32_t)m->in_offset);
	if (!IS_STRING(m->type))
		m->value.q = swap8(m->value.q);
	m->mask = swap8(m->mask);
}
