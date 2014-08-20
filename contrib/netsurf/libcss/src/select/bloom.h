/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
 */

/** \file
 * Bloom filter for CSS style selection optimisation.
 *
 * Attempting to match CSS rules by querying the client about DOM nodes via
 * the selection callbacks is slow.  To avoid this, clients may pass a node
 * bloom filter to css_get_style.  This bloom filter has bits set according
 * to the node's ancestor element names, class names and id names.
 *
 * Generate the bloom filter by adding calling css_bloom_add_hash() on each
 * ancestor element name, class name and id name for the node.
 *
 * Use the insesnsitive lwc_string:
 *
 *     lwc_string_hash_value(str->insensitive)
 */

#ifndef libcss_bloom_h_
#define libcss_bloom_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* Size of bloom filter as multiple of 32 bits.
 * Has to be 4, 8, or 16.
 * Larger increases optimisation of style selection engine but uses more memory.
 */
#define CSS_BLOOM_SIZE 4



/* Check valid bloom filter size */
#if !(CSS_BLOOM_SIZE == 4 || CSS_BLOOM_SIZE == 8 || CSS_BLOOM_SIZE == 16)
# error Unsupported bloom filter size.  Size must be {4|8|16}.
#endif

/* Setup index bit mask */
#define INDEX_BITS_N (CSS_BLOOM_SIZE - 1)



/* type for bloom */
typedef uint32_t css_bloom;


/**
 * Add a hash value to the bloom filter.
 *
 * \param bloom	bloom filter to insert into
 * \param hash	libwapcaplet hash value to insert
 */
static inline void css_bloom_add_hash(css_bloom bloom[CSS_BLOOM_SIZE],
		lwc_hash hash)
{
	unsigned int bit = hash & 0x1f; /* Top 5 bits */
	unsigned int index = (hash >> 5) & INDEX_BITS_N; /* Next N bits */

	bloom[index] |= (1 << bit);
}


/**
 * Test whether bloom filter contains given hash value.
 *
 * \param bloom	bloom filter to check inside
 * \param hash	libwapcaplet hash value to look for
 * \return true hash value is already set in bloom
 */
static inline bool css_bloom_has_hash(const css_bloom bloom[CSS_BLOOM_SIZE],
		lwc_hash hash)
{
	unsigned int bit = hash & 0x1f; /* Top 5 bits */
	unsigned int index = (hash >> 5) & INDEX_BITS_N; /* Next N bits */

	return (bloom[index] & (1 << bit));
}


/**
 * Test whether bloom 'a' is a subset of bloom 'b'.
 *
 * \param a	potential subset bloom to test
 * \param b	superset bloom
 * \return true iff 'a' is subset of 'b'
 */
static inline bool css_bloom_in_bloom(const css_bloom a[CSS_BLOOM_SIZE],
		const css_bloom b[CSS_BLOOM_SIZE])
{
	if ((a[0] & b[0]) != a[0])
		return false;
	if ((a[1] & b[1]) != a[1])
		return false;
	if ((a[2] & b[2]) != a[2])
		return false;
	if ((a[3] & b[3]) != a[3])
		return false;
#if (CSS_BLOOM_SIZE > 4)
	if ((a[4] & b[4]) != a[4])
		return false;
	if ((a[5] & b[5]) != a[5])
		return false;
	if ((a[6] & b[6]) != a[6])
		return false;
	if ((a[7] & b[7]) != a[7])
		return false;
#endif
#if (CSS_BLOOM_SIZE > 8)
	if ((a[8] & b[8]) != a[8])
		return false;
	if ((a[9] & b[9]) != a[9])
		return false;
	if ((a[10] & b[10]) != a[10])
		return false;
	if ((a[11] & b[11]) != a[11])
		return false;
	if ((a[12] & b[12]) != a[12])
		return false;
	if ((a[13] & b[13]) != a[13])
		return false;
	if ((a[14] & b[14]) != a[14])
		return false;
	if ((a[15] & b[15]) != a[15])
		return false;
#endif
	return true;
}


/**
 * Merge bloom 'a' into bloom 'b'.
 *
 * \param a	bloom to insert
 * \param b	target bloom
 */
static inline void css_bloom_merge(const css_bloom a[CSS_BLOOM_SIZE],
		css_bloom b[CSS_BLOOM_SIZE])
{
	b[0] |= a[0];
	b[1] |= a[1];
	b[2] |= a[2];
	b[3] |= a[3];
#if (CSS_BLOOM_SIZE > 4)
	b[4] |= a[4];
	b[5] |= a[5];
	b[6] |= a[6];
	b[7] |= a[7];
#endif
#if (CSS_BLOOM_SIZE > 8)
	b[8] |= a[8];
	b[9] |= a[9];
	b[10] |= a[10];
	b[11] |= a[11];
	b[12] |= a[12];
	b[13] |= a[13];
	b[14] |= a[14];
	b[15] |= a[15];
#endif
}


/**
 * Initialise a bloom filter to 0
 *
 * \param bloom	bloom filter to initialise
 */
static inline void css_bloom_init(css_bloom bloom[CSS_BLOOM_SIZE])
{
	bloom[0] = 0;
	bloom[1] = 0;
	bloom[2] = 0;
	bloom[3] = 0;
#if (CSS_BLOOM_SIZE > 4)
	bloom[4] = 0;
	bloom[5] = 0;
	bloom[6] = 0;
	bloom[7] = 0;
#endif
#if (CSS_BLOOM_SIZE > 8)
	bloom[8] = 0;
	bloom[9] = 0;
	bloom[10] = 0;
	bloom[11] = 0;
	bloom[12] = 0;
	bloom[13] = 0;
	bloom[14] = 0;
	bloom[15] = 0;
#endif
}

#ifdef __cplusplus
}
#endif

#endif

