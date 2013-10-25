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

#ifndef __EAV_H__
#define __EAV_H__

enum eav_error {
	EAV_SUCCESS = 0,
	EAV_ERR_MEM,
	EAV_ERR_DIGEST,
	EAV_ERR_DIGEST_UNKNOWN,
	EAV_ERR_DIGEST_UNSUPPORTED,
	EAV_ERR_COMP,
	EAV_ERR_COMP_UNKNOWN,
	EAV_ERR_COMP_UNSUPPORTED
};

enum eav_digest {
	EAV_DIGEST_NONE = 0,
	EAV_DIGEST_MD5
};

enum eav_compression {
	EAV_COMP_NONE = 0,
	EAV_COMP_BZIP2,
	EAV_COMP_GZIP,
	EAV_COMP_XZ,

	EAV_COMP_UNKNOWN
};

enum eav_compression eav_taste(const unsigned char *buf, off_t len);
const char *eav_strerror(enum eav_error error);
enum eav_error extract_and_verify(unsigned char *ibuf, size_t ilen,
    unsigned char **obufp, size_t *olenp, size_t blocksize,
    enum eav_compression ctype,
    enum eav_digest dtype, const unsigned char *digest);

#endif /* __EAV_H__ */
