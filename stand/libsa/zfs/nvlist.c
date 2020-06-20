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
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/endian.h>
#include <zfsimpl.h>
#include "libzfs.h"

typedef struct xdr {
	int (*xdr_getint)(const struct xdr *, const void *, int *);
} xdr_t;

static int xdr_int(const xdr_t *, const void *, int *);
static int mem_int(const xdr_t *, const void *, int *);
static void nvlist_decode_nvlist(const xdr_t *, nvlist_t *);
static int nvlist_size(const xdr_t *, const uint8_t *);

/*
 * transform data from network to host.
 */
xdr_t ntoh = {
	.xdr_getint = xdr_int
};

/*
 * transform data from host to host.
 */
xdr_t native = {
	.xdr_getint = mem_int
};

/*
 * transform data from host to network.
 */
xdr_t hton = {
	.xdr_getint = xdr_int
};

static int
xdr_short(const xdr_t *xdr, const uint8_t *buf, short *ip)
{
	int i, rv;

	rv = xdr->xdr_getint(xdr, buf, &i);
	*ip = i;
	return (rv);
}

static int
xdr_u_short(const xdr_t *xdr, const uint8_t *buf, unsigned short *ip)
{
	unsigned u;
	int rv;

	rv = xdr->xdr_getint(xdr, buf, &u);
	*ip = u;
	return (rv);
}

static int
xdr_int(const xdr_t *xdr __unused, const void *buf, int *ip)
{
	*ip = be32dec(buf);
	return (sizeof(int));
}

static int
xdr_u_int(const xdr_t *xdr __unused, const void *buf, unsigned *ip)
{
	*ip = be32dec(buf);
	return (sizeof(unsigned));
}

static int
xdr_string(const xdr_t *xdr, const void *buf, nv_string_t *s)
{
	int size;

	size = xdr->xdr_getint(xdr, buf, &s->nv_size);
	size = NV_ALIGN4(size + s->nv_size);
	return (size);
}

static int
xdr_int64(const xdr_t *xdr, const uint8_t *buf, int64_t *lp)
{
	int hi, rv;
	unsigned lo;

	rv = xdr->xdr_getint(xdr, buf, &hi);
	rv += xdr->xdr_getint(xdr, buf + rv, &lo);
	*lp = (((int64_t)hi) << 32) | lo;
	return (rv);
}

static int
xdr_uint64(const xdr_t *xdr, const uint8_t *buf, uint64_t *lp)
{
	unsigned hi, lo;
	int rv;

	rv = xdr->xdr_getint(xdr, buf, &hi);
	rv += xdr->xdr_getint(xdr, buf + rv, &lo);
	*lp = (((int64_t)hi) << 32) | lo;
	return (rv);
}

static int
xdr_char(const xdr_t *xdr, const uint8_t *buf, char *cp)
{
	int i, rv;

	rv = xdr->xdr_getint(xdr, buf, &i);
	*cp = i;
	return (rv);
}

/*
 * read native data.
 */
static int
mem_int(const xdr_t *xdr, const void *buf, int *i)
{
	*i = *(int *)buf;
	return (sizeof(int));
}

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

static void
nvlist_nvp_decode(const xdr_t *xdr, nvlist_t *nvl, nvp_header_t *nvph)
{
	nv_string_t *nv_string;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;

	nv_string = (nv_string_t *)nvl->nv_idx;
	nvl->nv_idx += xdr_string(xdr, &nv_string->nv_size, nv_string);
	nvp_data = (nv_pair_data_t *)nvl->nv_idx;

	nvl->nv_idx += xdr_u_int(xdr, &nvp_data->nv_type, &nvp_data->nv_type);
	nvl->nv_idx += xdr_u_int(xdr, &nvp_data->nv_nelem, &nvp_data->nv_nelem);

	switch (nvp_data->nv_type) {
	case DATA_TYPE_NVLIST:
	case DATA_TYPE_NVLIST_ARRAY:
		bzero(&nvlist, sizeof (nvlist));
		nvlist.nv_data = &nvp_data->nv_data[0];
		nvlist.nv_idx = nvlist.nv_data;
		for (int i = 0; i < nvp_data->nv_nelem; i++) {
			nvlist.nv_asize =
			    nvlist_size(xdr, nvlist.nv_data);
			nvlist_decode_nvlist(xdr, &nvlist);
			nvl->nv_idx = nvlist.nv_idx;
			nvlist.nv_data = nvlist.nv_idx;
		}
		break;

	case DATA_TYPE_BOOLEAN:
		/* BOOLEAN does not take value space */
		break;
	case DATA_TYPE_BYTE:
	case DATA_TYPE_INT8:
	case DATA_TYPE_UINT8:
		nvl->nv_idx += xdr_char(xdr, &nvp_data->nv_data[0],
		    (char *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_INT16:
		nvl->nv_idx += xdr_short(xdr, &nvp_data->nv_data[0],
		    (short *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT16:
		nvl->nv_idx += xdr_u_short(xdr, &nvp_data->nv_data[0],
		    (unsigned short *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_BOOLEAN_VALUE:
	case DATA_TYPE_INT32:
		nvl->nv_idx += xdr_int(xdr, &nvp_data->nv_data[0],
		    (int *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT32:
		nvl->nv_idx += xdr_u_int(xdr, &nvp_data->nv_data[0],
		    (unsigned *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_INT64:
		nvl->nv_idx += xdr_int64(xdr, &nvp_data->nv_data[0],
		    (int64_t *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_UINT64:
		nvl->nv_idx += xdr_uint64(xdr, &nvp_data->nv_data[0],
		    (uint64_t *)&nvp_data->nv_data[0]);
		break;

	case DATA_TYPE_STRING:
		nv_string = (nv_string_t *)&nvp_data->nv_data[0];
		nvl->nv_idx += xdr_string(xdr, &nvp_data->nv_data[0],
		    nv_string);

		break;
	}
}

static void
nvlist_decode_nvlist(const xdr_t *xdr, nvlist_t *nvl)
{
	nvp_header_t *nvph;
	nvs_data_t *nvs = (nvs_data_t *)nvl->nv_data;

	nvl->nv_idx = nvl->nv_data;
	nvl->nv_idx += xdr->xdr_getint(xdr, (const uint8_t *)&nvs->nvl_version,
	    &nvs->nvl_version);
	nvl->nv_idx += xdr->xdr_getint(xdr, (const uint8_t *)&nvs->nvl_nvflag,
	    &nvs->nvl_nvflag);

	nvph = &nvs->nvl_pair;
	nvl->nv_idx += xdr->xdr_getint(xdr,
	    (const uint8_t *)&nvph->encoded_size, &nvph->encoded_size);
	nvl->nv_idx += xdr->xdr_getint(xdr,
	    (const uint8_t *)&nvph->decoded_size, &nvph->decoded_size);

	while (nvph->encoded_size && nvph->decoded_size) {
		nvlist_nvp_decode(xdr, nvl, nvph);

		nvph = (nvp_header_t *)(nvl->nv_idx);
		nvl->nv_idx += xdr->xdr_getint(xdr, &nvph->encoded_size,
		    &nvph->encoded_size);
		nvl->nv_idx += xdr->xdr_getint(xdr, &nvph->decoded_size,
		    &nvph->decoded_size);
	}
}

static int
nvlist_size(const xdr_t *xdr, const uint8_t *stream)
{
	const uint8_t *p, *pair;
	unsigned encoded_size, decoded_size;

	p = stream;
	p += 2 * sizeof(unsigned);

	pair = p;
	p += xdr->xdr_getint(xdr, p, &encoded_size);
	p += xdr->xdr_getint(xdr, p, &decoded_size);
	while (encoded_size && decoded_size) {
		p = pair + encoded_size;
		pair = p;
		p += xdr->xdr_getint(xdr, p, &encoded_size);
		p += xdr->xdr_getint(xdr, p, &decoded_size);
	}
	return (p - stream);
}

/*
 * Import nvlist from byte stream.
 * Determine the stream size and allocate private copy.
 * Then translate the data.
 */
nvlist_t *
nvlist_import(const uint8_t *stream, char encoding, char endian)
{
	nvlist_t *nvl;

	if (encoding != NV_ENCODE_XDR)
		return (NULL);

	nvl = malloc(sizeof(*nvl));
	if (nvl == NULL)
		return (nvl);

	nvl->nv_asize = nvl->nv_size = nvlist_size(&ntoh, stream);
	nvl->nv_data = malloc(nvl->nv_asize);
	if (nvl->nv_data == NULL) {
		free(nvl);
		return (NULL);
	}
	nvl->nv_idx = nvl->nv_data;
	bcopy(stream, nvl->nv_data, nvl->nv_asize);

	nvlist_decode_nvlist(&ntoh, nvl);
	nvl->nv_idx = nvl->nv_data;
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

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	head = nvl->nv_data;
	data = (nvs_data_t *)head;
	nvp = &data->nvl_pair;	/* first pair in nvlist */
	head = (uint8_t *)nvp;

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)(head + sizeof(*nvp));

		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		if (memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    nvp_data->nv_type == type) {
			/*
			 * set tail to point to next nvpair and size
			 * is the length of the tail.
			 */
			tail = head + nvp->encoded_size;
			size = nvl->nv_data + nvl->nv_size - tail;

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

int
nvlist_find(const nvlist_t *nvl, const char *name, data_type_t type,
    int *elementsp, void *valuep, int *sizep)
{
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t *nvlist;

	if (nvl == NULL || nvl->nv_data == NULL || name == NULL)
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)((uint8_t *)nvp + sizeof(*nvp));

		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		if (memcmp(nvp_name->nv_data, name, nvp_name->nv_size) == 0 &&
		    nvp_data->nv_type == type) {
			if (elementsp != NULL)
				*elementsp = nvp_data->nv_nelem;
			switch (nvp_data->nv_type) {
			case DATA_TYPE_UINT64:
				*(uint64_t *)valuep = 
				    *(uint64_t *)nvp_data->nv_data;
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
			case DATA_TYPE_NVLIST_ARRAY:
				nvlist = malloc(sizeof(*nvlist));
				if (nvlist != NULL) {
					nvlist->nv_header = nvl->nv_header;
					nvlist->nv_asize = 0;
					nvlist->nv_size = 0;
					nvlist->nv_idx = NULL;
					nvlist->nv_data = &nvp_data->nv_data[0];
					*(nvlist_t **)valuep = nvlist;
					return (0);
				}
				return (ENOMEM);
			}
			return (EIO);
		}
		/* Not our pair, skip to next. */
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	return (ENOENT);
}

/*              
 * Return the next nvlist in an nvlist array.
 */
int
nvlist_next(nvlist_t *nvl)
{
	nvs_data_t *data;
	nvp_header_t *nvp;

	if (nvl == NULL || nvl->nv_data == NULL || nvl->nv_asize != 0)
		return (EINVAL);

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;	/* first pair in nvlist */

	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	nvl->nv_data = (uint8_t *)nvp + sizeof(*nvp);
	return (0);
}

void
nvlist_print(nvlist_t *nvl, unsigned int indent)
{
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
	nvs_data_t *data;
	nvp_header_t *nvp;
	nv_string_t *nvp_name;
	nv_pair_data_t *nvp_data;
	nvlist_t nvlist;
	int i, j;

	data = (nvs_data_t *)nvl->nv_data;
	nvp = &data->nvl_pair;  /* first pair in nvlist */
	while (nvp->encoded_size != 0 && nvp->decoded_size != 0) {
		nvp_name = (nv_string_t *)((uintptr_t)nvp + sizeof(*nvp));
		nvp_data = (nv_pair_data_t *)
		    NV_ALIGN4((uintptr_t)&nvp_name->nv_data[0] +
		    nvp_name->nv_size);

		for (int i = 0; i < indent; i++)
			printf(" ");

		printf("%s [%d] %.*s", typenames[nvp_data->nv_type],
		    nvp_data->nv_nelem, nvp_name->nv_size, nvp_name->nv_data);

		switch (nvp_data->nv_type) {
		case DATA_TYPE_UINT64: {
			uint64_t val;

			val = *(uint64_t *)nvp_data->nv_data;
			printf(" = 0x%jx\n", (uintmax_t)val);
			break;
		}

		case DATA_TYPE_STRING: {
			nvp_name = (nv_string_t *)&nvp_data->nv_data[0];
			printf(" = \"%.*s\"\n", nvp_name->nv_size,
			    nvp_name->nv_data );
			break;
		}

		case DATA_TYPE_NVLIST:
			printf("\n");
			nvlist.nv_data = &nvp_data->nv_data[0];
			nvlist_print(&nvlist, indent + 2);
			break;

		case DATA_TYPE_NVLIST_ARRAY:
			nvlist.nv_data = &nvp_data->nv_data[0];
			for (j = 0; j < nvp_data->nv_nelem; j++) {
				data = (nvs_data_t *)nvlist.nv_data;
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
				nvlist.nv_data = (uint8_t *)data +
				    nvlist_size(&native, nvlist.nv_data);
			}
			break;

		default:
			printf("\n");
		}
		nvp = (nvp_header_t *)((uint8_t *)nvp + nvp->encoded_size);
	}
	printf("%*s\n", indent + 13, "End of nvlist");
}
