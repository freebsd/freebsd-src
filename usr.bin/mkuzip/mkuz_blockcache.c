/*
 * Copyright (c) 2016 Maxim Sobolev <sobomax@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <md5.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(MKUZ_DEBUG)
# include <stdio.h>
#endif

#include "mkuz_blockcache.h"

struct mkuz_blkcache {
    struct mkuz_blkcache_hit hit;
    off_t data_offset;
    unsigned char digest[16];
    struct mkuz_blkcache *next;
};

static struct mkuz_blkcache blkcache;

struct mkuz_blkcache_hit *
mkuz_blkcache_regblock(int fd, uint32_t blkno, off_t offset, ssize_t len,
  void *data)
{
    struct mkuz_blkcache *bcep;
    MD5_CTX mcontext;
    off_t data_offset;
    unsigned char mdigest[16];

    data_offset = lseek(fd, 0, SEEK_CUR);
    if (data_offset < 0) {
        return (NULL);
    }
    MD5Init(&mcontext);
    MD5Update(&mcontext, data, len);
    MD5Final(mdigest, &mcontext);
    if (blkcache.hit.len == 0) {
        bcep = &blkcache;
    } else {
        for (bcep = &blkcache; bcep != NULL; bcep = bcep->next) {
            if (bcep->hit.len != len)
                continue;
            if (memcmp(mdigest, bcep->digest, sizeof(mdigest)) == 0) {
                break;
            }
        }
        if (bcep != NULL) {
#if defined(MKUZ_DEBUG)
            printf("cache hit %d, %d, %d\n", (int)bcep->hit.offset, (int)data_offset, (int)len);
#endif
            return (&bcep->hit);
        }
        bcep = malloc(sizeof(struct mkuz_blkcache));
        if (bcep == NULL)
            return (NULL);
        memset(bcep, '\0', sizeof(struct mkuz_blkcache));
        bcep->next = blkcache.next;
        blkcache.next = bcep;
    }
    memcpy(bcep->digest, mdigest, sizeof(mdigest));
    bcep->data_offset = data_offset;
    bcep->hit.offset = offset;
    bcep->hit.len = len;
    bcep->hit.blkno = blkno;
    return (NULL);
}
