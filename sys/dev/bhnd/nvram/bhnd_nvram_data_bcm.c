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

#include <sys/param.h>
#include <sys/endian.h>

#ifdef _KERNEL

#include <sys/bus.h>
#include <sys/ctype.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#else /* !_KERNEL */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif /* _KERNEL */

#include "bhnd_nvram_private.h"

#include "bhnd_nvram_datavar.h"

#include "bhnd_nvram_data_bcmreg.h"
#include "bhnd_nvram_data_bcmvar.h"

/*
 * Broadcom NVRAM data class.
 * 
 * The Broadcom NVRAM NUL-delimited ASCII format is used by most
 * Broadcom SoCs.
 * 
 * The NVRAM data is encoded as a standard header, followed by series of
 * NUL-terminated 'key=value' strings; the end of the stream is denoted
 * by a single extra NUL character.
 */

struct bhnd_nvram_bcm;

static struct bhnd_nvram_bcm_hvar	*bhnd_nvram_bcm_gethdrvar(
					     struct bhnd_nvram_bcm *bcm,
					     const char *name);
static struct bhnd_nvram_bcm_hvar	*bhnd_nvram_bcm_to_hdrvar(
					     struct bhnd_nvram_bcm *bcm,
					     void *cookiep);
static size_t				 bhnd_nvram_bcm_hdrvar_index(
					     struct bhnd_nvram_bcm *bcm,
					     struct bhnd_nvram_bcm_hvar *hvar);
/*
 * Set of BCM NVRAM header values that are required to be mirrored in the
 * NVRAM data itself.
 *
 * If they're not included in the parsed NVRAM data, we need to vend the
 * header-parsed values with their appropriate keys, and add them in any
 * updates to the NVRAM data.
 *
 * If they're modified in NVRAM, we need to sync the changes with the
 * the NVRAM header values.
 */
static const struct bhnd_nvram_bcm_hvar bhnd_nvram_bcm_hvars[] = {
	{
		.name	= BCM_NVRAM_CFG0_SDRAM_INIT_VAR,
		.type	= BHND_NVRAM_TYPE_UINT16,
		.len	= sizeof(uint16_t),
		.nelem	= 1,
	},
	{
		.name	= BCM_NVRAM_CFG1_SDRAM_CFG_VAR,
		.type	= BHND_NVRAM_TYPE_UINT16,
		.len	= sizeof(uint16_t),
		.nelem	= 1,
	},
	{
		.name	= BCM_NVRAM_CFG1_SDRAM_REFRESH_VAR,
		.type	= BHND_NVRAM_TYPE_UINT16,
		.len	= sizeof(uint16_t),
		.nelem	= 1,
	},
	{
		.name	= BCM_NVRAM_SDRAM_NCDL_VAR,
		.type	= BHND_NVRAM_TYPE_UINT32,
		.len	= sizeof(uint32_t),
		.nelem	= 1,
	},
};

/** BCM NVRAM data class instance */
struct bhnd_nvram_bcm {
	struct bhnd_nvram_data		 nv;	/**< common instance state */
	struct bhnd_nvram_io		*data;	/**< backing buffer */

	/** BCM header values */
	struct bhnd_nvram_bcm_hvar	 hvars[nitems(bhnd_nvram_bcm_hvars)];

	size_t				 count;	/**< total variable count */
};

BHND_NVRAM_DATA_CLASS_DEFN(bcm, "Broadcom", sizeof(struct bhnd_nvram_bcm))

static int
bhnd_nvram_bcm_probe(struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_bcmhdr	hdr;
	int				error;

	if ((error = bhnd_nvram_io_read(io, 0x0, &hdr, sizeof(hdr))))
		return (error);

	if (le32toh(hdr.magic) != BCM_NVRAM_MAGIC)
		return (ENXIO);

	return (BHND_NVRAM_DATA_PROBE_DEFAULT);
}

/**
 * Initialize @p bcm with the provided NVRAM data mapped by @p src.
 * 
 * @param bcm A newly allocated data instance.
 */
static int
bhnd_nvram_bcm_init(struct bhnd_nvram_bcm *bcm, struct bhnd_nvram_io *src)
{
	struct bhnd_nvram_bcmhdr	 hdr;
	uint8_t				*p;
	void				*ptr;
	size_t				 io_offset, io_size;
	uint8_t				 crc, valid;
	int				 error;

	if ((error = bhnd_nvram_io_read(src, 0x0, &hdr, sizeof(hdr))))
		return (error);

	if (le32toh(hdr.magic) != BCM_NVRAM_MAGIC)
		return (ENXIO);

	/* Fetch the actual NVRAM image size */
	io_size = le32toh(hdr.size);
	if (io_size < sizeof(hdr)) {
		/* The header size must include the header itself */
		BHND_NV_LOG("corrupt header size: %zu\n", io_size);
		return (EINVAL);
	}

	if (io_size > bhnd_nvram_io_getsize(src)) {
		BHND_NV_LOG("header size %zu exceeds input size %zu\n",
		    io_size, bhnd_nvram_io_getsize(src));
		return (EINVAL);
	}

	/* Allocate a buffer large enough to hold the NVRAM image, and
	 * an extra EOF-signaling NUL (on the chance it's missing from the
	 * source data) */
	if (io_size == SIZE_MAX)
		return (ENOMEM);

	bcm->data = bhnd_nvram_iobuf_empty(io_size, io_size + 1);
	if (bcm->data == NULL)
		return (ENOMEM);

	/* Fetch a pointer into our backing buffer and copy in the
	 * NVRAM image. */
	error = bhnd_nvram_io_write_ptr(bcm->data, 0x0, &ptr, io_size, NULL);
	if (error)
		return (error);

	p = ptr;
	if ((error = bhnd_nvram_io_read(src, 0x0, p, io_size)))
		return (error);

	/* Verify the CRC */
	valid = BCM_NVRAM_GET_BITS(hdr.cfg0, BCM_NVRAM_CFG0_CRC);
	crc = bhnd_nvram_crc8(p + BCM_NVRAM_CRC_SKIP,
	    io_size - BCM_NVRAM_CRC_SKIP, BHND_NVRAM_CRC8_INITIAL);

	if (crc != valid) {
		BHND_NV_LOG("warning: NVRAM CRC error (crc=%#hhx, "
		    "expected=%hhx)\n", crc, valid);
	}

	/* Populate header variable definitions */
#define	BCM_READ_HDR_VAR(_name, _dest, _swap) do {		\
	struct bhnd_nvram_bcm_hvar *data;				\
	data = bhnd_nvram_bcm_gethdrvar(bcm, _name ##_VAR);		\
	BHND_NV_ASSERT(data != NULL,						\
	    ("no such header variable: " __STRING(_name)));		\
									\
									\
	data->value. _dest = _swap(BCM_NVRAM_GET_BITS(			\
	    hdr. _name ## _FIELD, _name));				\
} while(0)

	BCM_READ_HDR_VAR(BCM_NVRAM_CFG0_SDRAM_INIT,	u16, le16toh);
	BCM_READ_HDR_VAR(BCM_NVRAM_CFG1_SDRAM_CFG,	u16, le16toh);
	BCM_READ_HDR_VAR(BCM_NVRAM_CFG1_SDRAM_REFRESH,	u16, le16toh);
	BCM_READ_HDR_VAR(BCM_NVRAM_SDRAM_NCDL,		u32, le32toh);

	_Static_assert(nitems(bcm->hvars) == 4, "missing initialization for"
	    "NVRAM header variable(s)");

#undef BCM_READ_HDR_VAR

	/* Process the buffer */
	bcm->count = 0;
	io_offset = sizeof(hdr);
	while (io_offset < io_size) {
		char		*envp;
		const char	*name, *value;
		size_t		 envp_len;
		size_t		 name_len, value_len;

		/* Parse the key=value string */
		envp = (char *) (p + io_offset);
		envp_len = strnlen(envp, io_size - io_offset);
		error = bhnd_nvram_parse_env(envp, envp_len, '=', &name,
					     &name_len, &value, &value_len);
		if (error) {
			BHND_NV_LOG("error parsing envp at offset %#zx: %d\n",
			    io_offset, error);
			return (error);
		}

		/* Insert a '\0' character, replacing the '=' delimiter and
		 * allowing us to vend references directly to the variable
		 * name */
		*(envp + name_len) = '\0';

		/* Record any NVRAM variables that mirror our header variables.
		 * This is a brute-force search -- for the amount of data we're
		 * operating on, it shouldn't be an issue. */
		for (size_t i = 0; i < nitems(bcm->hvars); i++) {
			struct bhnd_nvram_bcm_hvar	*hvar;
			union bhnd_nvram_bcm_hvar_value	 hval;
			size_t				 hval_len;

			hvar = &bcm->hvars[i];

			/* Already matched? */
			if (hvar->envp != NULL)
				continue;

			/* Name matches? */
			if ((strcmp(name, hvar->name)) != 0)
				continue;

			/* Save pointer to mirrored envp */
			hvar->envp = envp;

			/* Check for stale value */
			hval_len = sizeof(hval);
			error = bhnd_nvram_value_coerce(value, value_len,
			    BHND_NVRAM_TYPE_STRING, &hval, &hval_len,
			    hvar->type);
			if (error) {
				/* If parsing fails, we can likely only make
				 * things worse by trying to synchronize the
				 * variables */
				BHND_NV_LOG("error parsing header variable "
				    "'%s=%s': %d\n", name, value, error);
			} else if (hval_len != hvar->len) {
				hvar->stale = true;
			} else if (memcmp(&hval, &hvar->value, hval_len) != 0) {
				hvar->stale = true;
			}
		}

		/* Seek past the value's terminating '\0' */
		io_offset += envp_len;
		if (io_offset == io_size) {
			BHND_NV_LOG("missing terminating NUL at offset %#zx\n",
			    io_offset);
			return (EINVAL);
		}

		if (*(p + io_offset) != '\0') {
			BHND_NV_LOG("invalid terminator '%#hhx' at offset "
			    "%#zx\n", *(p + io_offset), io_offset);
			return (EINVAL);
		}

		/* Update variable count */
		bcm->count++;

		/* Seek to the next record */
		if (++io_offset == io_size) {
			char ch;
	
			/* Hit EOF without finding a terminating NUL
			 * byte; we need to grow our buffer and append
			 * it */
			io_size++;
			if ((error = bhnd_nvram_io_setsize(bcm->data, io_size)))
				return (error);

			/* Write NUL byte */
			ch = '\0';
			error = bhnd_nvram_io_write(bcm->data, io_size-1, &ch,
			    sizeof(ch));
			if (error)
				return (error);
		}

		/* Check for explicit EOF (encoded as a single empty NUL
		 * terminated string) */
		if (*(p + io_offset) == '\0')
			break;
	}

	/* Add non-mirrored header variables to total count variable */
	for (size_t i = 0; i < nitems(bcm->hvars); i++) {
		if (bcm->hvars[i].envp == NULL)
			bcm->count++;
	}

	return (0);
}

static int
bhnd_nvram_bcm_new(struct bhnd_nvram_data *nv, struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_bcm	*bcm;
	int			 error;

	bcm = (struct bhnd_nvram_bcm *)nv;

	/* Populate default BCM mirrored header variable set */
	_Static_assert(sizeof(bcm->hvars) == sizeof(bhnd_nvram_bcm_hvars),
	    "hvar declarations must match bhnd_nvram_bcm_hvars template");
	memcpy(bcm->hvars, bhnd_nvram_bcm_hvars, sizeof(bcm->hvars));

	/* Parse the BCM input data and initialize our backing
	 * data representation */
	if ((error = bhnd_nvram_bcm_init(bcm, io))) {
		bhnd_nvram_bcm_free(nv);
		return (error);
	}

	return (0);
}

static void
bhnd_nvram_bcm_free(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_bcm *bcm = (struct bhnd_nvram_bcm *)nv;

	if (bcm->data != NULL)
		bhnd_nvram_io_free(bcm->data);
}

size_t
bhnd_nvram_bcm_count(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_bcm *bcm = (struct bhnd_nvram_bcm *)nv;
	return (bcm->count);
}

static int
bhnd_nvram_bcm_size(struct bhnd_nvram_data *nv, size_t *size)
{
	return (bhnd_nvram_bcm_serialize(nv, NULL, size));
}

static int
bhnd_nvram_bcm_serialize(struct bhnd_nvram_data *nv, void *buf, size_t *len)
{
	struct bhnd_nvram_bcm		*bcm;
	struct bhnd_nvram_bcmhdr	 hdr;
	void				*cookiep;
	const char			*name;
	size_t				 nbytes, limit;
	uint8_t				 crc;
	int				 error;

	bcm = (struct bhnd_nvram_bcm *)nv;
	nbytes = 0;

	/* Save the output buffer limit */
	if (buf == NULL)
		limit = 0;
	else
		limit = *len;

	/* Reserve space for the NVRAM header */
	nbytes += sizeof(struct bhnd_nvram_bcmhdr);

	/* Write all variables to the output buffer */
	cookiep = NULL;
	while ((name = bhnd_nvram_data_next(nv, &cookiep))) {
		uint8_t		*outp;
		size_t		 olen;
		size_t		 name_len, val_len;

		if (limit > nbytes) {
			outp = (uint8_t *)buf + nbytes;
			olen = limit - nbytes;
		} else {
			outp = NULL;
			olen = 0;
		}

		/* Determine length of variable name */
		name_len = strlen(name) + 1;

		/* Write the variable name and '=' delimiter */
		if (olen >= name_len) {
			/* Copy name */
			memcpy(outp, name, name_len - 1);

			/* Append '=' */
			*(outp + name_len - 1) = '=';
		}

		/* Adjust byte counts */
		if (SIZE_MAX - name_len < nbytes)
			return (ERANGE);

		nbytes += name_len;

		/* Reposition output */
		if (limit > nbytes) {
			outp = (uint8_t *)buf + nbytes;
			olen = limit - nbytes;
		} else {
			outp = NULL;
			olen = 0;
		}

		/* Coerce to NUL-terminated C string, writing to the output
		 * buffer (or just calculating the length if outp is NULL) */
		val_len = olen;
		error = bhnd_nvram_data_getvar(nv, cookiep, outp, &val_len,
		    BHND_NVRAM_TYPE_STRING);

		if (error && error != ENOMEM)
			return (error);

		/* Adjust byte counts */
		if (SIZE_MAX - val_len < nbytes)
			return (ERANGE);

		nbytes += val_len;
	}

	/* Write terminating NUL */
	if (nbytes < limit)
		*((uint8_t *)buf + nbytes) = '\0';
	nbytes++;

	/* Provide actual size */
	*len = nbytes;
	if (buf == NULL || nbytes > limit) {
		if (buf != NULL)
			return (ENOMEM);

		return (0);
	}

	/* Fetch current NVRAM header */
	if ((error = bhnd_nvram_io_read(bcm->data, 0x0, &hdr, sizeof(hdr))))
		return (error);

	/* Update values covered by CRC and write to output buffer */
	hdr.size = htole32(*len);
	memcpy(buf, &hdr, sizeof(hdr));

	/* Calculate new CRC */
	crc = bhnd_nvram_crc8((uint8_t *)buf + BCM_NVRAM_CRC_SKIP,
	    *len - BCM_NVRAM_CRC_SKIP, BHND_NVRAM_CRC8_INITIAL);

	/* Update header with valid CRC */
	hdr.cfg0 &= ~BCM_NVRAM_CFG0_CRC_MASK;
	hdr.cfg0 |= (crc << BCM_NVRAM_CFG0_CRC_SHIFT);
	memcpy(buf, &hdr, sizeof(hdr));

	return (0);
}

static uint32_t
bhnd_nvram_bcm_caps(struct bhnd_nvram_data *nv)
{
	return (BHND_NVRAM_DATA_CAP_READ_PTR|BHND_NVRAM_DATA_CAP_DEVPATHS);
}

static const char *
bhnd_nvram_bcm_next(struct bhnd_nvram_data *nv, void **cookiep)
{
	struct bhnd_nvram_bcm		*bcm;
	struct bhnd_nvram_bcm_hvar	*hvar, *hvar_next;
	const void			*ptr;
	const char			*envp, *basep;
	size_t				 io_size, io_offset;
	int				 error;

	bcm = (struct bhnd_nvram_bcm *)nv;
	
	io_offset = sizeof(struct bhnd_nvram_bcmhdr);
	io_size = bhnd_nvram_io_getsize(bcm->data) - io_offset;

	/* Map backing buffer */
	error = bhnd_nvram_io_read_ptr(bcm->data, io_offset, &ptr, io_size,
	    NULL);
	if (error) {
		BHND_NV_LOG("error mapping backing buffer: %d\n", error);
		return (NULL);
	}

	basep = ptr;

	/* If cookiep pointers into our header variable array, handle as header
	 * variable iteration. */
	hvar = bhnd_nvram_bcm_to_hdrvar(bcm, *cookiep);
	if (hvar != NULL) {
		size_t idx;

		/* Advance to next entry, if any */
		idx = bhnd_nvram_bcm_hdrvar_index(bcm, hvar) + 1;

		/* Find the next header-defined variable that isn't defined in
		 * the NVRAM data, start iteration there */
		for (size_t i = idx; i < nitems(bcm->hvars); i++) {
			hvar_next = &bcm->hvars[i];
			if (hvar_next->envp != NULL && !hvar_next->stale)
				continue;

			*cookiep = hvar_next;
			return (hvar_next->name);
		}

		/* No further header-defined variables; iteration
		 * complete */
		return (NULL);
	}

	/* Handle standard NVRAM data iteration */
	if (*cookiep == NULL) {
		/* Start at the first NVRAM data record */
		envp = basep;
	} else {
		/* Seek to next record */
		envp = *cookiep;
		envp += strlen(envp) + 1;	/* key + '\0' */
		envp += strlen(envp) + 1;	/* value + '\0' */
	}

	/*
	 * Skip entries that have an existing header variable entry that takes
	 * precedence over the NVRAM data value.
	 * 
	 * The header's value will be provided when performing header variable
	 * iteration
	 */
	 while ((size_t)(envp - basep) < io_size && *envp != '\0') {
		/* Locate corresponding header variable */
		hvar = NULL;
		for (size_t i = 0; i < nitems(bcm->hvars); i++) {
			if (bcm->hvars[i].envp != envp)
				continue;

			hvar = &bcm->hvars[i];
			break;
		}

		/* If no corresponding hvar entry, or the entry does not take
		 * precedence over this NVRAM value, we can safely return this
		 * value as-is. */
		if (hvar == NULL || !hvar->stale)
			break;

		/* Seek to next record */
		envp += strlen(envp) + 1;	/* key + '\0' */
		envp += strlen(envp) + 1;	/* value + '\0' */
	 }

	/* On NVRAM data EOF, try switching to header variables */
	if ((size_t)(envp - basep) == io_size || *envp == '\0') {
		/* Find first valid header variable */
		for (size_t i = 0; i < nitems(bcm->hvars); i++) {
			if (bcm->hvars[i].envp != NULL)
				continue;
			
			*cookiep = &bcm->hvars[i];
			return (bcm->hvars[i].name);
		}

		/* No header variables */
		return (NULL);
	}

	*cookiep = (void *)(uintptr_t)envp;
	return (envp);
}

static void *
bhnd_nvram_bcm_find(struct bhnd_nvram_data *nv, const char *name)
{
	return (bhnd_nvram_data_generic_find(nv, name));
}

static int
bhnd_nvram_bcm_getvar(struct bhnd_nvram_data *nv, void *cookiep, void *buf,
    size_t *len, bhnd_nvram_type type)
{
	return (bhnd_nvram_data_generic_rp_getvar(nv, cookiep, buf, len, type));
}

static const void *
bhnd_nvram_bcm_getvar_ptr(struct bhnd_nvram_data *nv, void *cookiep,
    size_t *len, bhnd_nvram_type *type)
{
	struct bhnd_nvram_bcm		*bcm;
	struct bhnd_nvram_bcm_hvar	*hvar;
	const char			*envp;

	bcm = (struct bhnd_nvram_bcm *)nv;

	/* Handle header variables */
	if ((hvar = bhnd_nvram_bcm_to_hdrvar(bcm, cookiep)) != NULL) {
		BHND_NV_ASSERT(
		    hvar->len % bhnd_nvram_value_size(hvar->type, NULL, 0,
			hvar->nelem) == 0,
		    ("length is not aligned to type width"));

		*type = hvar->type;
		*len = hvar->len;
		return (&hvar->value);
	}

	/* Cookie points to key\0value\0 -- get the value address */
	BHND_NV_ASSERT(cookiep != NULL, ("NULL cookiep"));

	envp = cookiep;
	envp += strlen(envp) + 1;	/* key + '\0' */
	*len = strlen(envp) + 1;	/* value + '\0' */
	*type = BHND_NVRAM_TYPE_STRING;

	return (envp);
}

static const char *
bhnd_nvram_bcm_getvar_name(struct bhnd_nvram_data *nv, void *cookiep)
{
	struct bhnd_nvram_bcm		*bcm;
	struct bhnd_nvram_bcm_hvar	*hvar;

	bcm = (struct bhnd_nvram_bcm *)nv;

	/* Handle header variables */
	if ((hvar = bhnd_nvram_bcm_to_hdrvar(bcm, cookiep)) != NULL) {
		return (hvar->name);
	}

	/* Cookie points to key\0value\0 */
	return (cookiep);
}

/**
 * Return the internal BCM data reference for a header-defined variable
 * with @p name, or NULL if none exists.
 */
static struct bhnd_nvram_bcm_hvar *
bhnd_nvram_bcm_gethdrvar(struct bhnd_nvram_bcm *bcm, const char *name)
{
	for (size_t i = 0; i < nitems(bcm->hvars); i++) {
		if (strcmp(bcm->hvars[i].name, name) == 0)
			return (&bcm->hvars[i]);
	}

	/* Not found */
	return (NULL);
}

/**
 * If @p cookiep references a header-defined variable, return the
 * internal BCM data reference. Otherwise, returns NULL.
 */
static struct bhnd_nvram_bcm_hvar *
bhnd_nvram_bcm_to_hdrvar(struct bhnd_nvram_bcm *bcm, void *cookiep)
{
#ifdef BHND_NVRAM_INVARIANTS                                                                                                                                                                                                                                
	uintptr_t base, ptr;
#endif

	/* If the cookie falls within the hvar array, it's a
	 * header variable cookie */
	if (nitems(bcm->hvars) == 0)
		return (NULL);

	if (cookiep < (void *)&bcm->hvars[0])
		return (NULL);

	if (cookiep > (void *)&bcm->hvars[nitems(bcm->hvars)-1])
		return (NULL);

#ifdef BHND_NVRAM_INVARIANTS
	base = (uintptr_t)bcm->hvars;
	ptr = (uintptr_t)cookiep;

	BHND_NV_ASSERT((ptr - base) % sizeof(bcm->hvars[0]) == 0,
	    ("misaligned hvar pointer %p/%p", cookiep, bcm->hvars));
#endif /* INVARIANTS */

	return ((struct bhnd_nvram_bcm_hvar *)cookiep);
}

/**
 * Return the index of @p hdrvar within @p bcm's backing hvars array.
 */
static size_t
bhnd_nvram_bcm_hdrvar_index(struct bhnd_nvram_bcm *bcm,
    struct bhnd_nvram_bcm_hvar *hdrvar)
{
	BHND_NV_ASSERT(bhnd_nvram_bcm_to_hdrvar(bcm, (void *)hdrvar) != NULL,
	    ("%p is not a valid hdrvar reference", hdrvar));

	return (hdrvar - &bcm->hvars[0]);
}
