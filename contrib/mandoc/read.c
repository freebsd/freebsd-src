/* $Id: read.c,v 1.220 2021/06/27 17:57:54 schwarze Exp $ */
/*
 * Copyright (c) 2010-2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2012 Joerg Sonnenberger <joerg@netbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Top-level functions of the mandoc(3) parser:
 * Parser and input encoding selection, decompression,
 * handling of input bytes, characters, lines, and files,
 * handling of roff(7) loops and file inclusion,
 * and steering of the various parsers.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "mandoc_parse.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "tag.h"

#define	REPARSE_LIMIT	1000

struct	mparse {
	struct roff	 *roff; /* roff parser (!NULL) */
	struct roff_man	 *man; /* man parser */
	struct buf	 *primary; /* buffer currently being parsed */
	struct buf	 *secondary; /* copy of top level input */
	struct buf	 *loop; /* open .while request line */
	const char	 *os_s; /* default operating system */
	int		  options; /* parser options */
	int		  gzip; /* current input file is gzipped */
	int		  filenc; /* encoding of the current file */
	int		  reparse_count; /* finite interp. stack */
	int		  line; /* line number in the file */
};

static	void	  choose_parser(struct mparse *);
static	void	  free_buf_list(struct buf *);
static	void	  resize_buf(struct buf *, size_t);
static	int	  mparse_buf_r(struct mparse *, struct buf, size_t, int);
static	int	  read_whole_file(struct mparse *, int, struct buf *, int *);
static	void	  mparse_end(struct mparse *);


static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz > initial/2 ? 2 * buf->sz : initial;
	buf->buf = mandoc_realloc(buf->buf, buf->sz);
}

static void
free_buf_list(struct buf *buf)
{
	struct buf *tmp;

	while (buf != NULL) {
		tmp = buf;
		buf = tmp->next;
		free(tmp->buf);
		free(tmp);
	}
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
		curp->man->meta.macroset = MACROSET_MDOC;
		if (curp->man->mdocmac == NULL)
			curp->man->mdocmac = roffhash_alloc(MDOC_Dd, MDOC_MAX);
	} else {
		curp->man->meta.macroset = MACROSET_MAN;
		if (curp->man->manmac == NULL)
			curp->man->manmac = roffhash_alloc(MAN_TH, MAN_MAX);
	}
	curp->man->meta.first->tok = TOKEN_NONE;
}

/*
 * Main parse routine for a buffer.
 * It assumes encoding and line numbering are already set up.
 * It can recurse directly (for invocations of user-defined
 * macros, inline equations, and input line traps)
 * and indirectly (for .so file inclusion).
 */
static int
mparse_buf_r(struct mparse *curp, struct buf blk, size_t i, int start)
{
	struct buf	 ln;
	struct buf	*firstln, *lastln, *thisln, *loop;
	char		*cp;
	size_t		 pos; /* byte number in the ln buffer */
	size_t		 spos; /* at the start of the current line parse */
	int		 line_result, result;
	int		 of;
	int		 lnn; /* line number in the real file */
	int		 fd;
	int		 inloop; /* Saw .while on this level. */
	unsigned char	 c;

	ln.sz = 256;
	ln.buf = mandoc_malloc(ln.sz);
	ln.next = NULL;
	firstln = lastln = loop = NULL;
	lnn = curp->line;
	pos = 0;
	inloop = 0;
	result = ROFF_CONT;

	while (i < blk.sz && (blk.buf[i] != '\0' || pos != 0)) {
		if (start) {
			curp->line = lnn;
			curp->reparse_count = 0;

			if (lnn < 3 &&
			    curp->filenc & MPARSE_UTF8 &&
			    curp->filenc & MPARSE_LATIN1)
				curp->filenc = preconv_cue(&blk, i);
		}
		spos = pos;

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
			 * case of 12 bytes: "\\[u10ffff]\n\0"
			 */

			if (pos + 12 > ln.sz)
				resize_buf(&ln, 256);

			/*
			 * Encode 8-bit input.
			 */

			c = blk.buf[i];
			if (c & 0x80) {
				if ( ! (curp->filenc && preconv_encode(
				    &blk, &i, &ln, &pos, &curp->filenc))) {
					mandoc_msg(MANDOCERR_CHAR_BAD,
					    curp->line, pos, "0x%x", c);
					ln.buf[pos++] = '?';
					i++;
				}
				continue;
			}

			/*
			 * Exclude control characters.
			 */

			if (c == 0x7f || (c < 0x20 && c != 0x09)) {
				mandoc_msg(c == 0x00 || c == 0x04 ||
				    c > 0x0a ? MANDOCERR_CHAR_BAD :
				    MANDOCERR_CHAR_UNSUPP,
				    curp->line, pos, "0x%x", c);
				i++;
				if (c != '\r')
					ln.buf[pos++] = '?';
				continue;
			}

			ln.buf[pos++] = blk.buf[i++];
		}
		ln.buf[pos] = '\0';

		/*
		 * Maintain a lookaside buffer of all lines.
		 * parsed from this input source.
		 */

		thisln = mandoc_malloc(sizeof(*thisln));
		thisln->buf = mandoc_strdup(ln.buf);
		thisln->sz = strlen(ln.buf) + 1;
		thisln->next = NULL;
		if (firstln == NULL) {
			firstln = lastln = thisln;
			if (curp->secondary == NULL)
				curp->secondary = firstln;
		} else {
			lastln->next = thisln;
			lastln = thisln;
		}

		/* XXX Ugly hack to mark the end of the input. */

		if (i == blk.sz || blk.buf[i] == '\0') {
			if (pos + 2 > ln.sz)
				resize_buf(&ln, 256);
			ln.buf[pos++] = '\n';
			ln.buf[pos] = '\0';
		}

		/*
		 * A significant amount of complexity is contained by
		 * the roff preprocessor.  It's line-oriented but can be
		 * expressed on one line, so we need at times to
		 * readjust our starting point and re-run it.  The roff
		 * preprocessor can also readjust the buffers with new
		 * data, so we pass them in wholesale.
		 */

		of = 0;
rerun:
		line_result = roff_parseln(curp->roff, curp->line,
		    &ln, &of, start && spos == 0 ? pos : 0);

		/* Process options. */

		if (line_result & ROFF_APPEND)
			assert(line_result == (ROFF_IGN | ROFF_APPEND));

		if (line_result & ROFF_USERCALL)
			assert((line_result & ROFF_MASK) == ROFF_REPARSE);

		if (line_result & ROFF_USERRET) {
			assert(line_result == (ROFF_IGN | ROFF_USERRET));
			if (start == 0) {
				/* Return from the current macro. */
				result = ROFF_USERRET;
				goto out;
			}
		}

		switch (line_result & ROFF_LOOPMASK) {
		case ROFF_IGN:
			break;
		case ROFF_WHILE:
			if (curp->loop != NULL) {
				if (loop == curp->loop)
					break;
				mandoc_msg(MANDOCERR_WHILE_NEST,
				    curp->line, pos, NULL);
			}
			curp->loop = thisln;
			loop = NULL;
			inloop = 1;
			break;
		case ROFF_LOOPCONT:
		case ROFF_LOOPEXIT:
			if (curp->loop == NULL) {
				mandoc_msg(MANDOCERR_WHILE_FAIL,
				    curp->line, pos, NULL);
				break;
			}
			if (inloop == 0) {
				mandoc_msg(MANDOCERR_WHILE_INTO,
				    curp->line, pos, NULL);
				curp->loop = loop = NULL;
				break;
			}
			if (line_result & ROFF_LOOPCONT)
				loop = curp->loop;
			else {
				curp->loop = loop = NULL;
				inloop = 0;
			}
			break;
		default:
			abort();
		}

		/* Process the main instruction from the roff parser. */

		switch (line_result & ROFF_MASK) {
		case ROFF_IGN:
			break;
		case ROFF_CONT:
			if (curp->man->meta.macroset == MACROSET_NONE)
				choose_parser(curp);
			if ((curp->man->meta.macroset == MACROSET_MDOC ?
			     mdoc_parseln(curp->man, curp->line, ln.buf, of) :
			     man_parseln(curp->man, curp->line, ln.buf, of)
			    ) == 2)
				goto out;
			break;
		case ROFF_RERUN:
			goto rerun;
		case ROFF_REPARSE:
			if (++curp->reparse_count > REPARSE_LIMIT) {
				/* Abort and return to the top level. */
				result = ROFF_IGN;
				mandoc_msg(MANDOCERR_ROFFLOOP,
				    curp->line, pos, NULL);
				goto out;
			}
			result = mparse_buf_r(curp, ln, of, 0);
			if (line_result & ROFF_USERCALL) {
				roff_userret(curp->roff);
				/* Continue normally. */
				if (result & ROFF_USERRET)
					result = ROFF_CONT;
			}
			if (start == 0 && result != ROFF_CONT)
				goto out;
			break;
		case ROFF_SO:
			if ( ! (curp->options & MPARSE_SO) &&
			    (i >= blk.sz || blk.buf[i] == '\0')) {
				curp->man->meta.sodest =
				    mandoc_strdup(ln.buf + of);
				goto out;
			}
			if ((fd = mparse_open(curp, ln.buf + of)) != -1) {
				mparse_readfd(curp, fd, ln.buf + of);
				close(fd);
			} else {
				mandoc_msg(MANDOCERR_SO_FAIL,
				    curp->line, of, ".so %s: %s",
				    ln.buf + of, strerror(errno));
				ln.sz = mandoc_asprintf(&cp,
				    ".sp\nSee the file %s.\n.sp",
				    ln.buf + of);
				free(ln.buf);
				ln.buf = cp;
				of = 0;
				mparse_buf_r(curp, ln, of, 0);
			}
			break;
		default:
			abort();
		}

		/* Start the next input line. */

		if (loop != NULL &&
		    (line_result & ROFF_LOOPMASK) == ROFF_IGN)
			loop = loop->next;

		if (loop != NULL) {
			if ((line_result & ROFF_APPEND) == 0)
				*ln.buf = '\0';
			if (ln.sz < loop->sz)
				resize_buf(&ln, loop->sz);
			(void)strlcat(ln.buf, loop->buf, ln.sz);
			of = 0;
			goto rerun;
		}

		pos = (line_result & ROFF_APPEND) ? strlen(ln.buf) : 0;
	}
out:
	if (inloop) {
		if (result != ROFF_USERRET)
			mandoc_msg(MANDOCERR_WHILE_OUTOF,
			    curp->line, pos, NULL);
		curp->loop = NULL;
	}
	free(ln.buf);
	if (firstln != curp->secondary)
		free_buf_list(firstln);
	return result;
}

static int
read_whole_file(struct mparse *curp, int fd, struct buf *fb, int *with_mmap)
{
	struct stat	 st;
	gzFile		 gz;
	size_t		 off;
	ssize_t		 ssz;
	int		 gzerrnum, retval;

	if (fstat(fd, &st) == -1) {
		mandoc_msg(MANDOCERR_FSTAT, 0, 0, "%s", strerror(errno));
		return -1;
	}

	/*
	 * If we're a regular file, try just reading in the whole entry
	 * via mmap().  This is faster than reading it into blocks, and
	 * since each file is only a few bytes to begin with, I'm not
	 * concerned that this is going to tank any machines.
	 */

	if (curp->gzip == 0 && S_ISREG(st.st_mode)) {
		if (st.st_size > 0x7fffffff) {
			mandoc_msg(MANDOCERR_TOOLARGE, 0, 0, NULL);
			return -1;
		}
		*with_mmap = 1;
		fb->sz = (size_t)st.st_size;
		fb->buf = mmap(NULL, fb->sz, PROT_READ, MAP_SHARED, fd, 0);
		if (fb->buf != MAP_FAILED)
			return 0;
	}

	if (curp->gzip) {
		/*
		 * Duplicating the file descriptor is required
		 * because we will have to call gzclose(3)
		 * to free memory used internally by zlib,
		 * but that will also close the file descriptor,
		 * which this function must not do.
		 */
		if ((fd = dup(fd)) == -1) {
			mandoc_msg(MANDOCERR_DUP, 0, 0,
			    "%s", strerror(errno));
			return -1;
		}
		if ((gz = gzdopen(fd, "rb")) == NULL) {
			mandoc_msg(MANDOCERR_GZDOPEN, 0, 0,
			    "%s", strerror(errno));
			close(fd);
			return -1;
		}
	} else
		gz = NULL;

	/*
	 * If this isn't a regular file (like, say, stdin), then we must
	 * go the old way and just read things in bit by bit.
	 */

	*with_mmap = 0;
	off = 0;
	retval = -1;
	fb->sz = 0;
	fb->buf = NULL;
	for (;;) {
		if (off == fb->sz) {
			if (fb->sz == (1U << 31)) {
				mandoc_msg(MANDOCERR_TOOLARGE, 0, 0, NULL);
				break;
			}
			resize_buf(fb, 65536);
		}
		ssz = curp->gzip ?
		    gzread(gz, fb->buf + (int)off, fb->sz - off) :
		    read(fd, fb->buf + (int)off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			retval = 0;
			break;
		}
		if (ssz == -1) {
			if (curp->gzip)
				(void)gzerror(gz, &gzerrnum);
			mandoc_msg(MANDOCERR_READ, 0, 0, "%s",
			    curp->gzip && gzerrnum != Z_ERRNO ?
			    zError(gzerrnum) : strerror(errno));
			break;
		}
		off += (size_t)ssz;
	}

	if (curp->gzip && (gzerrnum = gzclose(gz)) != Z_OK)
		mandoc_msg(MANDOCERR_GZCLOSE, 0, 0, "%s",
		    gzerrnum == Z_ERRNO ? strerror(errno) :
		    zError(gzerrnum));
	if (retval == -1) {
		free(fb->buf);
		fb->buf = NULL;
	}
	return retval;
}

static void
mparse_end(struct mparse *curp)
{
	if (curp->man->meta.macroset == MACROSET_NONE)
		curp->man->meta.macroset = MACROSET_MAN;
	if (curp->man->meta.macroset == MACROSET_MDOC)
		mdoc_endparse(curp->man);
	else
		man_endparse(curp->man);
	roff_endparse(curp->roff);
}

/*
 * Read the whole file into memory and call the parsers.
 * Called recursively when an .so request is encountered.
 */
void
mparse_readfd(struct mparse *curp, int fd, const char *filename)
{
	static int	 recursion_depth;

	struct buf	 blk;
	struct buf	*save_primary;
	const char	*save_filename, *cp;
	size_t		 offset;
	int		 save_filenc, save_lineno;
	int		 with_mmap;

	if (recursion_depth > 64) {
		mandoc_msg(MANDOCERR_ROFFLOOP, curp->line, 0, NULL);
		return;
	} else if (recursion_depth == 0 &&
	    (cp = strrchr(filename, '.')) != NULL &&
            cp[1] >= '1' && cp[1] <= '9')
                curp->man->filesec = cp[1];
        else
                curp->man->filesec = '\0';

	if (read_whole_file(curp, fd, &blk, &with_mmap) == -1)
		return;

	/*
	 * Save some properties of the parent file.
	 */

	save_primary = curp->primary;
	save_filenc = curp->filenc;
	save_lineno = curp->line;
	save_filename = mandoc_msg_getinfilename();

	curp->primary = &blk;
	curp->filenc = curp->options & (MPARSE_UTF8 | MPARSE_LATIN1);
	curp->line = 1;
	mandoc_msg_setinfilename(filename);

	/* Skip an UTF-8 byte order mark. */
	if (curp->filenc & MPARSE_UTF8 && blk.sz > 2 &&
	    (unsigned char)blk.buf[0] == 0xef &&
	    (unsigned char)blk.buf[1] == 0xbb &&
	    (unsigned char)blk.buf[2] == 0xbf) {
		offset = 3;
		curp->filenc &= ~MPARSE_LATIN1;
	} else
		offset = 0;

	recursion_depth++;
	mparse_buf_r(curp, blk, offset, 1);
	if (--recursion_depth == 0)
		mparse_end(curp);

	/*
	 * Clean up and restore saved parent properties.
	 */

	if (with_mmap)
		munmap(blk.buf, blk.sz);
	else
		free(blk.buf);

	curp->primary = save_primary;
	curp->filenc = save_filenc;
	curp->line = save_lineno;
	if (save_filename != NULL)
		mandoc_msg_setinfilename(save_filename);
}

int
mparse_open(struct mparse *curp, const char *file)
{
	char		 *cp;
	int		  fd, save_errno;

	cp = strrchr(file, '.');
	curp->gzip = (cp != NULL && ! strcmp(cp + 1, "gz"));

	/* First try to use the filename as it is. */

	if ((fd = open(file, O_RDONLY)) != -1)
		return fd;

	/*
	 * If that doesn't work and the filename doesn't
	 * already  end in .gz, try appending .gz.
	 */

	if ( ! curp->gzip) {
		save_errno = errno;
		mandoc_asprintf(&cp, "%s.gz", file);
		fd = open(cp, O_RDONLY);
		free(cp);
		errno = save_errno;
		if (fd != -1) {
			curp->gzip = 1;
			return fd;
		}
	}

	/* Neither worked, give up. */

	return -1;
}

struct mparse *
mparse_alloc(int options, enum mandoc_os os_e, const char *os_s)
{
	struct mparse	*curp;

	curp = mandoc_calloc(1, sizeof(struct mparse));

	curp->options = options;
	curp->os_s = os_s;

	curp->roff = roff_alloc(options);
	curp->man = roff_man_alloc(curp->roff, curp->os_s,
		curp->options & MPARSE_QUICK ? 1 : 0);
	if (curp->options & MPARSE_MDOC) {
		curp->man->meta.macroset = MACROSET_MDOC;
		if (curp->man->mdocmac == NULL)
			curp->man->mdocmac = roffhash_alloc(MDOC_Dd, MDOC_MAX);
	} else if (curp->options & MPARSE_MAN) {
		curp->man->meta.macroset = MACROSET_MAN;
		if (curp->man->manmac == NULL)
			curp->man->manmac = roffhash_alloc(MAN_TH, MAN_MAX);
	}
	curp->man->meta.first->tok = TOKEN_NONE;
	curp->man->meta.os_e = os_e;
	tag_alloc();
	return curp;
}

void
mparse_reset(struct mparse *curp)
{
	tag_free();
	roff_reset(curp->roff);
	roff_man_reset(curp->man);
	free_buf_list(curp->secondary);
	curp->secondary = NULL;
	curp->gzip = 0;
	tag_alloc();
}

void
mparse_free(struct mparse *curp)
{
	tag_free();
	roffhash_free(curp->man->mdocmac);
	roffhash_free(curp->man->manmac);
	roff_man_free(curp->man);
	roff_free(curp->roff);
	free_buf_list(curp->secondary);
	free(curp);
}

struct roff_meta *
mparse_result(struct mparse *curp)
{
	roff_state_reset(curp->man);
	if (curp->options & MPARSE_VALIDATE) {
		if (curp->man->meta.macroset == MACROSET_MDOC)
			mdoc_validate(curp->man);
		else
			man_validate(curp->man);
		tag_postprocess(curp->man, curp->man->meta.first);
	}
	return &curp->man->meta;
}

void
mparse_copy(const struct mparse *p)
{
	struct buf	*buf;

	for (buf = p->secondary; buf != NULL; buf = buf->next)
		puts(buf->buf);
}
