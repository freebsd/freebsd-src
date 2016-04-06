/*-
 * Copyright (c) 2015 Allan Jude <allanjude@FreeBSD.org>
 * Copyright (c) 2005-2011 Pawel Jakub Dawidek <pawel@dawidek.net>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/endian.h>
#include <sys/queue.h>

#ifndef _GELIBOOT_H_
#define _GELIBOOT_H_

#define _STRING_H_
#define _STRINGS_H_
#define _STDIO_H_
#include <geom/eli/g_eli.h>
#include <geom/eli/pkcs5v2.h>

/* Pull in the md5, sha256, and sha512 implementations */
#include <md5.h>
#include <crypto/sha2/sha256.h>
#include <crypto/sha2/sha512.h>

/* Pull in AES implementation */
#include <crypto/rijndael/rijndael-api-fst.h>

/* AES-XTS implementation */
#define _STAND
#define STAND_H /* We don't want stand.h in {gpt,zfs,gptzfs}boot */
#include <opencrypto/xform_enc.h>

#ifndef DEV_BSIZE
#define DEV_BSIZE 			512
#endif
#ifndef DEV_GELIBOOT_BSIZE
#define DEV_GELIBOOT_BSIZE		4096
#endif

#ifndef MIN
#define    MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define GELI_PW_MAXLEN			256
extern void pwgets(char *buf, int n);

struct geli_entry {
	struct dsk		*dsk;
	off_t			part_end;
	struct g_eli_softc	sc;
	struct g_eli_metadata	md;
	SLIST_ENTRY(geli_entry)	entries;
} *geli_e, *geli_e_tmp;

int geli_count;

void geli_init(void);
int geli_taste(int read_func(void *vdev, void *priv, off_t off,
    void *buf, size_t bytes), struct dsk *dsk, daddr_t lastsector);
int geli_attach(struct dsk *dskp, const char *passphrase);
int is_geli(struct dsk *dsk);
int geli_read(struct dsk *dsk, off_t offset, u_char *buf, size_t bytes);
int geli_decrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize, const uint8_t* iv);
int geli_passphrase(char *pw, int disk, int parttype, int part, struct dsk *dskp);

#endif /* _GELIBOOT_H_ */
