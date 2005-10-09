/*	$OpenBSD: compress.h,v 1.11 2002/03/04 17:27:39 stevesk Exp $	*/

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

#ifndef COMPRESS_H
#define COMPRESS_H

void	 buffer_compress_init_send(int);
void	 buffer_compress_init_recv(void);
void     buffer_compress_uninit(void);
void     buffer_compress(Buffer *, Buffer *);
void     buffer_uncompress(Buffer *, Buffer *);

#endif				/* COMPRESS_H */
