/*
 * Copyright (c) 2016-2021 Chuck Tuffli <chuck@tuffli.net>
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
#ifndef _LIBSMART_H
#define _LIBSMART_H

#include <inttypes.h>
#include <stdbool.h>

/*
 * libsmart uses a common model for SMART data (a.k.a. "attributes") across
 * storage protocols. Each health value consists of:
 *  - The identifier of the log page containing this attribute
 *  - The attribute's identifier
 *  - A description of the attribute
 *  - A pointer to the raw data
 *  - The attribute's size in bytes
 *
 * This model most closely resembles SCSI's native representation, but it
 * can represent ATA and NVMe with the following substitutions:
 *  - ATA  : use the Command Feature field value for the log page ID
 *  - NVMe : use the field's starting byte offset for the attribute ID
 *
 * libsmart returns a "map" to the SMART/health data read from a device
 * in the smart_map_t structure. The map consists of:
 *  - A variable-length array of attributes
 *  - The length of the array
 *  - The raw data read from the device
 *
 * Consumers of the map will typically iterate through the array of attributes
 * to print or otherwise process the health data.
 */

/*
 * A smart handle is an opaque reference to the device
 */
typedef void * smart_h;

typedef enum {
	SMART_PROTO_AUTO,
	SMART_PROTO_ATA,
	SMART_PROTO_SCSI,
	SMART_PROTO_NVME,
	SMART_PROTO_MAX
} smart_protocol_e;

/*
 * A smart buffer contains the raw data returned from the protocol-specific
 * health command.
 */
typedef struct {
	smart_protocol_e protocol;
	void *b;		// buffer of raw data
	size_t bsize;		// buffer size
	uint32_t attr_count;	// number of SMART attributes
} smart_buf_t;

struct smart_map_s;

/*
 * A smart attribute is an individual health data element
 */
typedef struct smart_attr_s {
	uint32_t page;
	uint32_t id;
	const char *description;			/* human readable description */
	uint32_t bytes;
	uint32_t flags;
#define SMART_ATTR_F_BE		0x00000001	/* Attribute is big-endian */
#define SMART_ATTR_F_STR	0x00000002	/* Attribute is a string */
#define SMART_ATTR_F_ALLOC	0x00000004	/* Attribute description dynamically allocated */
	void *raw;
	struct smart_map_s *thresh;		/* Threshold values (if any) */
} smart_attr_t;

/*
 * A smart map is the collection of health data elements from the device
 */
typedef struct smart_map_s {
	smart_buf_t *sb;
	uint32_t count;				/* Number of attributes */
	smart_attr_t attr[];			/* Array of attributes */
} smart_map_t;

#define SMART_OPEN_F_HEX	0x1		/* Print values in hexadecimal */
#define SMART_OPEN_F_THRESH	0x2		/* Print threshold values */
#define SMART_OPEN_F_DESCR	0x4		/* Print textual description */

/* SMART attribute to match */
typedef struct smart_match_s {
	int32_t page;
	int32_t id;
} smart_match_t;

/* List of SMART attribute(s) to match */
typedef struct smart_matches_s {
	uint32_t count;
	smart_match_t m[];
} smart_matches_t;

/**
 * Connect to a device to read SMART data
 *
 * @param p	   The desired protocol or "auto" to automatically detect it
 * @param devname  The device name to open
 *
 * @return An opaque handle or NULL on failure
 */
smart_h smart_open(smart_protocol_e p, char *devname);
/**
 * Close device connection
 *
 * @param handle The handle returned from smart_open()
 *
 * @return None
 */
void smart_close(smart_h);
/**
 * Does the device support SMART/health data?
 *
 * @param handle The handle returned from smart_open()
 *
 * @return true / false
 */
bool smart_supported(smart_h);
/**
 * Read SMART/health data from the device
 *
 * @param handle The handle returned from smart_open()
 *
 * @return a pointer to the SMART map or NULL on failure
 */
smart_map_t *smart_read(smart_h);
/**
 * Free memory associated with the health data read from the device
 *
 * @param map Pointer returned from smart_read()
 *
 * @return None
 */
void smart_free(smart_map_t *);
/**
 * Print health data matching the desired attributes
 *
 * @param handle The handle returned from smart_open()
 * @param map Pointer returned from smart_read()
 * @param which Pointer to attributes to match or NULL to match all
 * @param flags Control display of attributes (hexadecimal, description, ...
 *
 * @return None
 */
void smart_print(smart_h, smart_map_t *, smart_matches_t *, uint32_t);
/**
 * Print high-level device information
 *
 * @param handle The handle returned from smart_open()
 *
 * @return None
 */
void smart_print_device_info(smart_h);

#endif
