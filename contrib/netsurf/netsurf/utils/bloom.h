/*
 * Copyright 2013 Rob Kendrick <rjek@netsurf-browser.org>
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
 * Trivial bloom filter */

#ifndef _NETSURF_UTILS_BLOOM_H_
#define _NETSURF_UTILS_BLOOM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bloom_filter;

/**
 * Create a new bloom filter.
 * 
 * \param size Size of bloom filter in bytes
 * \return Handle for newly-created bloom filter, or NULL
 */
struct bloom_filter *bloom_create(size_t size);

/**
 * Destroy a previously-created bloom filter
 * 
 * \param b Bloom filter to destroy
 */
void bloom_destroy(struct bloom_filter *b);

/**
 * Insert a string of given length (may include NULs) into the filter,
 * using an internal hash function.
 * 
 * \param b Bloom filter to add to
 * \param s Pointer to data
 * \param z Length of data
 */
void bloom_insert_str(struct bloom_filter *b, const char *s, size_t z);

/**
 * Insert a given hash value into the filter, should you already have
 * one to hand.
 * 
 * \param b Bloom filter to add to
 * \param hash Value to add
 */
void bloom_insert_hash(struct bloom_filter *b, uint32_t hash);

/**
 * Search the filter for the given string, assuming it was added by
 * bloom_insert_str().   May return false-positives.
 * 
 * \param b Bloom filter to search
 * \param s Pointer to data to search for
 * \param z Length of data
 * 
 * \return False if never added, True if it might have been.
 */
bool bloom_search_str(struct bloom_filter *b, const char *s, size_t z);

/**
 * Search the filter for the given hash value, assuming it was added by
 * bloom_insert_hash().  May return false-positives.
 * 
 * \param b Bloom filter to search
 * \param hash Hash value to search for
 * 
 * \return False if never added, True if it might have been.
 */
bool bloom_search_hash(struct bloom_filter *b, uint32_t hash);

/**
 * Find out how many items have been added to this bloom filter.  This
 * is useful for deciding the size of a new bloom filter should you
 * need to rehash it.
 * 
 * \param b Bloom filter to examine
 * 
 * \return Number of items that have been added
 */
uint32_t bloom_items(struct bloom_filter *b);

#endif
