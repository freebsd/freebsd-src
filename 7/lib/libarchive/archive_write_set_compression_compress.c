/*-
 * Copyright (c) 2008 Joerg Sonnenberger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1985, 1986, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis and James A. Woods, derived from original
 * work by Spencer Thomas and Joseph Orost.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "archive_platform.h"

__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_write_private.h"

#define	HSIZE		69001	/* 95% occupancy */
#define	HSHIFT		8	/* 8 - trunc(log2(HSIZE / 65536)) */
#define	CHECK_GAP 10000		/* Ratio check interval. */

#define	MAXCODE(bits)	((1 << (bits)) - 1)

/*
 * the next two codes should not be changed lightly, as they must not
 * lie within the contiguous general code space.
 */
#define	FIRST	257		/* First free entry. */
#define	CLEAR	256		/* Table clear output code. */

struct private_data {
	off_t in_count, out_count, checkpoint;

	int code_len;			/* Number of bits/code. */
	int cur_maxcode;		/* Maximum code, given n_bits. */
	int max_maxcode;		/* Should NEVER generate this code. */
	int hashtab [HSIZE];
	unsigned short codetab [HSIZE];
	int first_free;		/* First unused entry. */
	int compress_ratio;

	int cur_code, cur_fcode;

	int bit_offset;
	unsigned char bit_buf;

	unsigned char	*compressed;
	size_t		 compressed_buffer_size;
	size_t		 compressed_offset;
};

static int	archive_compressor_compress_finish(struct archive_write *);
static int	archive_compressor_compress_init(struct archive_write *);
static int	archive_compressor_compress_write(struct archive_write *,
		    const void *, size_t);

/*
 * Allocate, initialize and return a archive object.
 */
int
archive_write_set_compression_compress(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	__archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_compression_compress");
	a->compressor.init = &archive_compressor_compress_init;
	a->archive.compression_code = ARCHIVE_COMPRESSION_COMPRESS;
	a->archive.compression_name = "compress";
	return (ARCHIVE_OK);
}

/*
 * Setup callback.
 */
static int
archive_compressor_compress_init(struct archive_write *a)
{
	int ret;
	struct private_data *state;

	a->archive.compression_code = ARCHIVE_COMPRESSION_COMPRESS;
	a->archive.compression_name = "compress";

	if (a->bytes_per_block < 4) {
		archive_set_error(&a->archive, EINVAL,
		    "Can't write Compress header as single block");
		return (ARCHIVE_FATAL);
	}

	if (a->client_opener != NULL) {
		ret = (a->client_opener)(&a->archive, a->client_data);
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	state = (struct private_data *)malloc(sizeof(*state));
	if (state == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression");
		return (ARCHIVE_FATAL);
	}
	memset(state, 0, sizeof(*state));

	state->compressed_buffer_size = a->bytes_per_block;
	state->compressed = malloc(state->compressed_buffer_size);

	if (state->compressed == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate data for compression buffer");
		free(state);
		return (ARCHIVE_FATAL);
	}

	a->compressor.write = archive_compressor_compress_write;
	a->compressor.finish = archive_compressor_compress_finish;

	state->max_maxcode = 0x10000;	/* Should NEVER generate this code. */
	state->in_count = 0;		/* Length of input. */
	state->bit_buf = 0;
	state->bit_offset = 0;
	state->out_count = 3;		/* Includes 3-byte header mojo. */
	state->compress_ratio = 0;
	state->checkpoint = CHECK_GAP;
	state->code_len = 9;
	state->cur_maxcode = MAXCODE(state->code_len);
	state->first_free = FIRST;

	memset(state->hashtab, 0xff, sizeof(state->hashtab));

	/* Prime output buffer with a gzip header. */
	state->compressed[0] = 0x1f; /* Compress */
	state->compressed[1] = 0x9d;
	state->compressed[2] = 0x90; /* Block mode, 16bit max */
	state->compressed_offset = 3;

	a->compressor.data = state;
	return (0);
}

/*-
 * Output the given code.
 * Inputs:
 * 	code:	A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *		that n_bits =< (long)wordsize - 1.
 * Outputs:
 * 	Outputs code to the file.
 * Assumptions:
 *	Chars are 8 bits long.
 * Algorithm:
 * 	Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static unsigned char rmask[9] =
	{0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

static int
output_byte(struct archive_write *a, unsigned char c)
{
	struct private_data *state = a->compressor.data;
	ssize_t bytes_written;

	state->compressed[state->compressed_offset++] = c;
	++state->out_count;

	if (state->compressed_buffer_size == state->compressed_offset) {
		bytes_written = (a->client_writer)(&a->archive,
		    a->client_data,
		    state->compressed, state->compressed_buffer_size);
		if (bytes_written <= 0)
			return ARCHIVE_FATAL;
		a->archive.raw_position += bytes_written;
		state->compressed_offset = 0;
	}

	return ARCHIVE_OK;
}

static int
output_code(struct archive_write *a, int ocode)
{
	struct private_data *state = a->compressor.data;
	int bits, ret, clear_flg, bit_offset;

	clear_flg = ocode == CLEAR;
	bits = state->code_len;

	/*
	 * Since ocode is always >= 8 bits, only need to mask the first
	 * hunk on the left.
	 */
	bit_offset = state->bit_offset % 8;
	state->bit_buf |= (ocode << bit_offset) & 0xff;
	output_byte(a, state->bit_buf);

	bits = state->code_len - (8 - bit_offset);
	ocode >>= 8 - bit_offset;
	/* Get any 8 bit parts in the middle (<=1 for up to 16 bits). */
	if (bits >= 8) {
		output_byte(a, ocode & 0xff);
		ocode >>= 8;
		bits -= 8;
	}
	/* Last bits. */
	state->bit_offset += state->code_len;
	state->bit_buf = ocode & rmask[bits];
	if (state->bit_offset == state->code_len * 8)
		state->bit_offset = 0;

	/*
	 * If the next entry is going to be too big for the ocode size,
	 * then increase it, if possible.
	 */
	if (clear_flg || state->first_free > state->cur_maxcode) {
	       /*
		* Write the whole buffer, because the input side won't
		* discover the size increase until after it has read it.
		*/
		if (state->bit_offset > 0) {
			while (state->bit_offset < state->code_len * 8) {
				ret = output_byte(a, state->bit_buf);
				if (ret != ARCHIVE_OK)
					return ret;
				state->bit_offset += 8;
				state->bit_buf = 0;
			}
		}
		state->bit_buf = 0;
		state->bit_offset = 0;

		if (clear_flg) {
			state->code_len = 9;
			state->cur_maxcode = MAXCODE(state->code_len);
		} else {
			state->code_len++;
			if (state->code_len == 16)
				state->cur_maxcode = state->max_maxcode;
			else
				state->cur_maxcode = MAXCODE(state->code_len);
		}
	}

	return (ARCHIVE_OK);
}

static int
output_flush(struct archive_write *a)
{
	struct private_data *state = a->compressor.data;
	int ret;

	/* At EOF, write the rest of the buffer. */
	if (state->bit_offset % 8) {
		state->code_len = (state->bit_offset % 8 + 7) / 8;
		ret = output_byte(a, state->bit_buf);
		if (ret != ARCHIVE_OK)
			return ret;
	}

	return (ARCHIVE_OK);
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_compress_write(struct archive_write *a, const void *buff,
    size_t length)
{
	struct private_data *state;
	int i;
	int ratio;
	int c, disp, ret;
	const unsigned char *bp;

	state = (struct private_data *)a->compressor.data;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		return (ARCHIVE_FATAL);
	}

	if (length == 0)
		return ARCHIVE_OK;

	bp = buff;

	if (state->in_count == 0) {
		state->cur_code = *bp++;
		++state->in_count;
		--length;
	}

	while (length--) {
		c = *bp++;
		state->in_count++;
		state->cur_fcode = (c << 16) + state->cur_code;
		i = ((c << HSHIFT) ^ state->cur_code);	/* Xor hashing. */

		if (state->hashtab[i] == state->cur_fcode) {
			state->cur_code = state->codetab[i];
			continue;
		}
		if (state->hashtab[i] < 0)	/* Empty slot. */
			goto nomatch;
		/* Secondary hash (after G. Knott). */
		if (i == 0)
			disp = 1;
		else
			disp = HSIZE - i;
 probe:		
		if ((i -= disp) < 0)
			i += HSIZE;

		if (state->hashtab[i] == state->cur_fcode) {
			state->cur_code = state->codetab[i];
			continue;
		}
		if (state->hashtab[i] >= 0)
			goto probe;
 nomatch:	
		ret = output_code(a, state->cur_code);
		if (ret != ARCHIVE_OK)
			return ret;
		state->cur_code = c;
		if (state->first_free < state->max_maxcode) {
			state->codetab[i] = state->first_free++;	/* code -> hashtable */
			state->hashtab[i] = state->cur_fcode;
			continue;
		}
		if (state->in_count < state->checkpoint)
			continue;

		state->checkpoint = state->in_count + CHECK_GAP;

		if (state->in_count <= 0x007fffff)
			ratio = state->in_count * 256 / state->out_count;
		else if ((ratio = state->out_count / 256) == 0)
			ratio = 0x7fffffff;
		else
			ratio = state->in_count / ratio;

		if (ratio > state->compress_ratio)
			state->compress_ratio = ratio;
		else {
			state->compress_ratio = 0;
			memset(state->hashtab, 0xff, sizeof(state->hashtab));
			state->first_free = FIRST;
			ret = output_code(a, CLEAR);
			if (ret != ARCHIVE_OK)
				return ret;
		}
	}

	return (ARCHIVE_OK);
}


/*
 * Finish the compression...
 */
static int
archive_compressor_compress_finish(struct archive_write *a)
{
	ssize_t block_length, target_block_length, bytes_written;
	int ret;
	struct private_data *state;
	unsigned tocopy;

	state = (struct private_data *)a->compressor.data;
	ret = 0;
	if (a->client_writer == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "No write callback is registered?  "
		    "This is probably an internal programming error.");
		ret = ARCHIVE_FATAL;
		goto cleanup;
	}

	/* By default, always pad the uncompressed data. */
	if (a->pad_uncompressed) {
		while (state->in_count % a->bytes_per_block != 0) {
			tocopy = a->bytes_per_block -
			    (state->in_count % a->bytes_per_block);
			if (tocopy > a->null_length)
				tocopy = a->null_length;
			ret = archive_compressor_compress_write(a, a->nulls,
			    tocopy);
			if (ret != ARCHIVE_OK)
				goto cleanup;
		}
	}

	ret = output_code(a, state->cur_code);
	if (ret != ARCHIVE_OK)
		goto cleanup;
	ret = output_flush(a);
	if (ret != ARCHIVE_OK)
		goto cleanup;

	/* Optionally, pad the final compressed block. */
	block_length = state->compressed_offset;

	/* Tricky calculation to determine size of last block. */
	if (a->bytes_in_last_block <= 0)
		/* Default or Zero: pad to full block */
		target_block_length = a->bytes_per_block;
	else
		/* Round length to next multiple of bytes_in_last_block. */
		target_block_length = a->bytes_in_last_block *
		    ( (block_length + a->bytes_in_last_block - 1) /
			a->bytes_in_last_block);
	if (target_block_length > a->bytes_per_block)
		target_block_length = a->bytes_per_block;
	if (block_length < target_block_length) {
		memset(state->compressed + state->compressed_offset, 0,
		    target_block_length - block_length);
		block_length = target_block_length;
	}

	/* Write the last block */
	bytes_written = (a->client_writer)(&a->archive, a->client_data,
	    state->compressed, block_length);
	if (bytes_written <= 0)
		ret = ARCHIVE_FATAL;
	else
		a->archive.raw_position += bytes_written;

cleanup:
	free(state->compressed);
	free(state);
	return (ret);
}
