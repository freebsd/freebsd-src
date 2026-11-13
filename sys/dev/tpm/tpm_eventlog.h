/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Netflix, Inc.
 */

#ifndef _DEV_TPM_EVENTLOG_H_
#define _DEV_TPM_EVENTLOG_H_

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/tpmeventlog.h>

/* Bound firmware-controlled counts and lengths, including unknown hashes. */
#define TPM_EVENTLOG_MAX_ALGS	32
#define TPM_EVENTLOG_MAX_DIGEST	128

struct tpm_eventlog_algorithms {
	uint32_t count;
	uint16_t id[TPM_EVENTLOG_MAX_ALGS];
	uint16_t size[TPM_EVENTLOG_MAX_ALGS];
};

/* Return a complete record's size, or zero for an invalid/short record. */
static inline size_t
tpm_eventlog_event1(const uint8_t *p, size_t len)
{
	size_t size;

	/* PCR index, event type, SHA-1 digest, event size, then event data. */
	if (len < 32)
		return (0);
	size = le32dec(p + 28);
	if (size > len - 32)
		return (0);
	return (32 + size);
}

static inline size_t
tpm_eventlog_specid(const uint8_t *p, size_t len,
    struct tpm_eventlog_algorithms *algs)
{
	static const uint8_t signature[16] = "Spec ID Event03";
	size_t size, off;
	uint32_t i, j, count;

	/* TCG_PCR_EVENT followed by the variable Spec ID Event03 payload. */
	if (len < 32 || le32dec(p) != 0 || le32dec(p + 4) != 3)
		return (0);
	for (i = 8; i < 28; i++)
		if (p[i] != 0)
			return (0);
	size = le32dec(p + 28);
	if (size > len - 32 || size < 29)
		return (0);
	p += 32;
	for (i = 0; i < sizeof(signature); i++)
		if (p[i] != signature[i])
			return (0);
	if (p[21] != 2 || (p[23] != 1 && p[23] != 2))
		return (0);
	count = le32dec(p + 24);
	if (count == 0 || count > TPM_EVENTLOG_MAX_ALGS ||
	    count > (size - 29) / 4)
		return (0);
	for (i = 0; i < count; i++) {
		algs->id[i] = le16dec(p + 28 + i * 4);
		algs->size[i] = le16dec(p + 30 + i * 4);
		if (algs->size[i] == 0 ||
		    algs->size[i] > TPM_EVENTLOG_MAX_DIGEST)
			return (0);
		for (j = 0; j < i; j++)
			if (algs->id[i] == algs->id[j])
				return (0);
	}
	off = 28 + count * 4;
	if (p[off] != size - off - 1)
		return (0);
	algs->count = count;
	return (32 + size);
}

static inline size_t
tpm_eventlog_event2(const uint8_t *p, size_t len,
    const struct tpm_eventlog_algorithms *algs)
{
	size_t off, size;
	uint32_t count, seen, i, j;
	uint16_t id;

	if (len < 12)
		return (0);
	count = le32dec(p + 8);
	if (count == 0 || count > TPM_EVENTLOG_MAX_ALGS ||
	    count != algs->count)
		return (0);
	off = 12;
	seen = 0;
	for (i = 0; i < count; i++) {
		if (len - off < 2)
			return (0);
		id = le16dec(p + off);
		off += 2;
		for (j = 0; j < algs->count; j++)
			if (algs->id[j] == id)
				break;
		if (j == algs->count || (seen & (1U << j)) != 0 ||
		    algs->size[j] > len - off)
			return (0);
		seen |= 1U << j;
		off += algs->size[j];
	}
	if (len - off < 4)
		return (0);
	size = le32dec(p + off);
	off += 4;
	if (size > len - off)
		return (0);
	return (off + size);
}

static inline int
tpm_eventlog_valid(const uint8_t *p, size_t len, uint32_t format)
{
	struct tpm_eventlog_algorithms algs;
	size_t off, size;

	if (len == 0 || len > TPM_EVENTLOG_MAX_SIZE)
		return (0);
	if (format == TPM_EVENTLOG_TCG2) {
		off = tpm_eventlog_specid(p, len, &algs);
		if (off == 0)
			return (0);
	} else if (format == TPM_EVENTLOG_TCG12)
		off = 0;
	else
		return (0);
	while (off < len) {
		if (format == TPM_EVENTLOG_TCG12)
			size = tpm_eventlog_event1(p + off, len - off);
		else
			size = tpm_eventlog_event2(p + off, len - off, &algs);
		if (size == 0)
			return (0);
		off += size;
	}
	return (1);
}

/* Locate the suffix added to a TCG2 final-events table after a snapshot. */
static inline int
tpm_eventlog_final(const uint8_t *p, size_t len,
    const struct tpm_eventlog_algorithms *algs, uint64_t skip,
    size_t *start, size_t *end)
{
	uint64_t count, i;
	size_t off, size;

	if (len < 16 || le64dec(p) != 1)
		return (0);
	count = le64dec(p + 8);
	if (count < skip || count > (len - 16) / 16)
		return (0);
	off = 16;
	*start = off;
	for (i = 0; i < count; i++) {
		size = tpm_eventlog_event2(p + off, len - off, algs);
		if (size == 0)
			return (0);
		off += size;
		if (i + 1 == skip)
			*start = off;
	}
	*end = off;
	return (1);
}

#ifdef _KERNEL
struct tpm_eventlog;
struct tpm_eventlog *tpm_eventlog_create(device_t);
void tpm_eventlog_destroy(struct tpm_eventlog *);
#endif

#endif /* _DEV_TPM_EVENTLOG_H_ */
