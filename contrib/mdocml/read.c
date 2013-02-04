/*	$Id: read.c,v 1.28 2012/02/16 20:51:31 joerg Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2011 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MMAP
# include <sys/stat.h>
# include <sys/mman.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "mdoc.h"
#include "man.h"
#include "main.h"

#ifndef MAP_FILE
#define	MAP_FILE	0
#endif

#define	REPARSE_LIMIT	1000

struct	buf {
	char	 	 *buf; /* binary input buffer */
	size_t		  sz; /* size of binary buffer */
};

struct	mparse {
	enum mandoclevel  file_status; /* status of current parse */
	enum mandoclevel  wlevel; /* ignore messages below this */
	int		  line; /* line number in the file */
	enum mparset	  inttype; /* which parser to use */
	struct man	 *pman; /* persistent man parser */
	struct mdoc	 *pmdoc; /* persistent mdoc parser */
	struct man	 *man; /* man parser */
	struct mdoc	 *mdoc; /* mdoc parser */
	struct roff	 *roff; /* roff parser (!NULL) */
	int		  reparse_count; /* finite interp. stack */
	mandocmsg	  mmsg; /* warning/error message handler */
	void		 *arg; /* argument to mmsg */
	const char	 *file; 
	struct buf	 *secondary;
};

static	void	  resize_buf(struct buf *, size_t);
static	void	  mparse_buf_r(struct mparse *, struct buf, int);
static	void	  mparse_readfd_r(struct mparse *, int, const char *, int);
static	void	  pset(const char *, int, struct mparse *);
static	int	  read_whole_file(const char *, int, struct buf *, int *);
static	void	  mparse_end(struct mparse *);

static	const enum mandocerr	mandoclimits[MANDOCLEVEL_MAX] = {
	MANDOCERR_OK,
	MANDOCERR_WARNING,
	MANDOCERR_WARNING,
	MANDOCERR_ERROR,
	MANDOCERR_FATAL,
	MANDOCERR_MAX,
	MANDOCERR_MAX
};

static	const char * const	mandocerrs[MANDOCERR_MAX] = {
	"ok",

	"generic warning",

	/* related to the prologue */
	"no title in document",
	"document title should be all caps",
	"unknown manual section",
	"date missing, using today's date",
	"cannot parse date, using it verbatim",
	"prologue macros out of order",
	"duplicate prologue macro",
	"macro not allowed in prologue",
	"macro not allowed in body",

	/* related to document structure */
	".so is fragile, better use ln(1)",
	"NAME section must come first",
	"bad NAME section contents",
	"manual name not yet set",
	"sections out of conventional order",
	"duplicate section name",
	"section not in conventional manual section",

	/* related to macros and nesting */
	"skipping obsolete macro",
	"skipping paragraph macro",
	"skipping no-space macro",
	"blocks badly nested",
	"child violates parent syntax",
	"nested displays are not portable",
	"already in literal mode",
	"line scope broken",

	/* related to missing macro arguments */
	"skipping empty macro",
	"argument count wrong",
	"missing display type",
	"list type must come first",
	"tag lists require a width argument",
	"missing font type",
	"skipping end of block that is not open",

	/* related to bad macro arguments */
	"skipping argument",
	"duplicate argument",
	"duplicate display type",
	"duplicate list type",
	"unknown AT&T UNIX version",
	"bad Boolean value",
	"unknown font",
	"unknown standard specifier",
	"bad width argument",

	/* related to plain text */
	"blank line in non-literal context",
	"tab in non-literal context",
	"end of line whitespace",
	"bad comment style",
	"bad escape sequence",
	"unterminated quoted string",

	/* related to equations */
	"unexpected literal in equation",
	
	"generic error",

	/* related to equations */
	"unexpected equation scope closure",
	"equation scope open on exit",
	"overlapping equation scopes",
	"unexpected end of equation",
	"equation syntax error",

	/* related to tables */
	"bad table syntax",
	"bad table option",
	"bad table layout",
	"no table layout cells specified",
	"no table data cells specified",
	"ignore data in cell",
	"data block still open",
	"ignoring extra data cells",

	"input stack limit exceeded, infinite loop?",
	"skipping bad character",
	"escaped character not allowed in a name",
	"skipping text before the first section header",
	"skipping unknown macro",
	"NOT IMPLEMENTED, please use groff: skipping request",
	"argument count wrong",
	"skipping end of block that is not open",
	"missing end of block",
	"scope open on exit",
	"uname(3) system call failed",
	"macro requires line argument(s)",
	"macro requires body argument(s)",
	"macro requires argument(s)",
	"missing list type",
	"line argument(s) will be lost",
	"body argument(s) will be lost",

	"generic fatal error",

	"not a manual",
	"column syntax is inconsistent",
	"NOT IMPLEMENTED: .Bd -file",
	"argument count wrong, violates syntax",
	"child violates parent syntax",
	"argument count wrong, violates syntax",
	"NOT IMPLEMENTED: .so with absolute path or \"..\"",
	"no document body",
	"no document prologue",
	"static buffer exhausted",
};

static	const char * const	mandoclevels[MANDOCLEVEL_MAX] = {
	"SUCCESS",
	"RESERVED",
	"WARNING",
	"ERROR",
	"FATAL",
	"BADARG",
	"SYSERR"
};

static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz > initial/2 ? 2 * buf->sz : initial;
	buf->buf = mandoc_realloc(buf->buf, buf->sz);
}

static void
pset(const char *buf, int pos, struct mparse *curp)
{
	int		 i;

	/*
	 * Try to intuit which kind of manual parser should be used.  If
	 * passed in by command-line (-man, -mdoc), then use that
	 * explicitly.  If passed as -mandoc, then try to guess from the
	 * line: either skip dot-lines, use -mdoc when finding `.Dt', or
	 * default to -man, which is more lenient.
	 *
	 * Separate out pmdoc/pman from mdoc/man: the first persists
	 * through all parsers, while the latter is used per-parse.
	 */

	if ('.' == buf[0] || '\'' == buf[0]) {
		for (i = 1; buf[i]; i++)
			if (' ' != buf[i] && '\t' != buf[i])
				break;
		if ('\0' == buf[i])
			return;
	}

	switch (curp->inttype) {
	case (MPARSE_MDOC):
		if (NULL == curp->pmdoc) 
			curp->pmdoc = mdoc_alloc(curp->roff, curp);
		assert(curp->pmdoc);
		curp->mdoc = curp->pmdoc;
		return;
	case (MPARSE_MAN):
		if (NULL == curp->pman) 
			curp->pman = man_alloc(curp->roff, curp);
		assert(curp->pman);
		curp->man = curp->pman;
		return;
	default:
		break;
	}

	if (pos >= 3 && 0 == memcmp(buf, ".Dd", 3))  {
		if (NULL == curp->pmdoc) 
			curp->pmdoc = mdoc_alloc(curp->roff, curp);
		assert(curp->pmdoc);
		curp->mdoc = curp->pmdoc;
		return;
	} 

	if (NULL == curp->pman) 
		curp->pman = man_alloc(curp->roff, curp);
	assert(curp->pman);
	curp->man = curp->pman;
}

/*
 * Main parse routine for an opened file.  This is called for each
 * opened file and simply loops around the full input file, possibly
 * nesting (i.e., with `so').
 */
static void
mparse_buf_r(struct mparse *curp, struct buf blk, int start)
{
	const struct tbl_span	*span;
	struct buf	 ln;
	enum rofferr	 rr;
	int		 i, of, rc;
	int		 pos; /* byte number in the ln buffer */
	int		 lnn; /* line number in the real file */
	unsigned char	 c;

	memset(&ln, 0, sizeof(struct buf));

	lnn = curp->line; 
	pos = 0; 

	for (i = 0; i < (int)blk.sz; ) {
		if (0 == pos && '\0' == blk.buf[i])
			break;

		if (start) {
			curp->line = lnn;
			curp->reparse_count = 0;
		}

		while (i < (int)blk.sz && (start || '\0' != blk.buf[i])) {

			/*
			 * When finding an unescaped newline character,
			 * leave the character loop to process the line.
			 * Skip a preceding carriage return, if any.
			 */

			if ('\r' == blk.buf[i] && i + 1 < (int)blk.sz &&
			    '\n' == blk.buf[i + 1])
				++i;
			if ('\n' == blk.buf[i]) {
				++i;
				++lnn;
				break;
			}

			/* 
			 * Warn about bogus characters.  If you're using
			 * non-ASCII encoding, you're screwing your
			 * readers.  Since I'd rather this not happen,
			 * I'll be helpful and replace these characters
			 * with "?", so we don't display gibberish.
			 * Note to manual writers: use special characters.
			 */

			c = (unsigned char) blk.buf[i];

			if ( ! (isascii(c) && 
					(isgraph(c) || isblank(c)))) {
				mandoc_msg(MANDOCERR_BADCHAR, curp,
						curp->line, pos, NULL);
				i++;
				if (pos >= (int)ln.sz)
					resize_buf(&ln, 256);
				ln.buf[pos++] = '?';
				continue;
			}

			/* Trailing backslash = a plain char. */

			if ('\\' != blk.buf[i] || i + 1 == (int)blk.sz) {
				if (pos >= (int)ln.sz)
					resize_buf(&ln, 256);
				ln.buf[pos++] = blk.buf[i++];
				continue;
			}

			/*
			 * Found escape and at least one other character.
			 * When it's a newline character, skip it.
			 * When there is a carriage return in between,
			 * skip that one as well.
			 */

			if ('\r' == blk.buf[i + 1] && i + 2 < (int)blk.sz &&
			    '\n' == blk.buf[i + 2])
				++i;
			if ('\n' == blk.buf[i + 1]) {
				i += 2;
				++lnn;
				continue;
			}

			if ('"' == blk.buf[i + 1] || '#' == blk.buf[i + 1]) {
				i += 2;
				/* Comment, skip to end of line */
				for (; i < (int)blk.sz; ++i) {
					if ('\n' == blk.buf[i]) {
						++i;
						++lnn;
						break;
					}
				}

				/* Backout trailing whitespaces */
				for (; pos > 0; --pos) {
					if (ln.buf[pos - 1] != ' ')
						break;
					if (pos > 2 && ln.buf[pos - 2] == '\\')
						break;
				}
				break;
			}

			/* Some other escape sequence, copy & cont. */

			if (pos + 1 >= (int)ln.sz)
				resize_buf(&ln, 256);

			ln.buf[pos++] = blk.buf[i++];
			ln.buf[pos++] = blk.buf[i++];
		}

 		if (pos >= (int)ln.sz)
			resize_buf(&ln, 256);

		ln.buf[pos] = '\0';

		/*
		 * A significant amount of complexity is contained by
		 * the roff preprocessor.  It's line-oriented but can be
		 * expressed on one line, so we need at times to
		 * readjust our starting point and re-run it.  The roff
		 * preprocessor can also readjust the buffers with new
		 * data, so we pass them in wholesale.
		 */

		of = 0;

		/*
		 * Maintain a lookaside buffer of all parsed lines.  We
		 * only do this if mparse_keep() has been invoked (the
		 * buffer may be accessed with mparse_getkeep()).
		 */

		if (curp->secondary) {
			curp->secondary->buf = 
				mandoc_realloc
				(curp->secondary->buf, 
				 curp->secondary->sz + pos + 2);
			memcpy(curp->secondary->buf + 
					curp->secondary->sz, 
					ln.buf, pos);
			curp->secondary->sz += pos;
			curp->secondary->buf
				[curp->secondary->sz] = '\n';
			curp->secondary->sz++;
			curp->secondary->buf
				[curp->secondary->sz] = '\0';
		}
rerun:
		rr = roff_parseln
			(curp->roff, curp->line, 
			 &ln.buf, &ln.sz, of, &of);

		switch (rr) {
		case (ROFF_REPARSE):
			if (REPARSE_LIMIT >= ++curp->reparse_count)
				mparse_buf_r(curp, ln, 0);
			else
				mandoc_msg(MANDOCERR_ROFFLOOP, curp,
					curp->line, pos, NULL);
			pos = 0;
			continue;
		case (ROFF_APPEND):
			pos = (int)strlen(ln.buf);
			continue;
		case (ROFF_RERUN):
			goto rerun;
		case (ROFF_IGN):
			pos = 0;
			continue;
		case (ROFF_ERR):
			assert(MANDOCLEVEL_FATAL <= curp->file_status);
			break;
		case (ROFF_SO):
			/*
			 * We remove `so' clauses from our lookaside
			 * buffer because we're going to descend into
			 * the file recursively.
			 */
			if (curp->secondary) 
				curp->secondary->sz -= pos + 1;
			mparse_readfd_r(curp, -1, ln.buf + of, 1);
			if (MANDOCLEVEL_FATAL <= curp->file_status)
				break;
			pos = 0;
			continue;
		default:
			break;
		}

		/*
		 * If we encounter errors in the recursive parse, make
		 * sure we don't continue parsing.
		 */

		if (MANDOCLEVEL_FATAL <= curp->file_status)
			break;

		/*
		 * If input parsers have not been allocated, do so now.
		 * We keep these instanced between parsers, but set them
		 * locally per parse routine since we can use different
		 * parsers with each one.
		 */

		if ( ! (curp->man || curp->mdoc))
			pset(ln.buf + of, pos - of, curp);

		/* 
		 * Lastly, push down into the parsers themselves.  One
		 * of these will have already been set in the pset()
		 * routine.
		 * If libroff returns ROFF_TBL, then add it to the
		 * currently open parse.  Since we only get here if
		 * there does exist data (see tbl_data.c), we're
		 * guaranteed that something's been allocated.
		 * Do the same for ROFF_EQN.
		 */

		rc = -1;

		if (ROFF_TBL == rr)
			while (NULL != (span = roff_span(curp->roff))) {
				rc = curp->man ?
					man_addspan(curp->man, span) :
					mdoc_addspan(curp->mdoc, span);
				if (0 == rc)
					break;
			}
		else if (ROFF_EQN == rr)
			rc = curp->mdoc ? 
				mdoc_addeqn(curp->mdoc, 
					roff_eqn(curp->roff)) :
				man_addeqn(curp->man,
					roff_eqn(curp->roff));
		else if (curp->man || curp->mdoc)
			rc = curp->man ?
				man_parseln(curp->man, 
					curp->line, ln.buf, of) :
				mdoc_parseln(curp->mdoc, 
					curp->line, ln.buf, of);

		if (0 == rc) {
			assert(MANDOCLEVEL_FATAL <= curp->file_status);
			break;
		}

		/* Temporary buffers typically are not full. */

		if (0 == start && '\0' == blk.buf[i])
			break;

		/* Start the next input line. */

		pos = 0;
	}

	free(ln.buf);
}

static int
read_whole_file(const char *file, int fd, struct buf *fb, int *with_mmap)
{
	size_t		 off;
	ssize_t		 ssz;

#ifdef	HAVE_MMAP
	struct stat	 st;
	if (-1 == fstat(fd, &st)) {
		perror(file);
		return(0);
	}

	/*
	 * If we're a regular file, try just reading in the whole entry
	 * via mmap().  This is faster than reading it into blocks, and
	 * since each file is only a few bytes to begin with, I'm not
	 * concerned that this is going to tank any machines.
	 */

	if (S_ISREG(st.st_mode)) {
		if (st.st_size >= (1U << 31)) {
			fprintf(stderr, "%s: input too large\n", file);
			return(0);
		}
		*with_mmap = 1;
		fb->sz = (size_t)st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, 
				MAP_FILE|MAP_SHARED, fd, 0);
		if (fb->buf != MAP_FAILED)
			return(1);
	}
#endif

	/*
	 * If this isn't a regular file (like, say, stdin), then we must
	 * go the old way and just read things in bit by bit.
	 */

	*with_mmap = 0;
	off = 0;
	fb->sz = 0;
	fb->buf = NULL;
	for (;;) {
		if (off == fb->sz) {
			if (fb->sz == (1U << 31)) {
				fprintf(stderr, "%s: input too large\n", file);
				break;
			}
			resize_buf(fb, 65536);
		}
		ssz = read(fd, fb->buf + (int)off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			return(1);
		}
		if (ssz == -1) {
			perror(file);
			break;
		}
		off += (size_t)ssz;
	}

	free(fb->buf);
	fb->buf = NULL;
	return(0);
}

static void
mparse_end(struct mparse *curp)
{

	if (MANDOCLEVEL_FATAL <= curp->file_status)
		return;

	if (curp->mdoc && ! mdoc_endparse(curp->mdoc)) {
		assert(MANDOCLEVEL_FATAL <= curp->file_status);
		return;
	}

	if (curp->man && ! man_endparse(curp->man)) {
		assert(MANDOCLEVEL_FATAL <= curp->file_status);
		return;
	}

	if ( ! (curp->man || curp->mdoc)) {
		mandoc_msg(MANDOCERR_NOTMANUAL, curp, 1, 0, NULL);
		curp->file_status = MANDOCLEVEL_FATAL;
		return;
	}

	roff_endparse(curp->roff);
}

static void
mparse_parse_buffer(struct mparse *curp, struct buf blk, const char *file,
		int re)
{
	const char	*svfile;

	/* Line number is per-file. */
	svfile = curp->file;
	curp->file = file;
	curp->line = 1;

	mparse_buf_r(curp, blk, 1);

	if (0 == re && MANDOCLEVEL_FATAL > curp->file_status)
		mparse_end(curp);

	curp->file = svfile;
}

enum mandoclevel
mparse_readmem(struct mparse *curp, const void *buf, size_t len,
		const char *file)
{
	struct buf blk;

	blk.buf = UNCONST(buf);
	blk.sz = len;

	mparse_parse_buffer(curp, blk, file, 0);
	return(curp->file_status);
}

static void
mparse_readfd_r(struct mparse *curp, int fd, const char *file, int re)
{
	struct buf	 blk;
	int		 with_mmap;

	if (-1 == fd)
		if (-1 == (fd = open(file, O_RDONLY, 0))) {
			perror(file);
			curp->file_status = MANDOCLEVEL_SYSERR;
			return;
		}
	/*
	 * Run for each opened file; may be called more than once for
	 * each full parse sequence if the opened file is nested (i.e.,
	 * from `so').  Simply sucks in the whole file and moves into
	 * the parse phase for the file.
	 */

	if ( ! read_whole_file(file, fd, &blk, &with_mmap)) {
		curp->file_status = MANDOCLEVEL_SYSERR;
		return;
	}

	mparse_parse_buffer(curp, blk, file, re);

#ifdef	HAVE_MMAP
	if (with_mmap)
		munmap(blk.buf, blk.sz);
	else
#endif
		free(blk.buf);

	if (STDIN_FILENO != fd && -1 == close(fd))
		perror(file);
}

enum mandoclevel
mparse_readfd(struct mparse *curp, int fd, const char *file)
{

	mparse_readfd_r(curp, fd, file, 0);
	return(curp->file_status);
}

struct mparse *
mparse_alloc(enum mparset inttype, enum mandoclevel wlevel, mandocmsg mmsg, void *arg)
{
	struct mparse	*curp;

	assert(wlevel <= MANDOCLEVEL_FATAL);

	curp = mandoc_calloc(1, sizeof(struct mparse));

	curp->wlevel = wlevel;
	curp->mmsg = mmsg;
	curp->arg = arg;
	curp->inttype = inttype;

	curp->roff = roff_alloc(curp);
	return(curp);
}

void
mparse_reset(struct mparse *curp)
{

	roff_reset(curp->roff);

	if (curp->mdoc)
		mdoc_reset(curp->mdoc);
	if (curp->man)
		man_reset(curp->man);
	if (curp->secondary)
		curp->secondary->sz = 0;

	curp->file_status = MANDOCLEVEL_OK;
	curp->mdoc = NULL;
	curp->man = NULL;
}

void
mparse_free(struct mparse *curp)
{

	if (curp->pmdoc)
		mdoc_free(curp->pmdoc);
	if (curp->pman)
		man_free(curp->pman);
	if (curp->roff)
		roff_free(curp->roff);
	if (curp->secondary)
		free(curp->secondary->buf);

	free(curp->secondary);
	free(curp);
}

void
mparse_result(struct mparse *curp, struct mdoc **mdoc, struct man **man)
{

	if (mdoc)
		*mdoc = curp->mdoc;
	if (man)
		*man = curp->man;
}

void
mandoc_vmsg(enum mandocerr t, struct mparse *m,
		int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	mandoc_msg(t, m, ln, pos, buf);
}

void
mandoc_msg(enum mandocerr er, struct mparse *m, 
		int ln, int col, const char *msg)
{
	enum mandoclevel level;

	level = MANDOCLEVEL_FATAL;
	while (er < mandoclimits[level])
		level--;

	if (level < m->wlevel)
		return;

	if (m->mmsg)
		(*m->mmsg)(er, level, m->file, ln, col, msg);

	if (m->file_status < level)
		m->file_status = level;
}

const char *
mparse_strerror(enum mandocerr er)
{

	return(mandocerrs[er]);
}

const char *
mparse_strlevel(enum mandoclevel lvl)
{
	return(mandoclevels[lvl]);
}

void
mparse_keep(struct mparse *p)
{

	assert(NULL == p->secondary);
	p->secondary = mandoc_calloc(1, sizeof(struct buf));
}

const char *
mparse_getkeep(const struct mparse *p)
{

	assert(p->secondary);
	return(p->secondary->sz ? p->secondary->buf : NULL);
}
