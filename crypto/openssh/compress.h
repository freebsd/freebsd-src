/*
 * 
 * compress.h
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

/* RCSID("$Id: compress.h,v 1.3 1999/11/24 19:53:46 markus Exp $"); */

#ifndef COMPRESS_H
#define COMPRESS_H

/*
 * Initializes compression; level is compression level from 1 to 9 (as in
 * gzip).
 */
void    buffer_compress_init(int level);

/* Frees any data structures allocated by buffer_compress_init. */
void    buffer_compress_uninit();

/*
 * Compresses the contents of input_buffer into output_buffer.  All packets
 * compressed using this function will form a single compressed data stream;
 * however, data will be flushed at the end of every call so that each
 * output_buffer can be decompressed independently (but in the appropriate
 * order since they together form a single compression stream) by the
 * receiver.  This appends the compressed data to the output buffer.
 */
void    buffer_compress(Buffer * input_buffer, Buffer * output_buffer);

/*
 * Uncompresses the contents of input_buffer into output_buffer.  All packets
 * uncompressed using this function will form a single compressed data
 * stream; however, data will be flushed at the end of every call so that
 * each output_buffer.  This must be called for the same size units that the
 * buffer_compress was called, and in the same order that buffers compressed
 * with that.  This appends the uncompressed data to the output buffer.
 */
void    buffer_uncompress(Buffer * input_buffer, Buffer * output_buffer);

#endif				/* COMPRESS_H */
