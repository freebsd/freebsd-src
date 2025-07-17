/*
 * Copyright (c) 2004-2016 Maxim Sobolev <sobomax@FreeBSD.org>
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
 */

/* CLOOP format and related constants */

/*
 * Integer values (block size, number of blocks, offsets)
 * are stored in big-endian (network) order on disk.
 */

#define CLOOP_MAGIC_LEN 128
#define CLOOP_OFS_COMPR 0x0b
#define CLOOP_OFS_VERSN (CLOOP_OFS_COMPR + 1)

#define CLOOP_MAJVER_2	'2'
#define CLOOP_MAJVER_3	'3'
#define CLOOP_MAJVER_4	'4'

#define	CLOOP_COMP_LIBZ		'V'
#define	CLOOP_COMP_LIBZ_DDP	'v'
#define	CLOOP_COMP_LZMA		'L'
#define	CLOOP_COMP_LZMA_DDP	'l'
#define	CLOOP_COMP_ZSTD		'Z'
#define	CLOOP_COMP_ZSTD_DDP	'z'

#define	CLOOP_MINVER_LZMA	CLOOP_MAJVER_3
#define	CLOOP_MINVER_ZLIB	CLOOP_MAJVER_2
#define	CLOOP_MINVER_ZSTD	CLOOP_MAJVER_4

#define	CLOOP_MINVER_RELIABLE_LASTBLKSZ	CLOOP_MAJVER_4

struct cloop_header {
        char magic[CLOOP_MAGIC_LEN];    /* cloop magic */
        uint32_t blksz;                 /* block size */
        uint32_t nblocks;               /* number of blocks */
};
