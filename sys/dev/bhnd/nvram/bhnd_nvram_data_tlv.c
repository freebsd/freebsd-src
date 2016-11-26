/*-
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#else /* !_KERNEL */
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif /* _KERNEL */

#include "bhnd_nvram_private.h"

#include "bhnd_nvram_datavar.h"

#include "bhnd_nvram_data_tlvreg.h"

/*
 * CFE TLV NVRAM data class.
 * 
 * The CFE-defined TLV NVRAM format is used on the WGT634U.
 */

struct bhnd_nvram_tlv {
	struct bhnd_nvram_data	 nv;	/**< common instance state */
	struct bhnd_nvram_io	*data;	/**< backing buffer */
	size_t			 count;	/**< variable count */
};

BHND_NVRAM_DATA_CLASS_DEFN(tlv, "WGT634U", sizeof(struct bhnd_nvram_tlv))

/** Minimal TLV_ENV record header */
struct bhnd_nvram_tlv_env_hdr {
	uint8_t		tag;
	uint8_t		size;
} __packed;

/** Minimal TLV_ENV record */
struct bhnd_nvram_tlv_env {
	struct bhnd_nvram_tlv_env_hdr	hdr;
	uint8_t				flags;
	char				envp[];
} __packed;

/* Return the length in bytes of an TLV_ENV's envp data */
#define	NVRAM_TLV_ENVP_DATA_LEN(_env)	\
	(((_env)->hdr.size < sizeof((_env)->flags)) ? 0 :	\
	    ((_env)->hdr.size - sizeof((_env)->flags)))

	
static int				 bhnd_nvram_tlv_parse_size(
					     struct bhnd_nvram_io *io,
					     size_t *size);

static int				 bhnd_nvram_tlv_next_record(
					     struct bhnd_nvram_io *io,
					     size_t *next, size_t *offset,
					     uint8_t *tag);

static struct bhnd_nvram_tlv_env	*bhnd_nvram_tlv_next_env(
					     struct bhnd_nvram_tlv *tlv,
					     size_t *next, void **cookiep);

static struct bhnd_nvram_tlv_env	*bhnd_nvram_tlv_get_env(
					     struct bhnd_nvram_tlv *tlv,
					     void *cookiep);

static void				*bhnd_nvram_tlv_to_cookie(
					     struct bhnd_nvram_tlv *tlv,
					     size_t io_offset);
static size_t				 bhnd_nvram_tlv_to_offset(
					     struct bhnd_nvram_tlv *tlv,
					     void *cookiep);

static int
bhnd_nvram_tlv_probe(struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_tlv_env	ident;
	size_t				nbytes;
	int				error;

	nbytes = bhnd_nvram_io_getsize(io);

	/* Handle what might be an empty TLV image */
	if (nbytes < sizeof(ident)) {
		uint8_t tag;

		/* Fetch just the first tag */
		error = bhnd_nvram_io_read(io, 0x0, &tag, sizeof(tag));
		if (error)
			return (error);

		/* This *could* be an empty TLV image, but all we're
		 * testing for here is a single 0x0 byte followed by EOF */
		if (tag == NVRAM_TLV_TYPE_END)
			return (BHND_NVRAM_DATA_PROBE_MAYBE);

		return (ENXIO);
	}

	/* Otherwise, look at the initial header for a valid TLV ENV tag,
	 * plus one byte of the entry data */
	error = bhnd_nvram_io_read(io, 0x0, &ident,
	    sizeof(ident) + sizeof(ident.envp[0]));
	if (error)
		return (error);

	/* First entry should be a variable record (which we statically
	 * assert as being defined to use a single byte size field) */
	if (ident.hdr.tag != NVRAM_TLV_TYPE_ENV)
		return (ENXIO);

	_Static_assert(NVRAM_TLV_TYPE_ENV & NVRAM_TLV_TF_U8_LEN,
	    "TYPE_ENV is not a U8-sized field");

	/* The entry must be at least 3 characters ('x=\0') in length */
	if (ident.hdr.size < 3)
		return (ENXIO);

	/* The first character should be a valid key char (alpha) */
	if (!bhnd_nv_isalpha(ident.envp[0]))
		return (ENXIO);

	return (BHND_NVRAM_DATA_PROBE_DEFAULT);
}

/**
 * Initialize @p tlv with the provided NVRAM TLV data mapped by @p src.
 * 
 * @param tlv A newly allocated data instance.
 */
static int
bhnd_nvram_tlv_init(struct bhnd_nvram_tlv *tlv, struct bhnd_nvram_io *src)
{
	struct bhnd_nvram_tlv_env	*env;
	size_t				 size;
	size_t				 next;
	int				 error;

	BHND_NV_ASSERT(tlv->data == NULL, ("tlv data already initialized"));

	/* Determine the actual size of the TLV source data */
	if ((error = bhnd_nvram_tlv_parse_size(src, &size)))
		return (error);

	/* Copy to our own internal buffer */
	if ((tlv->data = bhnd_nvram_iobuf_copy_range(src, 0x0, size)) == NULL)
		return (ENOMEM);

	/* Initialize our backing buffer */
	tlv->count = 0;
	next = 0;
	while ((env = bhnd_nvram_tlv_next_env(tlv, &next, NULL)) != NULL) {
		size_t env_len;
		size_t name_len;

		/* TLV_ENV data must not be empty */
		env_len = NVRAM_TLV_ENVP_DATA_LEN(env);
		if (env_len == 0) {
			BHND_NV_LOG("cannot parse zero-length TLV_ENV record "
			    "data\n");
			return (EINVAL);
		}

		/* Parse the key=value string, and then replace the '='
		 * delimiter with '\0' to allow us to provide direct 
		 * name pointers from our backing buffer */
		error = bhnd_nvram_parse_env(env->envp, env_len, '=', NULL,
		    &name_len, NULL, NULL);
		if (error) {
			BHND_NV_LOG("error parsing TLV_ENV data: %d\n", error);
			return (error);
		}

		/* Replace '=' with '\0' */
		*(env->envp + name_len) = '\0';

		/* Add to variable count */
		tlv->count++;
	};

	return (0);
}

static int
bhnd_nvram_tlv_new(struct bhnd_nvram_data *nv, struct bhnd_nvram_io *io)
{
	
	struct bhnd_nvram_tlv	*tlv;
	int			 error;

	/* Allocate and initialize the TLV data instance */
	tlv = (struct bhnd_nvram_tlv *)nv;

	/* Parse the TLV input data and initialize our backing
	 * data representation */
	if ((error = bhnd_nvram_tlv_init(tlv, io))) {
		bhnd_nvram_tlv_free(nv);
		return (error);
	}

	return (0);
}

static void
bhnd_nvram_tlv_free(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_tlv *tlv = (struct bhnd_nvram_tlv *)nv;
	if (tlv->data != NULL)
		bhnd_nvram_io_free(tlv->data);
}

size_t
bhnd_nvram_tlv_count(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_tlv *tlv = (struct bhnd_nvram_tlv *)nv;
	return (tlv->count);
}

static int
bhnd_nvram_tlv_size(struct bhnd_nvram_data *nv, size_t *size)
{
	/* Let the serialization implementation calculate the length */
	return (bhnd_nvram_data_serialize(nv, NULL, size));
}

static int
bhnd_nvram_tlv_serialize(struct bhnd_nvram_data *nv, void *buf, size_t *len)
{
	struct bhnd_nvram_tlv	*tlv;
	size_t			 limit;
	size_t			 next;
	uint8_t			 tag;
	int			 error;

	tlv = (struct bhnd_nvram_tlv *)nv;

	/* Save the buffer capacity */
	if (buf == NULL)
		limit = 0;
	else
		limit = *len;

	/* Write all of our TLV records to the output buffer (or just
	 * calculate the buffer size that would be required) */
	next = 0;
	do {
		struct bhnd_nvram_tlv_env	*env;
		uint8_t				*p;
		size_t				 name_len;
		size_t				 rec_offset, rec_size;

		/* Parse the TLV record */
		error = bhnd_nvram_tlv_next_record(tlv->data, &next,
		    &rec_offset, &tag);
		if (error)
			return (error);

		rec_size = next - rec_offset;

		/* Calculate our output pointer */
		if (rec_offset > limit || limit - rec_offset < rec_size) {
			/* buffer is full; cannot write */
			p = NULL;
		} else {
			p = (uint8_t *)buf + rec_offset;
		}

		/* If not writing, nothing further to do for this record */
		if (p == NULL)
			continue;

		/* Copy to the output buffer */
		error = bhnd_nvram_io_read(tlv->data, rec_offset, p, rec_size);
		if (error)
			return (error);

		/* All further processing is TLV_ENV-specific */
		if (tag != NVRAM_TLV_TYPE_ENV)
			continue;

		/* Restore the original key=value format, rewriting '\0'
		 * delimiter back to '=' */
		env = (struct bhnd_nvram_tlv_env *)p;
		name_len = strlen(env->envp);	/* skip variable name */
		*(env->envp + name_len) = '=';	/* set '=' */
	} while (tag != NVRAM_TLV_TYPE_END);

	/* The 'next' offset should now point at EOF, and represents
	 * the total length of the serialized output. */
	*len = next;

	if (buf != NULL && limit < *len)
		return (ENOMEM);

	return (0);
}

static uint32_t
bhnd_nvram_tlv_caps(struct bhnd_nvram_data *nv)
{
	return (BHND_NVRAM_DATA_CAP_READ_PTR|BHND_NVRAM_DATA_CAP_DEVPATHS);
}

static const char *
bhnd_nvram_tlv_next(struct bhnd_nvram_data *nv, void **cookiep)
{
	struct bhnd_nvram_tlv		*tlv;
	struct bhnd_nvram_tlv_env	*env;
	size_t				 io_offset;

	tlv = (struct bhnd_nvram_tlv *)nv;

	/* Seek past the TLV_ENV record referenced by cookiep */
	io_offset = bhnd_nvram_tlv_to_offset(tlv, *cookiep);
	if (bhnd_nvram_tlv_next_env(tlv, &io_offset, NULL) == NULL)
		BHND_NV_PANIC("invalid cookiep: %p\n", cookiep);

	/* Fetch the next TLV_ENV record */
	if ((env = bhnd_nvram_tlv_next_env(tlv, &io_offset, cookiep)) == NULL) {
		/* No remaining ENV records */
		return (NULL);
	}

	/* Return the NUL terminated name */
	return (env->envp);
}

static void *
bhnd_nvram_tlv_find(struct bhnd_nvram_data *nv, const char *name)
{
	return (bhnd_nvram_data_generic_find(nv, name));
}

static int
bhnd_nvram_tlv_getvar(struct bhnd_nvram_data *nv, void *cookiep, void *buf,
    size_t *len, bhnd_nvram_type type)
{
	return (bhnd_nvram_data_generic_rp_getvar(nv, cookiep, buf, len, type));
}

static const void *
bhnd_nvram_tlv_getvar_ptr(struct bhnd_nvram_data *nv, void *cookiep,
    size_t *len, bhnd_nvram_type *type)
{
	struct bhnd_nvram_tlv		*tlv;
	struct bhnd_nvram_tlv_env	*env;
	const char			*val;
	int				 error;

	tlv = (struct bhnd_nvram_tlv *)nv;

	/* Fetch pointer to the TLV_ENV record */
	if ((env = bhnd_nvram_tlv_get_env(tlv, cookiep)) == NULL)
		BHND_NV_PANIC("invalid cookiep: %p", cookiep);

	/* Parse value pointer and length from key\0value data */
	error = bhnd_nvram_parse_env(env->envp, NVRAM_TLV_ENVP_DATA_LEN(env),
	    '\0', NULL, NULL, &val, len);
	if (error)
		BHND_NV_PANIC("unexpected error parsing '%s'", env->envp);

	/* Type is always CSTR */
	*type = BHND_NVRAM_TYPE_STRING;

	return (val);
}

static const char *
bhnd_nvram_tlv_getvar_name(struct bhnd_nvram_data *nv, void *cookiep)
{
	struct bhnd_nvram_tlv		*tlv;
	const struct bhnd_nvram_tlv_env	*env;

	tlv = (struct bhnd_nvram_tlv *)nv;

	/* Fetch pointer to the TLV_ENV record */
	if ((env = bhnd_nvram_tlv_get_env(tlv, cookiep)) == NULL)
		BHND_NV_PANIC("invalid cookiep: %p", cookiep);

	/* Return name pointer */
	return (&env->envp[0]);
}

/**
 * Iterate over the records starting at @p next, returning the parsed
 * record's @p tag, @p size, and @p offset.
 * 
 * @param		io		The I/O context to parse.
 * @param[in,out]	next		The next offset to be parsed, or 0x0
 *					to begin parsing. Upon successful
 *					return, will be set to the offset of the
 *					next record (or EOF, if
 *					NVRAM_TLV_TYPE_END was parsed).
 * @param[out]		offset		The record's value offset.
 * @param[out]		tag		The record's tag.
 * 
 * @retval 0		success
 * @retval EINVAL	if parsing @p io as TLV fails.
 * @retval non-zero	if reading @p io otherwise fails, a regular unix error
 *			code will be returned.
 */
static int
bhnd_nvram_tlv_next_record(struct bhnd_nvram_io *io, size_t *next, size_t
    *offset, uint8_t *tag)
{
	size_t		io_offset, io_size;
	uint16_t	parsed_len;
	uint8_t		len_hdr[2];
	int		error;

	io_offset = *next;
	io_size = bhnd_nvram_io_getsize(io);

	/* Save the record offset */
	if (offset != NULL)
		*offset = io_offset;

	/* Fetch initial tag */
	error = bhnd_nvram_io_read(io, io_offset, tag, sizeof(*tag));
	if (error)
		return (error);
	io_offset++;

	/* EOF */
	if (*tag == NVRAM_TLV_TYPE_END) {
		*next = io_offset;
		return (0);
	}

	/* Read length field */
	if (*tag & NVRAM_TLV_TF_U8_LEN) {
		error = bhnd_nvram_io_read(io, io_offset, &len_hdr,
		    sizeof(len_hdr[0]));
		if (error) {
			BHND_NV_LOG("error reading TLV record size: %d\n",
			    error);
			return (error);
		}

		parsed_len = len_hdr[0];
		io_offset++;
	} else {
		error = bhnd_nvram_io_read(io, io_offset, &len_hdr,
		    sizeof(len_hdr));
		if (error) {
			BHND_NV_LOG("error reading 16-bit TLV record "
			    "size: %d\n", error);
			return (error);
		}

		parsed_len = (len_hdr[0] << 8) | len_hdr[1];
		io_offset += 2;
	}

	/* Advance to next record */
	if (parsed_len > io_size || io_size - parsed_len < io_offset) {
		/* Hit early EOF */
		BHND_NV_LOG("TLV record length %hu truncated by input "
		    "size of %zu\n", parsed_len, io_size);
		return (EINVAL);
	}

	*next = io_offset + parsed_len;

	/* Valid record found */
	return (0);
}

/**
 * Parse the TLV data in @p io to determine the total size of the TLV
 * data mapped by @p io (which may be less than the size of @p io).
 */
static int
bhnd_nvram_tlv_parse_size(struct bhnd_nvram_io *io, size_t *size)
{
	size_t		next;
	uint8_t		tag;
	int		error;

	/* We have to perform a minimal parse to determine the actual length */
	next = 0x0;
	*size = 0x0;

	/* Iterate over the input until we hit END tag or the read fails */
	do {
		error = bhnd_nvram_tlv_next_record(io, &next, NULL, &tag);
		if (error)
			return (error);
	} while (tag != NVRAM_TLV_TYPE_END);

	/* Offset should now point to EOF */
	BHND_NV_ASSERT(next <= bhnd_nvram_io_getsize(io),
	    ("parse returned invalid EOF offset"));

	*size = next;
	return (0);
}

/**
 * Iterate over the records in @p tlv, returning a pointer to the next
 * NVRAM_TLV_TYPE_ENV record, or NULL if EOF is reached.
 * 
 * @param		tlv		The TLV instance.
 * @param[in,out]	next		The next offset to be parsed, or 0x0
 *					to begin parsing. Upon successful
 *					return, will be set to the offset of the
 *					next record.
 */
static struct bhnd_nvram_tlv_env *
bhnd_nvram_tlv_next_env(struct bhnd_nvram_tlv *tlv, size_t *next,
    void **cookiep)
{
	uint8_t	tag;
	int	error;

	/* Find the next TLV_ENV record, starting at @p next */
	do {
		void	*c;
		size_t	 offset;

		/* Fetch the next TLV record */
		error = bhnd_nvram_tlv_next_record(tlv->data, next, &offset,
		    &tag);
		if (error) {
			BHND_NV_LOG("unexpected error in next_record(): %d\n",
			    error);
			return (NULL);
		}

		/* Only interested in ENV records */
		if (tag != NVRAM_TLV_TYPE_ENV)
			continue;

		/* Map and return TLV_ENV record pointer */
		c = bhnd_nvram_tlv_to_cookie(tlv, offset);

		/* Provide the cookiep value for the returned record */
		if (cookiep != NULL)
			*cookiep = c;

		return (bhnd_nvram_tlv_get_env(tlv, c));
	} while (tag != NVRAM_TLV_TYPE_END);

	/* No remaining ENV records */
	return (NULL);
}

/**
 * Return a pointer to the TLV_ENV record for @p cookiep, or NULL
 * if none vailable.
 */
static struct bhnd_nvram_tlv_env *
bhnd_nvram_tlv_get_env(struct bhnd_nvram_tlv *tlv, void *cookiep)
{
	struct bhnd_nvram_tlv_env	*env;
	void				*ptr;
	size_t				 navail;
	size_t				 io_offset, io_size;
	int				 error;
	
	io_size = bhnd_nvram_io_getsize(tlv->data);
	io_offset = bhnd_nvram_tlv_to_offset(tlv, cookiep);

	/* At EOF? */
	if (io_offset == io_size)
		return (NULL);

	/* Fetch non-const pointer to the record entry */
	error = bhnd_nvram_io_write_ptr(tlv->data, io_offset, &ptr,
	    sizeof(env->hdr), &navail);
	if (error) {
		/* Should never occur with a valid cookiep */
		BHND_NV_LOG("error mapping record for cookiep: %d\n", error);
		return (NULL);
	}

	/* Validate the record pointer */
	env = ptr;
	if (env->hdr.tag != NVRAM_TLV_TYPE_ENV) {
		/* Should never occur with a valid cookiep */
		BHND_NV_LOG("non-ENV record mapped for %p\n", cookiep);
		return (NULL);
	}

	/* Is the required variable name data is mapped? */
	if (navail < sizeof(struct bhnd_nvram_tlv_env_hdr) + env->hdr.size ||
	    env->hdr.size == sizeof(env->flags))
	{
		/* Should never occur with a valid cookiep */
		BHND_NV_LOG("TLV_ENV variable data not mapped for %p\n",
		    cookiep);
		return (NULL);
	}

	return (env);
}

/**
 * Return a cookiep for the given I/O offset.
 */
static void *
bhnd_nvram_tlv_to_cookie(struct bhnd_nvram_tlv *tlv, size_t io_offset)
{
	BHND_NV_ASSERT(io_offset < bhnd_nvram_io_getsize(tlv->data),
	    ("io_offset %zu out-of-range", io_offset));
	BHND_NV_ASSERT(io_offset < UINTPTR_MAX,
	    ("io_offset %#zx exceeds UINTPTR_MAX", io_offset));

	return ((void *)(uintptr_t)(io_offset));
}

/* Convert a cookiep back to an I/O offset */
static size_t
bhnd_nvram_tlv_to_offset(struct bhnd_nvram_tlv *tlv, void *cookiep)
{
	size_t		io_size;
	uintptr_t	cval;

	io_size = bhnd_nvram_io_getsize(tlv->data);
	cval = (uintptr_t)cookiep;

	BHND_NV_ASSERT(cval < SIZE_MAX, ("cookie > SIZE_MAX)"));
	BHND_NV_ASSERT(cval <= io_size, ("cookie > io_size)"));

	return ((size_t)cval);
}
