/*	$OpenBSD: sftp-common.h,v 1.1 2001/02/04 11:11:54 djm Exp $	*/

/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

typedef struct Attrib Attrib;

/* File attributes */
struct Attrib {
	u_int32_t	flags;
	u_int64_t	size;
	u_int32_t	uid;
	u_int32_t	gid;
	u_int32_t	perm;
	u_int32_t	atime;
	u_int32_t	mtime;
};

/* Clear contents of attributes structure */
void attrib_clear(Attrib *a);

/* Convert from struct stat to filexfer attribs */
void stat_to_attrib(struct stat *st, Attrib *a);

/* Decode attributes in buffer */
Attrib *decode_attrib(Buffer *b);

/* Encode attributes to buffer */
void encode_attrib(Buffer *b, Attrib *a);

/* Convert from SSH2_FX_ status to text error message */
const char *fx2txt(int status);

