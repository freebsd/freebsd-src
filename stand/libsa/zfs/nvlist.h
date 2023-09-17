/*-
 * Copyright 2020 Toomas Soome <tsoome@me.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _BOOT_NVLIST_H_
#define	_BOOT_NVLIST_H_

typedef enum {
	DATA_TYPE_UNKNOWN = 0,
	DATA_TYPE_BOOLEAN,
	DATA_TYPE_BYTE,
	DATA_TYPE_INT16,
	DATA_TYPE_UINT16,
	DATA_TYPE_INT32,
	DATA_TYPE_UINT32,
	DATA_TYPE_INT64,
	DATA_TYPE_UINT64,
	DATA_TYPE_STRING,
	DATA_TYPE_BYTE_ARRAY,
	DATA_TYPE_INT16_ARRAY,
	DATA_TYPE_UINT16_ARRAY,
	DATA_TYPE_INT32_ARRAY,
	DATA_TYPE_UINT32_ARRAY,
	DATA_TYPE_INT64_ARRAY,
	DATA_TYPE_UINT64_ARRAY,
	DATA_TYPE_STRING_ARRAY,
	DATA_TYPE_HRTIME,
	DATA_TYPE_NVLIST,
	DATA_TYPE_NVLIST_ARRAY,
	DATA_TYPE_BOOLEAN_VALUE,
	DATA_TYPE_INT8,
	DATA_TYPE_UINT8,
	DATA_TYPE_BOOLEAN_ARRAY,
	DATA_TYPE_INT8_ARRAY,
	DATA_TYPE_UINT8_ARRAY
} data_type_t;

/* nvp implementation version */
#define	NV_VERSION		0

/* nvlist pack encoding */
#define	NV_ENCODE_NATIVE	0
#define	NV_ENCODE_XDR		1

/* nvlist persistent unique name flags, stored in nvl_nvflags */
#define	NV_UNIQUE_NAME		0x1
#define	NV_UNIQUE_NAME_TYPE	0x2

#define	NV_ALIGN4(x)		(((x) + 3) & ~3)
#define	NV_ALIGN(x)		(((x) + 7) & ~7)

/*
 * nvlist header.
 * nvlist has 4 bytes header followed by version and flags, then nvpairs
 * and the list is terminated by double zero.
 */
typedef struct {
	char nvh_encoding;
	char nvh_endian;
	char nvh_reserved1;
	char nvh_reserved2;
} nvs_header_t;

typedef struct {
	nvs_header_t nv_header;
	size_t nv_asize;
	size_t nv_size;
	uint8_t *nv_data;
	uint8_t *nv_idx;
} nvlist_t;

/*
 * nvpair header.
 * nvpair has encoded and decoded size
 * name string (size and data)
 * data type and number of elements
 * data
 */
typedef struct {
	unsigned encoded_size;
	unsigned decoded_size;
} nvp_header_t;

/*
 * nvlist stream head.
 */
typedef struct {
	unsigned nvl_version;
	unsigned nvl_nvflag;
	nvp_header_t nvl_pair;
} nvs_data_t;

typedef struct {
	unsigned nv_size;
	uint8_t nv_data[];	/* NV_ALIGN4(string) */
} nv_string_t;

typedef struct {
	unsigned nv_type;	/* data_type_t */
	unsigned nv_nelem;	/* number of elements */
	uint8_t nv_data[];	/* data stream */
} nv_pair_data_t;

nvlist_t *nvlist_create(int);
void nvlist_destroy(nvlist_t *);
nvlist_t *nvlist_import(const char *, size_t);
int nvlist_export(nvlist_t *);
int nvlist_remove(nvlist_t *, const char *, data_type_t);
int nvpair_type_from_name(const char *);
nvp_header_t *nvpair_find(nvlist_t *, const char *);
void nvpair_print(nvp_header_t *, unsigned int);
void nvlist_print(const nvlist_t *, unsigned int);
char *nvstring_get(nv_string_t *);
int nvlist_find(const nvlist_t *, const char *, data_type_t,
    int *, void *, int *);
nvp_header_t *nvlist_next_nvpair(nvlist_t *, nvp_header_t *);

int nvlist_add_boolean_value(nvlist_t *, const char *, int);
int nvlist_add_byte(nvlist_t *, const char *, uint8_t);
int nvlist_add_int8(nvlist_t *, const char *, int8_t);
int nvlist_add_uint8(nvlist_t *, const char *, uint8_t);
int nvlist_add_int16(nvlist_t *, const char *, int16_t);
int nvlist_add_uint16(nvlist_t *, const char *, uint16_t);
int nvlist_add_int32(nvlist_t *, const char *, int32_t);
int nvlist_add_uint32(nvlist_t *, const char *, uint32_t);
int nvlist_add_int64(nvlist_t *, const char *, int64_t);
int nvlist_add_uint64(nvlist_t *, const char *, uint64_t);
int nvlist_add_string(nvlist_t *, const char *, const char *);
int nvlist_add_boolean_array(nvlist_t *, const char *, int *, uint32_t);
int nvlist_add_byte_array(nvlist_t *, const char *, uint8_t *, uint32_t);
int nvlist_add_int8_array(nvlist_t *, const char *, int8_t *, uint32_t);
int nvlist_add_uint8_array(nvlist_t *, const char *, uint8_t *, uint32_t);
int nvlist_add_int16_array(nvlist_t *, const char *, int16_t *, uint32_t);
int nvlist_add_uint16_array(nvlist_t *, const char *, uint16_t *, uint32_t);
int nvlist_add_int32_array(nvlist_t *, const char *, int32_t *, uint32_t);
int nvlist_add_uint32_array(nvlist_t *, const char *, uint32_t *, uint32_t);
int nvlist_add_int64_array(nvlist_t *, const char *, int64_t *, uint32_t);
int nvlist_add_uint64_array(nvlist_t *, const char *, uint64_t *, uint32_t);
int nvlist_add_string_array(nvlist_t *, const char *, char * const *, uint32_t);
int nvlist_add_nvlist(nvlist_t *, const char *, nvlist_t *);
int nvlist_add_nvlist_array(nvlist_t *, const char *, nvlist_t **, uint32_t);

#endif /* !_BOOT_NVLIST_H_ */
