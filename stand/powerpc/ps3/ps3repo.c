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

#include <stand.h>

#include "lv1call.h"
#include "ps3.h"
#include "ps3repo.h"

static uint64_t make_n1(const char *text, unsigned int index)
{
	uint64_t n1;

	n1 = 0;
	strncpy((char *) &n1, text, sizeof(n1));
	n1 = (n1 >> 32) + index;

	return n1;
}

static uint64_t make_n(const char *text, unsigned int index)
{
	uint64_t n;

	n = 0;
	strncpy((char *) &n, text, sizeof(n));
	n = n + index;

	return n;
}

int ps3repo_read_bus_type(unsigned int bus_index, uint64_t *bus_type)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("type", 0), 0, 0, &v1, &v2);

	*bus_type = v1;

	return err;
}

int ps3repo_read_bus_id(unsigned int bus_index, uint64_t *bus_id)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("id", 0), 0, 0, &v1, &v2);

	*bus_id = v1;

	return err;
}

int ps3repo_read_bus_num_dev(unsigned int bus_index, uint64_t *num_dev)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("num_dev", 0), 0, 0, &v1, &v2);

	*num_dev = v1;

	return err;
}

int ps3repo_read_bus_dev_type(unsigned int bus_index, unsigned int dev_index, uint64_t *dev_type)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("type", 0), 0, &v1, &v2);

	*dev_type = v1;

	return err;
}

int ps3repo_read_bus_dev_id(unsigned int bus_index, unsigned int dev_index, uint64_t *dev_id)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("id", 0), 0, &v1, &v2);

	*dev_id = v1;

	return err;
}

int ps3repo_read_bus_dev_blk_size(unsigned int bus_index, unsigned int dev_index, uint64_t *blk_size)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("blk_size", 0), 0, &v1, &v2);

	*blk_size = v1;

	return err;
}

int ps3repo_read_bus_dev_nblocks(unsigned int bus_index, unsigned int dev_index, uint64_t *nblocks)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("n_blocks", 0), 0, &v1, &v2);

	*nblocks = v1;

	return err;
}

int ps3repo_read_bus_dev_nregs(unsigned int bus_index, unsigned int dev_index, uint64_t *nregs)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("n_regs", 0), 0, &v1, &v2);

	*nregs = v1;

	return err;
}

int ps3repo_read_bus_dev_reg_id(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_id)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("region", reg_index), make_n("id", 0), &v1, &v2);

	*reg_id = v1;

	return err;
}

int ps3repo_read_bus_dev_reg_start(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_start)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("region", reg_index), make_n("start", 0), &v1, &v2);

	*reg_start = v1;

	return err;
}

int ps3repo_read_bus_dev_reg_size(unsigned int bus_index, unsigned int dev_index,
	unsigned int reg_index, uint64_t *reg_size)
{
	uint64_t v1, v2;
	int err;

	err = lv1_get_repository_node_value(PS3_LPAR_ID_PME, make_n1("bus", bus_index),
		make_n("dev", dev_index), make_n("region", reg_index), make_n("size", 0), &v1, &v2);

	*reg_size = v1;

	return err;
}

int ps3repo_find_bus_by_type(uint64_t bus_type, unsigned int *bus_index)
{
	unsigned int i;
	uint64_t type;
	int err;

	for (i = 0; i < 10; i++) {
		err = ps3repo_read_bus_type(i, &type);
		if (err) {
			*bus_index = (unsigned int) -1;
			return err;
		}

		if (type == bus_type) {
			*bus_index = i;
			return 0;
		}
	}

	*bus_index = (unsigned int) -1;

	return ENODEV;
}

int ps3repo_find_bus_dev_by_type(unsigned int bus_index, uint64_t dev_type,
	unsigned int *dev_index)
{
	unsigned int i;
	uint64_t type;
	int err;

	for (i = 0; i < 10; i++) {
		err = ps3repo_read_bus_dev_type(bus_index, i, &type);
		if (err) {
			*dev_index = (unsigned int) -1;
			return err;
		}

		if (type == dev_type) {
			*dev_index = i;
			return 0;
		}
	}

	*dev_index = (unsigned int) -1;

	return ENODEV;
}
