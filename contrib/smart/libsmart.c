/*
 * Copyright (c) 2016-2026 Chuck Tuffli <chuck@tuffli.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <err.h>
#include <string.h>
#include <sys/endian.h>

#ifdef LIBXO
#include <libxo/xo.h>
#endif

#include "libsmart.h"
#include "libsmart_priv.h"
#include "libsmart_dev.h"

/* Default page lists */

static smart_page_list_t pg_list_ata = {
	.pg_count = 2,
	.pages = {
		{ .id = PAGE_ID_ATA_SMART_READ_DATA, .bytes = 512 },
		{ .id = PAGE_ID_ATA_SMART_RET_STATUS, .bytes = 4 }
	}
};

#define PAGE_ID_NVME_SMART_HEALTH	0x02

static smart_page_list_t pg_list_nvme = {
	.pg_count = 1,
	.pages = {
		{ .id = PAGE_ID_NVME_SMART_HEALTH, .bytes = 512 }
	}
};

static smart_page_list_t pg_list_scsi = {
	.pg_count = 8,
	.pages = {
		{ .id = PAGE_ID_SCSI_WRITE_ERR, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_READ_ERR, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_VERIFY_ERR, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_NON_MEDIUM_ERR, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_LAST_N_ERR, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_TEMPERATURE, .bytes = 64 },
		{ .id = PAGE_ID_SCSI_START_STOP_CYCLE, .bytes = 128 },
		{ .id = PAGE_ID_SCSI_INFO_EXCEPTION, .bytes = 64 },
	}
};

static uint32_t __smart_attribute_max(smart_buf_t *sb);
static uint32_t __smart_buffer_size(smart_h h);
static smart_map_t *__smart_map(smart_h h, smart_buf_t *sb);
static smart_page_list_t *__smart_page_list(smart_h h);
static int32_t __smart_read_pages(smart_h h, smart_buf_t *sb);

static const char *
smart_proto_str(smart_protocol_e p)
{

	switch (p) {
	case SMART_PROTO_AUTO:
		return "auto";
	case SMART_PROTO_ATA:
		return "ATA";
	case SMART_PROTO_SCSI:
		return "SCSI";
	case SMART_PROTO_NVME:
		return "NVME";
	default:
		return "Unknown";
	}
}

smart_h
smart_open(smart_protocol_e protocol, char *devname)
{
	smart_t *s;

	s = device_open(protocol, devname);

	if (s) {
		dprintf("protocol %s (specified %s%s)\n",
				smart_proto_str(s->protocol),
				smart_proto_str(protocol),
				s->info.tunneled ?  ", tunneled ATA" : "");
	}

	return s;
}

void
smart_close(smart_h h)
{

	device_close(h);
}

bool
smart_supported(smart_h h)
{
	smart_t *s = h;
	bool supported = false;

	if (s) {
		supported = s->info.supported;
		dprintf("SMART is %ssupported\n", supported ? "" : "not ");
	}

	return supported;
}

smart_map_t *
smart_read(smart_h h)
{
	smart_t *s = h;
	smart_buf_t *sb = NULL;
	smart_map_t *sm = NULL;

	sb = calloc(1, sizeof(smart_buf_t));
	if (sb) {
		sb->protocol = s->protocol;

		/*
		 * Need the page list to calculate the buffer size. If one
		 * isn't specified, get the default based on the protocol.
		 */
		if (s->pg_list == NULL) {
			s->pg_list = __smart_page_list(s);
			if (!s->pg_list) {
				goto smart_read_out;
			}
		}

		sb->b = NULL;
		sb->bsize = __smart_buffer_size(s);

		if (sb->bsize != 0) {
			sb->b = malloc(sb->bsize);
		}

		if (sb->b == NULL) {
			goto smart_read_out;
		}

		if (__smart_read_pages(s, sb) < 0) {
			goto smart_read_out;
		}

		sb->attr_count = __smart_attribute_max(sb);

		sm = __smart_map(h, sb);
		if (!sm) {
			free(sb->b);
			free(sb);
			sb = NULL;
		}
	}

smart_read_out:
	if (!sm) {
		if (sb) {
			if (sb->b) {
				free(sb->b);
			}

			free(sb);
		}
	}

	return sm;
}

void
smart_free(smart_map_t *sm)
{
	smart_buf_t *sb = NULL;
	uint32_t i;

	if (sm == NULL)
		return;

	sb = sm->sb;

	if (sb) {
		if (sb->b) {
			free(sb->b);
			sb->b = NULL;
		}

		free(sb);
	}

	for (i = 0; i < sm->count; i++) {
		smart_map_t *tm = sm->attr[i].thresh;

		if (tm) {
			free(tm);
		}

		if (sm->attr[i].flags & SMART_ATTR_F_ALLOC) {
			free((void *)(uintptr_t)sm->attr[i].description);
		}
	}

	free(sm);
}

/*
 * Format specifier for the various output types
 * Provides versions to use with libxo and without
 * TODO some of this is ATA specific
 */
#ifndef LIBXO
# define __smart_print_val(fmt, ...) 	printf(fmt, ##__VA_ARGS__)
# define VEND_STR	"Vendor\t%s\n"
# define DEV_STR	"Device\t%s\n"
# define REV_STR	"Revision\t%s\n"
# define SERIAL_STR	"Serial\t%s\n"
# define PAGE_HEX	"%#01.1x\t"
# define PAGE_DEC	"%d\t"
# define ID_HEX		"%#01.1x\t"
# define ID_DEC		"%d\t"
# define RAW_STR	"%s"
# define RAW_HEX	"%#01.1x"
# define RAW_DEC	"%d"
/* Long integer version of the format macro */
# define RAW_LHEX	"%#01.1" PRIx64
# define RAW_LDEC	"%" PRId64
# define THRESH_HEX	"\t%#02.2x\t%#01.1x\t%#01.1x"
# define THRESH_DEC	"\t%d\t%d\t%d"
# define DESC_STR	"%s"
#else
# define __smart_print_val(fmt, ...) 	 xo_emit(fmt, ##__VA_ARGS__)
# define VEND_STR	"{L:Vendor}{P:\t}{:vendor/%s}\n"
# define DEV_STR	"{L:Device}{P:\t}{:device/%s}\n"
# define REV_STR	"{L:Revision}{P:\t}{:rev/%s}\n"
# define SERIAL_STR	"{L:Serial}{P:\t}{:serial/%s}\n"
# define PAGE_HEX	"{k:page/%#01.1x}{P:\t}"
# define PAGE_DEC	"{k:page/%d}{P:\t}"
# define ID_HEX		"{k:id/%#01.1x}{P:\t}"
# define ID_DEC		"{k:id/%d}{P:\t}"
# define RAW_STR	"{k:raw/%s}"
# define RAW_HEX	"{k:raw/%#01.1x}"
# define RAW_DEC	"{k:raw/%d}"
/* Long integer version of the format macro */
# define RAW_LHEX	"{k:raw/%#01.1" PRIx64 "}"
# define RAW_LDEC	"{k:raw/%" PRId64 "}"
# define THRESH_HEX	"{P:\t}{k:flags/%#02.2x}{P:\t}{k:nominal/%#01.1x}{P:\t}{k:worst/%#01.1x}"
# define THRESH_DEC	"{P:\t}{k:flags/%d}{P:\t}{k:nominal/%d}{P:\t}{k:worst/%d}"
# define DESC_STR	"{:description}{P:\t}"
#endif

#define THRESH_COUNT	3


/* Convert an 128-bit unsigned integer to a string */
static char *
__smart_u128_str(smart_attr_t *sa)
{
	/* Max size is log10(x) = log2(x) / log2(10) ~= log2(x) / 3.322 */
#define MAX_LEN (128 / 3 + 1 + 1)
	static char s[MAX_LEN];
	char *p = s + MAX_LEN - 1;
	uint32_t *a = (uint32_t *)sa->raw;
	uint64_t r, d;

	*p-- = '\0';

	do {
		r = a[3];

		d = r / 10;
		r = ((r - d * 10) << 32) + a[2];
		a[3] = d;

		d = r / 10;
		r = ((r - d * 10) << 32) + a[1];
		a[2] = d;

		d = r / 10;
		r = ((r - d * 10) << 32) + a[0];
		a[1] = d;

		d = r / 10;
		r = r - d * 10;
		a[0] = d;

		*p-- = '0' + r;
	} while (a[0] || a[1] || a[2] || a[3]);

	p++;

	while ((*p == '0') && (p < &s[sizeof(s) - 2]))
		p++;

	return p;
}

static void
__smart_print_thresh(smart_map_t *tm, uint32_t flags)
{
	bool do_hex = false;

	if (!tm) {
		return;
	}

	if (flags & SMART_OPEN_F_HEX)
		do_hex = true;

	__smart_print_val(do_hex ? THRESH_HEX : THRESH_DEC,
			*((uint16_t *)tm->attr[0].raw),
			*((uint8_t *)tm->attr[1].raw),
			*((uint8_t *)tm->attr[2].raw));
}

/* Does the attribute match one requested by the caller? */
static bool
__smart_attr_match(smart_matches_t *match, smart_attr_t *attr)
{
	uint32_t i;

	assert((match != NULL) && (attr != NULL));

	for (i = 0; i < match->count; i++) {
		if ((match->m[i].page != -1) && ((uint32_t)match->m[i].page != attr->page))
			continue;

		if ((uint32_t)match->m[i].id == attr->id)
			return true;
	}

	return false;
}

void
smart_print(__attribute__((unused)) smart_h h, smart_map_t *sm, smart_matches_t *which, uint32_t flags)
{
	uint32_t i;
	bool do_hex = false, do_descr = false;
	uint32_t bytes = 0;

	if (!sm) {
		return;
	}

	if (flags & SMART_OPEN_F_HEX)
		do_hex = true;
	if (flags & SMART_OPEN_F_DESCR)
		do_descr = true;

#ifdef LIBXO
	xo_open_container("attributes");
	xo_open_list("attribute");
#endif
	for (i = 0; i < sm->count; i++) {
		/* If we're printing a specific attribute, is this it? */
		if ((which != NULL) && !__smart_attr_match(which, &sm->attr[i])) {
			continue;
		}

#ifdef LIBXO
		xo_open_instance("attribute");
#endif
		/* Print the page / attribute ID if selecting all attributes */
		if (which == NULL) {
			if (do_descr && (sm->attr[i].description != NULL))
				__smart_print_val(DESC_STR, sm->attr[i].description);
			else {
				__smart_print_val(do_hex ? PAGE_HEX : PAGE_DEC, sm->attr[i].page);
				__smart_print_val(do_hex ? ID_HEX : ID_DEC, sm->attr[i].id);
			}
		}

		bytes = sm->attr[i].bytes;

		/* Print the attribute based on its size */
		if (sm->attr[i].flags & SMART_ATTR_F_STR) {
			__smart_print_val(RAW_STR, (char *)sm->attr[i].raw);
		} else if (bytes > 8) {
			if (do_hex)
				;
			else
				__smart_print_val(RAW_STR,
				    __smart_u128_str(&sm->attr[i]));

		} else if (bytes > 4) {
			uint64_t v64 = 0;
			uint64_t mask = UINT64_MAX;

			memcpy(&v64, sm->attr[i].raw, bytes);

			if (sm->attr[i].flags & SMART_ATTR_F_BE) {
				v64 = be64toh(v64);
			} else {
				v64 = le64toh(v64);
			}

			mask >>= 8 * (sizeof(uint64_t) - bytes);

			v64 &= mask;

			__smart_print_val(do_hex ? RAW_LHEX : RAW_LDEC, v64);

		} else if (bytes > 2) {
			uint32_t v32 = 0;
			uint32_t mask = UINT32_MAX;

			memcpy(&v32, sm->attr[i].raw, bytes);

			if (sm->attr[i].flags & SMART_ATTR_F_BE) {
				v32 = be32toh(v32);
			} else {
				v32 = le32toh(v32);
			}

			mask >>= 8 * (sizeof(uint32_t) - bytes);

			v32 &= mask;

			__smart_print_val(do_hex ? RAW_HEX : RAW_DEC, v32);

		} else if (bytes > 1) {
			uint16_t v16 = 0;
			uint16_t mask = UINT16_MAX;

			memcpy(&v16, sm->attr[i].raw, bytes);

			if (sm->attr[i].flags & SMART_ATTR_F_BE) {
				v16 = be16toh(v16);
			} else {
				v16 = le16toh(v16);
			}

			mask >>= 8 * (sizeof(uint16_t) - bytes);

			v16 &= mask;

			__smart_print_val(do_hex ? RAW_HEX : RAW_DEC, v16);

		} else if (bytes > 0) {
			uint8_t v8 = *((uint8_t *)sm->attr[i].raw);

			__smart_print_val(do_hex ? RAW_HEX : RAW_DEC, v8);
		}

		if ((flags & SMART_OPEN_F_THRESH) && sm->attr[i].thresh) {
			xo_open_container("threshold");
			__smart_print_thresh(sm->attr[i].thresh, flags);
			xo_close_container("threshold");
		}

		__smart_print_val("\n");

#ifdef LIBXO
		xo_close_instance("attribute");
#endif
	}
#ifdef LIBXO
	xo_close_list("attribute");
	xo_close_container("attributes");
#endif
}

void
smart_print_device_info(smart_h h)
{
	smart_t *s = h;

	if (!s) {
		return;
	}

	if (*s->info.vendor != '\0')
		__smart_print_val(VEND_STR, s->info.vendor);
	if (*s->info.device != '\0')
		__smart_print_val(DEV_STR, s->info.device);
	if (*s->info.rev != '\0')
		__smart_print_val(REV_STR, s->info.device);
	if (*s->info.serial != '\0')
		__smart_print_val(SERIAL_STR, s->info.serial);
}

static uint32_t
__smart_attr_max_ata(smart_buf_t *sb)
{
	uint32_t max = 0;

	if (sb) {
		max = 30;
	}

	return max;
}

static uint32_t
__smart_attr_max_nvme(smart_buf_t *sb)
{
	uint32_t max = 0;

	if (sb) {
		max = 512;
	}

	return max;
}

static uint32_t
__smart_attr_max_scsi(smart_buf_t *sb)
{
	uint32_t max = 0;

	if (sb) {
		max = 512;
	}

	return max;
}

static uint32_t
__smart_attribute_max(smart_buf_t *sb)
{
	uint32_t count = 0;

	if (sb != NULL) {
		switch (sb->protocol) {
		case SMART_PROTO_ATA:
			count = __smart_attr_max_ata(sb);
			break;
		case SMART_PROTO_NVME:
			count = __smart_attr_max_nvme(sb);
			break;
		case SMART_PROTO_SCSI:
			count = __smart_attr_max_scsi(sb);
			break;
		default:
			;
		}
	}

	return count;
}

/**
 * Return the total buffer size needed by the protocol's page list
 */
static uint32_t
__smart_buffer_size(smart_h h)
{
	smart_t *s = h;
	uint32_t size = 0;

	if ((s != NULL) && (s->pg_list != NULL)) {
		smart_page_list_t *plist = s->pg_list;
		uint32_t p = 0;

		for (p = 0; p < plist->pg_count; p++) {
			size += plist->pages[p].bytes;
		}
	}

	return size;
}

/*
 * Map SMART READ DATA threshold attributes
 *
 * Read the 3 consecutive values (flags, nominal, and worst)
 */
static smart_map_t *
__smart_map_ata_thresh(uint8_t *b)
{
	smart_map_t *sm = NULL;

	sm = malloc(sizeof(smart_map_t) + (THRESH_COUNT * sizeof(smart_attr_t)));
	if (sm) {
		uint32_t i;

		sm->count = THRESH_COUNT;

		sm->attr[0].page = 0;
		sm->attr[0].id = 0;
		sm->attr[0].bytes = 2;
		sm->attr[0].flags = 0;
		sm->attr[0].raw = b;
		sm->attr[0].thresh = NULL;

		b +=2;

		for (i = 1; i < sm->count; i++) {
			sm->attr[i].page = 0;
			sm->attr[i].id = i;
			sm->attr[i].bytes = 1;
			sm->attr[i].flags = 0;
			sm->attr[i].raw = b;
			sm->attr[i].thresh = NULL;

			b ++;
		}
	}

	return sm;
}

/*
 * Map SMART READ DATA attributes
 *
 * The format for the READ DATA buffer is:
 *    2 bytes Revision
 *  360 bytes Attributes (12 bytes each)
 *
 * Each attribute consists of:
 *   1 byte ID
 *   2 byte Status Flags
 *   1 byte Nominal value
 *   1 byte Worst value
 *   7 byte Raw value
 * Note that many attributes do not use the entire 7 bytes of the raw value.
 */
static void
__smart_map_ata_read_data(smart_map_t *sm, void *buf, size_t bsize)
{
	uint8_t *b = NULL;
	uint8_t *b_end = NULL;
	uint32_t max_attr = 0;
	uint32_t a;

	max_attr = __smart_attr_max_ata(sm->sb);
	a = sm->count;

	b = buf;

	/* skip revision */
	b += 2;

	b_end = b + (max_attr * 12);
	if (b_end > (b + bsize)) {
		sm->count = 0;
		return;
	}

	while (b < b_end) {
		if (*b != 0) {
			if ((a - sm->count) >= max_attr) {
				warnx("More attributes (%d) than fit in map",
						a - sm->count);
				break;
			}

			sm->attr[a].page = PAGE_ID_ATA_SMART_READ_DATA;
			sm->attr[a].id = b[0];
			sm->attr[a].description = __smart_ata_desc(
			    PAGE_ID_ATA_SMART_READ_DATA, sm->attr[a].id);
			sm->attr[a].bytes = 7;
			sm->attr[a].flags = 0;
			sm->attr[a].raw = b + 5;
			sm->attr[a].thresh = __smart_map_ata_thresh(b + 1);

			a++;
		}

		b += 12;
	}

	sm->count = a;
}

static void
__smart_map_ata_return_status(smart_map_t *sm, void *buf)
{
	uint8_t *b = NULL;
	uint32_t a;

	a = sm->count;

	b = buf;

	sm->attr[a].page = PAGE_ID_ATA_SMART_RET_STATUS;
	sm->attr[a].id = 0;
	sm->attr[a].description = __smart_ata_desc(PAGE_ID_ATA_SMART_RET_STATUS,
	    sm->attr[a].id);
	sm->attr[a].bytes = 1;
	sm->attr[a].flags = 0;
	sm->attr[a].raw = b;
	sm->attr[a].thresh = NULL;

	a++;

	sm->count = a;
}

static void
__smart_map_ata(smart_h h, smart_buf_t *sb, smart_map_t *sm)
{
	smart_t *s = h;
	smart_page_list_t *pg_list = NULL;
	uint8_t *b = NULL;
	uint32_t p;

	pg_list = s->pg_list;
	b = sb->b;

	sm->count = 0;

	for (p = 0; p < pg_list->pg_count; p++) {
		switch (pg_list->pages[p].id) {
		case PAGE_ID_ATA_SMART_READ_DATA:
			__smart_map_ata_read_data(sm, b, pg_list->pages[p].bytes);
			break;
		case PAGE_ID_ATA_SMART_RET_STATUS:
			__smart_map_ata_return_status(sm, b);
			break;
		}

		b += pg_list->pages[p].bytes;
	}
}

#ifndef ARRAYLEN
#define ARRAYLEN(p) sizeof(p)/sizeof(p[0])
#endif

#define NVME_VS(mjr,mnr,ter) (((mjr) << 16) | ((mnr) << 8) | (ter))
#define NVME_VS_1_0	NVME_VS(1,0,0)
#define NVME_VS_1_1	NVME_VS(1,1,0)
#define NVME_VS_1_2	NVME_VS(1,2,0)
#define NVME_VS_1_2_1	NVME_VS(1,2,1)
#define NVME_VS_1_3	NVME_VS(1,3,0)
#define NVME_VS_1_4	NVME_VS(1,4,0)
static struct {
	uint32_t off;		/* buffer offset */
	uint32_t bytes;		/* size in bytes */
	uint32_t ver;		/* first version available */
	const char *description;
} __smart_nvme_values[] = {
	{   0,  1, NVME_VS_1_0, "Critical Warning" },
	{   1,  2, NVME_VS_1_0, "Composite Temperature" },
	{   3,  1, NVME_VS_1_0, "Available Spare" },
	{   4,  1, NVME_VS_1_0, "Available Spare Threshold" },
	{   5,  1, NVME_VS_1_0, "Percentage Used" },
	{   6,  1, NVME_VS_1_4, "Endurance Group Critical Warning Summary" },
	{  32, 16, NVME_VS_1_0, "Data Units Read" },
	{  48, 16, NVME_VS_1_0, "Data Units Written" },
	{  64, 16, NVME_VS_1_0, "Host Read Commands" },
	{  80, 16, NVME_VS_1_0, "Host Write Commands" },
	{  96, 16, NVME_VS_1_0, "Controller Busy Time" },
	{ 112, 16, NVME_VS_1_0, "Power Cycles" },
	{ 128, 16, NVME_VS_1_0, "Power On Hours" },
	{ 144, 16, NVME_VS_1_0, "Unsafe Shutdowns" },
	{ 160, 16, NVME_VS_1_0, "Media and Data Integrity Errors" },
	{ 176, 16, NVME_VS_1_0, "Number of Error Information Log Entries" },
	{ 192,  4, NVME_VS_1_2, "Warning Composite Temperature Time" },
	{ 196,  4, NVME_VS_1_2, "Critical Composite Temperature Time" },
	{ 200,  2, NVME_VS_1_2, "Temperature Sensor 1" },
	{ 202,  2, NVME_VS_1_2, "Temperature Sensor 2" },
	{ 204,  2, NVME_VS_1_2, "Temperature Sensor 3" },
	{ 206,  2, NVME_VS_1_2, "Temperature Sensor 4" },
	{ 208,  2, NVME_VS_1_2, "Temperature Sensor 5" },
	{ 210,  2, NVME_VS_1_2, "Temperature Sensor 6" },
	{ 212,  2, NVME_VS_1_2, "Temperature Sensor 7" },
	{ 214,  2, NVME_VS_1_2, "Temperature Sensor 8" },
	{ 216,  4, NVME_VS_1_3, "Thermal Management Temperature 1 Transition Count" },
	{ 220,  4, NVME_VS_1_3, "Thermal Management Temperature 2 Transition Count" },
	{ 224,  4, NVME_VS_1_3, "Total Time For Thermal Management Temperature 1" },
	{ 228,  4, NVME_VS_1_3, "Total Time For Thermal Management Temperature 2" },
};

/**
 * NVMe doesn't define attribute IDs like ATA does, but we can
 * approximate this behavior by treating the byte offset as the
 * attribute ID.
 */
static void
__smart_map_nvme(smart_buf_t *sb, smart_map_t *sm)
{
	uint8_t *b = NULL;
	uint32_t vs = NVME_VS_1_0;	// XXX assume device is 1.0
	uint32_t i, a;

	sm->count = 0;
	b = sb->b;

	for (i = 0, a = 0; i < ARRAYLEN(__smart_nvme_values); i++) {
		if (vs >= __smart_nvme_values[i].ver) {
			sm->attr[a].page = 0x2;
			sm->attr[a].id = __smart_nvme_values[i].off;
			sm->attr[a].description = __smart_nvme_values[i].description;
			sm->attr[a].bytes = __smart_nvme_values[i].bytes;
			sm->attr[a].flags = 0;
			sm->attr[a].raw = b + __smart_nvme_values[i].off;
			sm->attr[a].thresh = NULL;

			a++;
		}
	}

	sm->count = a;
}

/*
 * Create a SMART map for SCSI error counter pages
 *
 * Several SCSI log pages have a similar format for the error counter log
 * pages
 */
static void
__smart_map_scsi_err_page(smart_map_t *sm, void *b)
{
	struct scsi_err_page {
		uint8_t page_code;
		uint8_t subpage_code;
		uint16_t page_length;
		uint8_t param[];
	} __attribute__((packed)) *err = b;
	struct scsi_err_counter_param {
		uint16_t	code;
		uint8_t		format:2,
				tmc:2,
				etc:1,
				tsd:1,
				:1,
				du:1;
		uint8_t		length;
		uint8_t		counter[];
	} __attribute__((packed)) *param = NULL;
	uint32_t a, p, page_length;
	const char *cmd = NULL, *desc = NULL;

	switch (err->page_code) {
	case PAGE_ID_SCSI_WRITE_ERR:
		cmd = "Write";
		break;
	case PAGE_ID_SCSI_READ_ERR:
		cmd = "Read";
		break;
	case PAGE_ID_SCSI_VERIFY_ERR:
		cmd = "Verify";
		break;
	case PAGE_ID_SCSI_NON_MEDIUM_ERR:
		cmd = "Non-Medium";
		break;
	default:
		fprintf(stderr, "Unknown command %#x\n", err->page_code);
		cmd = "Unknown";
		break;
	}

	a = sm->count;

	p = 0;
	page_length = be16toh(err->page_length);

	while (p < page_length) {
		param = (struct scsi_err_counter_param *) (err->param + p);

		sm->attr[a].page = err->page_code;
		sm->attr[a].id = be16toh(param->code);
		desc = __smart_scsi_err_desc(sm->attr[a].id);
		if (desc != NULL) {
			size_t bytes;
			char *str;

			bytes = snprintf(NULL, 0, "%s %s", cmd, desc);
			str = malloc(bytes + 1);
			if (str != NULL) {
				snprintf(str, bytes + 1, "%s %s", cmd, desc);
				sm->attr[a].description = str;
				sm->attr[a].flags |= SMART_ATTR_F_ALLOC;
			}
		}
		sm->attr[a].bytes = param->length;
		sm->attr[a].flags = SMART_ATTR_F_BE;
		sm->attr[a].raw = param->counter;
		sm->attr[a].thresh = NULL;

		p += 4 + param->length;

		a++;
	}

	sm->count = a;
}

static void
__smart_map_scsi_last_err(smart_map_t *sm, void *b)
{
	struct scsi_last_n_error_event_page {
		uint8_t page_code:6,
			spf:1,
			ds:1;
		uint8_t	subpage_code;
		uint16_t page_length;
		uint8_t event[];
	} __attribute__((packed)) *lastn = b;
	struct scsi_last_n_error_event {
		uint16_t	code;
		uint8_t		format:2,
				tmc:2,
				etc:1,
				tsd:1,
				:1,
				du:1;
		uint8_t		length;
		uint8_t		data[];
	} __attribute__((packed)) *event = NULL;
	uint32_t a, p, page_length;

	a = sm->count;

	p = 0;
	page_length = be16toh(lastn->page_length);

	while (p < page_length) {
		event = (struct scsi_last_n_error_event *) (lastn->event + p);

		sm->attr[a].page = lastn->page_code;
		sm->attr[a].id = be16toh(event->code);
		sm->attr[a].bytes = event->length;
		sm->attr[a].flags = SMART_ATTR_F_BE;
		sm->attr[a].raw = event->data;
		sm->attr[a].thresh = NULL;

		p += 4 + event->length;

		a++;
	}

	sm->count = a;
}

static void
__smart_map_scsi_temp(smart_map_t *sm, void *b)
{
	struct scsi_temperature_log_page {
		uint8_t page_code;
		uint8_t subpage_code;
		uint16_t page_length;
		struct scsi_temperature_log_entry {
			uint16_t code;
			uint8_t control;
			uint8_t length;
			uint8_t	rsvd;
			uint8_t temperature;
		} param[];
	} __attribute__((packed)) *temp = b;
	uint32_t a, p, count;

	count = be16toh(temp->page_length) / sizeof(struct scsi_temperature_log_entry);

	a = sm->count;

	for (p = 0; p < count; p++) {
		uint16_t code = be16toh(temp->param[p].code);
		switch (code) {
		case 0:
		case 1:
			sm->attr[a].page = temp->page_code;
			sm->attr[a].id = be16toh(temp->param[p].code);
			sm->attr[a].description = code == 0 ? "Temperature" : "Reference Temperature";
			sm->attr[a].bytes = 1;
			sm->attr[a].flags = 0;
			sm->attr[a].raw = &(temp->param[p].temperature);
			sm->attr[a].thresh = NULL;
			a++;
			break;
		default:
			break;
		}
	}

	sm->count = a;
}

static void
__smart_map_scsi_start_stop(smart_map_t *sm, void *b)
{
	struct scsi_start_stop_page {
		uint8_t page_code;
#define START_STOP_CODE_DATE_MFG	0x0001
#define START_STOP_CODE_DATE_ACCTN	0x0002
#define START_STOP_CODE_CYCLES_LIFE	0x0003
#define START_STOP_CODE_CYCLES_ACCUM	0x0004
#define START_STOP_CODE_LOAD_LIFE	0x0005
#define START_STOP_CODE_LOAD_ACCUM	0x0006
		uint8_t subpage_code;
		uint16_t page_length;
		uint8_t param[];
	} __attribute__((packed)) *sstop = b;
	struct scsi_start_stop_param {
		uint16_t code;
		uint8_t	format:2,
			tmc:2,
			etc:1,
			tsd:1,
			:1,
			du:1;
		uint8_t length;
		uint8_t data[];
	} __attribute__((packed)) *param;
	uint32_t a, p, page_length;

	a = sm->count;

	p = 0;
	page_length = be16toh(sstop->page_length);

	while (p < page_length) {
		param = (struct scsi_start_stop_param *) (sstop->param + p);

		sm->attr[a].page = sstop->page_code;
		sm->attr[a].id = be16toh(param->code);
		sm->attr[a].bytes = param->length;

		switch (sm->attr[a].id) {
		case START_STOP_CODE_DATE_MFG:
			sm->attr[a].description = "Date of Manufacture";
			sm->attr[a].flags = SMART_ATTR_F_STR;
			break;
		case START_STOP_CODE_DATE_ACCTN:
			sm->attr[a].description = "Accounting Date";
			sm->attr[a].flags = SMART_ATTR_F_STR;
			break;
		case START_STOP_CODE_CYCLES_LIFE:
			sm->attr[a].description = "Specified Cycle Count Over Device Lifetime";
			sm->attr[a].flags = SMART_ATTR_F_BE;
			break;
		case START_STOP_CODE_CYCLES_ACCUM:
			sm->attr[a].description = "Accumulated Start-Stop Cycles";
			sm->attr[a].flags = SMART_ATTR_F_BE;
			break;
		case START_STOP_CODE_LOAD_LIFE:
			sm->attr[a].description = "Specified Load-Unload Count Over Device Lifetime";
			sm->attr[a].flags = SMART_ATTR_F_BE;
			break;
		case START_STOP_CODE_LOAD_ACCUM:
			sm->attr[a].description = "Accumulated Load-Unload Cycles";
			sm->attr[a].flags = SMART_ATTR_F_BE;
			break;
		}

		sm->attr[a].raw = param->data;
		sm->attr[a].thresh = NULL;

		p += 4 + param->length;

		a++;
	}

	sm->count = a;
}

static void
__smart_map_scsi_info_exception(smart_map_t *sm, void *b)
{
	struct scsi_info_exception_log_page {
		uint8_t page_code;
		uint8_t subpage_code;
		uint16_t page_length;
		uint8_t param[];
	} __attribute__((packed)) *ie = b;
	struct scsi_ie_param {
		uint16_t code;
		uint8_t control;
		uint8_t length;
		uint8_t asc;	/* IE Additional Sense Code */
		uint8_t ascq;	/* IE Additional Sense Code Qualifier */
		uint8_t temp_recent;
		uint8_t temp_trip_point;
		uint8_t temp_max;
	} __attribute__((packed)) *param;
	uint32_t a, p, page_length;

	a = sm->count;

	p = 0;
	page_length = be16toh(ie->page_length);

	while (p < page_length) {
		param = (struct scsi_ie_param *)(ie->param + p);

		p += 4 + param->length;

		sm->attr[a].page = ie->page_code;
		sm->attr[a].id = offsetof(struct scsi_ie_param, asc);
		sm->attr[a].description = "Informational Exception ASC";
		sm->attr[a].bytes = 1;
		sm->attr[a].flags = 0;
		sm->attr[a].raw = &param->asc;
		sm->attr[a].thresh = NULL;
		a++;

		sm->attr[a].page = ie->page_code;
		sm->attr[a].id = offsetof(struct scsi_ie_param, ascq);
		sm->attr[a].description = "Informational Exception ASCQ";
		sm->attr[a].bytes = 1;
		sm->attr[a].flags = 0;
		sm->attr[a].raw = &param->ascq;
		sm->attr[a].thresh = NULL;
		a++;

		sm->attr[a].page = ie->page_code;
		sm->attr[a].id = offsetof(struct scsi_ie_param, temp_recent);
		sm->attr[a].description = "Informational Exception Most recent temperature";
		sm->attr[a].bytes = 1;
		sm->attr[a].flags = 0;
		sm->attr[a].raw = &param->temp_recent;
		sm->attr[a].thresh = NULL;
		a++;

		sm->attr[a].page = ie->page_code;
		sm->attr[a].id = offsetof(struct scsi_ie_param, temp_trip_point);
		sm->attr[a].description = "Informational Exception Vendor HDA temperature trip point";
		sm->attr[a].bytes = 1;
		sm->attr[a].flags = 0;
		sm->attr[a].raw = &param->temp_trip_point;
		sm->attr[a].thresh = NULL;
		a++;

		sm->attr[a].page = ie->page_code;
		sm->attr[a].id = offsetof(struct scsi_ie_param, temp_max);
		sm->attr[a].description = "Informational Exception Maximum temperature";
		sm->attr[a].bytes = 1;
		sm->attr[a].flags = 0;
		sm->attr[a].raw = &param->temp_max;
		sm->attr[a].thresh = NULL;
		a++;
	}

	sm->count = a;
}

/*
 * Create a map based on the page list
 */
static void
__smart_map_scsi(smart_h h, smart_buf_t *sb, smart_map_t *sm)
{
	smart_t *s = h;
	smart_page_list_t *pg_list = NULL;
	uint8_t *b = NULL;
	uint32_t p;

	pg_list = s->pg_list;
	b = sb->b;

	sm->count = 0;

	for (p = 0; p < pg_list->pg_count; p++) {
		switch (pg_list->pages[p].id) {
		case PAGE_ID_SCSI_WRITE_ERR:
		case PAGE_ID_SCSI_READ_ERR:
		case PAGE_ID_SCSI_VERIFY_ERR:
		case PAGE_ID_SCSI_NON_MEDIUM_ERR:
			__smart_map_scsi_err_page(sm, b);
			break;
		case PAGE_ID_SCSI_LAST_N_ERR:
			__smart_map_scsi_last_err(sm, b);
			break;
		case PAGE_ID_SCSI_TEMPERATURE:
			__smart_map_scsi_temp(sm, b);
			break;
		case PAGE_ID_SCSI_START_STOP_CYCLE:
			__smart_map_scsi_start_stop(sm, b);
			break;
		case PAGE_ID_SCSI_INFO_EXCEPTION:
			__smart_map_scsi_info_exception(sm, b);
			break;
		}

		b += pg_list->pages[p].bytes;
	}
}

/**
 * Create a map of SMART values
 */
static void
__smart_attribute_map(smart_h h, smart_buf_t *sb, smart_map_t *sm)
{

	if (!sb || !sm) {
		return;
	}

	switch (sb->protocol) {
	case SMART_PROTO_ATA:
		__smart_map_ata(h, sb, sm);
		break;
	case SMART_PROTO_NVME:
		__smart_map_nvme(sb, sm);
		break;
	case SMART_PROTO_SCSI:
		__smart_map_scsi(h, sb, sm);
		break;
	default:
		sm->count = 0;
	}
}

static smart_map_t *
__smart_map(smart_h h, smart_buf_t *sb)
{
	smart_map_t *sm = NULL;
	uint32_t max = 0;

	max = sb->attr_count;
	if (max == 0) {
		warnx("Attribute count is zero?!?");
		return NULL;
	}

	sm = malloc(sizeof(smart_map_t) + (max * sizeof(smart_attr_t)));
	if (sm) {
		memset(sm, 0, sizeof(smart_map_t) + (max * sizeof(smart_attr_t)));
		sm->sb = sb;

		/* count starts as the max but is adjusted to reflect the actual number */
		sm->count = max;

		__smart_attribute_map(h, sb, sm);
	}

	return sm;
}

typedef struct {
	uint8_t	page_code;
	uint8_t	subpage_code;
	uint16_t page_length;
	uint8_t supported_pages[];
} __attribute__((packed)) scsi_supported_log_pages;

static smart_page_list_t *
__smart_page_list_scsi(smart_t *s)
{
	smart_page_list_t *pg_list = NULL;
	scsi_supported_log_pages *b = NULL;
	uint32_t bsize = 68;	/* 4 byte header + 63 entries + 1 just cuz */
	int32_t rc;

	b = malloc(bsize);
	if (!b) {
		return NULL;
	}

	/* Supported Pages page ID is 0 */
	rc = device_read_log(s, PAGE_ID_SCSI_SUPPORTED_PAGES, (uint8_t *)b,
			bsize);
	if (rc < 0) {
		fprintf(stderr, "Read Supported Log Pages failed\n");
	} else {
		uint8_t *supported_page = b->supported_pages;
		uint32_t n_supported = be16toh(b->page_length);
		uint32_t pg, p, pmax = pg_list_scsi.pg_count;

		/* Build a page list using only pages the device supports */
		pg_list = malloc(sizeof(pg_list_scsi));
		if (pg_list == NULL) {
			n_supported = 0;
		} else {
			pg_list->pg_count = 0;
		}

		/*
		 * Loop through all supported pages looking for those related
		 * to SMART. The below assumes the supported page list from the
		 * device and in pg_lsit_scsi are sorted in increasing order.
		 */
		dprintf("Supported SCSI pages:\n");
		for (pg = 0, p = 0; (pg < n_supported) && (p < pmax); pg++) {
			dprintf("\t[%u] = %#x\n", pg, supported_page[pg]);
			while ((supported_page[pg] > pg_list_scsi.pages[p].id) &&
					(p < pmax)) {
				p++;
			}

			if (supported_page[pg] == pg_list_scsi.pages[p].id) {
				pg_list->pages[pg_list->pg_count] = pg_list_scsi.pages[p];
				pg_list->pg_count++;
				p++;
			}
		}
	}

	free(b);

	return pg_list;
}

static smart_page_list_t *
__smart_page_list(smart_h h)
{
	smart_t *s = h;
	smart_page_list_t *pg_list = NULL;

	if (!s) {
		return NULL;
	}

	switch (s->protocol) {
	case SMART_PROTO_ATA:
		pg_list = &pg_list_ata;
		break;
	case SMART_PROTO_NVME:
		pg_list = &pg_list_nvme;
		break;
	case SMART_PROTO_SCSI:
		pg_list = __smart_page_list_scsi(s);
		break;
	default:
		pg_list = NULL;
	}

	return pg_list;
}

static int32_t
__smart_read_pages(smart_h h, smart_buf_t *sb)
{
	smart_t *s = h;
	smart_page_list_t *plist = NULL;
	uint8_t *buf = NULL;
	int32_t rc = 0;
	uint32_t p = 0;

	plist = s->pg_list;

	buf = sb->b;

	for (p = 0; p < s->pg_list->pg_count; p++) {
		memset(buf, 0, plist->pages[p].bytes);
		rc = device_read_log(h, plist->pages[p].id, buf, plist->pages[p].bytes);
		if (rc) {
			dprintf("bad read (%d) from page %#x (bytes=%lu)\n", rc,
					plist->pages[p].id, plist->pages[p].bytes);
			break;
		}

		buf += plist->pages[p].bytes;
	}

	return rc;
}
