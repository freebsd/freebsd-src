/*-
 * Copyright (c) 2024, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

struct memory_segments
{
	uint64_t	start;
	uint64_t	end;
	uint64_t	type;	/* MD defined */
};

#define SYSTEM_RAM 1
void init_avail(void);
void need_avail(int n);
void add_avail(uint64_t start, uint64_t end, uint64_t type);
void remove_avail(uint64_t start, uint64_t end, uint64_t type);
uint64_t first_avail(uint64_t align, uint64_t min_size, uint64_t type);
void print_avail(void);
bool populate_avail_from_iomem(void);
uint64_t space_avail(uint64_t start);
