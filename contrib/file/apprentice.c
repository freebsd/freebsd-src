/*
 * apprentice - make one pass through /etc/magic, learning its secrets.
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef QUICK
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif
#include "file.h"

#ifndef	lint
FILE_RCSID("@(#)$Id: apprentice.c,v 1.39 2001/04/24 14:40:24 christos Exp $")
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

#ifdef __EMX__
  char PATHSEP=';';
#else
  char PATHSEP=':';
#endif


static int getvalue	__P((struct magic *, char **));
static int hextoint	__P((int));
static char *getstr	__P((char *, char *, int, int *));
static int parse	__P((struct magic **, uint32 *, char *, int));
static void eatsize	__P((char **));
static int apprentice_1	__P((const char *, int));
static int apprentice_file	__P((struct magic **, uint32 *,
    const char *, int));
#ifdef QUICK
static void byteswap	__P((struct magic *, uint32));
static void bs1		__P((struct magic *));
static uint16 swap2	__P((uint16));
static uint32 swap4	__P((uint32));
static char * mkdbname	__P((const char *));
static int apprentice_map	__P((struct magic **, uint32 *,
    const char *, int));
static int apprentice_compile	__P((struct magic **, uint32 *,
    const char *, int));
#endif

static int maxmagic = 0;

struct mlist mlist;


/*
 * Handle one file.
 */
static int
apprentice_1(fn, action)
	const char *fn;
	int action;
{
	struct magic *magic = NULL;
	uint32 nmagic = 0;
	struct mlist *ml;
	int rv = -1;

#ifdef QUICK
	if (action == COMPILE) {
		rv = apprentice_file(&magic, &nmagic, fn, action);
		if (rv == 0)
			return apprentice_compile(&magic, &nmagic, fn, action);
		else
			return rv;
	}
	if ((rv = apprentice_map(&magic, &nmagic, fn, action)) != 0)
		(void)fprintf(stderr, "%s: Using regular magic file `%s'\n",
		    progname, fn);
#endif
		
	if (rv != 0)
		rv = apprentice_file(&magic, &nmagic, fn, action);

	if (rv != 0)
		return rv;
	     
	if ((ml = malloc(sizeof(*ml))) == NULL) {
		(void) fprintf(stderr, "%s: Out of memory.\n", progname);
		if (action == CHECK)
			return -1;
	}

	if (magic == NULL || nmagic == 0)
		return rv;

	ml->magic = magic;
	ml->nmagic = nmagic;

	mlist.prev->next = ml;
	ml->prev = mlist.prev;
	ml->next = &mlist;
	mlist.prev = ml;

	return rv;
}


int
apprentice(fn, action)
	const char *fn;			/* list of magic files */
	int action;
{
	char *p, *mfn;
	int file_err, errs = -1;

	mlist.next = mlist.prev = &mlist;
	mfn = malloc(strlen(fn)+1);
	if (mfn == NULL) {
		(void) fprintf(stderr, "%s: Out of memory.\n", progname);
		if (action == CHECK)
			return -1;
		else
			exit(1);
	}
	fn = strcpy(mfn, fn);
  
	while (fn) {
		p = strchr(fn, PATHSEP);
		if (p)
			*p++ = '\0';
		file_err = apprentice_1(fn, action);
		if (file_err > errs)
			errs = file_err;
		fn = p;
	}
	if (errs == -1)
		(void) fprintf(stderr, "%s: couldn't find any magic files!\n",
		    progname);
	if (action == CHECK && errs)
		exit(1);

	free(mfn);
	return errs;
}

/*
 * parse from a file
 */
static int
apprentice_file(magicp, nmagicp, fn, action)
	struct magic **magicp;
	uint32 *nmagicp;
	const char *fn;			/* name of magic file */
	int action;
{
	static const char hdr[] =
		"cont\toffset\ttype\topcode\tmask\tvalue\tdesc";
	FILE *f;
	char line[BUFSIZ+1];
	int errs = 0;

	f = fopen(fn, "r");
	if (f == NULL) {
		if (errno != ENOENT)
			(void) fprintf(stderr,
			    "%s: can't read magic file %s (%s)\n", 
			    progname, fn, strerror(errno));
		return -1;
	}

        maxmagic = MAXMAGIS;
	*magicp = (struct magic *) calloc(sizeof(struct magic), maxmagic);
	if (*magicp == NULL) {
		(void) fprintf(stderr, "%s: Out of memory.\n", progname);
		if (action == CHECK)
			return -1;
	}

	/* parse it */
	if (action == CHECK)	/* print silly verbose header for USG compat. */
		(void) printf("%s\n", hdr);

	for (lineno = 1;fgets(line, BUFSIZ, f) != NULL; lineno++) {
		if (line[0]=='#')	/* comment, do not parse */
			continue;
		if (strlen(line) <= (unsigned)1) /* null line, garbage, etc */
			continue;
		line[strlen(line)-1] = '\0'; /* delete newline */
		if (parse(magicp, nmagicp, line, action) != 0)
			errs = 1;
	}

	(void) fclose(f);
	if (errs) {
		free(*magicp);
		*magicp = NULL;
		*nmagicp = 0;
	}
	return errs;
}

/*
 * extend the sign bit if the comparison is to be signed
 */
uint32
signextend(m, v)
	struct magic *m;
	uint32 v;
{
	if (!(m->flag & UNSIGNED))
		switch(m->type) {
		/*
		 * Do not remove the casts below.  They are
		 * vital.  When later compared with the data,
		 * the sign extension must have happened.
		 */
		case BYTE:
			v = (char) v;
			break;
		case SHORT:
		case BESHORT:
		case LESHORT:
			v = (short) v;
			break;
		case DATE:
		case BEDATE:
		case LEDATE:
		case LONG:
		case BELONG:
		case LELONG:
			v = (int32) v;
			break;
		case STRING:
			break;
		default:
			magwarn("can't happen: m->type=%d\n",
				m->type);
			return -1;
		}
	return v;
}

/*
 * parse one line from magic file, put into magic[index++] if valid
 */
static int
parse(magicp, nmagicp, l, action)
	struct magic **magicp;
	uint32 *nmagicp;
	char *l;
	int action;
{
	int i = 0;
	struct magic *m;
	char *t, *s;

#define ALLOC_INCR	200
	if (*nmagicp + 1 >= maxmagic){
		maxmagic += ALLOC_INCR;
		if ((m = (struct magic *) realloc(*magicp,
		    sizeof(struct magic) * maxmagic)) == NULL) {
			(void) fprintf(stderr, "%s: Out of memory.\n",
			    progname);
			if (*magicp)
				free(*magicp);
			if (action == CHECK)
				return -1;
			else
				exit(1);
		}
		*magicp = m;
		memset(&(*magicp)[*nmagicp], 0, sizeof(struct magic)
		    * ALLOC_INCR);
	}
	m = &(*magicp)[*nmagicp];
	m->flag = 0;
	m->cont_level = 0;

	while (*l == '>') {
		++l;		/* step over */
		m->cont_level++; 
	}

	if (m->cont_level != 0 && *l == '(') {
		++l;		/* step over */
		m->flag |= INDIR;
	}
	if (m->cont_level != 0 && *l == '&') {
                ++l;            /* step over */
                m->flag |= ADD;
        }

	/* get offset, then skip over it */
	m->offset = (int) strtoul(l,&t,0);
        if (l == t)
		magwarn("offset %s invalid", l);
        l = t;

	if (m->flag & INDIR) {
		m->in_type = LONG;
		m->in_offset = 0;
		/*
		 * read [.lbs][+-]nnnnn)
		 */
		if (*l == '.') {
			l++;
			switch (*l) {
			case 'l':
				m->in_type = LELONG;
				break;
			case 'L':
				m->in_type = BELONG;
				break;
			case 'h':
			case 's':
				m->in_type = LESHORT;
				break;
			case 'H':
			case 'S':
				m->in_type = BESHORT;
				break;
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				m->in_type = BYTE;
				break;
			default:
				magwarn("indirect offset type %c invalid", *l);
				break;
			}
			l++;
		}
		s = l;
		if (*l == '+' || *l == '-') l++;
		if (isdigit((unsigned char)*l)) {
			m->in_offset = strtoul(l, &t, 0);
			if (*s == '-') m->in_offset = - m->in_offset;
		}
		else
			t = l;
		if (*t++ != ')') 
			magwarn("missing ')' in indirect offset");
		l = t;
	}


	while (isascii((unsigned char)*l) && isdigit((unsigned char)*l))
		++l;
	EATAB;

#define NBYTE		4
#define NSHORT		5
#define NLONG		4
#define NSTRING 	6
#define NDATE		4
#define NBESHORT	7
#define NBELONG		6
#define NBEDATE		6
#define NLESHORT	7
#define NLELONG		6
#define NLEDATE		6

	if (*l == 'u') {
		++l;
		m->flag |= UNSIGNED;
	}

	/* get type, skip it */
	if (strncmp(l, "char", NBYTE)==0) {	/* HP/UX compat */
		m->type = BYTE;
		l += NBYTE;
	} else if (strncmp(l, "byte", NBYTE)==0) {
		m->type = BYTE;
		l += NBYTE;
	} else if (strncmp(l, "short", NSHORT)==0) {
		m->type = SHORT;
		l += NSHORT;
	} else if (strncmp(l, "long", NLONG)==0) {
		m->type = LONG;
		l += NLONG;
	} else if (strncmp(l, "string", NSTRING)==0) {
		m->type = STRING;
		l += NSTRING;
	} else if (strncmp(l, "date", NDATE)==0) {
		m->type = DATE;
		l += NDATE;
	} else if (strncmp(l, "beshort", NBESHORT)==0) {
		m->type = BESHORT;
		l += NBESHORT;
	} else if (strncmp(l, "belong", NBELONG)==0) {
		m->type = BELONG;
		l += NBELONG;
	} else if (strncmp(l, "bedate", NBEDATE)==0) {
		m->type = BEDATE;
		l += NBEDATE;
	} else if (strncmp(l, "leshort", NLESHORT)==0) {
		m->type = LESHORT;
		l += NLESHORT;
	} else if (strncmp(l, "lelong", NLELONG)==0) {
		m->type = LELONG;
		l += NLELONG;
	} else if (strncmp(l, "ledate", NLEDATE)==0) {
		m->type = LEDATE;
		l += NLEDATE;
	} else {
		magwarn("type %s invalid", l);
		return -1;
	}
	/* New-style anding: "0 byte&0x80 =0x80 dynamically linked" */
	if (*l == '&') {
		++l;
		m->mask = signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
	} else if (STRING == m->type) {
		m->mask = 0L;
		if (*l == '/') { 
			while (!isspace(*++l)) {
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
					magwarn("string extension %c invalid",
					    *l);
					return -1;
				}
			}
		}
	} else
		m->mask = ~0L;
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
		if (m->type != STRING) {
			m->reln = *l;
			++l;
			break;
		}
		/* FALL THROUGH */
	default:
		if (*l == 'x' && isascii((unsigned char)l[1]) && 
		    isspace((unsigned char)l[1])) {
			m->reln = *l;
			++l;
			goto GetDesc;	/* Bill The Cat */
		}
  		m->reln = '=';
		break;
	}
  	EATAB;
  
	if (getvalue(m, &l))
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
	while ((m->desc[i++] = *l++) != '\0' && i<MAXDESC)
		/* NULLBODY */;

	if (action == CHECK) {
		mdump(m);
	}
	++(*nmagicp);		/* make room for next */
	return 0;
}

/* 
 * Read a numeric value from a pointer, into the value union of a magic 
 * pointer, according to the magic type.  Update the string pointer to point 
 * just after the number read.  Return 0 for success, non-zero for failure.
 */
static int
getvalue(m, p)
	struct magic *m;
	char **p;
{
	int slen;

	if (m->type == STRING) {
		*p = getstr(*p, m->value.s, sizeof(m->value.s), &slen);
		m->vallen = slen;
	} else
		if (m->reln != 'x') {
			m->value.l = signextend(m, strtoul(*p, p, 0));
			eatsize(p);
		}
	return 0;
}

/*
 * Convert a string containing C character escapes.  Stop at an unescaped
 * space or tab.
 * Copy the converted version to "p", returning its length in *slen.
 * Return updated scan pointer as function result.
 */
static char *
getstr(s, p, plen, slen)
	char	*s;
	char	*p;
	int	plen, *slen;
{
	char	*origs = s, *origp = p;
	char	*pmax = p + plen - 1;
	int	c;
	int	val;

	while ((c = *s++) != '\0') {
		if (isspace((unsigned char) c))
			break;
		if (p >= pmax) {
			fprintf(stderr, "String too long: %s\n", origs);
			break;
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
static int
hextoint(c)
	int c;
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
void
showstr(fp, s, len)
	FILE *fp;
	const char *s;
	int len;
{
	char	c;

	for (;;) {
		c = *s++;
		if (len == -1) {
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
static void
eatsize(p)
	char **p;
{
	char *l = *p;

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

#ifdef QUICK
/*
 * handle an mmaped file.
 */
static int
apprentice_map(magicp, nmagicp, fn, action)
	struct magic **magicp;
	uint32 *nmagicp;
	const char *fn;
	int action;
{
	int fd;
	struct stat st;
	uint32 *ptr;
	uint32 version;
	int needsbyteswap;
	char *dbname = mkdbname(fn);

	if ((fd = open(dbname, O_RDONLY)) == -1)
		return -1;

	if (fstat(fd, &st) == -1) {
		(void)fprintf(stderr, "%s: Cannot stat `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		goto error;
	}

	if ((*magicp = mmap(0, (size_t)st.st_size, PROT_READ|PROT_WRITE,
	    MAP_PRIVATE|MAP_FILE, fd, (off_t)0)) == MAP_FAILED) {
		(void)fprintf(stderr, "%s: Cannot map `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		goto error;
	}
	(void)close(fd);
	fd = -1;
	ptr = (uint32 *) *magicp;
	if (*ptr != MAGICNO) {
		if (swap4(*ptr) != MAGICNO) {
			(void)fprintf(stderr, "%s: Bad magic in `%s'\n",
			    progname, dbname);
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
		(void)fprintf(stderr, 
		    "%s: version mismatch (%d != %d) in `%s'\n",
		    progname, version, VERSIONNO, dbname);
		goto error;
	}
	*nmagicp = (st.st_size / sizeof(struct magic)) - 1;
	(*magicp)++;
	if (needsbyteswap)
		byteswap(*magicp, *nmagicp);
	return 0;

error:
	if (fd != -1)
		(void)close(fd);
	if (*magicp)
		(void)munmap(*magicp, (size_t)st.st_size);
	else {
		*magicp = NULL;
		*nmagicp = 0;
	}
	return -1;
}

/*
 * handle an mmaped file.
 */
static int
apprentice_compile(magicp, nmagicp, fn, action)
	struct magic **magicp;
	uint32 *nmagicp;
	const char *fn;
	int action;
{
	int fd;
	char *dbname = mkdbname(fn);
	static const uint32 ar[] = {
	    MAGICNO, VERSIONNO
	};

	if ((fd = open(dbname, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
		(void)fprintf(stderr, "%s: Cannot open `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		return -1;
	}

	if (write(fd, ar, sizeof(ar)) != sizeof(ar)) {
		(void)fprintf(stderr, "%s: error writing `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		return -1;
	}

	if (lseek(fd, sizeof(struct magic), SEEK_SET) != sizeof(struct magic)) {
		(void)fprintf(stderr, "%s: error seeking `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		return -1;
	}

	if (write(fd, *magicp,  sizeof(struct magic) * *nmagicp) 
	    != sizeof(struct magic) * *nmagicp) {
		(void)fprintf(stderr, "%s: error writing `%s' (%s)\n",
		    progname, dbname, strerror(errno));
		return -1;
	}

	(void)close(fd);
	return 0;
}

/*
 * make a dbname
 */
char *
mkdbname(fn)
	const char *fn;
{
	static const char ext[] = ".mgc";
	static char *buf = NULL;
	size_t len = strlen(fn) + sizeof(ext) + 1;
	if (buf == NULL)
		buf = malloc(len);
	else
		buf = realloc(buf, len);
	(void)strcpy(buf, fn);
	(void)strcat(buf, ext);
	return buf;
}

/*
 * Byteswap an mmap'ed file if needed
 */
static void
byteswap(magic, nmagic)
	struct magic *magic;
	uint32 nmagic;
{
	uint32 i;
	for (i = 0; i < nmagic; i++)
		bs1(&magic[i]);
}

/*
 * swap a short
 */
static uint16
swap2(sv) 
	uint16 sv;
{
	uint16 rv;
	uint8 *s = (uint8 *) &sv; 
	uint8 *d = (uint8 *) &rv; 
	d[0] = s[1];
	d[1] = s[0];
	return rv;
}

/*
 * swap an int
 */
static uint32
swap4(sv) 
	uint32 sv;
{
	uint32 rv;
	uint8 *s = (uint8 *) &sv; 
	uint8 *d = (uint8 *) &rv; 
	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];
	return rv;
}

/*
 * byteswap a single magic entry
 */
static
void bs1(m)
	struct magic *m;
{
	m->cont_level = swap2(m->cont_level);
	m->offset = swap4(m->offset);
	m->in_offset = swap4(m->in_offset);
	if (m->type != STRING)
		m->value.l = swap4(m->value.l);
	m->mask = swap4(m->mask);
}
#endif
