/*
 * 
 * compress.c
 * 
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * 
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * 
 * Created: Wed Oct 25 22:12:46 1995 ylo
 * 
 * Interface to packet compression for ssh.
 * 
 */

#include "includes.h"
RCSID("$Id: compress.c,v 1.4 1999/11/24 19:53:46 markus Exp $");

#include "ssh.h"
#include "buffer.h"
#include "zlib.h"

static z_stream incoming_stream;
static z_stream outgoing_stream;

/*
 * Initializes compression; level is compression level from 1 to 9
 * (as in gzip).
 */

void 
buffer_compress_init(int level)
{
	debug("Enabling compression at level %d.", level);
	if (level < 1 || level > 9)
		fatal("Bad compression level %d.", level);
	inflateInit(&incoming_stream);
	deflateInit(&outgoing_stream, level);
}

/* Frees any data structures allocated for compression. */

void 
buffer_compress_uninit()
{
	debug("compress outgoing: raw data %lu, compressed %lu, factor %.2f",
	      outgoing_stream.total_in, outgoing_stream.total_out,
	      outgoing_stream.total_in == 0 ? 0.0 :
	      (double) outgoing_stream.total_out / outgoing_stream.total_in);
	debug("compress incoming: raw data %lu, compressed %lu, factor %.2f",
	      incoming_stream.total_out, incoming_stream.total_in,
	      incoming_stream.total_out == 0 ? 0.0 :
	      (double) incoming_stream.total_in / incoming_stream.total_out);
	inflateEnd(&incoming_stream);
	deflateEnd(&outgoing_stream);
}

/*
 * Compresses the contents of input_buffer into output_buffer.  All packets
 * compressed using this function will form a single compressed data stream;
 * however, data will be flushed at the end of every call so that each
 * output_buffer can be decompressed independently (but in the appropriate
 * order since they together form a single compression stream) by the
 * receiver.  This appends the compressed data to the output buffer.
 */

void 
buffer_compress(Buffer * input_buffer, Buffer * output_buffer)
{
	char buf[4096];
	int status;

	/* This case is not handled below. */
	if (buffer_len(input_buffer) == 0)
		return;

	/* Input is the contents of the input buffer. */
	outgoing_stream.next_in = buffer_ptr(input_buffer);
	outgoing_stream.avail_in = buffer_len(input_buffer);

	/* Loop compressing until deflate() returns with avail_out != 0. */
	do {
		/* Set up fixed-size output buffer. */
		outgoing_stream.next_out = buf;
		outgoing_stream.avail_out = sizeof(buf);

		/* Compress as much data into the buffer as possible. */
		status = deflate(&outgoing_stream, Z_PARTIAL_FLUSH);
		switch (status) {
		case Z_OK:
			/* Append compressed data to output_buffer. */
			buffer_append(output_buffer, buf,
				      sizeof(buf) - outgoing_stream.avail_out);
			break;
		case Z_STREAM_END:
			fatal("buffer_compress: deflate returned Z_STREAM_END");
			/* NOTREACHED */
		case Z_STREAM_ERROR:
			fatal("buffer_compress: deflate returned Z_STREAM_ERROR");
			/* NOTREACHED */
		case Z_BUF_ERROR:
			fatal("buffer_compress: deflate returned Z_BUF_ERROR");
			/* NOTREACHED */
		default:
			fatal("buffer_compress: deflate returned %d", status);
			/* NOTREACHED */
		}
	}
	while (outgoing_stream.avail_out == 0);
}

/*
 * Uncompresses the contents of input_buffer into output_buffer.  All packets
 * uncompressed using this function will form a single compressed data
 * stream; however, data will be flushed at the end of every call so that
 * each output_buffer.  This must be called for the same size units that the
 * buffer_compress was called, and in the same order that buffers compressed
 * with that.  This appends the uncompressed data to the output buffer.
 */

void 
buffer_uncompress(Buffer * input_buffer, Buffer * output_buffer)
{
	char buf[4096];
	int status;

	incoming_stream.next_in = buffer_ptr(input_buffer);
	incoming_stream.avail_in = buffer_len(input_buffer);

	incoming_stream.next_out = buf;
	incoming_stream.avail_out = sizeof(buf);

	for (;;) {
		status = inflate(&incoming_stream, Z_PARTIAL_FLUSH);
		switch (status) {
		case Z_OK:
			buffer_append(output_buffer, buf,
				      sizeof(buf) - incoming_stream.avail_out);
			incoming_stream.next_out = buf;
			incoming_stream.avail_out = sizeof(buf);
			break;
		case Z_STREAM_END:
			fatal("buffer_uncompress: inflate returned Z_STREAM_END");
			/* NOTREACHED */
		case Z_DATA_ERROR:
			fatal("buffer_uncompress: inflate returned Z_DATA_ERROR");
			/* NOTREACHED */
		case Z_STREAM_ERROR:
			fatal("buffer_uncompress: inflate returned Z_STREAM_ERROR");
			/* NOTREACHED */
		case Z_BUF_ERROR:
			/*
			 * Comments in zlib.h say that we should keep calling
			 * inflate() until we get an error.  This appears to
			 * be the error that we get.
			 */
			return;
		case Z_MEM_ERROR:
			fatal("buffer_uncompress: inflate returned Z_MEM_ERROR");
			/* NOTREACHED */
		default:
			fatal("buffer_uncompress: inflate returned %d", status);
		}
	}
}
