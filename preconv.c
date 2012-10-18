/*	$Id: preconv.c,v 1.5 2011/07/24 18:15:14 kristaps Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 
 * The read_whole_file() and resize_buf() functions are copied from
 * read.c, including all dependency code (MAP_FILE, etc.).
 */

#ifndef MAP_FILE
#define	MAP_FILE	0
#endif

enum	enc {
	ENC_UTF_8, /* UTF-8 */
	ENC_US_ASCII, /* US-ASCII */
	ENC_LATIN_1, /* Latin-1 */
	ENC__MAX
};

struct	buf {
	char		 *buf; /* binary input buffer */
	size_t	 	  sz; /* size of binary buffer */
	size_t		  offs; /* starting buffer offset */
};

struct	encode {
	const char	 *name;
	int		(*conv)(const struct buf *);
};

static	int	 cue_enc(const struct buf *, size_t *, enum enc *);
static	int	 conv_latin_1(const struct buf *);
static	int	 conv_us_ascii(const struct buf *);
static	int	 conv_utf_8(const struct buf *);
static	int	 read_whole_file(const char *, int, 
			struct buf *, int *);
static	void	 resize_buf(struct buf *, size_t);
static	void	 usage(void);

static	const struct encode encs[ENC__MAX] = {
	{ "utf-8", conv_utf_8 }, /* ENC_UTF_8 */
	{ "us-ascii", conv_us_ascii }, /* ENC_US_ASCII */
	{ "latin-1", conv_latin_1 }, /* ENC_LATIN_1 */
};

static	const char	 *progname;

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-D enc] "
			"[-e ENC] "
			"[file]\n", progname);
}

static int
conv_latin_1(const struct buf *b)
{
	size_t		 i;
	unsigned char	 cu;
	const char	*cp;

	cp = b->buf + (int)b->offs;

	/*
	 * Latin-1 falls into the first 256 code-points of Unicode, so
	 * there's no need for any sort of translation.  Just make the
	 * 8-bit characters use the Unicode escape.
	 * Note that binary values 128 < v < 160 are passed through
	 * unmodified to mandoc.
	 */

	for (i = b->offs; i < b->sz; i++) {
		cu = (unsigned char)*cp++;
		cu < 160U ? putchar(cu) : printf("\\[u%.4X]", cu);
	}

	return(1);
}

static int
conv_us_ascii(const struct buf *b)
{

	/*
	 * US-ASCII has no conversion since it falls into the first 128
	 * bytes of Unicode.
	 */

	fwrite(b->buf, 1, b->sz, stdout);
	return(1);
}

static int
conv_utf_8(const struct buf *b)
{
	int		 state, be;
	unsigned int	 accum;
	size_t		 i;
	unsigned char	 cu;
	const char	*cp;
	const long	 one = 1L;

	cp = b->buf + (int)b->offs;
	state = 0;
	accum = 0U;
	be = 0;

	/* Quick test for big-endian value. */

	if ( ! (*((const char *)(&one))))
		be = 1;

	for (i = b->offs; i < b->sz; i++) {
		cu = (unsigned char)*cp++;
		if (state) {
			if ( ! (cu & 128) || (cu & 64)) {
				/* Bad sequence header. */
				return(0);
			}

			/* Accept only legitimate bit patterns. */

			if (cu > 191 || cu < 128) {
				/* Bad in-sequence bits. */
				return(0);
			}

			accum |= (cu & 63) << --state * 6;

			/*
			 * Accum is held in little-endian order as
			 * stipulated by the UTF-8 sequence coding.  We
			 * need to convert to a native big-endian if our
			 * architecture requires it.
			 */

			if (0 == state && be) 
				accum = (accum >> 24) | 
					((accum << 8) & 0x00FF0000) |
					((accum >> 8) & 0x0000FF00) |
					(accum << 24);

			if (0 == state) {
				accum < 128U ? putchar(accum) : 
					printf("\\[u%.4X]", accum);
				accum = 0U;
			}
		} else if (cu & (1 << 7)) {
			/*
			 * Entering a UTF-8 state:  if we encounter a
			 * UTF-8 bitmask, calculate the expected UTF-8
			 * state from it.
			 */
			for (state = 0; state < 7; state++) 
				if ( ! (cu & (1 << (7 - state))))
					break;

			/* Accept only legitimate bit patterns. */

			switch (state) {
			case (4):
				if (cu <= 244 && cu >= 240) {
					accum = (cu & 7) << 18;
					break;
				}
				/* Bad 4-sequence start bits. */
				return(0);
			case (3):
				if (cu <= 239 && cu >= 224) {
					accum = (cu & 15) << 12;
					break;
				}
				/* Bad 3-sequence start bits. */
				return(0);
			case (2):
				if (cu <= 223 && cu >= 194) {
					accum = (cu & 31) << 6;
					break;
				}
				/* Bad 2-sequence start bits. */
				return(0);
			default:
				/* Bad sequence bit mask. */
				return(0);
			}
			state--;
		} else
			putchar(cu);
	}

	if (0 != state) {
		/* Bad trailing bits. */
		return(0);
	}

	return(1);
}

static void
resize_buf(struct buf *buf, size_t initial)
{

	buf->sz = buf->sz > initial / 2 ? 
		2 * buf->sz : initial;

	buf->buf = realloc(buf->buf, buf->sz);
	if (NULL == buf->buf) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
}

static int
read_whole_file(const char *f, int fd, 
		struct buf *fb, int *with_mmap)
{
	size_t		 off;
	ssize_t		 ssz;

#ifdef	HAVE_MMAP
	struct stat	 st;
	if (-1 == fstat(fd, &st)) {
		perror(f);
		return(0);
	}

	/*
	 * If we're a regular file, try just reading in the whole entry
	 * via mmap().  This is faster than reading it into blocks, and
	 * since each file is only a few bytes to begin with, I'm not
	 * concerned that this is going to tank any machines.
	 */

	if (S_ISREG(st.st_mode) && st.st_size >= (1U << 31)) {
		fprintf(stderr, "%s: input too large\n", f);
		return(0);
	} 
	
	if (S_ISREG(st.st_mode)) {
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
		if (off == fb->sz && fb->sz == (1U << 31)) {
			fprintf(stderr, "%s: input too large\n", f);
			break;
		} 
		
		if (off == fb->sz)
			resize_buf(fb, 65536);

		ssz = read(fd, fb->buf + (int)off, fb->sz - off);
		if (ssz == 0) {
			fb->sz = off;
			return(1);
		}
		if (ssz == -1) {
			perror(f);
			break;
		}
		off += (size_t)ssz;
	}

	free(fb->buf);
	fb->buf = NULL;
	return(0);
}

static int
cue_enc(const struct buf *b, size_t *offs, enum enc *enc)
{
	const char	*ln, *eoln, *eoph;
	size_t		 sz, phsz, nsz;
	int		 i;

	ln = b->buf + (int)*offs;
	sz = b->sz - *offs;

	/* Look for the end-of-line. */

	if (NULL == (eoln = memchr(ln, '\n', sz)))
		return(-1);

	/* Set next-line marker. */

	*offs = (size_t)((eoln + 1) - b->buf);

	/* Check if we have the correct header/trailer. */

	if ((sz = (size_t)(eoln - ln)) < 10 || 
			memcmp(ln, ".\\\" -*-", 7) ||
			memcmp(eoln - 3, "-*-", 3))
		return(0);

	/* Move after the header and adjust for the trailer. */

	ln += 7;
	sz -= 10;

	while (sz > 0) {
		while (sz > 0 && ' ' == *ln) {
			ln++;
			sz--;
		}
		if (0 == sz)
			break;

		/* Find the end-of-phrase marker (or eoln). */

		if (NULL == (eoph = memchr(ln, ';', sz)))
			eoph = eoln - 3;
		else
			eoph++;

		/* Only account for the "coding" phrase. */

		if ((phsz = (size_t)(eoph - ln)) < 7 ||
				strncasecmp(ln, "coding:", 7)) {
			sz -= phsz;
			ln += phsz;
			continue;
		} 

		sz -= 7;
		ln += 7;

		while (sz > 0 && ' ' == *ln) {
			ln++;
			sz--;
		}
		if (0 == sz)
			break;

		/* Check us against known encodings. */

		for (i = 0; i < (int)ENC__MAX; i++) {
			nsz = strlen(encs[i].name);
			if (phsz < nsz)
				continue;
			if (strncasecmp(ln, encs[i].name, nsz))
				continue;

			*enc = (enum enc)i;
			return(1);
		}

		/* Unknown encoding. */

		*enc = ENC__MAX;
		return(1);
	}

	return(0);
}

int
main(int argc, char *argv[])
{
	int	 	 i, ch, map, fd, rc;
	struct buf	 b;
	const char	*fn;
	enum enc	 enc, def;
	unsigned char 	 bom[3] = { 0xEF, 0xBB, 0xBF };
	size_t		 offs;
	extern int	 optind;
	extern char	*optarg;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	fn = "<stdin>";
	fd = STDIN_FILENO;
	rc = EXIT_FAILURE;
	enc = def = ENC__MAX;
	map = 0;

	memset(&b, 0, sizeof(struct buf));

	while (-1 != (ch = getopt(argc, argv, "D:e:rdvh")))
		switch (ch) {
		case ('D'):
			/* FALLTHROUGH */
		case ('e'):
			for (i = 0; i < (int)ENC__MAX; i++) {
				if (strcasecmp(optarg, encs[i].name))
					continue;
				break;
			}
			if (i < (int)ENC__MAX) {
				if ('D' == ch)
					def = (enum enc)i;
				else
					enc = (enum enc)i;
				break;
			}

			fprintf(stderr, "%s: Bad encoding\n", optarg);
			return(EXIT_FAILURE);
		case ('r'):
			/* FALLTHROUGH */
		case ('d'):
			/* FALLTHROUGH */
		case ('v'):
			/* Compatibility with GNU preconv. */
			break;
		case ('h'):
			/* Compatibility with GNU preconv. */
			/* FALLTHROUGH */
		default:
			usage();
			return(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;
	
	/* 
	 * Open and read the first argument on the command-line.
	 * If we don't have one, we default to stdin.
	 */

	if (argc > 0) {
		fn = *argv;
		fd = open(fn, O_RDONLY, 0);
		if (-1 == fd) {
			perror(fn);
			return(EXIT_FAILURE);
		}
	}

	if ( ! read_whole_file(fn, fd, &b, &map))
		goto out;

	/* Try to read the UTF-8 BOM. */

	if (ENC__MAX == enc)
		if (b.sz > 3 && 0 == memcmp(b.buf, bom, 3)) {
			b.offs = 3;
			enc = ENC_UTF_8;
		}

	/* Try reading from the "-*-" cue. */

	if (ENC__MAX == enc) {
		offs = b.offs;
		ch = cue_enc(&b, &offs, &enc);
		if (0 == ch)
			ch = cue_enc(&b, &offs, &enc);
	}

	/*
	 * No encoding has been detected.
	 * Thus, we either fall into our default encoder, if specified,
	 * or use Latin-1 if all else fails.
	 */

	if (ENC__MAX == enc) 
		enc = ENC__MAX == def ? ENC_LATIN_1 : def;

	if ( ! (*encs[(int)enc].conv)(&b)) {
		fprintf(stderr, "%s: Bad encoding\n", fn);
		goto out;
	}

	rc = EXIT_SUCCESS;
out:
#ifdef	HAVE_MMAP
	if (map)
		munmap(b.buf, b.sz);
	else 
#endif
		free(b.buf);

	if (fd > STDIN_FILENO)
		close(fd);

	return(rc);
}
