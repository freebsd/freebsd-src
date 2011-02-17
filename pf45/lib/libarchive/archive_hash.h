/*-
 * Copyright (c) 2009 Joerg Sonnenberger
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
 *
 * $FreeBSD$
 */

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif

/*
 * Hash function support in various Operating Systems:
 *
 * NetBSD:
 * - MD5 and SHA1 in libc: without _ after algorithm name
 * - SHA2 in libc: with _ after algorithm name
 *
 * OpenBSD:
 * - MD5, SHA1 and SHA2 in libc: without _ after algorithm name
 * - OpenBSD 4.4 and earlier have SHA2 in libc with _ after algorithm name
 *
 * DragonFly and FreeBSD (XXX not used yet):
 * - MD5 and SHA1 in libmd: without _ after algorithm name
 * - SHA256: with _ after algorithm name
 *
 * OpenSSL:
 * - MD5, SHA1 and SHA2 in libcrypto: with _ after algorithm name
 */

#if defined(HAVE_MD5_H) && defined(HAVE_MD5INIT)
#  include <md5.h>
#  define ARCHIVE_HAS_MD5
typedef MD5_CTX archive_md5_ctx;
#  define archive_md5_init(ctx)			MD5Init(ctx)
#  define archive_md5_final(ctx, buf)		MD5Final(buf, ctx)
#  define archive_md5_update(ctx, buf, n)	MD5Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_MD5_H)
#  include <openssl/md5.h>
#  define ARCHIVE_HAS_MD5
typedef MD5_CTX archive_md5_ctx;
#  define archive_md5_init(ctx)			MD5_Init(ctx)
#  define archive_md5_final(ctx, buf)		MD5_Final(buf, ctx)
#  define archive_md5_update(ctx, buf, n)	MD5_Update(ctx, buf, n)
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(CALG_MD5)
#  define ARCHIVE_HAS_MD5
typedef MD5_CTX archive_md5_ctx;
#  define archive_md5_init(ctx)			MD5_Init(ctx)
#  define archive_md5_final(ctx, buf)		MD5_Final(buf, ctx)
#  define archive_md5_update(ctx, buf, n)	MD5_Update(ctx, buf, n)
#endif

#if defined(HAVE_RMD160_H) && defined(HAVE_RMD160INIT)
#  include <rmd160.h>
#  define ARCHIVE_HAS_RMD160
typedef RMD160_CTX archive_rmd160_ctx;
#  define archive_rmd160_init(ctx)		RMD160Init(ctx)
#  define archive_rmd160_final(ctx, buf)	RMD160Final(buf, ctx)
#  define archive_rmd160_update(ctx, buf, n)	RMD160Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_RIPEMD_H)
#  include <openssl/ripemd.h>
#  define ARCHIVE_HAS_RMD160
typedef RIPEMD160_CTX archive_rmd160_ctx;
#  define archive_rmd160_init(ctx)		RIPEMD160_Init(ctx)
#  define archive_rmd160_final(ctx, buf)	RIPEMD160_Final(buf, ctx)
#  define archive_rmd160_update(ctx, buf, n)	RIPEMD160_Update(ctx, buf, n)
#endif

#if defined(HAVE_SHA1_H) && defined(HAVE_SHA1INIT)
#  include <sha1.h>
#  define ARCHIVE_HAS_SHA1
typedef SHA1_CTX archive_sha1_ctx;
#  define archive_sha1_init(ctx)		SHA1Init(ctx)
#  define archive_sha1_final(ctx, buf)		SHA1Final(buf, ctx)
#  define archive_sha1_update(ctx, buf, n)	SHA1Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_SHA_H)
#  include <openssl/sha.h>
#  define ARCHIVE_HAS_SHA1
typedef SHA_CTX archive_sha1_ctx;
#  define archive_sha1_init(ctx)		SHA1_Init(ctx)
#  define archive_sha1_final(ctx, buf)		SHA1_Final(buf, ctx)
#  define archive_sha1_update(ctx, buf, n)	SHA1_Update(ctx, buf, n)
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(CALG_SHA1)
#  define ARCHIVE_HAS_SHA1
typedef SHA1_CTX archive_sha1_ctx;
#  define archive_sha1_init(ctx)		SHA1_Init(ctx)
#  define archive_sha1_final(ctx, buf)		SHA1_Final(buf, ctx)
#  define archive_sha1_update(ctx, buf, n)	SHA1_Update(ctx, buf, n)
#endif

#if defined(HAVE_SHA2_H) && defined(HAVE_SHA256_INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA256
typedef SHA256_CTX archive_sha256_ctx;
#  define archive_sha256_init(ctx)		SHA256_Init(ctx)
#  define archive_sha256_final(ctx, buf)	SHA256_Final(buf, ctx)
#  define archive_sha256_update(ctx, buf, n)	SHA256_Update(ctx, buf, n)
#elif defined(HAVE_SHA2_H) && defined(HAVE_SHA256INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA256
typedef SHA256_CTX archive_sha256_ctx;
#  define archive_sha256_init(ctx)		SHA256Init(ctx)
#  define archive_sha256_final(ctx, buf)	SHA256Final(buf, ctx)
#  define archive_sha256_update(ctx, buf, n)	SHA256Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_SHA_H) && defined(HAVE_OPENSSL_SHA256_INIT)
#  include <openssl/sha.h>
#  define ARCHIVE_HAS_SHA256
typedef SHA256_CTX archive_sha256_ctx;
#  define archive_sha256_init(ctx)		SHA256_Init(ctx)
#  define archive_sha256_final(ctx, buf)	SHA256_Final(buf, ctx)
#  define archive_sha256_update(ctx, buf, n)	SHA256_Update(ctx, buf, n)
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(CALG_SHA_256)
#  define ARCHIVE_HAS_SHA256
typedef SHA256_CTX archive_sha256_ctx;
#  define archive_sha256_init(ctx)		SHA256_Init(ctx)
#  define archive_sha256_final(ctx, buf)	SHA256_Final(buf, ctx)
#  define archive_sha256_update(ctx, buf, n)	SHA256_Update(ctx, buf, n)
#endif

#if defined(HAVE_SHA2_H) && defined(HAVE_SHA384_INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA384
typedef SHA384_CTX archive_sha384_ctx;
#  define archive_sha384_init(ctx)		SHA384_Init(ctx)
#  define archive_sha384_final(ctx, buf)	SHA384_Final(buf, ctx)
#  define archive_sha384_update(ctx, buf, n)	SHA384_Update(ctx, buf, n)
#elif defined(HAVE_SHA2_H) && defined(HAVE_SHA384INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA384
typedef SHA384_CTX archive_sha384_ctx;
#  define archive_sha384_init(ctx)		SHA384Init(ctx)
#  define archive_sha384_final(ctx, buf)	SHA384Final(buf, ctx)
#  define archive_sha384_update(ctx, buf, n)	SHA384Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_SHA_H) && defined(HAVE_OPENSSL_SHA384_INIT)
#  include <openssl/sha.h>
#  define ARCHIVE_HAS_SHA384
typedef SHA512_CTX archive_sha384_ctx;
#  define archive_sha384_init(ctx)		SHA384_Init(ctx)
#  define archive_sha384_final(ctx, buf)	SHA384_Final(buf, ctx)
#  define archive_sha384_update(ctx, buf, n)	SHA384_Update(ctx, buf, n)
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(CALG_SHA_384)
#  define ARCHIVE_HAS_SHA384
typedef SHA512_CTX archive_sha384_ctx;
#  define archive_sha384_init(ctx)		SHA384_Init(ctx)
#  define archive_sha384_final(ctx, buf)	SHA384_Final(buf, ctx)
#  define archive_sha384_update(ctx, buf, n)	SHA384_Update(ctx, buf, n)
#endif

#if defined(HAVE_SHA2_H) && defined(HAVE_SHA512_INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA512
typedef SHA512_CTX archive_sha512_ctx;
#  define archive_sha512_init(ctx)		SHA512_Init(ctx)
#  define archive_sha512_final(ctx, buf)	SHA512_Final(buf, ctx)
#  define archive_sha512_update(ctx, buf, n)	SHA512_Update(ctx, buf, n)
#elif defined(HAVE_SHA2_H) && defined(HAVE_SHA512INIT)
#  include <sha2.h>
#  define ARCHIVE_HAS_SHA512
typedef SHA512_CTX archive_sha512_ctx;
#  define archive_sha512_init(ctx)		SHA512Init(ctx)
#  define archive_sha512_final(ctx, buf)	SHA512Final(buf, ctx)
#  define archive_sha512_update(ctx, buf, n)	SHA512Update(ctx, buf, n)
#elif defined(HAVE_OPENSSL_SHA_H) && defined(HAVE_OPENSSL_SHA512_INIT)
#  include <openssl/sha.h>
#  define ARCHIVE_HAS_SHA512
typedef SHA512_CTX archive_sha512_ctx;
#  define archive_sha512_init(ctx)		SHA512_Init(ctx)
#  define archive_sha512_final(ctx, buf)	SHA512_Final(buf, ctx)
#  define archive_sha512_update(ctx, buf, n)	SHA512_Update(ctx, buf, n)
#elif defined(_WIN32) && !defined(__CYGWIN__) && defined(CALG_SHA_512)
#  define ARCHIVE_HAS_SHA512
typedef SHA512_CTX archive_sha512_ctx;
#  define archive_sha512_init(ctx)		SHA512_Init(ctx)
#  define archive_sha512_final(ctx, buf)	SHA512_Final(buf, ctx)
#  define archive_sha512_update(ctx, buf, n)	SHA512_Update(ctx, buf, n)
#endif
