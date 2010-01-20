/*-
 * Copyright (c) 2005-2008 Poul-Henning Kamp
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define FIFOLOG_PT_BYTES_PRE		0
#define FIFOLOG_PT_BYTES_POST		1
#define FIFOLOG_PT_WRITES		2
#define FIFOLOG_PT_FLUSH		3
#define FIFOLOG_PT_SYNC			4
#define FIFOLOG_PT_RUNTIME		5
#define FIFOLOG_NPOINT			6

struct fifolog_writer {
	unsigned			magic;
#define FIFOLOG_WRITER_MAGIC		0xf1f0706

	struct fifolog_file		*ff;

	unsigned			writerate;
	unsigned			syncrate;
	unsigned			compression;

	unsigned			writes_since_sync;

	int				cleanup;

	intmax_t			cnt[FIFOLOG_NPOINT];

	uint32_t			seq;
	off_t				recno;
	int				flag;
	time_t				last;

	u_int				ibufsize;
	u_char				*ibuf;
	u_char				*iptr;

	time_t				starttime;
	time_t				lastwrite;
	time_t				lastsync;
};
