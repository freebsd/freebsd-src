/*
 * Bitfield
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef BITFIELD_H
#define BITFIELD_H

struct bitfield;

struct bitfield * bitfield_alloc(size_t max_bits);
struct bitfield * bitfield_alloc_data(const u8 *data, size_t len);
struct bitfield * bitfield_dup(const struct bitfield *orig);
void bitfield_free(struct bitfield *bf);
void bitfield_set(struct bitfield *bf, size_t bit);
void bitfield_clear(struct bitfield *bf, size_t bit);
int bitfield_is_set(const struct bitfield *bf, size_t bit);
int bitfield_get_first_zero(struct bitfield *bf);
int bitfield_union_in_place(struct bitfield *a, const struct bitfield *b);
struct bitfield * bitfield_union(const struct bitfield *a,
				 const struct bitfield *b);
int bitfield_intersect_in_place(struct bitfield *a, const struct bitfield *b);
int bitfield_is_subset(const struct bitfield *a, const struct bitfield *b);
size_t bitfield_size(const struct bitfield *bf);
int bitfield_intersects(const struct bitfield *a, const struct bitfield *b);
void bitfield_dump(struct bitfield *bf, const char *title);

#endif /* BITFIELD_H */
