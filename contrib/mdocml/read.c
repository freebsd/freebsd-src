/*	$Id: read.c,v 1.101 2014/11/28 18:09:01 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2014 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010, 2012 Joerg Sonnenberger <joerg@netbsd.org>
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
#include "config.h"

#include <sys/types.h>
#if HAVE_MMAP
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmandoc.h"
#include "mdoc.h"
#include "man.h"
#include "main.h"

#define	REPARSE_LIMIT	1000

struct	mparse {
	struct man	 *pman; /* persistent man parser */
	struct mdoc	 *pmdoc; /* persistent mdoc parser */
	struct man	 *man; /* man parser */
	struct mdoc	 *mdoc; /* mdoc parser */
	struct roff	 *roff; /* roff parser (!NULL) */
	const struct mchars *mchars; /* character table */
	char		 *sodest; /* filename pointed to by .so */
	const char	 *file; /* filename of current input file */
	struct buf	 *primary; /* buffer currently being parsed */
	struct buf	 *secondary; /* preprocessed copy of input */
	const char	 *defos; /* default operating system */
	mandocmsg	  mmsg; /* warning/error message handler */
	enum mandoclevel  file_status; /* status of current parse */
	enum mandoclevel  wlevel; /* ignore messages below this */
	int		  options; /* parser options */
	int		  filenc; /* encoding of the current file */
	int		  reparse_count; /* finite interp. stack */
	int		  line; /* line number in the file */
	pid_t		  child; /* the gunzip(1) process */
};

static	void	  choose_parser(struct mparse *);
static	void	  resize_buf(struct buf *, size_t);
static	void	  mparse_buf_r(struct mparse *, struct buf, size_t, int);
static	int	  read_whole_file(struct mparse *, const char *, int,
				struct buf *, int *);
static	void	  mparse_end(struct mparse *);
static	void	  mparse_parse_buffer(struct mparse *, struct buf,
			const char *);

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
	"missing manual title, using UNTITLED",
	"missing manual title, using \"\"",
	"lower case character in document title",
	"missing manual section, using \"\"",
	"unknown manual section",
	"missing date, using today's date",
	"cannot parse date, using it verbatim",
	"missing Os macro, using \"\"",
	"duplicate prologue macro",
	"late prologue macro",
	"skipping late title macro",
	"prologue macros out of order",

	/* related to document structure */
	".so is fragile, better use ln(1)",
	"no document body",
	"content before first section header",
	"first section is not \"NAME\"",
	"bad NAME section contents",
	"sections out of conventional order",
	"duplicate section title",
	"unexpected section",
	"unusual Xr order",
	"unusual Xr punctuation",
	"AUTHORS section without An macro",

	/* related to macros and nesting */
	"obsolete macro",
	"skipping paragraph macro",
	"moving paragraph macro out of list",
	"skipping no-space macro",
	"blocks badly nested",
	"nested displays are not portable",
	"moving content out of list",
	".Vt block has child macro",
	"fill mode already enabled, skipping",
	"fill mode already disabled, skipping",
	"line scope broken",

	/* related to missing macro arguments */
	"skipping empty request",
	"conditional request controls empty scope",
	"skipping empty macro",
	"empty argument, using 0n",
	"argument count wrong",
	"missing display type, using -ragged",
	"list type is not the first argument",
	"missing -width in -tag list, using 8n",
	"missing utility name, using \"\"",
	"empty head in list item",
	"empty list item",
	"missing font type, using \\fR",
	"unknown font type, using \\fR",
	"missing -std argument, adding it",
	"missing eqn box, using \"\"",

	/* related to bad macro arguments */
	"unterminated quoted argument",
	"duplicate argument",
	"skipping duplicate argument",
	"skipping duplicate display type",
	"skipping duplicate list type",
	"skipping -width argument",
	"unknown AT&T UNIX version",
	"comma in function argument",
	"parenthesis in function name",
	"invalid content in Rs block",
	"invalid Boolean argument",
	"unknown font, skipping request",

	/* related to plain text */
	"blank line in fill mode, using .sp",
	"tab in filled text",
	"whitespace at end of input line",
	"bad comment style",
	"invalid escape sequence",
	"undefined string, using \"\"",

	"generic error",

	/* related to equations */
	"unexpected equation scope closure",
	"equation scope open on exit",
	"overlapping equation scopes",
	"unexpected end of equation",

	/* related to tables */
	"bad table syntax",
	"bad table option",
	"bad table layout",
	"no table layout cells specified",
	"no table data cells specified",
	"ignore data in cell",
	"data block still open",
	"ignoring extra data cells",

	/* related to document structure and macros */
	"input stack limit exceeded, infinite loop?",
	"skipping bad character",
	"skipping unknown macro",
	"skipping item outside list",
	"skipping column outside column list",
	"skipping end of block that is not open",
	"inserting missing end of block",
	"appending missing end of block",

	/* related to request and macro arguments */
	"escaped character not allowed in a name",
	"argument count wrong",
	"NOT IMPLEMENTED: Bd -file",
	"missing list type, using -item",
	"missing manual name, using \"\"",
	"uname(3) system call failed, using UNKNOWN",
	"unknown standard specifier",
	"skipping request without numeric argument",
	"skipping all arguments",
	"skipping excess arguments",
	"divide by zero",

	"generic fatal error",

	"input too large",
	"NOT IMPLEMENTED: .so with absolute path or \"..\"",
	".so request failed",

	/* system errors */
	"cannot dup file descriptor",
	"cannot exec",
	"gunzip failed with code",
	"cannot fork",
	NULL,
	"cannot open pipe",
	"cannot read file",
	"gunzip died from signal",
	"cannot stat file",
	"wait failed",
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
choose_parser(struct mparse *curp)
{
	char		*cp, *ep;
	int		 format;

	/*
	 * If neither command line arguments -mdoc or -man select
	 * a parser nor the roff parser found a .Dd or .TH macro
	 * yet, look ahead in the main input buffer.
	 */

	if ((format = roff_getformat(curp->roff)) == 0) {
		cp = curp->primary->buf;
		ep = cp + curp->primary->sz;
		while (cp < ep) {
			if (*cp == '.' || *cp == '\'') {
				cp++;
				if (cp[0] == 'D' && cp[1] == 'd') {
					format = MPARSE_MDOC;
					break;
				}
				if (cp[0] == 'T' && cp[1] == 'H') {
					format = MPARSE_MAN;
					break;
				}
			}
			cp = memchr(cp, '\n', ep - cp);
			if (cp == NULL)
				break;
			cp++;
		}
	}

	if (format == MPARSE_MDOC) {
		if (NULL == curp->pmdoc)
			curp->pmdoc = mdoc_alloc(
			    curp->roff, curp, curp->defos,
			    MPARSE_QUICK & curp->options ? 1 : 0);
		assert(curp->pmdoc);
		curp->mdoc = curp->pmdoc;
		return;
	}

	/* Fall back to man(7) as a last resort. */

	if (NULL == curp->pman)
		curp->pman = man_alloc(curp->roff, curp,
		    MPARSE_QUICK & curp->options ? 1 : 0);
	assert(curp->pman);
	curp->man = curp->pman;
}

/*
 * Main parse routine for a buffer.
 * It assumes encoding and line numbering are already set up.
 * It can recurse directly (for invocations of user-defined
 * macros, inline equations, and input line traps)
 * and indirectly (for .so file inclusion).
 */
static void
mparse_buf_r(struct mparse *curp, struct buf blk, size_t i, int start)
{
	const struct tbl_span	*span;
	struct buf	 ln;
	size_t		 pos; /* byte number in the ln buffer */
	enum rofferr	 rr;
	int		 of;
	int		 lnn; /* line number in the real file */
	unsigned char	 c;

	memset(&ln, 0, sizeof(ln));

	lnn = curp->line;
	pos = 0;

	while (i < blk.sz) {
		if (0 == pos && '\0' == blk.buf[i])
			break;

		if (start) {
			curp->line = lnn;
			curp->reparse_count = 0;

			if (lnn < 3 &&
			    curp->filenc & MPARSE_UTF8 &&
			    curp->filenc & MPARSE_LATIN1)
				curp->filenc = preconv_cue(&blk, i);
		}

		while (i < blk.sz && (start || blk.buf[i] != '\0')) {

			/*
			 * When finding an unescaped newline character,
			 * leave the character loop to process the line.
			 * Skip a preceding carriage return, if any.
			 */

			if ('\r' == blk.buf[i] && i + 1 < blk.sz &&
			    '\n' == blk.buf[i + 1])
				++i;
			if ('\n' == blk.buf[i]) {
				++i;
				++lnn;
				break;
			}

			/*
			 * Make sure we have space for the worst
			 * case of 11 bytes: "\\[u10ffff]\0"
			 */

			if (pos + 11 > ln.sz)
				resize_buf(&ln, 256);

			/*
			 * Encode 8-bit input.
			 */

			c = blk.buf[i];
			if (c & 0x80) {
				if ( ! (curp->filenc && preconv_encode(
				    &blk, &i, &ln, &pos, &curp->filenc))) {
					mandoc_vmsg(MANDOCERR_BADCHAR,
					    curp, curp->line, pos,
					    "0x%x", c);
					ln.buf[pos++] = '?';
					i++;
				}
				continue;
			}

			/*
			 * Exclude control characters.
			 */

			if (c == 0x7f || (c < 0x20 && c != 0x09)) {
				mandoc_vmsg(MANDOCERR_BADCHAR, curp,
				    curp->line, pos, "0x%x", c);
				i++;
				ln.buf[pos++] = '?';
				continue;
			}

			/* Trailing backslash = a plain char. */

			if (blk.buf[i] != '\\' || i + 1 == blk.sz) {
				ln.buf[pos++] = blk.buf[i++];
				continue;
			}

			/*
			 * Found escape and at least one other character.
			 * When it's a newline character, skip it.
			 * When there is a carriage return in between,
			 * skip that one as well.
			 */

			if ('\r' == blk.buf[i + 1] && i + 2 < blk.sz &&
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
				for (; i < blk.sz; ++i) {
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

			/* Catch escaped bogus characters. */

			c = (unsigned char) blk.buf[i+1];

			if ( ! (isascii(c) &&
			    (isgraph(c) || isblank(c)))) {
				mandoc_vmsg(MANDOCERR_BADCHAR, curp,
				    curp->line, pos, "0x%x", c);
				i += 2;
				ln.buf[pos++] = '?';
				continue;
			}

			/* Some other escape sequence, copy & cont. */

			ln.buf[pos++] = blk.buf[i++];
			ln.buf[pos++] = blk.buf[i++];
		}

		if (pos >= ln.sz)
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
			curp->secondary->buf = mandoc_realloc(
			    curp->secondary->buf,
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
		rr = roff_parseln(curp->roff, curp->line, &ln, &of);

		switch (rr) {
		case ROFF_REPARSE:
			if (REPARSE_LIMIT >= ++curp->reparse_count)
				mparse_buf_r(curp, ln, of, 0);
			else
				mandoc_msg(MANDOCERR_ROFFLOOP, curp,
				    curp->line, pos, NULL);
			pos = 0;
			continue;
		case ROFF_APPEND:
			pos = strlen(ln.buf);
			continue;
		case ROFF_RERUN:
			goto rerun;
		case ROFF_IGN:
			pos = 0;
			continue;
		case ROFF_ERR:
			assert(MANDOCLEVEL_FATAL <= curp->file_status);
			break;
		case ROFF_SO:
			if ( ! (curp->options & MPARSE_SO) &&
			    (i >= blk.sz || blk.buf[i] == '\0')) {
				curp->sodest = mandoc_strdup(ln.buf + of);
				free(ln.buf);
				return;
			}
			/*
			 * We remove `so' clauses from our lookaside
			 * buffer because we're going to descend into
			 * the file recursively.
			 */
			if (curp->secondary)
				curp->secondary->sz -= pos + 1;
			mparse_readfd(curp, -1, ln.buf + of);
			if (MANDOCLEVEL_FATAL <= curp->file_status) {
				mandoc_vmsg(MANDOCERR_SO_FAIL,
				    curp, curp->line, pos,
				    ".so %s", ln.buf + of);
				break;
			}
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
			choose_parser(curp);

		/*
		 * Lastly, push down into the parsers themselves.
		 * If libroff returns ROFF_TBL, then add it to the
		 * currently open parse.  Since we only get here if
		 * there does exist data (see tbl_data.c), we're
		 * guaranteed that something's been allocated.
		 * Do the same for ROFF_EQN.
		 */

		if (rr == ROFF_TBL) {
			while ((span = roff_span(curp->roff)) != NULL)
				if (curp->man == NULL)
					mdoc_addspan(curp->mdoc, span);
				else
					man_addspan(curp->man, span);
		} else if (rr == ROFF_EQN) {
			if (curp->man == NULL)
				mdoc_addeqn(curp->mdoc, roff_eqn(curp->roff));
			else
				man_addeqn(curp->man, roff_eqn(curp->roff));
		} else if ((curp->man == NULL ?
		    mdoc_parseln(curp->mdoc, curp->line, ln.buf, of) :
		    man_parseln(curp->man, curp->line, ln.buf, of)) == 2)
				break;

		/* Temporary buffers typically are not full. */

		if (0 == start && '\0' == blk.buf[i])
			break;

		/* Start the next input line. */

		pos = 0;
	}

	free(ln.buf);
}

static int
read_whole_file(struct mparse *curp, const char *file, int fd,
		struct buf *fb, int *with_mmap)
{
	size_t		 off;
	ssize_t		 ssz;

#if HAVE_MMAP
	struct stat	 st;
	if (-1 == fstat(fd, &st)) {
		curp->file_status = MANDOCLEVEL_SYSERR;
		if (curp->mmsg)
			(*curp->mmsg)(MANDOCERR_SYSSTAT, curp->file_status,
			    file, 0, 0, strerror(errno));
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
			curp->file_status = MANDOCLEVEL_FATAL;
			if (curp->mmsg)
				(*curp->mmsg)(MANDOCERR_TOOLARGE,
				    curp->file_status, file, 0, 0, NULL);
			return(0);
		}
		*with_mmap = 1;
		fb->sz = (size_t)st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, MAP_SHARED, fd, 0);
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
				curp->file_status = MANDOCLEVEL_FATAL;
				if (curp->mmsg)
					(*curp->mmsg)(MANDOCERR_TOOLARGE,
					    curp->file_status,
					    file, 0, 0, NULL);
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
			curp->file_status = MANDOCLEVEL_SYSERR;
			if (curp->mmsg)
				(*curp->mmsg)(MANDOCERR_SYSREAD,
				    curp->file_status, file, 0, 0,
				    strerror(errno));
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

	if (curp->mdoc == NULL &&
	    curp->man == NULL &&
	    curp->sodest == NULL) {
		if (curp->options & MPARSE_MDOC)
			curp->mdoc = curp->pmdoc;
		else {
			if (curp->pman == NULL)
				curp->pman = man_alloc(curp->roff, curp,
				    curp->options & MPARSE_QUICK ? 1 : 0);
			curp->man = curp->pman;
		}
	}

	if (curp->mdoc && ! mdoc_endparse(curp->mdoc)) {
		assert(MANDOCLEVEL_FATAL <= curp->file_status);
		return;
	}

	if (curp->man && ! man_endparse(curp->man)) {
		assert(MANDOCLEVEL_FATAL <= curp->file_status);
		return;
	}

	roff_endparse(curp->roff);
}

static void
mparse_parse_buffer(struct mparse *curp, struct buf blk, const char *file)
{
	struct buf	*svprimary;
	const char	*svfile;
	size_t		 offset;
	static int	 recursion_depth;

	if (64 < recursion_depth) {
		mandoc_msg(MANDOCERR_ROFFLOOP, curp, curp->line, 0, NULL);
		return;
	}

	/* Line number is per-file. */
	svfile = curp->file;
	curp->file = file;
	svprimary = curp->primary;
	curp->primary = &blk;
	curp->line = 1;
	recursion_depth++;

	/* Skip an UTF-8 byte order mark. */
	if (curp->filenc & MPARSE_UTF8 && blk.sz > 2 &&
	    (unsigned char)blk.buf[0] == 0xef &&
	    (unsigned char)blk.buf[1] == 0xbb &&
	    (unsigned char)blk.buf[2] == 0xbf) {
		offset = 3;
		curp->filenc &= ~MPARSE_LATIN1;
	} else
		offset = 0;

	mparse_buf_r(curp, blk, offset, 1);

	if (0 == --recursion_depth && MANDOCLEVEL_FATAL > curp->file_status)
		mparse_end(curp);

	curp->primary = svprimary;
	curp->file = svfile;
}

enum mandoclevel
mparse_readmem(struct mparse *curp, const void *buf, size_t len,
		const char *file)
{
	struct buf blk;

	blk.buf = UNCONST(buf);
	blk.sz = len;

	mparse_parse_buffer(curp, blk, file);
	return(curp->file_status);
}

/*
 * If a file descriptor is given, use it and assume it points
 * to the named file.  Otherwise, open the named file.
 * Read the whole file into memory and call the parsers.
 * Called recursively when an .so request is encountered.
 */
enum mandoclevel
mparse_readfd(struct mparse *curp, int fd, const char *file)
{
	struct buf	 blk;
	int		 with_mmap;
	int		 save_filenc;
	pid_t		 save_child;

	save_child = curp->child;
	if (fd != -1)
		curp->child = 0;
	else if (mparse_open(curp, &fd, file) >= MANDOCLEVEL_SYSERR)
		goto out;

	if (read_whole_file(curp, file, fd, &blk, &with_mmap)) {
		save_filenc = curp->filenc;
		curp->filenc = curp->options &
		    (MPARSE_UTF8 | MPARSE_LATIN1);
		mparse_parse_buffer(curp, blk, file);
		curp->filenc = save_filenc;
#if HAVE_MMAP
		if (with_mmap)
			munmap(blk.buf, blk.sz);
		else
#endif
			free(blk.buf);
	}

	if (fd != STDIN_FILENO && close(fd) == -1)
		perror(file);

	mparse_wait(curp);
out:
	curp->child = save_child;
	return(curp->file_status);
}

enum mandoclevel
mparse_open(struct mparse *curp, int *fd, const char *file)
{
	int		  pfd[2];
	int		  save_errno;
	char		 *cp;
	enum mandocerr	  err;

	pfd[1] = -1;
	curp->file = file;

	/* Unless zipped, try to just open the file. */

	if ((cp = strrchr(file, '.')) == NULL ||
	    strcmp(cp + 1, "gz")) {
		curp->child = 0;
		if ((*fd = open(file, O_RDONLY)) != -1)
			return(MANDOCLEVEL_OK);

		/* Open failed; try to append ".gz". */

		mandoc_asprintf(&cp, "%s.gz", file);
		file = cp;
	} else
		cp = NULL;

	/* Before forking, make sure the file can be read. */

	save_errno = errno;
	if (access(file, R_OK) == -1) {
		if (cp != NULL)
			errno = save_errno;
		err = MANDOCERR_SYSOPEN;
		goto out;
	}

	/* Run gunzip(1). */

	if (pipe(pfd) == -1) {
		err = MANDOCERR_SYSPIPE;
		goto out;
	}

	switch (curp->child = fork()) {
	case -1:
		err = MANDOCERR_SYSFORK;
		close(pfd[0]);
		close(pfd[1]);
		pfd[1] = -1;
		break;
	case 0:
		close(pfd[0]);
		if (dup2(pfd[1], STDOUT_FILENO) == -1) {
			err = MANDOCERR_SYSDUP;
			break;
		}
		execlp("gunzip", "gunzip", "-c", file, NULL);
		err = MANDOCERR_SYSEXEC;
		break;
	default:
		close(pfd[1]);
		*fd = pfd[0];
		return(MANDOCLEVEL_OK);
	}

out:
	free(cp);
	*fd = -1;
	curp->child = 0;
	curp->file_status = MANDOCLEVEL_SYSERR;
	if (curp->mmsg)
		(*curp->mmsg)(err, curp->file_status, curp->file,
		    0, 0, strerror(errno));
	if (pfd[1] != -1)
		exit(1);
	return(curp->file_status);
}

enum mandoclevel
mparse_wait(struct mparse *curp)
{
	int	  status;

	if (curp->child == 0)
		return(MANDOCLEVEL_OK);

	if (waitpid(curp->child, &status, 0) == -1) {
		mandoc_msg(MANDOCERR_SYSWAIT, curp, 0, 0,
		    strerror(errno));
		curp->file_status = MANDOCLEVEL_SYSERR;
		return(curp->file_status);
	}
	if (WIFSIGNALED(status)) {
		mandoc_vmsg(MANDOCERR_SYSSIG, curp, 0, 0,
		    "%d", WTERMSIG(status));
		curp->file_status = MANDOCLEVEL_SYSERR;
		return(curp->file_status);
	}
	if (WEXITSTATUS(status)) {
		mandoc_vmsg(MANDOCERR_SYSEXIT, curp, 0, 0,
		    "%d", WEXITSTATUS(status));
		curp->file_status = MANDOCLEVEL_SYSERR;
		return(curp->file_status);
	}
	return(MANDOCLEVEL_OK);
}

struct mparse *
mparse_alloc(int options, enum mandoclevel wlevel, mandocmsg mmsg,
    const struct mchars *mchars, const char *defos)
{
	struct mparse	*curp;

	assert(wlevel <= MANDOCLEVEL_FATAL);

	curp = mandoc_calloc(1, sizeof(struct mparse));

	curp->options = options;
	curp->wlevel = wlevel;
	curp->mmsg = mmsg;
	curp->defos = defos;

	curp->mchars = mchars;
	curp->roff = roff_alloc(curp, curp->mchars, options);
	if (curp->options & MPARSE_MDOC)
		curp->pmdoc = mdoc_alloc(
		    curp->roff, curp, curp->defos,
		    curp->options & MPARSE_QUICK ? 1 : 0);
	if (curp->options & MPARSE_MAN)
		curp->pman = man_alloc(curp->roff, curp,
		    curp->options & MPARSE_QUICK ? 1 : 0);

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

	free(curp->sodest);
	curp->sodest = NULL;
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
	free(curp->sodest);
	free(curp);
}

void
mparse_result(struct mparse *curp,
	struct mdoc **mdoc, struct man **man, char **sodest)
{

	if (sodest && NULL != (*sodest = curp->sodest)) {
		*mdoc = NULL;
		*man = NULL;
		return;
	}
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
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
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
