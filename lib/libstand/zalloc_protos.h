/*
 * This module derived from code donated to the FreeBSD Project by 
 * Matthew Dillon <dillon@backplane.com>
 *
 * Copyright (c) 1998 The FreeBSD Project
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
 *	$Id$
 */

Prototype struct MemPool *DummyStructMemPool;
Library void *znalloc(struct MemPool *mpool, iaddr_t bytes);
Library void *zalloc(struct MemPool *mpool, iaddr_t bytes);
Library void *zallocAlign(struct MemPool *mpool, iaddr_t bytes, iaddr_t align);
Library void *zxalloc(struct MemPool *mp, void *addr1, void *addr2, iaddr_t bytes);
Library void *znxalloc(struct MemPool *mp, void *addr1, void *addr2, iaddr_t bytes);
Library char *zallocStr(struct MemPool *mpool, const char *s, int slen);
Library void zfree(struct MemPool *mpool, void *ptr, iaddr_t bytes);
Library void zfreeStr(struct MemPool *mpool, char *s);
Library void zinitPool(struct MemPool *mp, const char *id, void (*fpanic)(const char *ctl, ...), int (*freclaim)(struct MemPool *memPool, iaddr_t bytes), void *pBase, iaddr_t pSize);
Library void zextendPool(MemPool *mp, void *base, iaddr_t bytes);
Library void zclearPool(struct MemPool *mp);
Library void znop(const char *ctl, ...);
Library int znot(struct MemPool *memPool, iaddr_t bytes);
Library void zallocstats(struct MemPool *mp);
