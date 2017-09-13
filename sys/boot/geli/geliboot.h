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

#include <crypto/intake.h>

#ifndef _GELIBOOT_H_
#define _GELIBOOT_H_

#ifndef DEV_BSIZE
#define DEV_BSIZE 			512
#endif
#ifndef DEV_GELIBOOT_BSIZE
#define DEV_GELIBOOT_BSIZE		4096
#endif

#ifndef MIN
#define    MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define	GELI_MAX_KEYS			64
#define GELI_PW_MAXLEN			256

extern void pwgets(char *buf, int n, int hide);

struct dsk;

void geli_init(void);
int geli_taste(int read_func(void *vdev, void *priv, off_t off,
    void *buf, size_t bytes), struct dsk *dsk, daddr_t lastsector);
int is_geli(struct dsk *dsk);
int geli_read(struct dsk *dsk, off_t offset, u_char *buf, size_t bytes);
int geli_decrypt(u_int algo, u_char *data, size_t datasize,
    const u_char *key, size_t keysize, const uint8_t* iv);
int geli_havekey(struct dsk *dskp);
int geli_passphrase(char *pw, int disk, int parttype, int part, struct dsk *dskp);

int geliboot_crypt(u_int algo, int enc, u_char *data, size_t datasize,
    const u_char *key, size_t keysize, u_char *iv);

void geli_fill_keybuf(struct keybuf *keybuf);
void geli_save_keybuf(struct keybuf *keybuf);

#endif /* _GELIBOOT_H_ */
