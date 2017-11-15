/*-
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _PS3_STOR_H
#define _PS3_STOR_H

#define PS3_STOR_DEV_MAXREGS	8

struct ps3_storreg {
	uint64_t sr_id;
	uint64_t sr_start;
	uint64_t sr_size;
};

struct ps3_stordev {
	int			sd_type;
	unsigned int		sd_busidx;
	unsigned int		sd_devidx;
	uint64_t		sd_busid;
	uint64_t		sd_devid;
	uint64_t		sd_blksize;
	uint64_t		sd_nblocks;
	uint64_t		sd_nregs;
	struct ps3_storreg	sd_regs[PS3_STOR_DEV_MAXREGS];
	uint64_t		sd_dmabase;
};

int ps3stor_setup(struct ps3_stordev *sd, int type);

int ps3stor_read_sectors(struct ps3_stordev *sd, int regidx,
	uint64_t start_sector, uint64_t sector_count, uint64_t flags, char *buf);

void ps3stor_print(struct ps3_stordev *sd);

#endif
