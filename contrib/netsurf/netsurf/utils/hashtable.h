/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Write-Once hash table for string to string mappings */

#ifndef _NETSURF_UTILS_HASHTABLE_H_
#define _NETSURF_UTILS_HASHTABLE_H_

#include <stdbool.h>

struct hash_table;

struct hash_table *hash_create(unsigned int chains);
void hash_destroy(struct hash_table *ht);
bool hash_add(struct hash_table *ht, const char *key, const char *value);
const char *hash_get(struct hash_table *ht, const char *key);
const char *hash_iterate(struct hash_table *ht, unsigned int *c1,
		unsigned int **c2);

#endif
