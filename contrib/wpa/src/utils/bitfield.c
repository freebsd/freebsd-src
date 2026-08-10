/*
 * Bitfield
 * Copyright (c) 2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "bitfield.h"


struct bitfield {
	u8 *bits;
	size_t max_bits;
};


struct bitfield * bitfield_alloc(size_t max_bits)
{
	struct bitfield *bf;

	bf = os_zalloc(sizeof(*bf) + (max_bits + 7) / 8);
	if (bf == NULL)
		return NULL;
	bf->bits = (u8 *) (bf + 1);
	bf->max_bits = max_bits;
	return bf;
}


struct bitfield * bitfield_alloc_data(const u8 *data, size_t len)
{
	struct bitfield *bf;

	bf = os_zalloc(sizeof(*bf) + len);
	if (!bf)
		return NULL;

	bf->bits = (u8 *) (bf + 1);
	os_memcpy(bf->bits, data, len);
	bf->max_bits = len * 8;

	return bf;
}


struct bitfield * bitfield_dup(const struct bitfield *orig)
{
	struct bitfield *bf;

	if (!orig)
		return NULL;

	bf = os_memdup(orig, sizeof(*orig) + (orig->max_bits + 7) / 8);
	if (!bf)
		return NULL;

	bf->bits = (u8 *) (bf + 1);

	return bf;
}


void bitfield_free(struct bitfield *bf)
{
	os_free(bf);
}


void bitfield_set(struct bitfield *bf, size_t bit)
{
	if (bit >= bf->max_bits)
		return;
	bf->bits[bit / 8] |= BIT(bit % 8);
}


void bitfield_clear(struct bitfield *bf, size_t bit)
{
	if (bit >= bf->max_bits)
		return;
	bf->bits[bit / 8] &= ~BIT(bit % 8);
}


int bitfield_is_set(const struct bitfield *bf, size_t bit)
{
	if (bit >= bf->max_bits)
		return 0;
	return !!(bf->bits[bit / 8] & BIT(bit % 8));
}


static int first_zero(u8 val)
{
	int i;
	for (i = 0; i < 8; i++) {
		if (!(val & 0x01))
			return i;
		val >>= 1;
	}
	return -1;
}


int bitfield_get_first_zero(struct bitfield *bf)
{
	size_t i;
	for (i = 0; i < (bf->max_bits + 7) / 8; i++) {
		if (bf->bits[i] != 0xff)
			break;
	}
	if (i == (bf->max_bits + 7) / 8)
		return -1;
	i = i * 8 + first_zero(bf->bits[i]);
	if (i >= bf->max_bits)
		return -1;
	return i;
}


int bitfield_union_in_place(struct bitfield *a, const struct bitfield *b)
{
	size_t i, upper;

	if (!a || !b || a->max_bits < b->max_bits)
		return -1;

	upper = (b->max_bits + 7) / 8;

	for (i = 0 ; i < upper; i++)
		a->bits[i] |= b->bits[i];

	return 0;
}


struct bitfield * bitfield_union(const struct bitfield *a,
				 const struct bitfield *b)
{
	struct bitfield *res;
	int ret;

	if (!a || !b)
		return NULL;

	if (a->max_bits > b->max_bits) {
		res = bitfield_dup(a);
		ret = bitfield_union_in_place(res, b);
	}  else {
		res = bitfield_dup(b);
		ret = bitfield_union_in_place(res, a);
	}

	if (!ret)
		return res;

	os_free(res);
	return NULL;
}


int bitfield_intersect_in_place(struct bitfield *a,
				const struct bitfield *b)
{
	size_t i, upper;

	if (!a || !b)
		return -1;

	if (a->max_bits < b->max_bits)
		upper = (a->max_bits + 7) / 8;
	else
		upper = (b->max_bits + 7) / 8;


	for (i = 0 ; i < upper; i++)
		a->bits[i] &= b->bits[i];

	upper = (a->max_bits + 7) / 8;
	for (; i < upper; i++)
		a->bits[i] = 0;

	return 0;
}


int bitfield_is_subset(const struct bitfield *a, const struct bitfield *b)
{
	size_t i, upper;

	if (!a || !b)
		return -1;

	if (a->max_bits < b->max_bits)
		return 0;

	upper = (b->max_bits + 7) / 8;

	for (i = 0; i < upper; i++) {
		u8 res = a->bits[i] & b->bits[i];

		if (res != b->bits[i])
			return 0;
	}

	return 1;
}


size_t bitfield_size(const struct bitfield *bf)
{
	if (!bf)
		return 0;
	return bf->max_bits;
}


int bitfield_intersects(const struct bitfield *a, const struct bitfield *b)
{
	size_t i, upper;

	if (!a || !b)
		return -1;

	if (a->max_bits < b->max_bits)
		upper = (a->max_bits + 7) / 8;
	else
		upper = (b->max_bits + 7) / 8;

	for (i = 0; i < upper; i++)
		if (a->bits[i] & b->bits[i])
			return 1;

	return 0;
}


void bitfield_dump(struct bitfield *bf, const char *title)
{
	wpa_printf(MSG_DEBUG, "bitfield: %s: max_bits=%zu",
		   title, bf->max_bits);
	wpa_hexdump(MSG_DEBUG, "bits: ", bf->bits, bf->max_bits / 8);
}
