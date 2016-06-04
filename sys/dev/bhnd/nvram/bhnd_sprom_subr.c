/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
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
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/bhnd/bhndvar.h>

#include "nvramvar.h"

#include "bhnd_spromreg.h"
#include "bhnd_spromvar.h"

/*
 * BHND SPROM Parser
 * 
 * Provides identification, decoding, and encoding of BHND SPROM data.
 */

static int	sprom_direct_read(struct bhnd_sprom *sc, size_t offset,
		    void *buf, size_t nbytes, uint8_t *crc);
static int	sprom_extend_shadow(struct bhnd_sprom *sc, size_t image_size,
		    uint8_t *crc);
static int	sprom_populate_shadow(struct bhnd_sprom *sc);

static int	sprom_var_defn(struct bhnd_sprom *sc, const char *name,
		    const struct bhnd_nvram_var **var,
		    const struct bhnd_sprom_var **sprom, size_t *size);

/* SPROM revision is always located at the second-to-last byte */
#define	SPROM_REV(_sc)		SPROM_READ_1((_sc), (_sc)->sp_size - 2)

/* SPROM CRC is always located at the last byte */
#define	SPROM_CRC_OFF(_sc)	SPROM_CRC_LEN(_sc)

/* SPROM CRC covers all but the final CRC byte */
#define	SPROM_CRC_LEN(_sc)	((_sc)->sp_size - 1)

/* SPROM shadow I/O (with byte-order translation) */
#define	SPROM_READ_1(_sc, _off)		SPROM_READ_ENC_1(_sc, _off)
#define	SPROM_READ_2(_sc, _off)		le16toh(SPROM_READ_ENC_2(_sc, _off))
#define	SPROM_READ_4(_sc, _off)		le32toh(SPROM_READ_ENC_4(_sc, _off))

#define	SPROM_WRITE_1(_sc, _off, _v)	SPROM_WRITE_ENC_1(_sc, _off, (_v))
#define	SPROM_WRITE_2(_sc, _off, _v)	SPROM_WRITE_ENC_2(_sc, _off,	\
    htole16(_v))
#define	SPROM_WRITE_4(_sc, _off, _v)	SPROM_WRITE_ENC_4(_sc, _off,	\
    htole32(_v))

/* SPROM shadow I/O (without byte-order translation) */
#define	SPROM_READ_ENC_1(_sc, _off)	(*(uint8_t *)((_sc)->sp_shadow + _off))
#define	SPROM_READ_ENC_2(_sc, _off)	(*(uint16_t *)((_sc)->sp_shadow + _off))
#define	SPROM_READ_ENC_4(_sc, _off)	(*(uint32_t *)((_sc)->sp_shadow + _off))

#define	SPROM_WRITE_ENC_1(_sc, _off, _v)	\
	*((uint8_t *)((_sc)->sp_shadow + _off)) = (_v)
#define	SPROM_WRITE_ENC_2(_sc, _off, _v)	\
	*((uint16_t *)((_sc)->sp_shadow + _off)) = (_v)
#define	SPROM_WRITE_ENC_4(_sc, _off, _v)	\
	*((uint32_t *)((_sc)->sp_shadow + _off)) = (_v)
	
/* Call @p _next macro with the C type, widened (signed or unsigned) C
 * type, and width associated with @p _dtype */
#define	SPROM_SWITCH_TYPE(_dtype, _next, ...)				\
do {									\
	switch (_dtype) {						\
	case BHND_NVRAM_DT_UINT8:					\
		_next (uint8_t,		uint32_t,	1,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_UINT16:					\
		_next (uint16_t,	uint32_t,	2,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_UINT32:					\
		_next (uint32_t,	uint32_t,	4,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_INT8:					\
		_next (int8_t,		int32_t,	1,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_INT16:					\
		_next (int16_t,		int32_t,	2,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_INT32:					\
		_next (int32_t,		int32_t,	4,		\
		    ## __VA_ARGS__);					\
		break;							\
	case BHND_NVRAM_DT_CHAR:					\
		_next (uint8_t,		uint32_t,	1,		\
		    ## __VA_ARGS__);					\
		break;							\
	}								\
} while (0)

/*
 * Table of supported SPROM image formats, sorted by image size, ascending.
 */
#define	SPROM_FMT(_sz, _revmin, _revmax, _sig)	\
	{ SPROM_SZ_ ## _sz, _revmin, _revmax,	\
	    SPROM_SIG_ ## _sig ## _OFF,		\
	    SPROM_SIG_ ## _sig }

static const struct sprom_fmt {
	size_t		size;
	uint8_t		rev_min;
	uint8_t		rev_max;
	size_t		sig_offset;
	uint16_t	sig_req;
} sprom_fmts[] = {
	SPROM_FMT(R1_3,		1, 3,	NONE),
	SPROM_FMT(R4_8_9,	4, 4,	R4),
	SPROM_FMT(R4_8_9,	8, 9,	R8_9),
	SPROM_FMT(R10,		10, 10,	R10),
	SPROM_FMT(R11,		11, 11,	R11)
};

/**
 * Identify the SPROM format at @p offset within @p r, verify the CRC,
 * and allocate a local shadow copy of the SPROM data.
 * 
 * After successful initialization, @p r will not be accessed; any pin
 * configuration required for SPROM access may be reset.
 * 
 * @param[out] sprom On success, will be initialized with shadow of the SPROM
 * data. 
 * @param r An active resource mapping the SPROM data.
 * @param offset Offset of the SPROM data within @p resource.
 */
int
bhnd_sprom_init(struct bhnd_sprom *sprom, struct bhnd_resource *r,
    bus_size_t offset)
{
	bus_size_t	 res_size;
	int		 error;

	sprom->dev = rman_get_device(r->res);
	sprom->sp_res = r;
	sprom->sp_res_off = offset;

	/* Determine maximum possible SPROM image size */
	res_size = rman_get_size(r->res);
	if (offset >= res_size)
		return (EINVAL);

	sprom->sp_size_max = MIN(res_size - offset, SPROM_SZ_MAX); 

	/* Allocate and populate SPROM shadow */
	sprom->sp_size = 0;
	sprom->sp_capacity = sprom->sp_size_max;
	sprom->sp_shadow = malloc(sprom->sp_capacity, M_BHND, M_NOWAIT);
	if (sprom->sp_shadow == NULL)
		return (ENOMEM);

	/* Read and identify SPROM image */
	if ((error = sprom_populate_shadow(sprom)))
		return (error);

	return (0);
}

/**
 * Release all resources held by @p sprom.
 * 
 * @param sprom A SPROM instance previously initialized via bhnd_sprom_init().
 */
void
bhnd_sprom_fini(struct bhnd_sprom *sprom)
{
	free(sprom->sp_shadow, M_BHND);
}

/* Perform a read using a SPROM offset descriptor, safely widening the
 * result to its 32-bit representation before assigning it to @p _dest. */
#define	SPROM_GETVAR_READ(_type, _widen, _width, _sc, _off, _dest)	\
do {									\
	_type _v = (_type)SPROM_READ_ ## _width(_sc, _off->offset);	\
	if (_off->shift > 0) {						\
		_v >>= _off->shift;					\
	} else if (off->shift < 0) {					\
		_v <<= -_off->shift;					\
	}								\
	_dest = ((uint32_t) (_widen) _v) & _off->mask;			\
} while(0)

/* Emit a value read using a SPROM offset descriptor, narrowing the
 * result output representation and, if necessary, OR'ing it with the
 * previously read value from @p _buf. */
#define	SPROM_GETVAR_WRITE(_type, _widen, _width, _off, _src, _buf)	\
do {									\
	_type _v = (_type) (_widen) _src;				\
	if (_off->cont)							\
		_v |= *((_type *)_buf);					\
	*((_type *)_buf) = _v;						\
} while(0)

/**
 * Read a SPROM variable, performing conversion to host byte order.
 *
 * @param		sc	The SPROM parser state.
 * @param		name	The SPROM variable name.
 * @param[out]		buf	On success, the requested value will be written
 *				to this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	len	The capacity of @p buf. On success, will be set
 *				to the actual size of the requested value.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval ENOMEM	If @p buf is non-NULL and a buffer of @p len is too
 *			small to hold the requested value.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
int
bhnd_sprom_getvar(struct bhnd_sprom *sc, const char *name, void *buf,
    size_t *len)
{
	const struct bhnd_nvram_var	*nv;
	const struct bhnd_sprom_var	*sv;
	size_t				 all1_offs;
	size_t				 req_size;
	int				 error;

	if ((error = sprom_var_defn(sc, name, &nv, &sv, &req_size)))
		return (error);

	/* Provide required size */
	if (buf == NULL) {
		*len = req_size;
		return (0);
	}

	/* Check (and update) target buffer len */
	if (*len < req_size)
		return (ENOMEM);
	else
		*len = req_size;

	/* Read data */
	all1_offs = 0;
	for (size_t i = 0; i < sv->num_offsets; i++) {
		const struct bhnd_sprom_offset	*off;
		uint32_t			 val;
		
		off = &sv->offsets[i];		
		KASSERT(!off->cont || i > 0, ("cont marked on first offset"));

		/* If not a continuation, advance the output buffer */
		if (i > 0 && !off->cont) {
			buf = ((uint8_t *)buf) +
			    bhnd_nvram_type_width(sv->offsets[i-1].type);
		}

		/* Read the value, widening to a common uint32
		 * representation */
		SPROM_SWITCH_TYPE(off->type, SPROM_GETVAR_READ, sc, off, val);

		/* If IGNALL1, record whether value has all bits set. */
		if (nv->flags & BHND_NVRAM_VF_IGNALL1) {
			uint32_t	all1;
			
			all1 = off->mask;
			if (off->shift > 0)
				all1 >>= off->shift;
			else if (off->shift < 0)
				all1 <<= -off->shift;

			if ((val & all1) == all1)
				all1_offs++;
		}

		/* Write the value, narrowing to the appropriate output
		 * width. */
		SPROM_SWITCH_TYPE(nv->type, SPROM_GETVAR_WRITE, off, val, buf);
	}

	/* Should value should be treated as uninitialized? */
	if (nv->flags & BHND_NVRAM_VF_IGNALL1 && all1_offs == sv->num_offsets)
		return (ENOENT);

	return (0);
}

/* Perform a read of a variable offset from _src, safely widening the result
 * to its 32-bit representation before assigning it to @p
 * _dest. */
#define	SPROM_SETVAR_READ(_type, _widen, _width, _off, _src, _dest)	\
do {									\
	_type _v = *(const _type *)_src;				\
	if (_off->shift > 0) {						\
		_v <<= _off->shift;					\
	} else if (off->shift < 0) {					\
		_v >>= -_off->shift;					\
	}								\
	_dest = ((uint32_t) (_widen) _v) & _off->mask;			\
} while(0)


/* Emit a value read using a SPROM offset descriptor, narrowing the
 * result output representation and, if necessary, OR'ing it with the
 * previously read value from @p _buf. */
#define	SPROM_SETVAR_WRITE(_type, _widen, _width, _sc, _off, _src)	\
do {									\
	_type _v = (_type) (_widen) _src;				\
	if (_off->cont)							\
		_v |= SPROM_READ_ ## _width(_sc, _off->offset);		\
	SPROM_WRITE_ ## _width(_sc, _off->offset, _v);			\
} while(0)

/**
 * Set a local value for a SPROM variable, performing conversion to SPROM byte
 * order.
 * 
 * The new value will be written to the backing SPROM shadow.
 * 
 * @param		sc	The SPROM parser state.
 * @param		name	The SPROM variable name.
 * @param[out]		buf	The new value.
 * @param[in,out]	len	The size of @p buf.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval EINVAL	If @p len does not match the expected variable size.
 */
int
bhnd_sprom_setvar(struct bhnd_sprom *sc, const char *name, const void *buf,
    size_t len)
{
	const struct bhnd_nvram_var	*nv;
	const struct bhnd_sprom_var	*sv;
	size_t				 req_size;
	int				 error;
	uint8_t				 crc;

	if ((error = sprom_var_defn(sc, name, &nv, &sv, &req_size)))
		return (error);

	/* Provide required size */
	if (len != req_size)
		return (EINVAL);

	/* Write data */
	for (size_t i = 0; i < sv->num_offsets; i++) {
		const struct bhnd_sprom_offset	*off;
		uint32_t			 val;
		
		off = &sv->offsets[i];		
		KASSERT(!off->cont || i > 0, ("cont marked on first offset"));

		/* If not a continuation, advance the input pointer */
		if (i > 0 && !off->cont) {
			buf = ((const uint8_t *)buf) +
			    bhnd_nvram_type_width(sv->offsets[i-1].type);
		}

		/* Read the value, widening to a common uint32
		 * representation */
		SPROM_SWITCH_TYPE(nv->type, SPROM_SETVAR_READ, off, buf, val);

		/* Write the value, narrowing to the appropriate output
		 * width. */
		SPROM_SWITCH_TYPE(off->type, SPROM_SETVAR_WRITE, sc, off, val);
	}

	/* Update CRC */
	crc = ~bhnd_nvram_crc8(sc->sp_shadow, SPROM_CRC_LEN(sc),
	    BHND_NVRAM_CRC8_INITIAL);
	SPROM_WRITE_1(sc, SPROM_CRC_OFF(sc), crc);

	return (0);
}

/* Read and identify the SPROM image by incrementally performing
 * read + CRC of all supported image formats */
static int
sprom_populate_shadow(struct bhnd_sprom *sc)
{
	const struct sprom_fmt	*fmt;
	int			 error;
	uint16_t		 sig;
	uint8_t			 srom_rev;
	uint8_t			 crc;

	crc = BHND_NVRAM_CRC8_INITIAL;

	/* Identify the SPROM revision (and populate the SPROM shadow) */
	for (size_t i = 0; i < nitems(sprom_fmts); i++) {
		fmt = &sprom_fmts[i];

		/* Read image data and check CRC */
		if ((error = sprom_extend_shadow(sc, fmt->size, &crc)))
			return (error);

		/* Skip on invalid CRC */
		if (crc != BHND_NVRAM_CRC8_VALID)
			continue;

		/* Fetch SROM revision */
		srom_rev = SPROM_REV(sc);

		/* Early sromrev 1 devices (specifically some BCM440x enet
		 * cards) are reported to have been incorrectly programmed
		 * with a revision of 0x10. */
		if (fmt->size == SPROM_SZ_R1_3 && srom_rev == 0x10)
			srom_rev = 0x1;

		/* Verify revision range */
		if (srom_rev < fmt->rev_min || srom_rev > fmt->rev_max)
			continue;

		/* Verify signature (if any) */
		sig = SPROM_SIG_NONE;
		if (fmt->sig_offset != SPROM_SIG_NONE_OFF)
			sig = SPROM_READ_2(sc, fmt->sig_offset);
		
		if (sig != fmt->sig_req) {
			device_printf(sc->dev,
			    "invalid sprom %hhu signature: 0x%hx "
			    "(expected 0x%hx)\n",
			    srom_rev, sig, fmt->sig_req);
			return (EINVAL);
		}

		/* Identified */
		sc->sp_rev = srom_rev;
		return (0);
	}

	/* identification failed */
	device_printf(sc->dev, "unrecognized SPROM format\n");
	return (EINVAL);
}

/*
 * Extend the shadowed SPROM buffer to image_size, reading any required
 * data from the backing SPROM resource and updating the CRC.
 */
static int
sprom_extend_shadow(struct bhnd_sprom *sc, size_t image_size,
    uint8_t *crc)
{
	int	error;

	KASSERT(image_size >= sc->sp_size, (("shadow truncation unsupported")));

	/* Verify the request fits within our shadow buffer */
	if (image_size > sc->sp_capacity)
		return (ENOSPC);

	/* Skip no-op requests */
	if (sc->sp_size == image_size)
		return (0);

	/* Populate the extended range */
	error = sprom_direct_read(sc, sc->sp_size, sc->sp_shadow + sc->sp_size,
	     image_size - sc->sp_size, crc);
	if (error)
		return (error);

	sc->sp_size = image_size;
	return (0);
}

/**
 * Read nbytes at the given offset from the backing SPROM resource, and
 * update the CRC.
 */
static int
sprom_direct_read(struct bhnd_sprom *sc, size_t offset, void *buf,
    size_t nbytes, uint8_t *crc)
{
	bus_size_t	 res_offset;
	uint16_t	*p;

	KASSERT(nbytes % sizeof(uint16_t) == 0, ("unaligned sprom size"));
	KASSERT(offset % sizeof(uint16_t) == 0, ("unaligned sprom offset"));

	/* Check for read overrun */
	if (offset >= sc->sp_size_max || sc->sp_size_max - offset < nbytes) {
		device_printf(sc->dev, "requested SPROM read would overrun\n");
		return (EINVAL);
	}

	/* Perform read and update CRC */
	p = (uint16_t *)buf;
	res_offset = sc->sp_res_off + offset;

	bhnd_bus_read_region_stream_2(sc->sp_res, res_offset, p, nbytes);
	*crc = bhnd_nvram_crc8(p, nbytes, *crc);

	return (0);
}


/**
 * Locate the variable and SPROM revision-specific definitions
 * for variable with @p name.
 */
static int
sprom_var_defn(struct bhnd_sprom *sc, const char *name,
    const struct bhnd_nvram_var **var,
    const struct bhnd_sprom_var **sprom,
    size_t *size)
{
	/* Find variable definition */
	*var = bhnd_nvram_var_defn(name);
	if (*var == NULL)
		return (ENOENT);

	/* Find revision-specific SPROM definition */
	for (size_t i = 0; i < (*var)->num_sp_descs; i++) {
		const struct bhnd_sprom_var *sp = &(*var)->sprom_descs[i];

		if (sc->sp_rev < sp->compat.first)
			continue;
		
		if (sc->sp_rev > sp->compat.last)
			continue;

		/* Found */
		*sprom = sp;
		
		/* Calculate size in bytes */
		*size = bhnd_nvram_type_width((*var)->type) * sp->num_offsets;
		return (0);
	}

	/* Not supported by this SPROM revision */
	return (ENOENT);
}
