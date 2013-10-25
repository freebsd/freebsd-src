/*-
 * Copyright (c) 2013 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>

#include <bzlib.h>
#ifdef MD5_SUPPORT
#include <md5.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "eav.h"

#ifdef __linux__
#define        roundup2(x, y)  (((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */

static void *
reallocf(void *ptr, size_t size)
{
	void *tmp;

	tmp = ptr;
	ptr = realloc(ptr, size);
	if (ptr == NULL)
		free(tmp);
	return (ptr);
}
#endif

enum eav_compression
eav_taste(const unsigned char *buf, off_t len)
{

	/*
	 * BZIP header from wikipedia:
	 * .magic:16	= 'BZ' signature/magic number
	 * .version:8	= 'h' for Bzip2
	 *                 ('H'uffman coding),
	 *		   '0' for Bzip1 (deprecated)
	 * .hundred_k_blocksize:8 = '1'..'9'
	 *		   block-size 100 kB-900 kB
	 * .compressed_magic:48 =
	 *		   0x314159265359 (BCD (pi))
	 */
	if( len > 10 && buf[0] == 'B' && buf[1] == 'Z' &&
	    buf[4] == 0x31 && buf[5] == 0x41 && buf[6] == 0x59 &&
	    buf[7] == 0x26 && buf[8] == 0x53 && buf[9] == 0x59) {
		if (buf[2] == 'h')
			return (EAV_COMP_BZIP2);
		else
			/* Could be bzip 1, but that is unsupported */
			return (EAV_COMP_UNKNOWN);
	} else if (len > 2 && buf[0] == 0x1f && buf[1] == 0x8b) {
		/* gzip per RFC1952 */
		return (EAV_COMP_GZIP);
	} else if (len > 6 && buf[0] == 0xfd && buf[1] == '7' &&
	    buf[2] == 'z' && buf[3] == 'X' &&
	    buf[4] == 'Z' && buf[5] == 0x00) {
		/* XZ per Wikipedia */
		return (EAV_COMP_XZ);
	} else
		return (EAV_COMP_UNKNOWN);
}

const char *
eav_strerror(enum eav_error error)
{

	switch (error) {
	case EAV_SUCCESS:
		return "Success";
	case EAV_ERR_MEM:
		return "malloc error";
	case EAV_ERR_DIGEST:
		return "checksum mismatch";
	case EAV_ERR_DIGEST_UNKNOWN:
		return "unknown digest";
	case EAV_ERR_DIGEST_UNSUPPORTED:
		return "unsupported digest";
	case EAV_ERR_COMP:
		return "decompression error";
	case EAV_ERR_COMP_UNKNOWN:
		return "Unknown compression type";
	case EAV_ERR_COMP_UNSUPPORTED:
		return "Unsupported compression type";
	default:
		return "Unknown error";
	}
}

enum eav_error
extract_and_verify(unsigned char *ibuf, size_t ilen,
    unsigned char **obufp, size_t *olenp, size_t blocksize,
    enum eav_compression ctype,
    enum eav_digest dtype, const unsigned char *digest)
{
	int ret;
	unsigned char *obuf = NULL;
	size_t olen = 0, total_in, total_out;
	bz_stream bzs;
#ifdef MD5_SUPPORT
	size_t prev_total_in;
	MD5_CTX md5ctx;
	char i_md5sum[33];
#endif

	switch (ctype) {
	case EAV_COMP_NONE:
	case EAV_COMP_BZIP2:
		break;
	case EAV_COMP_GZIP:
	case EAV_COMP_XZ:
		return (EAV_ERR_COMP_UNSUPPORTED);
	default:
		return (EAV_ERR_COMP_UNKNOWN);
	}

	switch (dtype) {
	case EAV_DIGEST_NONE:
		break;
	case EAV_DIGEST_MD5:
#ifdef MD5_SUPPORT
		break;
#else
		return (EAV_ERR_DIGEST_UNSUPPORTED);
#endif

	default:
		return (EAV_ERR_DIGEST_UNKNOWN);
	}

	if (dtype || ctype) {
#ifdef MD5_SUPPORT
		if (dtype == EAV_DIGEST_MD5)
			MD5Init(&md5ctx);
#endif

		if (ctype) {
			/* XXX: assume bzip2 for now */
			olen = 1024 * 1024;
			if ((obuf = malloc(olen)) == NULL)
				return (EAV_ERR_MEM);

			total_in = 0;
#ifdef MD5_SUPPORT
			prev_total_in = 0;
#endif

			bzs.bzalloc = NULL;
			bzs.bzfree = NULL;
			bzs.opaque = NULL;
			bzs.next_in = (char *)ibuf;
			bzs.avail_in = MIN(ilen, 1024 * 1024);
			bzs.next_out = (char *)obuf;
			bzs.avail_out = olen;
			if (BZ2_bzDecompressInit(&bzs, 0, 0) != BZ_OK)
				return (EAV_ERR_COMP);

			while ((ret = BZ2_bzDecompress(&bzs)) !=
			    BZ_STREAM_END) {
				if (ret != BZ_OK) {
					free(obuf);
					BZ2_bzDecompressEnd(&bzs);
					return (EAV_ERR_COMP);
				}

				total_in = ((size_t)bzs.total_in_hi32 << 32) +
				    bzs.total_in_lo32;
				total_out = ((size_t)bzs.total_out_hi32 << 32) +
				    bzs.total_out_lo32;

#ifdef MD5_SUPPORT
				if (dtype == EAV_DIGEST_MD5)
					MD5Update(&md5ctx, ibuf + prev_total_in,
					    total_in - prev_total_in);
				prev_total_in = total_in;
#endif

				if (bzs.avail_in == 0)
					bzs.avail_in =
					    MIN(ilen - total_in, 1024 * 1024);

				if (bzs.avail_out == 0) {
					olen *= 2;
					if ((obuf = reallocf(obuf, olen))
					    == NULL) {
						BZ2_bzDecompressEnd(&bzs);
						return (EAV_ERR_COMP);
					}
					bzs.next_out = (char *)obuf + total_out;
					bzs.avail_out = olen - total_out;
				}
			}
			BZ2_bzDecompressEnd(&bzs);
			total_in = ((size_t)bzs.total_in_hi32 << 32) +
			    bzs.total_in_lo32;
			total_out = ((size_t)bzs.total_out_hi32 << 32) +
			    bzs.total_out_lo32;

#ifdef MD5_SUPPORT
			/* Push the last read block in the MD5 machine */
			if (dtype == EAV_DIGEST_MD5)
				MD5Update(&md5ctx, ibuf + prev_total_in,
				    total_in - prev_total_in);
#endif

			/* Round up to blocksize and zero pad */
			olen = roundup2(total_out, blocksize);
			if (olen != total_out)
				memset(obuf + total_out, '\0',
				    olen - total_out);
			/* XXX: realloc to shorten allocation? */
		} else if (dtype) {
#ifdef MD5_SUPPORT
			if (dtype == EAV_DIGEST_MD5)
				MD5Update(&md5ctx, ibuf, ilen);
#endif
		}

		if (dtype) {
#ifdef MD5_SUPPORT
			if (dtype == EAV_DIGEST_MD5) {
				MD5End(&md5ctx, i_md5sum);
				if (strcmp(digest, i_md5sum) != 0)
					return (EAV_ERR_DIGEST);
			}
#endif
		}
	}

	if (ctype == EAV_COMP_NONE) {
		*obufp = ibuf;
		*olenp = ilen;
	} else {
		*obufp = obuf;
		*olenp = olen;
	}
	return (EAV_SUCCESS);
}
