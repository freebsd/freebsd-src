/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * Interface to packet compression for ssh.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

/* RCSID("$OpenBSD: compress.h,v 1.8 2001/04/05 10:39:02 markus Exp $"); */

#ifndef COMPRESS_H
#define COMPRESS_H

/*
 * Initializes compression; level is compression level from 1 to 9 (as in
 * gzip).
 */
void	buffer_compress_init_send(int level);
void	buffer_compress_init_recv(void);

/* Frees any data structures allocated by buffer_compress_init. */
void    buffer_compress_uninit(void);

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
