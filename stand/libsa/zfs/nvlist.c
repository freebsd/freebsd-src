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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/stdint.h>
#ifdef _STANDALONE
#include <stand.h>
#else
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "nvlist.h"

enum xdr_op {
	XDR_OP_ENCODE = 1,
	XDR_OP_DECODE = 2
};

typedef struct xdr {
	enum xdr_op xdr_op;
	int (*xdr_getint)(struct xdr *, int *);
	int (*xdr_putint)(struct xdr *, int);
	int (*xdr_getuint)(struct xdr *, unsigned *);
	int (*xdr_putuint)(struct xdr *, unsigned);
	const uint8_t *xdr_buf;
	uint8_t *xdr_idx;
	size_t xdr_buf_size;
} xdr_t;

static int nvlist_xdr_nvlist(xdr_t *, nvlist_t *);
static bool nvlist_size_xdr(xdr_t *, size_t *);
static bool nvlist_size_native(xdr_t *, size_t *);
static bool xdr_int(xdr_t *, int *);
static bool xdr_u_int(xdr_t *, unsigned *);

typedef bool (*xdrproc_t)(xdr_t *, void *);

/* Basic primitives for XDR translation operations, getint and putint. */
static int
_getint(struct xdr *xdr, int *ip)
{
	*ip = be32dec(xdr->xdr_idx);
	return (sizeof(int));
}

static int
_putint(struct xdr *xdr, int i)
{
	int *ip = (int *)xdr->xdr_idx;

	*ip = htobe32(i);
	return (sizeof(int));
}

static int
_getuint(struct xdr *xdr, unsigned *ip)
{
	*ip = be32dec(xdr->xdr_idx);
	return (sizeof(unsigned));
}

static int
_putuint(struct xdr *xdr, unsigned i)
{
	unsigned *up = (unsigned *)xdr->xdr_idx;

	*up = htobe32(i);
	return (sizeof(int));
}

static int
_getint_mem(struct xdr *xdr, int *ip)
{
	*ip = *(int *)xdr->xdr_idx;
	return (sizeof(int));
}

static int
_putint_mem(struct xdr *xdr, int i)
{
	int *ip = (int *)xdr->xdr_idx;

	*ip = i;
	return (sizeof(int));
}

static int
_getuint_mem(struct xdr *xdr, unsigned *ip)
{
	*ip = *(unsigned *)xdr->xdr_idx;
	return (sizeof(unsigned));
}

static int
_putuint_mem(struct xdr *xdr, unsigned i)
{
	unsigned *up = (unsigned *)xdr->xdr_idx;

	*up = i;
	return (sizeof(int));
}

/*
 * XDR data translations.
 */
static bool
xdr_short(xdr_t *xdr, short *ip)
{
	int i;
	bool rv;

	i = *ip;
	if ((rv = xdr_int(xdr, &i))) {
		if (xdr->xdr_op == XDR_OP_DECODE)
			*ip = i;
	}
	return (rv);
}

static bool
xdr_u_short(xdr_t *xdr, unsigned short *ip)
{
	unsigned u;
	bool rv;

	u = *ip;
	if ((rv = xdr_u_int(xdr, &u))) {
		if (xdr->xdr_op == XDR_OP_DECODE)
			*ip = u;
	}
	return (rv);
}

/*
 * translate xdr->xdr_idx, increment it by size of int.
 */
static bool
xdr_int(xdr_t *xdr, int *ip)
{
	bool rv = false;
	int *i = (int *)xdr->xdr_idx;

	if (xdr->xdr_idx + sizeof(int) > xdr->xdr_buf + xdr->xdr_buf_size)
		return (rv);

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		xdr->xdr_idx += xdr->xdr_putint(xdr, *ip);
		rv = true;
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		xdr->xdr_idx += xdr->xdr_getint(xdr, i);
		*ip = *i;
		rv = true;
		break;
	}
	return (rv);
}

/*
 * translate xdr->xdr_idx, increment it by size of unsigned int.
 */
static bool
xdr_u_int(xdr_t *xdr, unsigned *ip)
{
	bool rv = false;
	unsigned *u = (unsigned *)xdr->xdr_idx;

	if (xdr->xdr_idx + sizeof(unsigned) > xdr->xdr_buf + xdr->xdr_buf_size)
		return (rv);

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		xdr->xdr_idx += xdr->xdr_putuint(xdr, *ip);
		rv = true;
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		xdr->xdr_idx += xdr->xdr_getuint(xdr, u);
		*ip = *u;
		rv = true;
		break;
	}
	return (rv);
}

static bool
xdr_int64(xdr_t *xdr, int64_t *lp)
{
	bool rv = false;

	if (xdr->xdr_idx + sizeof(int64_t) > xdr->xdr_buf + xdr->xdr_buf_size)
		return (rv);

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *lp, store to buf */
		if (xdr->xdr_putint == _putint)
			*(int64_t *)xdr->xdr_idx = htobe64(*lp);
		else
			*(int64_t *)xdr->xdr_idx = *lp;
		xdr->xdr_idx += sizeof(int64_t);
		rv = true;
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		if (xdr->xdr_getint == _getint)
			*lp = be64toh(*(int64_t *)xdr->xdr_idx);
		else
			*lp = *(int64_t *)xdr->xdr_idx;
		xdr->xdr_idx += sizeof(int64_t);
		rv = true;
	}
	return (rv);
}

static bool
xdr_uint64(xdr_t *xdr, uint64_t *lp)
{
	bool rv = false;

	if (xdr->xdr_idx + sizeof(uint64_t) > xdr->xdr_buf + xdr->xdr_buf_size)
		return (rv);

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		/* Encode value *ip, store to buf */
		if (xdr->xdr_putint == _putint)
			*(uint64_t *)xdr->xdr_idx = htobe64(*lp);
		else
			*(uint64_t *)xdr->xdr_idx = *lp;
		xdr->xdr_idx += sizeof(uint64_t);
		rv = true;
		break;

	case XDR_OP_DECODE:
		/* Decode buf, return value to *ip */
		if (xdr->xdr_getuint == _getuint)
			*lp = be64toh(*(uint64_t *)xdr->xdr_idx);
		else
			*lp = *(uint64_t *)xdr->xdr_idx;
		xdr->xdr_idx += sizeof(uint64_t);
		rv = true;
	}
	return (rv);
}

static bool
xdr_char(xdr_t *xdr, char *cp)
{
	int i;
	bool rv = false;

	i = *cp;
	if ((rv = xdr_int(xdr, &i))) {
		if (xdr->xdr_op == XDR_OP_DECODE)
			*cp = i;
	}
	return (rv);
}

static bool
xdr_string(xdr_t *xdr, nv_string_t *s)
{
	int size = 0;
	bool rv = false;

	switch (xdr->xdr_op) {
	case XDR_OP_ENCODE:
		size = s->nv_size;
		if (xdr->xdr_idx + sizeof(unsigned) + NV_ALIGN4(size) >
		    xdr->xdr_buf + xdr->xdr_buf_size)
			break;
		xdr->xdr_idx += xdr->xdr_putuint(xdr, s->nv_size);
		xdr->xdr_idx += NV_ALIGN4(size);
		rv = true;
		break;

	case XDR_OP_DECODE:
		if (xdr->xdr_idx + sizeof(unsigned) >
		    xdr->xdr_buf + xdr->xdr_buf_size)
			break;
		size = xdr->xdr_getuint(xdr, &s->nv_size);
		size = NV_ALIGN4(size + s->nv_size);
		if (xdr->xdr_idx + size > xdr->xdr_buf + xdr->xdr_buf_size)
			break;
		xdr->xdr_idx += size;
		rv = true;
		break;
	}
	return (rv);
}

static bool
xdr_array(xdr_t *xdr, const unsigned nelem, const xdrproc_t elproc)
{
	bool rv = true;
	unsigned c = nelem;

	if (!xdr_u_int(xdr, &c))
		return (false);

	for (unsigned i = 0; i < nelem; i++) {
		if (!elproc(xdr, xdr->xdr_idx))
			return (false);
	}
	return (rv);
}

/*
 * nvlist management functions.
 */
void
nvlist_destroy(nvlist_t *nvl)
{
	if (nvl != NULL) {
		/* Free data if it was allocated by us. */
		if (nvl->nv_asize > 0)
			free(nvl->nv_data);
	}
	free(nvl);
}

char *
nvstring_get(nv_string_t *nvs)
{
	char *s;

	s = malloc(nvs->nv_size + 1);
	if (s != NULL) {
		bcopy(nvs->nv_data, s, nvs->nv_size);
		s[nvs->nv_size] = '\0';
	}
	return (s);
}

/*
 * Create empty nvlist.
 * The nvlist is terminated by 2x zeros (8 bytes).
 */
nvlist_t *
nvlist_create(int flag)
{
	nvlist_t *nvl;
	nvs_data_t *nvs;

	nvl = calloc(1, sizeof(*nvl));
	if (nvl == NULL)
		return (nvl);

	nvl->nv_header.nvh_encoding = NV_ENCODE_XDR;
	nvl->nv_header.nvh_endian = _BYTE_ORDER == _LITTLE_ENDIAN;

	nvl->nv_asize = nvl->nv_size = sizeof(*nvs);
	nvs = calloc(1, nvl->nv_asize);
	if (nvs == NULL) {
		free(nvl);
		return (NULL);
	}
	/* data in nvlist is byte stream */
	nvl->nv_data = (uint8_t *)nvs;

	nvs->nvl_version = NV_VERSION;
	nvs->nvl_nvflag = flag;
	return (nvl);
}

static bool
nvlist_xdr_nvp(xdr_t *xdr, nvlist_t *nvl)
{
	nv_string_t *nv_string;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;
	unsigned type, nelem;
	xdr_t nv_xdr;

	nv_string = (nv_string_t *)xdr->xdr_idx;
	if (!xdr_string(xdr, nv_string)) {
		return (false);
	}
	nvp_data = (nv_pair_data_t *)xdr->xdr_idx;

	type = nvp_data->nv_type;
	nelem = nvp_data->nv_nelem;
	if (!xdr_u_int(xdr, &type) || !xdr_u_int(xdr, &nelem))
		return (false);

	switch (type) {
	case DATA_TYPE_NVLIST:
	case DATA_TYPE_NVLIST_ARRAY:
		bzero(&nvlist, sizeof(nvlist));
		nvlist.nv_data = xdr->xdr_idx;
		nvlist.nv_idx = nvlist.nv_data;

		/* Set up xdr for this nvlist. */
		nv_xdr = *xdr;
		nv_xdr.xdr_buf = nvlist.nv_data;
		nv_xdr.xdr_idx = nvlist.nv_data;
		nv_xdr.xdr_buf_size =
		    nvl->nv_data + nvl->nv_size - nvlist.nv_data;

		for (unsigned i = 0; i < nelem; i++) {
			if (xdr->xdr_op == XDR_OP_ENCODE) {
				if (!nvlist_size_native(&nv_xdr,
				    &nvlist.nv_size))
					return (false);
			} else {
				if (!nvlist_size_xdr(&nv_xdr,
				    &nvlist.nv_size))
					return (false);
			}
			if (nvlist_xdr_nvlist(xdr, &nvlist) != 0)
				return (false);

			nvlist.nv_data = nv_xdr.xdr_idx;
			nvlist.nv_idx = nv_xdr.xdr_idx;

			nv_xdr.xdr_buf = nv_xdr.xdr_idx;
			nv_xdr.xdr_buf_size =
			    nvl->nv_data + nvl->nv_size - nvlist.nv_data;
		}
		break;

	case DATA_TYPE_BOOLEAN:
		/* BOOLEAN does not take value space */
		break;
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
		if (!xdr_char(xdr, (char *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_INT16:
		if (!xdr_short(xdr, (short *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_UINT16:
		if (!xdr_u_short(xdr, (unsigned short *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_INT32:
		if (!xdr_int(xdr, (int *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_UINT32:
		if (!xdr_u_int(xdr, (unsigned *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_HRTIME:
	case DATA_TYPE_INT64:
		if (!xdr_int64(xdr, (int64_t *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_UINT64:
		if (!xdr_uint64(xdr, (uint64_t *)&nvp_data->nv_data[0]))
			return (false);
		break;

	case DATA_TYPE_BYTE_ARRAY:
	case DATA_TYPE_STRING:
		nv_string = (nv_string_t *)&nvp_data->nv_data[0];
		if (!xdr_string(xdr, nv_string))
			return (false);
		break;

	case DATA_TYPE_STRING_ARRAY:
		nv_string = (nv_string_t *)&nvp_data->nv_data[0];
		for (unsigned i = 0; i < nelem; i++) {
			if (!xdr_string(xdr, nv_string))
				return (false);
			nv_string = (nv_string_t *)xdr->xdr_idx;
		}
		break;

	case DATA_TYPE_INT8_ARRAY:
	case DATA_TYPE_UINT8_ARRAY:
	case DATA_TYPE_INT16_ARRAY:
	case DATA_TYPE_UINT16_ARRAY:
	case DATA_TYPE_BOOLEAN_ARRAY:
	case DATA_TYPE_INT32_ARRAY:
	case DATA_TYPE_UINT32_ARRAY:
		if (!xdr_array(xdr, nelem, (xdrproc_t)xdr_u_int))
			return (false);
		break;

	case DATA_TYPE_INT64_ARRAY:
	case DATA_TYPE_UINT64_ARRAY:
		if (!xdr_array(xdr, nelem, (xdrproc_t)xdr_uint64))
			return (false);
		break;
	}
	return (true);
}

static int
nvlist_xdr_nvlist(xdr_t *xdr, nvlist_t *nvl)
{
	nvp_header_t *nvph;
	nvs_data_t *nvs;
	unsigned encoded_size, decoded_size;
	int rv;

	nvs = (nvs_data_t *)xdr->xdr_idx;
	nvph = &nvs->nvl_pair;

	if (!xdr_u_int(xdr, &nvs->nvl_version))
		return (EINVAL);
	if (!xdr_u_int(xdr, &nvs->nvl_nvflag))
		return (EINVAL);

	encoded_size = nvph->encoded_size;
	decoded_size = nvph->decoded_size;

	if (xdr->xdr_op == XDR_OP_ENCODE) {
		if (!xdr_u_int(xdr, &nvph->encoded_size))
			return (EINVAL);
		if (!xdr_u_int(xdr, &nvph->decoded_size))
			return (EINVAL);
	} else {
		xdr->xdr_idx += 2 * sizeof(unsigned);
	}

	rv = 0;
	while (encoded_size && decoded_size) {
		if (!nvlist_xdr_nvp(xdr, nvl))
			return (EINVAL);

		nvph = (nvp_header_t *)(xdr->xdr_idx);
		encoded_size = nvph->encoded_size;
		decoded_size = nvph->decoded_size;
		if (xdr->xdr_op == XDR_OP_ENCODE) {
			if (!xdr_u_int(xdr, &nvph->encoded_size))
				return (EINVAL);
			if (!xdr_u_int(xdr, &nvph->decoded_size))
				return (EINVAL);
		} else {
			xdr->xdr_idx += 2 * sizeof(unsigned);
		}
	}
	return (rv);
}

/*
 * Calculate nvlist size, translating encoded_size and decoded_size.
 */
static bool
nvlist_size_xdr(xdr_t *xdr, size_t *size)
{
	uint8_t *pair;
	unsigned encoded_size, decoded_size;

	xdr->xdr_idx += 2 * sizeof(unsigned);

	pair = xdr->xdr_idx;
	if (!xdr_u_int(xdr, &encoded_size) || !xdr_u_int(xdr, &decoded_size))
		return (false);

	while (encoded_size && decoded_size) {
		xdr->xdr_idx = pair + encoded_size;
		pair = xdr->xdr_idx;
		if (!xdr_u_int(xdr, &encoded_size) ||
		    !xdr_u_int(xdr, &decoded_size))
			return (false);
	}
	*size = xdr->xdr_idx - xdr->xdr_buf;

	return (true);
}

nvp_header_t *
nvlist_next_nvpair(nvlist_t *nvl, nvp_header_t *nvh)
{
	uint8_t *pair;
	unsigned encoded_size, decoded_size;
	xdr_t xdr;

	if (nvl == NULL)
		return (NULL);

	xdr.xdr_buf = nvl->nv_data;
	xdr.xdr_idx = nvl->nv_data;
	xdr.xdr_buf_size = nvl->nv_size;

	xdr.xdr_idx += 2 * sizeof(unsigned);

	/* Skip tp current pair */
	if (nvh != NULL) {
		xdr.xdr_idx = (uint8_t *)nvh;
	}

	pair = xdr.xdr_idx;
	if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
		return (NULL);

	encoded_size = *(unsigned *)xdr.xdr_idx;
	xdr.xdr_idx += sizeof(unsigned);
	if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
		return (NULL);

	decoded_size = *(unsigned *)xdr.xdr_idx;
	xdr.xdr_idx += sizeof(unsigned);
	if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
		return (NULL);

	while (encoded_size && decoded_size) {
		if (nvh == NULL)
			return ((nvp_header_t *)pair);

		xdr.xdr_idx = pair + encoded_size;
		nvh = (nvp_header_t *)xdr.xdr_idx;

		if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
			return (NULL);

		encoded_size = *(unsigned *)xdr.xdr_idx;
		xdr.xdr_idx += sizeof(unsigned);
		if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
			return (NULL);
		decoded_size = *(unsigned *)xdr.xdr_idx;
		xdr.xdr_idx += sizeof(unsigned);
		if (xdr.xdr_idx > xdr.xdr_buf + xdr.xdr_buf_size)
			return (NULL);

		if (encoded_size != 0 && decoded_size != 0) {
			return (nvh);
		}
	}
	return (NULL);
}

/*
 * Calculate nvlist size by walking in memory data.
 */
static bool
nvlist_size_native(xdr_t *xdr, size_t *size)
{
	uint8_t *pair;
	unsigned encoded_size, decoded_size;

	xdr->xdr_idx += 2 * sizeof(unsigned);

	pair = xdr->xdr_idx;
	if (xdr->xdr_idx > xdr->xdr_buf + xdr->xdr_buf_size)
		return (false);

	encoded_size = *(unsigned *)xdr->xdr_idx;
	xdr->xdr_idx += sizeof(unsigned);
	if (xdr->xdr_idx > xdr->xdr_buf + xdr->xdr_buf_size)
		return (false);
	decoded_size = *(unsigned *)xdr->xdr_idx;
	xdr->xdr_idx += sizeof(unsigned);
	while (encoded_size && decoded_size) {
		xdr->xdr_idx = pair + encoded_size;
		pair = xdr->xdr_idx;
		if (xdr->xdr_idx > xdr->xdr_buf + xdr->xdr_buf_size)
			return (false);
		encoded_size = *(unsigned *)xdr->xdr_idx;
		xdr->xdr_idx += sizeof(unsigned);
		if (xdr->xdr_idx > xdr->xdr_buf + xdr->xdr_buf_size)
			return (false);
		decoded_size = *(unsigned *)xdr->xdr_idx;
		xdr->xdr_idx += sizeof(unsigned);
	}
	*size = xdr->xdr_idx - xdr->xdr_buf;

	return (true);
}

/*
 * Export nvlist to byte stream format.
 */
int
nvlist_export(nvlist_t *nvl)
{
	int rv;
	xdr_t xdr = {
		.xdr_op = XDR_OP_ENCODE,
		.xdr_putint = _putint,
		.xdr_putuint = _putuint,
		.xdr_buf = nvl->nv_data,
		.xdr_idx = nvl->nv_data,
		.xdr_buf_size = nvl->nv_size
	};

	if (nvl->nv_header.nvh_encoding != NV_ENCODE_XDR)
		return (ENOTSUP);

	nvl->nv_idx = nvl->nv_data;
	rv = nvlist_xdr_nvlist(&xdr, nvl);

	return (rv);
}

/*
 * Import nvlist from byte stream.
 * Determine the stream size and allocate private copy.
 * Then translate the data.
 */
nvlist_t *
nvlist_import(const char *stream, size_t size)
{
	nvlist_t *nvl;
	xdr_t xdr = {
		.xdr_op = XDR_OP_DECODE,
		.xdr_getint = _getint,
		.xdr_getuint = _getuint
	};

	/* Check the nvlist head. */
	if (stream[0] != NV_ENCODE_XDR ||
	    (stream[1] != '\0' && stream[1] != '\1') ||
	    stream[2] != '\0' || stream[3] != '\0' ||
	    be32toh(*(uint32_t *)(stream + 4)) != NV_VERSION ||
	    be32toh(*(uint32_t *)(stream + 8)) != NV_UNIQUE_NAME)
		return (NULL);

	nvl = malloc(sizeof(*nvl));
	if (nvl == NULL)
		return (nvl);

	nvl->nv_header.nvh_encoding = stream[0];
	nvl->nv_header.nvh_endian = stream[1];
	nvl->nv_header.nvh_reserved1 = stream[2];
	nvl->nv_header.nvh_reserved2 = stream[3];

	xdr.xdr_buf = xdr.xdr_idx = (uint8_t *)stream + 4;
	xdr.xdr_buf_size = size - 4;

	if (!nvlist_size_xdr(&xdr, &nvl->nv_asize)) {
		free(nvl);
		return (NULL);
	}
	nvl->nv_size = nvl->nv_asize;
	nvl->nv_data = malloc(nvl->nv_asize);
	if (nvl->nv_data == NULL) {
		free(nvl);
		return (NULL);
	}
	nvl->nv_idx = nvl->nv_data;
	bcopy(stream + 4, nvl->nv_data, nvl->nv_asize);

	xdr.xdr_buf = xdr.xdr_idx = nvl->nv_data;
	xdr.xdr_buf_size = nvl->nv_asize;

	if (nvlist_xdr_nvlist(&xdr, nvl) != 0) {
		free(nvl->nv_data);
		free(nvl);
		nvl = NULL;
	}

	return (nvl);
}

/*
 * remove pair from this nvlist.
 */
int
nvlist_remove(nvlist_t *nvl, const char *name, data_type_t type)
{
	uint8_t *head, *tail;
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	size_t size;
	xdr_t xdr;

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	/* Make sure the nvlist size is set correct */
	xdr.xdr_idx = nvl->nv_data;
	xdr.xdr_buf = xdr.xdr_idx;
	xdr.xdr_buf_size = nvl->nv_size;
	if (!nvlist_size_native(&xdr, &nvl->nv_size))
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */
	head = (uint8_t *)nvp;

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)(nvp + 1);

		nvp_data = (nv_pair_data_t *)(&nvp_name->nv_data[0] +
		    NV_ALIGN4(nvp_name->nv_size));

		if (strlen(name) == nvp_name->nv_size &&
		    memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    (nvp_data->nv_type == type || type == DATA_TYPE_UNKNOWN)) {
			/*
			 * set tail to point to next nvpair and size
			 * is the length of the tail.
			 */
			tail = head + nvp->encoded_size;
			size = nvl->nv_size - (tail - nvl->nv_data);

			/* adjust the size of the nvlist. */
			nvl->nv_size -= nvp->encoded_size;
			bcopy(tail, head, size);
			return (0);
		}
		/* Not our pair, skip to next. */
		head = head + nvp->encoded_size;
		nvp = (nvp_header_t *)head;
	}
	return (ENOENT);
}

static int
clone_nvlist(const nvlist_t *nvl, const uint8_t *ptr, unsigned size,
    nvlist_t **nvlist)
{
	nvlist_t *nv;

	nv = calloc(1, sizeof(*nv));
	if (nv == NULL)
		return (ENOMEM);

	nv->nv_header = nvl->nv_header;
	nv->nv_asize = size;
	nv->nv_size = size;
	nv->nv_data = malloc(nv->nv_asize);
	if (nv->nv_data == NULL) {
		free(nv);
		return (ENOMEM);
	}

	bcopy(ptr, nv->nv_data, nv->nv_asize);
	*nvlist = nv;
	return (0);
}

/*
 * Return the next nvlist in an nvlist array.
 */
static uint8_t *
nvlist_next(const uint8_t *ptr)
{
	nvs_data_t *data;
	nvp_header_t *nvp;

	data = (nvs_data_t *)ptr;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	return ((uint8_t *)nvp + sizeof(*nvp));
}

/*
 * Note: nvlist and nvlist array must be freed by caller.
 */
int
nvlist_find(const nvlist_t *nvl, const char *name, data_type_t type,
    int *elementsp, void *valuep, int *sizep)
{
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t **nvlist, *nv;
	uint8_t *ptr;
	int rv;

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)((uint8_t *)nvp + sizeof(*nvp));
		if (nvl->nv_data + nvl->nv_size <
		    nvp_name->nv_data + nvp_name->nv_size)
			return (EIO);

		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		if (strlen(name) == nvp_name->nv_size &&
		    memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    (nvp_data->nv_type == type || type == DATA_TYPE_UNKNOWN)) {
			if (elementsp != NULL)
				*elementsp = nvp_data->nv_nelem;
			switch (nvp_data->nv_type) {
			case DATA_TYPE_UINT64:
				bcopy(nvp_data->nv_data, valuep,
				    sizeof(uint64_t));
				return (0);
			case DATA_TYPE_STRING:
				nvp_name = (nv_string_t *)nvp_data->nv_data;
				if (sizep != NULL) {
					*sizep = nvp_name->nv_size;
				}
				*(const uint8_t **)valuep =
				    &nvp_name->nv_data[0];
				return (0);
			case DATA_TYPE_NVLIST:
				ptr = &nvp_data->nv_data[0];
				rv = clone_nvlist(nvl, ptr,
				    nvlist_next(ptr) - ptr, &nv);
				if (rv == 0) {
					*(nvlist_t **)valuep = nv;
				}
				return (rv);

			case DATA_TYPE_NVLIST_ARRAY:
				nvlist = calloc(nvp_data->nv_nelem,
				    sizeof(nvlist_t *));
				if (nvlist == NULL)
					return (ENOMEM);
				ptr = &nvp_data->nv_data[0];
				rv = 0;
				for (unsigned i = 0; i < nvp_data->nv_nelem;
				    i++) {
					rv = clone_nvlist(nvl, ptr,
					    nvlist_next(ptr) - ptr, &nvlist[i]);
					if (rv != 0)
						goto error;
					ptr = nvlist_next(ptr);
				}
				*(nvlist_t ***)valuep = nvlist;
				return (rv);
			}
			return (EIO);
		}
		/* Not our pair, skip to next. */
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
		if (nvl->nv_data + nvl->nv_size < (uint8_t *)nvp)
			return (EIO);
	}
	return (ENOENT);
error:
	for (unsigned i = 0; i < nvp_data->nv_nelem; i++) {
		free(nvlist[i]->nv_data);
		free(nvlist[i]);
	}
	free(nvlist);
	return (rv);
}

static int
get_value_size(data_type_t type, const void *data, uint32_t nelem)
{
	uint64_t value_sz = 0;

	switch (type) {
	case DATA_TYPE_BOOLEAN:
		value_sz = 0;
		break;
	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
	case DATA_TYPE_INT16:
	case DATA_TYPE_UINT16:
	case DATA_TYPE_INT32:
	case DATA_TYPE_UINT32:
		/* Our smallest data unit is 32-bit */
		value_sz = sizeof(uint32_t);
		break;
	case DATA_TYPE_HRTIME:
	case DATA_TYPE_INT64:
		value_sz = sizeof(int64_t);
		break;
	case DATA_TYPE_UINT64:
		value_sz = sizeof(uint64_t);
		break;
	case DATA_TYPE_STRING:
		if (data == NULL)
			value_sz = 0;
		else
			value_sz = strlen(data) + 1;
		break;
	case DATA_TYPE_BYTE_ARRAY:
		value_sz = nelem * sizeof(uint8_t);
		break;
	case DATA_TYPE_BOOLEAN_ARRAY:
	case DATA_TYPE_INT8_ARRAY:
	case DATA_TYPE_UINT8_ARRAY:
	case DATA_TYPE_INT16_ARRAY:
	case DATA_TYPE_UINT16_ARRAY:
	case DATA_TYPE_INT32_ARRAY:
	case DATA_TYPE_UINT32_ARRAY:
		value_sz = (uint64_t)nelem * sizeof(uint32_t);
		break;
	case DATA_TYPE_INT64_ARRAY:
		value_sz = (uint64_t)nelem * sizeof(int64_t);
		break;
	case DATA_TYPE_UINT64_ARRAY:
		value_sz = (uint64_t)nelem * sizeof(uint64_t);
		break;
	case DATA_TYPE_STRING_ARRAY:
		value_sz = (uint64_t)nelem * sizeof(uint64_t);

		if (data != NULL) {
			char *const *strs = data;
			uint32_t i;

			for (i = 0; i < nelem; i++) {
				if (strs[i] == NULL)
					return (-1);
				value_sz += strlen(strs[i]) + 1;
			}
		}
		break;
	case DATA_TYPE_NVLIST:
		/*
		 * The decoded size of nvlist is constant.
		 */
		value_sz = NV_ALIGN(6 * 4); /* sizeof nvlist_t */
		break;
	case DATA_TYPE_NVLIST_ARRAY:
		value_sz = (uint64_t)nelem * sizeof(uint64_t) +
		    (uint64_t)nelem * NV_ALIGN(6 * 4); /* sizeof nvlist_t */
		break;
	default:
		return (-1);
	}

	return (value_sz > INT32_MAX ? -1 : (int)value_sz);
}

static int
get_nvp_data_size(data_type_t type, const void *data, uint32_t nelem)
{
	uint64_t value_sz = 0;
	xdr_t xdr;
	size_t size;

	switch (type) {
	case DATA_TYPE_BOOLEAN:
		value_sz = 0;
		break;
	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
	case DATA_TYPE_INT16:
	case DATA_TYPE_UINT16:
	case DATA_TYPE_INT32:
	case DATA_TYPE_UINT32:
		/* Our smallest data unit is 32-bit */
		value_sz = sizeof(uint32_t);
		break;
	case DATA_TYPE_HRTIME:
	case DATA_TYPE_INT64:
	case DATA_TYPE_UINT64:
		value_sz = sizeof(uint64_t);
		break;
	case DATA_TYPE_STRING:
		value_sz = 4 + NV_ALIGN4(strlen(data));
		break;
	case DATA_TYPE_BYTE_ARRAY:
		value_sz = NV_ALIGN4(nelem);
		break;
	case DATA_TYPE_BOOLEAN_ARRAY:
	case DATA_TYPE_INT8_ARRAY:
	case DATA_TYPE_UINT8_ARRAY:
	case DATA_TYPE_INT16_ARRAY:
	case DATA_TYPE_UINT16_ARRAY:
	case DATA_TYPE_INT32_ARRAY:
	case DATA_TYPE_UINT32_ARRAY:
		value_sz = 4 + (uint64_t)nelem * sizeof(uint32_t);
		break;
	case DATA_TYPE_INT64_ARRAY:
	case DATA_TYPE_UINT64_ARRAY:
		value_sz = 4 + (uint64_t)nelem * sizeof(uint64_t);
		break;
	case DATA_TYPE_STRING_ARRAY:
		if (data != NULL) {
			char *const *strs = data;
			uint32_t i;

			for (i = 0; i < nelem; i++) {
				value_sz += 4 + NV_ALIGN4(strlen(strs[i]));
			}
		}
		break;
	case DATA_TYPE_NVLIST:
		xdr.xdr_idx = ((nvlist_t *)data)->nv_data;
		xdr.xdr_buf = xdr.xdr_idx;
		xdr.xdr_buf_size = ((nvlist_t *)data)->nv_size;

		if (!nvlist_size_native(&xdr, &size))
			return (-1);

		value_sz = size;
		break;
	case DATA_TYPE_NVLIST_ARRAY:
		value_sz = 0;
		for (uint32_t i = 0; i < nelem; i++) {
			xdr.xdr_idx = ((nvlist_t **)data)[i]->nv_data;
			xdr.xdr_buf = xdr.xdr_idx;
			xdr.xdr_buf_size = ((nvlist_t **)data)[i]->nv_size;

			if (!nvlist_size_native(&xdr, &size))
				return (-1);
			value_sz += size;
		}
		break;
	default:
		return (-1);
	}

	return (value_sz > INT32_MAX ? -1 : (int)value_sz);
}

#define	NVPE_SIZE(name_len, data_len) \
	(4 + 4 + 4 + NV_ALIGN4(name_len) + 4 + 4 + data_len)
#define	NVP_SIZE(name_len, data_len) \
	(NV_ALIGN((4 * 4) + (name_len)) + NV_ALIGN(data_len))

static int
nvlist_add_common(nvlist_t *nvl, const char *name, data_type_t type,
    uint32_t nelem, const void *data)
{
	nvs_data_t *nvs;
	nvp_header_t head, *hp;
	uint8_t *ptr;
	size_t namelen;
	int decoded_size, encoded_size;
	xdr_t xdr = {
		.xdr_op = XDR_OP_ENCODE,
		.xdr_putint = _putint_mem,
		.xdr_putuint = _putuint_mem,
		.xdr_buf = nvl->nv_data,
		.xdr_idx = nvl->nv_data,
		.xdr_buf_size = nvl->nv_size
	};

	nvs = (nvs_data_t *)nvl->nv_data;
	if (nvs->nvl_nvflag & NV_UNIQUE_NAME)
		(void) nvlist_remove(nvl, name, type);

	xdr.xdr_buf = nvl->nv_data;
	xdr.xdr_idx = nvl->nv_data;
	xdr.xdr_buf_size = nvl->nv_size;
	if (!nvlist_size_native(&xdr, &nvl->nv_size))
		return (EINVAL);

	namelen = strlen(name);
	if ((decoded_size = get_value_size(type, data, nelem)) < 0)
		return (EINVAL);
	if ((encoded_size = get_nvp_data_size(type, data, nelem)) < 0)
		return (EINVAL);

	/*
	 * The encoded size is calculated as:
	 * encode_size (4) + decode_size (4) +
	 * name string size  (4 + NV_ALIGN4(namelen) +
	 * data type (4) + nelem size (4) + datalen
	 *
	 * The decoded size is calculated as:
	 * Note: namelen is with terminating 0.
	 * NV_ALIGN(sizeof(nvpair_t) (4 * 4) + namelen + 1) +
	 * NV_ALIGN(data_len)
	 */

	head.encoded_size = NVPE_SIZE(namelen, encoded_size);
	head.decoded_size = NVP_SIZE(namelen + 1, decoded_size);

	if (nvl->nv_asize - nvl->nv_size < head.encoded_size + 8) {
		ptr = realloc(nvl->nv_data, nvl->nv_asize + head.encoded_size);
		if (ptr == NULL)
			return (ENOMEM);
		nvl->nv_data = ptr;
		nvl->nv_asize += head.encoded_size;
	}
	nvl->nv_idx = nvl->nv_data + nvl->nv_size - sizeof(*hp);
	bzero(nvl->nv_idx, head.encoded_size + 8);
	hp = (nvp_header_t *)nvl->nv_idx;
	*hp = head;
	nvl->nv_idx += sizeof(*hp);

	xdr.xdr_buf = nvl->nv_data;
	xdr.xdr_buf_size = nvl->nv_asize;
	xdr.xdr_idx = nvl->nv_idx;

	xdr.xdr_idx += xdr.xdr_putuint(&xdr, namelen);
	strlcpy((char *)xdr.xdr_idx, name, namelen + 1);
	xdr.xdr_idx += NV_ALIGN4(namelen);
	xdr.xdr_idx += xdr.xdr_putuint(&xdr, type);
	xdr.xdr_idx += xdr.xdr_putuint(&xdr, nelem);

	switch (type) {
	case DATA_TYPE_BOOLEAN:
		break;

	case DATA_TYPE_BYTE_ARRAY:
		xdr.xdr_idx += xdr.xdr_putuint(&xdr, encoded_size);
		bcopy(data, xdr.xdr_idx, nelem);
		xdr.xdr_idx += NV_ALIGN4(encoded_size);
		break;

	case DATA_TYPE_STRING:
		encoded_size = strlen(data);
		xdr.xdr_idx += xdr.xdr_putuint(&xdr, encoded_size);
		strlcpy((char *)xdr.xdr_idx, data, encoded_size + 1);
		xdr.xdr_idx += NV_ALIGN4(encoded_size);
		break;

	case DATA_TYPE_STRING_ARRAY:
		for (uint32_t i = 0; i < nelem; i++) {
			encoded_size = strlen(((char **)data)[i]);
			xdr.xdr_idx += xdr.xdr_putuint(&xdr, encoded_size);
			strlcpy((char *)xdr.xdr_idx, ((char **)data)[i],
			    encoded_size + 1);
			xdr.xdr_idx += NV_ALIGN4(encoded_size);
		}
		break;

	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
		xdr_char(&xdr, (char *)data);
		break;

	case DATA_TYPE_INT8_ARRAY:
	case DATA_TYPE_UINT8_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_char);
		break;

	case DATA_TYPE_INT16:
		xdr_short(&xdr, (short *)data);
		break;

	case DATA_TYPE_UINT16:
		xdr_u_short(&xdr, (unsigned short *)data);
		break;

	case DATA_TYPE_INT16_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_short);
		break;

	case DATA_TYPE_UINT16_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_u_short);
		break;

	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_INT32:
		xdr_int(&xdr, (int *)data);
		break;

	case DATA_TYPE_UINT32:
		xdr_u_int(&xdr, (unsigned int *)data);
		break;

	case DATA_TYPE_BOOLEAN_ARRAY:
	case DATA_TYPE_INT32_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_int);
		break;

	case DATA_TYPE_UINT32_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_u_int);
		break;

	case DATA_TYPE_INT64:
		xdr_int64(&xdr, (int64_t *)data);
		break;

	case DATA_TYPE_UINT64:
		xdr_uint64(&xdr, (uint64_t *)data);
		break;

	case DATA_TYPE_INT64_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_int64);
		break;

	case DATA_TYPE_UINT64_ARRAY:
		xdr_array(&xdr, nelem, (xdrproc_t)xdr_uint64);
		break;

	case DATA_TYPE_NVLIST:
		bcopy(((nvlist_t *)data)->nv_data, xdr.xdr_idx, encoded_size);
		break;

	case DATA_TYPE_NVLIST_ARRAY: {
		size_t size;
		xdr_t xdr_nv;

		for (uint32_t i = 0; i < nelem; i++) {
			xdr_nv.xdr_idx = ((nvlist_t **)data)[i]->nv_data;
			xdr_nv.xdr_buf = xdr_nv.xdr_idx;
			xdr_nv.xdr_buf_size = ((nvlist_t **)data)[i]->nv_size;

			if (!nvlist_size_native(&xdr_nv, &size))
				return (EINVAL);

			bcopy(((nvlist_t **)data)[i]->nv_data, xdr.xdr_idx,
			    size);
			xdr.xdr_idx += size;
		}
		break;
	}
	default:
		bcopy(data, xdr.xdr_idx, encoded_size);
	}

	nvl->nv_size += head.encoded_size;

	return (0);
}

int
nvlist_add_boolean_value(nvlist_t *nvl, const char *name, int value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_BOOLEAN_VALUE, 1,
	    &value));
}

int
nvlist_add_byte(nvlist_t *nvl, const char *name, uint8_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_BYTE, 1, &value));
}

int
nvlist_add_int8(nvlist_t *nvl, const char *name, int8_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT8, 1, &value));
}

int
nvlist_add_uint8(nvlist_t *nvl, const char *name, uint8_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT8, 1, &value));
}

int
nvlist_add_int16(nvlist_t *nvl, const char *name, int16_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT16, 1, &value));
}

int
nvlist_add_uint16(nvlist_t *nvl, const char *name, uint16_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT16, 1, &value));
}

int
nvlist_add_int32(nvlist_t *nvl, const char *name, int32_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT32, 1, &value));
}

int
nvlist_add_uint32(nvlist_t *nvl, const char *name, uint32_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT32, 1, &value));
}

int
nvlist_add_int64(nvlist_t *nvl, const char *name, int64_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT64, 1, &value));
}

int
nvlist_add_uint64(nvlist_t *nvl, const char *name, uint64_t value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT64, 1, &value));
}

int
nvlist_add_string(nvlist_t *nvl, const char *name, const char *value)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_STRING, 1, value));
}

int
nvlist_add_boolean_array(nvlist_t *nvl, const char *name, int *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_BOOLEAN_ARRAY, n, a));
}

int
nvlist_add_byte_array(nvlist_t *nvl, const char *name, uint8_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_BYTE_ARRAY, n, a));
}

int
nvlist_add_int8_array(nvlist_t *nvl, const char *name, int8_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT8_ARRAY, n, a));
}

int
nvlist_add_uint8_array(nvlist_t *nvl, const char *name, uint8_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT8_ARRAY, n, a));
}

int
nvlist_add_int16_array(nvlist_t *nvl, const char *name, int16_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT16_ARRAY, n, a));
}

int
nvlist_add_uint16_array(nvlist_t *nvl, const char *name, uint16_t *a,
    uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT16_ARRAY, n, a));
}

int
nvlist_add_int32_array(nvlist_t *nvl, const char *name, int32_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT32_ARRAY, n, a));
}

int
nvlist_add_uint32_array(nvlist_t *nvl, const char *name, uint32_t *a,
    uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT32_ARRAY, n, a));
}

int
nvlist_add_int64_array(nvlist_t *nvl, const char *name, int64_t *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_INT64_ARRAY, n, a));
}

int
nvlist_add_uint64_array(nvlist_t *nvl, const char *name, uint64_t *a,
    uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_UINT64_ARRAY, n, a));
}

int
nvlist_add_string_array(nvlist_t *nvl, const char *name,
    char * const *a, uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_STRING_ARRAY, n, a));
}

int
nvlist_add_nvlist(nvlist_t *nvl, const char *name, nvlist_t *val)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_NVLIST, 1, val));
}

int
nvlist_add_nvlist_array(nvlist_t *nvl, const char *name, nvlist_t **a,
    uint32_t n)
{
	return (nvlist_add_common(nvl, name, DATA_TYPE_NVLIST_ARRAY, n, a));
}

static const char *typenames[] = {
	"DATA_TYPE_UNKNOWN",
	"DATA_TYPE_BOOLEAN",
	"DATA_TYPE_BYTE",
	"DATA_TYPE_INT16",
	"DATA_TYPE_UINT16",
	"DATA_TYPE_INT32",
	"DATA_TYPE_UINT32",
	"DATA_TYPE_INT64",
	"DATA_TYPE_UINT64",
	"DATA_TYPE_STRING",
	"DATA_TYPE_BYTE_ARRAY",
	"DATA_TYPE_INT16_ARRAY",
	"DATA_TYPE_UINT16_ARRAY",
	"DATA_TYPE_INT32_ARRAY",
	"DATA_TYPE_UINT32_ARRAY",
	"DATA_TYPE_INT64_ARRAY",
	"DATA_TYPE_UINT64_ARRAY",
	"DATA_TYPE_STRING_ARRAY",
	"DATA_TYPE_HRTIME",
	"DATA_TYPE_NVLIST",
	"DATA_TYPE_NVLIST_ARRAY",
	"DATA_TYPE_BOOLEAN_VALUE",
	"DATA_TYPE_INT8",
	"DATA_TYPE_UINT8",
	"DATA_TYPE_BOOLEAN_ARRAY",
	"DATA_TYPE_INT8_ARRAY",
	"DATA_TYPE_UINT8_ARRAY"
};

int
nvpair_type_from_name(const char *name)
{
	unsigned i;

	for (i = 0; i < nitems(typenames); i++) {
		if (strcmp(name, typenames[i]) == 0)
			return (i);
	}
	return (0);
}

nvp_header_t *
nvpair_find(nvlist_t *nv, const char *name)
{
	nvp_header_t *nvh;

	nvh = NULL;
	while ((nvh = nvlist_next_nvpair(nv, nvh)) != NULL) {
		nv_string_t *nvp_name;

		nvp_name = (nv_string_t *)(nvh + 1);
		if (nvp_name->nv_size == strlen(name) &&
		    memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0)
			break;
	}
	return (nvh);
}

void
nvpair_print(nvp_header_t *nvp, unsigned int indent)
{
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;
	unsigned i, j;
	xdr_t xdr = {
		.xdr_op = XDR_OP_DECODE,
		.xdr_getint = _getint_mem,
		.xdr_getuint = _getuint_mem,
		.xdr_buf = (const uint8_t *)nvp,
		.xdr_idx = NULL,
		.xdr_buf_size = nvp->encoded_size
	};

	nvp_name = (nv_string_t *)((uintptr_t)nvp + sizeof(*nvp));
	nvp_data = (nv_pair_data_t *)
	    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] + nvp_name->nv_size);

	for (i = 0; i < indent; i++)
		printf(" ");

	printf("%s [%d] %.*s", typenames[nvp_data->nv_type],
	    nvp_data->nv_nelem, nvp_name->nv_size, nvp_name->nv_data);

	xdr.xdr_idx = nvp_data->nv_data;
	switch (nvp_data->nv_type) {
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8: {
		char c;

		if (xdr_char(&xdr, &c))
			printf(" = 0x%x\n", c);
		break;
	}

	case DATA_TYPE_INT16:
	case DATA_TYPE_UINT16: {
		unsigned short u;

		if (xdr_u_short(&xdr, &u))
			printf(" = 0x%hx\n", u);
		break;
	}

	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_INT32:
	case DATA_TYPE_UINT32: {
		unsigned u;

		if (xdr_u_int(&xdr, &u))
			printf(" = 0x%x\n", u);
		break;
	}

	case DATA_TYPE_INT64:
	case DATA_TYPE_UINT64: {
		uint64_t u;

		if (xdr_uint64(&xdr, &u))
			printf(" = 0x%jx\n", (uintmax_t)u);
		break;
	}

	case DATA_TYPE_INT64_ARRAY:
	case DATA_TYPE_UINT64_ARRAY: {
		uint64_t *u;

		if (xdr_array(&xdr, nvp_data->nv_nelem,
		    (xdrproc_t)xdr_uint64)) {
			u = (uint64_t *)(nvp_data->nv_data + sizeof(unsigned));
			for (i = 0; i < nvp_data->nv_nelem; i++)
				printf(" [%u] = 0x%jx", i, (uintmax_t)u[i]);
			printf("\n");
		}

		break;
	}

	case DATA_TYPE_STRING:
	case DATA_TYPE_STRING_ARRAY:
		nvp_name = (nv_string_t *)&nvp_data->nv_data[0];
		for (i = 0; i < nvp_data->nv_nelem; i++) {
			printf(" = \"%.*s\"\n", nvp_name->nv_size,
			    nvp_name->nv_data);
		}
		break;

	case DATA_TYPE_NVLIST:
		printf("\n");
		nvlist.nv_data = &nvp_data->nv_data[0];
		nvlist_print(&nvlist, indent + 2);
		break;

	case DATA_TYPE_NVLIST_ARRAY:
		nvlist.nv_data = &nvp_data->nv_data[0];
		for (j = 0; j < nvp_data->nv_nelem; j++) {
			size_t size;

			printf("[%d]\n", j);
			nvlist_print(&nvlist, indent + 2);
			if (j != nvp_data->nv_nelem - 1) {
				for (i = 0; i < indent; i++)
					printf(" ");
				printf("%s %.*s",
				    typenames[nvp_data->nv_type],
				    nvp_name->nv_size,
				    nvp_name->nv_data);
			}
			xdr.xdr_idx = nvlist.nv_data;
			xdr.xdr_buf = xdr.xdr_idx;
			xdr.xdr_buf_size = nvp->encoded_size -
			    (xdr.xdr_idx - (uint8_t *)nvp);

			if (!nvlist_size_native(&xdr, &size))
				return;

			nvlist.nv_data += size;
		}
		break;

	default:
		printf("\n");
	}
}

void
nvlist_print(const nvlist_t *nvl, unsigned int indent)
{
	nvs_data_t *data;
	nvp_header_t *nvp;

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;  /* first pair in nvlist */
	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvpair_print(nvp, indent);
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	printf("%*s\n", indent + 13, "End of nvlist");
}
