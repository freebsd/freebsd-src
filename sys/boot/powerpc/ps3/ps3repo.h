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

#ifndef _PS3_REPO_H
#define _PS3_REPO_H

#define PS3_LPAR_ID_PME	1

int ps3repo_read_bus_type(unsigned int bus_index, uint64_t *bus_type);
int ps3repo_read_bus_id(unsigned int bus_index, uint64_t *bus_id);
int ps3repo_read_bus_num_dev(unsigned int bus_index, uint64_t *num_dev);
int ps3repo_read_bus_dev_type(unsigned int bus_index, unsigned int dev_index, uint64_t *dev_type);
int ps3repo_read_bus_dev_id(unsigned int bus_index, unsigned int dev_index, uint64_t *dev_id);
int ps3repo_read_bus_dev_blk_size(unsigned int bus_index, unsigned int dev_index, uint64_t *blk_size);
int ps3repo_read_bus_dev_nblocks(unsigned int bus_index, unsigned int dev_index, uint64_t *nblocks);
int ps3repo_read_bus_dev_nregs(unsigned int bus_index, unsigned int dev_index, uint64_t *nregs);
int ps3repo_read_bus_dev_reg_id(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_id);
int ps3repo_read_bus_dev_reg_start(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_start);
int ps3repo_read_bus_dev_reg_size(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_size);
int ps3repo_find_bus_by_type(uint64_t bus_type, unsigned int *bus_index);
int ps3repo_find_bus_dev_by_type(unsigned int bus_index, uint64_t dev_type,
	unsigned int *dev_index);

#endif
