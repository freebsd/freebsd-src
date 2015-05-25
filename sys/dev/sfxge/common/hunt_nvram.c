/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"

#if EFSYS_OPT_HUNTINGTON

#if EFSYS_OPT_VPD || EFSYS_OPT_NVRAM

#include "ef10_tlv_layout.h"

/* Cursor for TLV partition format */
typedef struct tlv_cursor_s {
	uint32_t	*block;			/* Base of data block */
	uint32_t	*current;		/* Cursor position */
	uint32_t	*end;			/* End tag position */
	uint32_t	*limit;			/* Last dword of data block */
} tlv_cursor_t;

static	__checkReturn		int
tlv_validate_state(
	__in			tlv_cursor_t *cursor);


/*
 * Operations on TLV formatted partition data.
 */
static				uint32_t
tlv_tag(
	__in	tlv_cursor_t	*cursor)
{
	uint32_t dword, tag;

	dword = cursor->current[0];
	tag = __LE_TO_CPU_32(dword);

	return (tag);
}

static				size_t
tlv_length(
	__in	tlv_cursor_t	*cursor)
{
	uint32_t dword, length;

	if (tlv_tag(cursor) == TLV_TAG_END)
		return (0);

	dword = cursor->current[1];
	length = __LE_TO_CPU_32(dword);

	return ((size_t)length);
}

static				uint8_t *
tlv_value(
	__in	tlv_cursor_t	*cursor)
{
	if (tlv_tag(cursor) == TLV_TAG_END)
		return (NULL);

	return ((uint8_t *)(&cursor->current[2]));
}

static				uint8_t *
tlv_item(
	__in	tlv_cursor_t	*cursor)
{
	if (tlv_tag(cursor) == TLV_TAG_END)
		return (NULL);

	return ((uint8_t *)cursor->current);
}

/*
 * TLV item DWORD length is tag + length + value (rounded up to DWORD)
 * equivalent to tlv_n_words_for_len in mc-comms tlv.c
 */
#define	TLV_DWORD_COUNT(length) \
	(1 + 1 + (((length) + sizeof (uint32_t) - 1) / sizeof (uint32_t)))


static				uint32_t *
tlv_next_item_ptr(
	__in	tlv_cursor_t	*cursor)
{
	uint32_t length;

	length = tlv_length(cursor);

	return (cursor->current + TLV_DWORD_COUNT(length));
}

static				int
tlv_advance(
	__in	tlv_cursor_t	*cursor)
{
	int rc;

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail1;

	if (cursor->current == cursor->end) {
		/* No more tags after END tag */
		cursor->current = NULL;
		rc = ENOENT;
		goto fail2;
	}

	/* Advance to next item and validate */
	cursor->current = tlv_next_item_ptr(cursor);

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static				int
tlv_rewind(
	__in	tlv_cursor_t	*cursor)
{
	int rc;

	cursor->current = cursor->block;

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static				int
tlv_find(
	__in	tlv_cursor_t	*cursor,
	__in	uint32_t	tag)
{
	int rc;

	rc = tlv_rewind(cursor);
	while (rc == 0) {
		if (tlv_tag(cursor) == tag)
			break;

		rc = tlv_advance(cursor);
	}
	return (rc);
}

static	__checkReturn		int
tlv_validate_state(
	__in	tlv_cursor_t	*cursor)
{
	int rc;

	/* Check cursor position */
	if (cursor->current < cursor->block) {
		rc = EINVAL;
		goto fail1;
	}
	if (cursor->current > cursor->limit) {
		rc = EINVAL;
		goto fail2;
	}

	if (tlv_tag(cursor) != TLV_TAG_END) {
		/* Check current item has space for tag and length */
		if (cursor->current > (cursor->limit - 2)) {
			cursor->current = NULL;
			rc = EFAULT;
			goto fail3;
		}

		/* Check we have value data for current item and another tag */
		if (tlv_next_item_ptr(cursor) > (cursor->limit - 1)) {
			cursor->current = NULL;
			rc = EFAULT;
			goto fail4;
		}
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static				int
tlv_init_cursor(
	__in	tlv_cursor_t	*cursor,
	__in	uint32_t	*block,
	__in	uint32_t	*limit)
{
	cursor->block	= block;
	cursor->limit	= limit;

	cursor->current	= cursor->block;
	cursor->end	= NULL;

	return (tlv_validate_state(cursor));
}

static				int
tlv_init_cursor_from_size(
	__in	tlv_cursor_t	*cursor,
	__in	uint8_t	*block,
	__in	size_t		size)
{
	uint32_t *limit;
	limit = (uint32_t *)(block + size - sizeof (uint32_t));
	return (tlv_init_cursor(cursor, (uint32_t *)block, limit));
}

static				int
tlv_require_end(
	__in	tlv_cursor_t	*cursor)
{
	uint32_t *pos;
	int rc;

	if (cursor->end == NULL) {
		pos = cursor->current;
		if ((rc = tlv_find(cursor, TLV_TAG_END)) != 0)
			goto fail1;

		cursor->end = cursor->current;
		cursor->current = pos;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static				size_t
tlv_block_length_used(
	__in	tlv_cursor_t	*cursor)
{
	int rc;

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail1;

	if ((rc = tlv_require_end(cursor)) != 0)
		goto fail2;

	/* Return space used (including the END tag) */
	return (cursor->end + 1 - cursor->block) * sizeof (uint32_t);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (0);
}


static	__checkReturn		uint32_t *
tlv_write(
	__in			tlv_cursor_t *cursor,
	__in			uint32_t tag,
	__in_bcount(size)	uint8_t *data,
	__in			size_t size)
{
	uint32_t len = size;
	uint32_t *ptr;

	ptr = cursor->current;

	*ptr++ = __CPU_TO_LE_32(tag);
	*ptr++ = __CPU_TO_LE_32(len);

	if (len > 0) {
		ptr[(len - 1) / sizeof (uint32_t)] = 0;
		memcpy(ptr, data, len);
		ptr += P2ROUNDUP(len, sizeof (uint32_t)) / sizeof (*ptr);
	}

	return (ptr);
}

static	__checkReturn		int
tlv_insert(
	__in	tlv_cursor_t	*cursor,
	__in	uint32_t	tag,
	__in	uint8_t		*data,
	__in	size_t		size)
{
	unsigned int delta;
	int rc;

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail1;

	if ((rc = tlv_require_end(cursor)) != 0)
		goto fail2;

	if (tag == TLV_TAG_END) {
		rc = EINVAL;
		goto fail3;
	}

	delta = TLV_DWORD_COUNT(size);
	if (cursor->end + 1 + delta > cursor->limit) {
		rc = ENOSPC;
		goto fail4;
	}

	/* Move data up: new space at cursor->current */
	memmove(cursor->current + delta, cursor->current,
	    (cursor->end + 1 - cursor->current) * sizeof (uint32_t));

	/* Adjust the end pointer */
	cursor->end += delta;

	/* Write new TLV item */
	tlv_write(cursor, tag, data, size);

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
tlv_modify(
	__in	tlv_cursor_t	*cursor,
	__in	uint32_t	tag,
	__in	uint8_t		*data,
	__in	size_t		size)
{
	uint32_t *pos;
	unsigned int old_ndwords;
	unsigned int new_ndwords;
	unsigned int delta;
	int rc;

	if ((rc = tlv_validate_state(cursor)) != 0)
		goto fail1;

	if (tlv_tag(cursor) == TLV_TAG_END) {
		rc = EINVAL;
		goto fail2;
	}
	if (tlv_tag(cursor) != tag) {
		rc = EINVAL;
		goto fail3;
	}

	old_ndwords = TLV_DWORD_COUNT(tlv_length(cursor));
	new_ndwords = TLV_DWORD_COUNT(size);

	if ((rc = tlv_require_end(cursor)) != 0)
		goto fail4;

	if (new_ndwords > old_ndwords) {
		/* Expand space used for TLV item */
		delta = new_ndwords - old_ndwords;
		pos = cursor->current + old_ndwords;

		if (cursor->end + 1 + delta > cursor->limit) {
			rc = ENOSPC;
			goto fail5;
		}

		/* Move up: new space at (cursor->current + old_ndwords) */
		memmove(pos + delta, pos,
		    (cursor->end + 1 - pos) * sizeof (uint32_t));

		/* Adjust the end pointer */
		cursor->end += delta;

	} else if (new_ndwords < old_ndwords) {
		/* Shrink space used for TLV item */
		delta = old_ndwords - new_ndwords;
		pos = cursor->current + new_ndwords;

		/* Move down: remove words at (cursor->current + new_ndwords) */
		memmove(pos, pos + delta,
		    (cursor->end + 1 - pos) * sizeof (uint32_t));

		/* Zero the new space at the end of the TLV chain */
		memset(cursor->end + 1 - delta, 0, delta * sizeof (uint32_t));

		/* Adjust the end pointer */
		cursor->end -= delta;
	}

	/* Write new data */
	tlv_write(cursor, tag, data, size);

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/* Validate TLV formatted partition contents (before writing to flash) */
	__checkReturn		int
efx_nvram_tlv_validate(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in_bcount(partn_size)	caddr_t partn_data,
	__in			size_t partn_size)
{
	tlv_cursor_t cursor;
	struct tlv_partition_header *header;
	struct tlv_partition_trailer *trailer;
	size_t total_length;
	uint32_t cksum;
	int pos;
	int rc;

	EFX_STATIC_ASSERT(sizeof (*header) <= HUNTINGTON_NVRAM_CHUNK);

	if ((partn_data == NULL) || (partn_size == 0)) {
		rc = EINVAL;
		goto fail1;
	}

	/* The partition header must be the first item (at offset zero) */
	if ((rc = tlv_init_cursor_from_size(&cursor, partn_data,
		    partn_size)) != 0) {
		rc = EFAULT;
		goto fail2;
	}
	if (tlv_tag(&cursor) != TLV_TAG_PARTITION_HEADER) {
		rc = EINVAL;
		goto fail3;
	}
	header = (struct tlv_partition_header *)tlv_item(&cursor);

	/* Check TLV partition length (includes the END tag) */
	total_length = __LE_TO_CPU_32(header->total_length);
	if (total_length > partn_size) {
		rc = EFBIG;
		goto fail4;
	}

	/* Check partition ends with PARTITION_TRAILER and END tags */
	if ((rc = tlv_find(&cursor, TLV_TAG_PARTITION_TRAILER)) != 0) {
		rc = EINVAL;
		goto fail5;
	}
	trailer = (struct tlv_partition_trailer *)tlv_item(&cursor);

	if ((rc = tlv_advance(&cursor)) != 0) {
		rc = EINVAL;
		goto fail6;
	}
	if (tlv_tag(&cursor) != TLV_TAG_END) {
		rc = EINVAL;
		goto fail7;
	}

	/* Check generation counts are consistent */
	if (trailer->generation != header->generation) {
		rc = EINVAL;
		goto fail8;
	}

	/* Verify partition checksum */
	cksum = 0;
	for (pos = 0; (size_t)pos < total_length; pos += sizeof (uint32_t)) {
		cksum += *((uint32_t *)(partn_data + pos));
	}
	if (cksum != 0) {
		rc = EINVAL;
		goto fail9;
	}

	return (0);

fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/* Read and validate an entire TLV formatted partition */
static	__checkReturn		int
hunt_nvram_read_tlv_partition(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in_bcount(partn_size)	caddr_t partn_data,
	__in			size_t partn_size)
{
	tlv_cursor_t cursor;
	struct tlv_partition_header *header;
	struct tlv_partition_trailer *trailer;
	size_t total_length;
	uint32_t cksum;
	int pos;
	int rc;

	EFX_STATIC_ASSERT(sizeof (*header) <= HUNTINGTON_NVRAM_CHUNK);

	if ((partn_data == NULL) || (partn_size == 0)) {
		rc = EINVAL;
		goto fail1;
	}

	/* Read initial chunk of partition */
	if ((rc = hunt_nvram_partn_read(enp, partn, 0, partn_data,
		    HUNTINGTON_NVRAM_CHUNK)) != 0) {
		goto fail2;
	}

	/* The partition header must be the first item (at offset zero) */
	if ((rc = tlv_init_cursor_from_size(&cursor, partn_data,
		    partn_size)) != 0) {
		rc = EFAULT;
		goto fail3;
	}
	if (tlv_tag(&cursor) != TLV_TAG_PARTITION_HEADER) {
		rc = EINVAL;
		goto fail4;
	}
	header = (struct tlv_partition_header *)tlv_item(&cursor);

	/* Check TLV partition length (includes the END tag) */
	total_length = __LE_TO_CPU_32(header->total_length);
	if (total_length > partn_size) {
		rc = EFBIG;
		goto fail5;
	}

	/* Read the remaining partition content */
	if (total_length > HUNTINGTON_NVRAM_CHUNK) {
		if ((rc = hunt_nvram_partn_read(enp, partn,
			    HUNTINGTON_NVRAM_CHUNK,
			    partn_data + HUNTINGTON_NVRAM_CHUNK,
			    total_length - HUNTINGTON_NVRAM_CHUNK)) != 0)
			goto fail6;
	}

	/* Check partition ends with PARTITION_TRAILER and END tags */
	if ((rc = tlv_find(&cursor, TLV_TAG_PARTITION_TRAILER)) != 0) {
		rc = EINVAL;
		goto fail7;
	}
	trailer = (struct tlv_partition_trailer *)tlv_item(&cursor);

	if ((rc = tlv_advance(&cursor)) != 0) {
		rc = EINVAL;
		goto fail8;
	}
	if (tlv_tag(&cursor) != TLV_TAG_END) {
		rc = EINVAL;
		goto fail9;
	}

	/* Check data read from partition is consistent */
	if (trailer->generation != header->generation) {
		/*
		 * The partition data may have been modified between successive
		 * MCDI NVRAM_READ requests by the MC or another PCI function.
		 *
		 * The caller must retry to obtain consistent partition data.
		 */
		rc = EAGAIN;
		goto fail10;
	}

	/* Verify partition checksum */
	cksum = 0;
	for (pos = 0; (size_t)pos < total_length; pos += sizeof (uint32_t)) {
		cksum += *((uint32_t *)(partn_data + pos));
	}
	if (cksum != 0) {
		rc = EINVAL;
		goto fail11;
	}

	return (0);

fail11:
	EFSYS_PROBE(fail11);
fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/*
 * Read a single TLV item from a host memory
 * buffer containing a TLV formatted partition.
 */
	__checkReturn		int
hunt_nvram_buf_read_tlv(
	__in				efx_nic_t *enp,
	__in_bcount(partn_size)		caddr_t partn_data,
	__in				size_t partn_size,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep)
{
	tlv_cursor_t cursor;
	caddr_t data;
	size_t length;
	caddr_t value;
	int rc;

	if ((partn_data == NULL) || (partn_size == 0)) {
		rc = EINVAL;
		goto fail1;
	}

	/* Find requested TLV tag in partition data */
	if ((rc = tlv_init_cursor_from_size(&cursor, partn_data,
		    partn_size)) != 0) {
		rc = EFAULT;
		goto fail2;
	}
	if ((rc = tlv_find(&cursor, tag)) != 0) {
		rc = ENOENT;
		goto fail3;
	}
	value = tlv_value(&cursor);
	length = tlv_length(&cursor);

	if (length == 0)
		data = NULL;
	else {
		/* Copy out data from TLV item */
		EFSYS_KMEM_ALLOC(enp->en_esip, length, data);
		if (data == NULL) {
			rc = ENOMEM;
			goto fail4;
		}
		memcpy(data, value, length);
	}

	*datap = data;
	*sizep = length;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}



/* Read a single TLV item from a TLV formatted partition */
	__checkReturn		int
hunt_nvram_partn_read_tlv(
	__in				efx_nic_t *enp,
	__in				uint32_t partn,
	__in				uint32_t tag,
	__deref_out_bcount_opt(*sizep)	caddr_t *datap,
	__out				size_t *sizep)
{
	caddr_t partn_data = NULL;
	size_t partn_size = 0;
	size_t length;
	caddr_t data;
	int retry;
	int rc;

	/* Allocate sufficient memory for the entire partition */
	if ((rc = hunt_nvram_partn_size(enp, partn, &partn_size)) != 0)
		goto fail1;

	if (partn_size == 0) {
		rc = ENOENT;
		goto fail2;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip, partn_size, partn_data);
	if (partn_data == NULL) {
		rc = ENOMEM;
		goto fail3;
	}

	/*
	 * Read the entire TLV partition. Retry until consistent partition
	 * contents are returned. Inconsistent data may be read if:
	 *  a) the partition contents are invalid
	 *  b) the MC has rebooted while we were reading the partition
	 *  c) the partition has been modified while we were reading it
	 * Limit retry attempts to ensure forward progress.
	 */
	retry = 10;
	do {
		rc = hunt_nvram_read_tlv_partition(enp, partn,
		    partn_data, partn_size);
	} while ((rc == EAGAIN) && (--retry > 0));

	if (rc != 0) {
		/* Failed to obtain consistent partition data */
		goto fail4;
	}

	if ((rc = hunt_nvram_buf_read_tlv(enp, partn_data, partn_size,
		    tag, &data, &length)) != 0)
		goto fail5;

	EFSYS_KMEM_FREE(enp->en_esip, partn_size, partn_data);

	*datap = data;
	*sizep = length;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);

	EFSYS_KMEM_FREE(enp->en_esip, partn_size, partn_data);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/*
 * Add or update a single TLV item in a host memory buffer containing a TLV
 * formatted partition.
 */
	__checkReturn		int
hunt_nvram_buf_write_tlv(
	__inout_bcount(partn_size)	caddr_t partn_data,
	__in				size_t partn_size,
	__in				uint32_t tag,
	__in_bcount(tag_size)		caddr_t tag_data,
	__in				size_t tag_size,
	__out				size_t *total_lengthp)
{
	tlv_cursor_t cursor;
	struct tlv_partition_header *header;
	struct tlv_partition_trailer *trailer;
	uint32_t generation;
	uint32_t cksum;
	int pos;
	int rc;

	/* The partition header must be the first item (at offset zero) */
	if ((rc = tlv_init_cursor_from_size(&cursor, partn_data,
			partn_size)) != 0) {
		rc = EFAULT;
		goto fail1;
	}
	if (tlv_tag(&cursor) != TLV_TAG_PARTITION_HEADER) {
		rc = EINVAL;
		goto fail2;
	}
	header = (struct tlv_partition_header *)tlv_item(&cursor);

	/* Update the TLV chain to contain the new data */
	if ((rc = tlv_find(&cursor, tag)) == 0) {
		/* Modify existing TLV item */
		if ((rc = tlv_modify(&cursor, tag,
			    tag_data, tag_size)) != 0)
			goto fail3;
	} else {
		/* Insert a new TLV item before the PARTITION_TRAILER */
		rc = tlv_find(&cursor, TLV_TAG_PARTITION_TRAILER);
		if (rc != 0) {
			rc = EINVAL;
			goto fail4;
		}
		if ((rc = tlv_insert(&cursor, tag,
			    tag_data, tag_size)) != 0) {
			rc = EINVAL;
			goto fail5;
		}
	}

	/* Find the trailer tag */
	if ((rc = tlv_find(&cursor, TLV_TAG_PARTITION_TRAILER)) != 0) {
		rc = EINVAL;
		goto fail6;
	}
	trailer = (struct tlv_partition_trailer *)tlv_item(&cursor);

	/* Update PARTITION_HEADER and PARTITION_TRAILER fields */
	*total_lengthp = tlv_block_length_used(&cursor);
	EFSYS_ASSERT3U(*total_lengthp, <=, partn_size);
	generation = __LE_TO_CPU_32(header->generation) + 1;

	header->total_length	= __CPU_TO_LE_32(*total_lengthp);
	header->generation	= __CPU_TO_LE_32(generation);
	trailer->generation	= __CPU_TO_LE_32(generation);

	/* Recompute PARTITION_TRAILER checksum */
	trailer->checksum = 0;
	cksum = 0;
	for (pos = 0; (size_t)pos < *total_lengthp; pos += sizeof (uint32_t)) {
		cksum += *((uint32_t *)(partn_data + pos));
	}
	trailer->checksum = ~cksum + 1;

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/* Add or update a single TLV item in a TLV formatted partition */
	__checkReturn		int
hunt_nvram_partn_write_tlv(
	__in			efx_nic_t *enp,
	__in			uint32_t partn,
	__in			uint32_t tag,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	size_t partn_size;
	caddr_t partn_data;
	size_t total_length;
	int rc;

	EFSYS_ASSERT3U(partn, ==, NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG);

	/* Allocate sufficient memory for the entire partition */
	if ((rc = hunt_nvram_partn_size(enp, partn, &partn_size)) != 0)
		goto fail1;

	EFSYS_KMEM_ALLOC(enp->en_esip, partn_size, partn_data);
	if (partn_data == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	/* Lock the partition */
	if ((rc = hunt_nvram_partn_lock(enp, partn)) != 0)
		goto fail3;

	/* Read the partition contents (no need to retry when locked). */
	if ((rc = hunt_nvram_read_tlv_partition(enp, partn,
		    partn_data, partn_size)) != 0) {
		/* Failed to obtain consistent partition data */
		goto fail4;
	}

	/* Update the contents in memory */
	if ((rc = hunt_nvram_buf_write_tlv(partn_data, partn_size,
		    tag, data, size, &total_length)) != 0)
		goto fail5;

	/* Erase the whole partition */
	if ((rc = hunt_nvram_partn_erase(enp, partn, 0, partn_size)) != 0)
		goto fail6;

	/* Write new partition contents to NVRAM */
	if ((rc = hunt_nvram_partn_write(enp, partn, 0, partn_data,
		    total_length)) != 0)
		goto fail7;

	/* Unlock the partition */
	hunt_nvram_partn_unlock(enp, partn);

	EFSYS_KMEM_FREE(enp->en_esip, partn_size, partn_data);

	return (0);

fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);

	hunt_nvram_partn_unlock(enp, partn);
fail3:
	EFSYS_PROBE(fail3);

	EFSYS_KMEM_FREE(enp->en_esip, partn_size, partn_data);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_partn_size(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__out			size_t *sizep)
{
	int rc;

	if ((rc = efx_mcdi_nvram_info(enp, partn, sizep, NULL, NULL)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_partn_lock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn)
{
	int rc;

	if ((rc = efx_mcdi_nvram_update_start(enp, partn)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_partn_read(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	size_t chunk;
	int rc;

	while (size > 0) {
		chunk = MIN(size, HUNTINGTON_NVRAM_CHUNK);

		if ((rc = efx_mcdi_nvram_read(enp, partn, offset,
			    data, chunk)) != 0) {
			goto fail1;
		}

		size -= chunk;
		data += chunk;
		offset += chunk;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_partn_erase(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__in			size_t size)
{
	int rc;

	if ((rc = efx_mcdi_nvram_erase(enp, partn, offset, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_partn_write(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	size_t chunk;
	int rc;

	while (size > 0) {
		chunk = MIN(size, HUNTINGTON_NVRAM_CHUNK);

		if ((rc = efx_mcdi_nvram_write(enp, partn, offset,
			    data, chunk)) != 0) {
			goto fail1;
		}

		size -= chunk;
		data += chunk;
		offset += chunk;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

				void
hunt_nvram_partn_unlock(
	__in			efx_nic_t *enp,
	__in			unsigned int partn)
{
	boolean_t reboot;
	int rc;

	reboot = B_FALSE;
	if ((rc = efx_mcdi_nvram_update_finish(enp, partn, reboot)) != 0)
		goto fail1;

	return;

fail1:
	EFSYS_PROBE1(fail1, int, rc);
}

	__checkReturn		int
hunt_nvram_partn_set_version(
	__in			efx_nic_t *enp,
	__in			unsigned int partn,
	__in_ecount(4)		uint16_t version[4])
{
	struct tlv_partition_version partn_version;
	size_t size;
	int rc;

	/* Add or modify partition version TLV item */
	partn_version.version_w = __CPU_TO_LE_16(version[0]);
	partn_version.version_x = __CPU_TO_LE_16(version[1]);
	partn_version.version_y = __CPU_TO_LE_16(version[2]);
	partn_version.version_z = __CPU_TO_LE_16(version[3]);

	size = sizeof (partn_version) - (2 * sizeof (uint32_t));

	if ((rc = hunt_nvram_partn_write_tlv(enp,
		    NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,
		    TLV_TAG_PARTITION_VERSION(partn),
		    (caddr_t)&partn_version.version_w, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif /* EFSYS_OPT_VPD || EFSYS_OPT_NVRAM */

#if EFSYS_OPT_NVRAM

typedef struct hunt_parttbl_entry_s {
	unsigned int		partn;
	unsigned int		port;
	efx_nvram_type_t	nvtype;
} hunt_parttbl_entry_t;

/* Translate EFX NVRAM types to firmware partition types */
static hunt_parttbl_entry_t hunt_parttbl[] = {
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE,	   1, EFX_NVRAM_MC_FIRMWARE},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE,	   2, EFX_NVRAM_MC_FIRMWARE},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE,	   3, EFX_NVRAM_MC_FIRMWARE},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE,	   4, EFX_NVRAM_MC_FIRMWARE},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE_BACKUP,  1, EFX_NVRAM_MC_GOLDEN},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE_BACKUP,  2, EFX_NVRAM_MC_GOLDEN},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE_BACKUP,  3, EFX_NVRAM_MC_GOLDEN},
	{NVRAM_PARTITION_TYPE_MC_FIRMWARE_BACKUP,  4, EFX_NVRAM_MC_GOLDEN},
	{NVRAM_PARTITION_TYPE_EXPANSION_ROM,	   1, EFX_NVRAM_BOOTROM},
	{NVRAM_PARTITION_TYPE_EXPANSION_ROM,	   2, EFX_NVRAM_BOOTROM},
	{NVRAM_PARTITION_TYPE_EXPANSION_ROM,	   3, EFX_NVRAM_BOOTROM},
	{NVRAM_PARTITION_TYPE_EXPANSION_ROM,	   4, EFX_NVRAM_BOOTROM},
	{NVRAM_PARTITION_TYPE_EXPROM_CONFIG_PORT0, 1, EFX_NVRAM_BOOTROM_CFG},
	{NVRAM_PARTITION_TYPE_EXPROM_CONFIG_PORT1, 2, EFX_NVRAM_BOOTROM_CFG},
	{NVRAM_PARTITION_TYPE_EXPROM_CONFIG_PORT2, 3, EFX_NVRAM_BOOTROM_CFG},
	{NVRAM_PARTITION_TYPE_EXPROM_CONFIG_PORT3, 4, EFX_NVRAM_BOOTROM_CFG},
	{NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,	   1, EFX_NVRAM_DYNAMIC_CFG},
	{NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,	   2, EFX_NVRAM_DYNAMIC_CFG},
	{NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,	   3, EFX_NVRAM_DYNAMIC_CFG},
	{NVRAM_PARTITION_TYPE_DYNAMIC_CONFIG,	   4, EFX_NVRAM_DYNAMIC_CFG}
};

static	__checkReturn		hunt_parttbl_entry_t *
hunt_parttbl_entry(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	hunt_parttbl_entry_t *entry;
	int i;

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	for (i = 0; i < EFX_ARRAY_SIZE(hunt_parttbl); i++) {
		entry = &hunt_parttbl[i];

		if (entry->port == emip->emi_port && entry->nvtype == type)
			return (entry);
	}

	return (NULL);
}


#if EFSYS_OPT_DIAG

	__checkReturn		int
hunt_nvram_test(
	__in			efx_nic_t *enp)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	hunt_parttbl_entry_t *entry;
	unsigned int npartns = 0;
	uint32_t *partns = NULL;
	size_t size;
	int i;
	unsigned int j;
	int rc;

	/* Find supported partitions */
	size = MC_CMD_NVRAM_PARTITIONS_OUT_TYPE_ID_MAXNUM * sizeof (uint32_t);
	EFSYS_KMEM_ALLOC(enp->en_esip, size, partns);
	if (partns == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	if ((rc = efx_mcdi_nvram_partitions(enp, (caddr_t)partns, size,
		    &npartns)) != 0) {
		goto fail2;
	}

	/*
	 * Iterate over the list of supported partition types
	 * applicable to *this* port
	 */
	for (i = 0; i < EFX_ARRAY_SIZE(hunt_parttbl); i++) {
		entry = &hunt_parttbl[i];

		if (entry->port != emip->emi_port)
			continue;

		for (j = 0; j < npartns; j++) {
			if (entry->partn == partns[j]) {
				rc = efx_mcdi_nvram_test(enp, entry->partn);
				if (rc != 0)
					goto fail3;
			}
		}
	}

	EFSYS_KMEM_FREE(enp->en_esip, size, partns);
	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
	EFSYS_KMEM_FREE(enp->en_esip, size, partns);
fail1:
	EFSYS_PROBE1(fail1, int, rc);
	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn		int
hunt_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep)
{
	hunt_parttbl_entry_t *entry;
	uint32_t partn;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	partn = entry->partn;

	if ((rc = hunt_nvram_partn_size(enp, partn, sizep)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	*sizep = 0;

	return (rc);
}

	__checkReturn		int
hunt_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	hunt_parttbl_entry_t *entry;
	uint32_t partn;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	partn = entry->partn;

	/* FIXME: get highest partn version from all ports */
	/* FIXME: return partn description if available */

	if ((rc = efx_mcdi_nvram_metadata(enp, partn, subtypep,
		    version, NULL, 0)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *chunk_sizep)
{
	hunt_parttbl_entry_t *entry;
	uint32_t partn;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	partn = entry->partn;

	if ((rc = hunt_nvram_partn_lock(enp, partn)) != 0)
		goto fail2;

	if (chunk_sizep != NULL)
		*chunk_sizep = HUNTINGTON_NVRAM_CHUNK;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	hunt_parttbl_entry_t *entry;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = hunt_nvram_partn_read(enp, entry->partn,
		    offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	hunt_parttbl_entry_t *entry;
	size_t size;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = hunt_nvram_partn_size(enp, entry->partn, &size)) != 0)
		goto fail2;

	if ((rc = hunt_nvram_partn_erase(enp, entry->partn, 0, size)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
hunt_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	hunt_parttbl_entry_t *entry;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = hunt_nvram_partn_write(enp, entry->partn,
		    offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

				void
hunt_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	hunt_parttbl_entry_t *entry;

	if ((entry = hunt_parttbl_entry(enp, type)) != NULL)
		hunt_nvram_partn_unlock(enp, entry->partn);
}

	__checkReturn		int
hunt_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in_ecount(4)		uint16_t version[4])
{
	hunt_parttbl_entry_t *entry;
	unsigned int partn;
	int rc;

	if ((entry = hunt_parttbl_entry(enp, type)) == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}
	partn = entry->partn;

	if ((rc = hunt_nvram_partn_set_version(enp, partn, version)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_NVRAM */

#endif	/* EFSYS_OPT_HUNTINGTON */
