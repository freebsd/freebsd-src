/*-
 * Copyright (c) 2015-2016 Landon Fuller <landonf@FreeBSD.org>
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

#include <sys/endian.h>

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/_inttypes.h>
#else /* !_KERNEL */
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif /* _KERNEL */

#include "bhnd_nvram_private.h"

#include "bhnd_nvram_datavar.h"

#include "bhnd_nvram_data_spromvar.h"

/*
 * BHND SPROM NVRAM data class
 *
 * The SPROM data format is a fixed-layout, non-self-descriptive binary format,
 * used on Broadcom wireless and wired adapters, that provides a subset of the
 * variables defined by Broadcom SoC NVRAM formats.
 */
BHND_NVRAM_DATA_CLASS_DEFN(sprom, "Broadcom SPROM",
   sizeof(struct bhnd_nvram_sprom))

static int	sprom_sort_idx(const void *lhs, const void *rhs);

static int	sprom_opcode_state_init(struct sprom_opcode_state *state,
		    const struct bhnd_sprom_layout *layout);
static int	sprom_opcode_state_reset(struct sprom_opcode_state *state);
static int	sprom_opcode_state_seek(struct sprom_opcode_state *state,
		    struct sprom_opcode_idx *indexed);

static int	sprom_opcode_next_var(struct sprom_opcode_state *state);
static int	sprom_opcode_parse_var(struct sprom_opcode_state *state,
		    struct sprom_opcode_idx *indexed);
  
static int	sprom_opcode_next_binding(struct sprom_opcode_state *state);

static int	sprom_opcode_set_type(struct sprom_opcode_state *state,
		    bhnd_nvram_type type);

static int	sprom_opcode_set_var(struct sprom_opcode_state *state,
		    size_t vid);
static int	sprom_opcode_clear_var(struct sprom_opcode_state *state);
static int	sprom_opcode_flush_bind(struct sprom_opcode_state *state);
static int	sprom_opcode_read_opval32(struct sprom_opcode_state *state,
		    uint8_t type, uint32_t *opval);
static int	sprom_opcode_apply_scale(struct sprom_opcode_state *state,
		    uint32_t *value);

static int	sprom_opcode_step(struct sprom_opcode_state *state,
		    uint8_t *opcode);

#define	SPROM_OP_BAD(_state, _fmt, ...)					\
	BHND_NV_LOG("bad encoding at %td: " _fmt,			\
	    (_state)->input - (_state)->layout->bindings, ##__VA_ARGS__)

#define	SPROM_COOKIE_TO_NVRAM(_cookie)	\
	bhnd_nvram_get_vardefn(((struct sprom_opcode_idx *)_cookie)->vid)

/**
 * Read the magic value from @p io, and verify that it matches
 * the @p layout's expected magic value.
 * 
 * If @p layout does not defined a magic value, @p magic is set to 0x0
 * and success is returned.
 * 
 * @param	io	An I/O context mapping the SPROM data to be identified.
 * @param	layout	The SPROM layout against which @p io should be verified.
 * @param[out]	magic	On success, the SPROM magic value.
 * 
 * @retval 0		success
 * @retval non-zero	If checking @p io otherwise fails, a regular unix
 *			error code will be returned.
 */
static int
bhnd_nvram_sprom_check_magic(struct bhnd_nvram_io *io,
    const struct bhnd_sprom_layout *layout, uint16_t *magic)
{
	int error;

	/* Skip if layout does not define a magic value */
	if (layout->flags & SPROM_LAYOUT_MAGIC_NONE)
		return (0);

	/* Read the magic value */
	error = bhnd_nvram_io_read(io, layout->magic_offset, magic,
	    sizeof(*magic));
	if (error)
		return (error);

	*magic = le16toh(*magic);

	/* If the signature does not match, skip to next layout */
	if (*magic != layout->magic_value)
		return (ENXIO);

	return (0);
}

/**
 * Attempt to identify the format of the SPROM data mapped by @p io.
 *
 * The SPROM data format does not provide any identifying information at a
 * known offset, instead requiring that we iterate over the known SPROM image
 * sizes until we are able to compute a valid checksum (and, for later
 * revisions, validate a signature at a revision-specific offset).
 *
 * @param	io	An I/O context mapping the SPROM data to be identified.
 * @param[out]	ident	On success, the identified SPROM layout.
 * @param[out]	shadow	On success, a correctly sized iobuf instance mapping
 *			a copy of the identified SPROM image. The caller is
 *			responsible for deallocating this instance via
 *			bhnd_nvram_io_free()
 *
 * @retval 0		success
 * @retval non-zero	If identifying @p io otherwise fails, a regular unix
 *			error code will be returned.
 */
static int
bhnd_nvram_sprom_ident(struct bhnd_nvram_io *io,
    const struct bhnd_sprom_layout **ident, struct bhnd_nvram_io **shadow)
{
	struct bhnd_nvram_io	*buf;
	uint8_t			 crc;
	size_t			 crc_errors;
	size_t			 sprom_sz_max;
	int			 error;

	/* Find the largest SPROM layout size */
	sprom_sz_max = 0;
	for (size_t i = 0; i < bhnd_sprom_num_layouts; i++) {
		sprom_sz_max = bhnd_nv_ummax(sprom_sz_max,
		    bhnd_sprom_layouts[i].size);
	}

	/* Allocate backing buffer and initialize CRC state */
	buf = bhnd_nvram_iobuf_empty(0, sprom_sz_max);
	crc = BHND_NVRAM_CRC8_INITIAL;
	crc_errors = 0;

	/* We iterate the SPROM layouts smallest to largest, allowing us to
	 * perform incremental checksum calculation */
	for (size_t i = 0; i < bhnd_sprom_num_layouts; i++) {
		const struct bhnd_sprom_layout	*layout;
		void				*ptr;
		size_t				 nbytes, nr;
		uint16_t			 magic;
		uint8_t				 srev;
		bool				 crc_valid;
		bool				 have_magic;

		layout = &bhnd_sprom_layouts[i];
		nbytes = bhnd_nvram_io_getsize(buf);

		if ((layout->flags & SPROM_LAYOUT_MAGIC_NONE)) {
			have_magic = false;
		} else {
			have_magic = true;
		}

		/* Layout instances must be ordered from smallest to largest by
		 * the nvram_map compiler */
		if (nbytes > layout->size)
			BHND_NV_PANIC("SPROM layout is defined out-of-order");

		/* Calculate number of additional bytes to be read */
		nr = layout->size - nbytes;

		/* Adjust the buffer size and fetch a write pointer */
		if ((error = bhnd_nvram_io_setsize(buf, layout->size)))
			goto failed;

		error = bhnd_nvram_io_write_ptr(buf, nbytes, &ptr, nr, NULL);
		if (error)
			goto failed;

		/* Read image data and update CRC (errors are reported
		 * after the signature check) */
		if ((error = bhnd_nvram_io_read(io, nbytes, ptr, nr)))
			goto failed;

		crc = bhnd_nvram_crc8(ptr, nr, crc);
		crc_valid = (crc == BHND_NVRAM_CRC8_VALID);
		if (!crc_valid)
			crc_errors++;

		/* Fetch SPROM revision */
		error = bhnd_nvram_io_read(buf, layout->srev_offset, &srev,
		    sizeof(srev));
		if (error)
			goto failed;

		/* Early sromrev 1 devices (specifically some BCM440x enet
		 * cards) are reported to have been incorrectly programmed
		 * with a revision of 0x10. */
		if (layout->rev == 1 && srev == 0x10)
			srev = 0x1;
		
		/* Check revision against the layout definition */
		if (srev != layout->rev)
			continue;

		/* Check the magic value, skipping to the next layout on
		 * failure. */
		error = bhnd_nvram_sprom_check_magic(buf, layout, &magic);
		if (error) {
			/* If the CRC is was valid, log the mismatch */
			if (crc_valid || BHND_NV_VERBOSE) {
				BHND_NV_LOG("invalid sprom %hhu signature: "
					    "0x%hx (expected 0x%hx)\n", srev,
					    magic, layout->magic_value);

					error = ENXIO;
					goto failed;
			}
	
			continue;
		}

		/* Check for an earlier CRC error */
		if (!crc_valid) {
			/* If the magic check succeeded, then we may just have
			 * data corruption -- log the CRC error */
			if (have_magic || BHND_NV_VERBOSE) {
				BHND_NV_LOG("sprom %hhu CRC error (crc=%#hhx, "
					    "expected=%#x)\n", srev, crc,
					    BHND_NVRAM_CRC8_VALID);
			}

			continue;
		}

		/* Identified */
		*shadow = buf;
		*ident = layout;
		return (0);
	}

	/* No match -- set error and fallthrough */
	error = ENXIO;
	if (crc_errors > 0 && BHND_NV_VERBOSE) {
		BHND_NV_LOG("sprom parsing failed with %zu CRC errors\n",
		    crc_errors);
	}

failed:
	bhnd_nvram_io_free(buf);
	return (error);
}

static int
bhnd_nvram_sprom_probe(struct bhnd_nvram_io *io)
{
	const struct bhnd_sprom_layout	*layout;
	struct bhnd_nvram_io		*shadow;
	int				 error;

	/* Try to parse the input */
	if ((error = bhnd_nvram_sprom_ident(io, &layout, &shadow)))
		return (error);

	/* Clean up the shadow iobuf */
	bhnd_nvram_io_free(shadow);

	return (BHND_NVRAM_DATA_PROBE_DEFAULT);
}

static int
bhnd_nvram_sprom_new(struct bhnd_nvram_data *nv, struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_sprom		*sp;
	size_t				 num_vars;
	int				 error;

	sp = (struct bhnd_nvram_sprom *)nv;

	/* Identify the SPROM input data */
	if ((error = bhnd_nvram_sprom_ident(io, &sp->layout, &sp->data)))
		goto failed;

	/* Initialize SPROM binding eval state */
	if ((error = sprom_opcode_state_init(&sp->state, sp->layout)))
		goto failed;

	/* Allocate our opcode index */
	sp->num_idx = sp->layout->num_vars;
	if ((sp->idx = bhnd_nv_calloc(sp->num_idx, sizeof(*sp->idx))) == NULL)
		goto failed;

	/* Parse out index entries from our stateful opcode stream */
	for (num_vars = 0; num_vars < sp->num_idx; num_vars++) {
		size_t opcodes;

		/* Seek to next entry */
		if ((error = sprom_opcode_next_var(&sp->state))) {
			SPROM_OP_BAD(&sp->state,
			    "error reading expected variable entry: %d\n",
			    error);
			goto failed;
		}

		/* We limit the SPROM index representations to the minimal
		 * type widths capable of covering all known layouts */

		/* Save SPROM image offset */
		if (sp->state.offset > UINT16_MAX) {
			SPROM_OP_BAD(&sp->state,
			    "cannot index large offset %u\n", sp->state.offset);
		}
		sp->idx[num_vars].offset = sp->state.offset;

		/* Save current variable ID */
		if (sp->state.vid > UINT16_MAX) {
			SPROM_OP_BAD(&sp->state,
			    "cannot index large vid %zu\n",  sp->state.vid);
		}
		sp->idx[num_vars].vid = sp->state.vid;

		/* Save opcode position */
		opcodes = (sp->state.input - sp->layout->bindings);
		if (opcodes > UINT16_MAX) {
			SPROM_OP_BAD(&sp->state,
			    "cannot index large opcode offset %zu\n", opcodes);
		}
		sp->idx[num_vars].opcodes = opcodes;
	}

	/* Should have reached end of binding table; next read must return
	 * ENOENT */
	if ((error = sprom_opcode_next_var(&sp->state)) != ENOENT) {
		BHND_NV_LOG("expected EOF parsing binding table: %d\n", error);
		goto failed;
	}

        /* Sort index by variable ID, ascending */
        qsort(sp->idx, sp->num_idx, sizeof(sp->idx[0]), sprom_sort_idx);

	return (0);
	
failed:
	if (sp->data != NULL)
		bhnd_nvram_io_free(sp->data);

	if (sp->idx != NULL)
		bhnd_nv_free(sp->idx);

	return (error);
}

/* sort function for sprom_opcode_idx values */
static int
sprom_sort_idx(const void *lhs, const void *rhs)
{
	const struct sprom_opcode_idx	*l, *r;

	l = lhs;
	r = rhs;

	if (l->vid < r->vid)
		return (-1);
	if (l->vid > r->vid)
		return (1);
	return (0);
}

static void
bhnd_nvram_sprom_free(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_sprom *sp = (struct bhnd_nvram_sprom *)nv;
	
	bhnd_nvram_io_free(sp->data);
	bhnd_nv_free(sp->idx);
}

size_t
bhnd_nvram_sprom_count(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_sprom *sprom = (struct bhnd_nvram_sprom *)nv;
	return (sprom->layout->num_vars);
}

static int
bhnd_nvram_sprom_size(struct bhnd_nvram_data *nv, size_t *size)
{
	struct bhnd_nvram_sprom *sprom = (struct bhnd_nvram_sprom *)nv;

	/* The serialized form will be identical in length
	 * to our backing buffer representation */
	*size = bhnd_nvram_io_getsize(sprom->data);
	return (0);
}

static int
bhnd_nvram_sprom_serialize(struct bhnd_nvram_data *nv, void *buf, size_t *len)
{
	struct bhnd_nvram_sprom	*sprom;
	size_t			 limit, req_len;
	int			 error;

	sprom = (struct bhnd_nvram_sprom *)nv;
	limit = *len;

	/* Provide the required size */
	if ((error = bhnd_nvram_sprom_size(nv, &req_len)))
		return (error);

	*len = req_len;

	if (buf == NULL) {
		return (0);
	} else if (*len > limit) {
		return (ENOMEM);
	}

	/* Write to the output buffer */
	return (bhnd_nvram_io_read(sprom->data, 0x0, buf, *len));
}

static uint32_t
bhnd_nvram_sprom_caps(struct bhnd_nvram_data *nv)
{
	return (BHND_NVRAM_DATA_CAP_INDEXED);
}

static const char *
bhnd_nvram_sprom_next(struct bhnd_nvram_data *nv, void **cookiep)
{
	struct bhnd_nvram_sprom		*sp;
	struct sprom_opcode_idx		*idx_entry;
	size_t				 idx_next;
	const struct bhnd_nvram_vardefn	*var;

	sp = (struct bhnd_nvram_sprom *)nv;

	/* Seek to appropriate starting point */
	if (*cookiep == NULL) {
		/* Start search at first index entry */
		idx_next = 0;
	} else {
		/* Determine current index position */
		idx_entry = *cookiep;
		idx_next = (size_t)(idx_entry - sp->idx);
		BHND_NV_ASSERT(idx_next < sp->num_idx,
		    ("invalid index %zu; corrupt cookie?", idx_next));

		/* Advance to next entry */
		idx_next++;

		/* Check for EOF */
		if (idx_next == sp->num_idx)
			return (NULL);
	}

	/* Skip entries that are disabled by virtue of IGNALL1 */
	for (; idx_next < sp->num_idx; idx_next++) {
		/* Fetch index entry and update cookiep  */
		idx_entry = &sp->idx[idx_next];
		*cookiep = idx_entry;

		/* Fetch variable definition */
		var = bhnd_nvram_get_vardefn(idx_entry->vid);

		/* We might need to parse the variable's value to determine
		 * whether it should be treated as unset */
		if (var->flags & BHND_NVRAM_VF_IGNALL1) {
			int     error;
			size_t  len;

			error = bhnd_nvram_sprom_getvar(nv, *cookiep, NULL,
			    &len, var->type);
			if (error) {
				BHND_NV_ASSERT(error == ENOENT, ("unexpected "
				    "error parsing variable: %d", error));

				continue;
			}
		}

		/* Found! */
		return (var->name);
	}

	/* Reached end of index entries */
	return (NULL);
}

/* bsearch function used by bhnd_nvram_sprom_find() */
static int
bhnd_nvram_sprom_find_vid_compare(const void *key, const void *rhs)
{
	const struct sprom_opcode_idx	*r;
	size_t				 l;

	l = *(const size_t *)key;
	r = rhs;

	if (l < r->vid)
		return (-1);
	if (l > r->vid)
		return (1);
	return (0);
}

static void *
bhnd_nvram_sprom_find(struct bhnd_nvram_data *nv, const char *name)
{
	struct bhnd_nvram_sprom		*sp;
	const struct bhnd_nvram_vardefn	*var;
	size_t				 vid;

	sp = (struct bhnd_nvram_sprom *)nv;

	/* Determine the variable ID for the given name */
	if ((var = bhnd_nvram_find_vardefn(name)) == NULL)
		return (NULL);

	vid = bhnd_nvram_get_vardefn_id(var);

	/* Search our index for the variable ID */
	return (bsearch(&vid, sp->idx, sp->num_idx, sizeof(sp->idx[0]),
	    bhnd_nvram_sprom_find_vid_compare));
}

/**
 * Read the value of @p type from the SPROM data at @p offset, apply @p mask
 * and @p shift, and OR with the existing @p value.
 * 
 * @param sp The SPROM data instance.
 * @param var The NVRAM variable definition
 * @param type The type to read at @p offset
 * @param offset The data offset to be read.
 * @param mask The mask to be applied to the value read at @p offset.
 * @param shift The shift to be applied after masking; if positive, a right
 * shift will be applied, if negative, a left shift.
 * @param value The read destination; the parsed value will be OR'd with the
 * current contents of @p value.
 */
static int
bhnd_nvram_sprom_read_offset(struct bhnd_nvram_sprom *sp,
    const struct bhnd_nvram_vardefn *var, bhnd_nvram_type type,
    size_t offset, uint32_t mask, int8_t shift,
    union bhnd_nvram_sprom_intv *value)
{
	size_t	sp_width;
	int	error;
	union {
		uint8_t		u8;
		uint16_t	u16;
		uint32_t	u32;
		int8_t		s8;
		int16_t		s16;
		int32_t		s32;
	} sp_value;

	/* Determine type width */
	sp_width = bhnd_nvram_value_size(type, NULL, 0, 1);
	if (sp_width == 0) {
		/* Variable-width types are unsupported */
		BHND_NV_LOG("invalid %s SPROM offset type %d\n", var->name,
		    type);
		return (EFTYPE);
	}

	/* Perform read */
	error = bhnd_nvram_io_read(sp->data, offset, &sp_value,
	    sp_width);
	if (error) {
		BHND_NV_LOG("error reading %s SPROM offset %#zx: %d\n",
		    var->name, offset, error);
		return (EFTYPE);
	}

#define	NV_PARSE_INT(_type, _src, _dest, _swap)	do {			\
	/* Swap to host byte order */					\
	sp_value. _src = (_type) _swap(sp_value. _src);			\
									\
	/* Mask and shift the value */					\
	sp_value. _src &= mask;				\
	if (shift > 0) {					\
		sp_value. _src >>= shift;			\
	} else if (shift < 0) {				\
		sp_value. _src <<= -shift;			\
	}								\
									\
	/* Emit output, widening to 32-bit representation  */		\
	value-> _dest |= sp_value. _src;				\
} while(0)

	/* Apply mask/shift and widen to a common 32bit representation */
	switch (type) {
	case BHND_NVRAM_TYPE_UINT8:
		NV_PARSE_INT(uint8_t,	u8,	u32,	);
		break;
	case BHND_NVRAM_TYPE_UINT16:
		NV_PARSE_INT(uint16_t,	u16,	u32,	le16toh);
		break;
	case BHND_NVRAM_TYPE_UINT32:
		NV_PARSE_INT(uint32_t,	u32,	u32,	le32toh);
		break;
	case BHND_NVRAM_TYPE_INT8:
		NV_PARSE_INT(int8_t,	s8,	s32,	);
		break;
	case BHND_NVRAM_TYPE_INT16:
		NV_PARSE_INT(int16_t,	s16,	s32,	le16toh);
		break;
	case BHND_NVRAM_TYPE_INT32:
		NV_PARSE_INT(int32_t,	s32,	s32,	le32toh);
		break;
	case BHND_NVRAM_TYPE_CHAR:
		NV_PARSE_INT(uint8_t,	u8,	u32,	);
		break;

	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_INT64:
	case BHND_NVRAM_TYPE_STRING:
		/* fallthrough (unused by SPROM) */
	default:
		BHND_NV_LOG("unhandled %s offset type: %d\n", var->name, type);
		return (EFTYPE);
	}

	return (0);
}

static int
bhnd_nvram_sprom_getvar(struct bhnd_nvram_data *nv, void *cookiep, void *buf,
    size_t *len, bhnd_nvram_type otype)
{
	bhnd_nvram_val_t		 val;
	struct bhnd_nvram_sprom		*sp;
	struct sprom_opcode_idx		*idx;
	const struct bhnd_nvram_vardefn	*var;
	union bhnd_nvram_sprom_storage	 storage;
	union bhnd_nvram_sprom_storage	*inp;
	union bhnd_nvram_sprom_intv	 intv;
	bhnd_nvram_type			 var_btype;
	size_t				 ilen, ipos, iwidth;
	size_t				 nelem;
	bool				 all_bits_set;
	int				 error;

	sp = (struct bhnd_nvram_sprom *)nv;
	idx = cookiep;

	BHND_NV_ASSERT(cookiep != NULL, ("NULL variable cookiep"));

	/* Fetch canonical variable definition */
	var = SPROM_COOKIE_TO_NVRAM(cookiep);
	BHND_NV_ASSERT(var != NULL, ("invalid cookiep %p", cookiep));

	/*
	 * Fetch the array length from the SPROM variable definition.
	 *
	 * This generally be identical to the array length provided by the
	 * canonical NVRAM variable definition, but some SPROM layouts may
	 * define a smaller element count.
	 */
	if ((error = sprom_opcode_parse_var(&sp->state, idx))) {
		BHND_NV_LOG("variable evaluation failed: %d\n", error);
		return (error);
	}

	nelem = sp->state.var.nelem;
	if (nelem > var->nelem) {
		BHND_NV_LOG("SPROM array element count %zu cannot be "
		    "represented by '%s' element count of %hhu\n", nelem,
		    var->name, var->nelem);
		return (EFTYPE);
	}

	/* Fetch the var's base element type */
	var_btype = bhnd_nvram_base_type(var->type);

	/* Calculate total byte length of the native encoding */
	if ((iwidth = bhnd_nvram_value_size(var_btype, NULL, 0, 1)) == 0) {
		/* SPROM does not use (and we do not support) decoding of
		 * variable-width data types */
		BHND_NV_LOG("invalid SPROM data type: %d", var->type);
		return (EFTYPE);
	}
	ilen = nelem * iwidth;

	/* Decode into our own local storage. */
	inp = &storage;
	if (ilen > sizeof(storage)) {
		BHND_NV_LOG("error decoding '%s', SPROM_ARRAY_MAXLEN "
		    "incorrect\n", var->name);
		return (EFTYPE);
	}

	/* Zero-initialize our decode buffer; any output elements skipped
	 * during decode should default to zero. */
	memset(inp, 0, ilen);

	/*
	 * Decode the SPROM data, iteratively decoding up to nelem values.
	 */
	if ((error = sprom_opcode_state_seek(&sp->state, idx))) {
		BHND_NV_LOG("variable seek failed: %d\n", error);
		return (error);
	}

	ipos = 0;
	intv.u32 = 0x0;
	if (var->flags & BHND_NVRAM_VF_IGNALL1)
		all_bits_set = true;
	else
		all_bits_set = false;
	while ((error = sprom_opcode_next_binding(&sp->state)) == 0) {
		struct sprom_opcode_bind	*binding;
		struct sprom_opcode_var		*binding_var;
		bhnd_nvram_type			 intv_type;
		size_t				 offset;
		size_t				 nbyte;
		uint32_t			 skip_in_bytes;
		void				*ptr;

		BHND_NV_ASSERT(
		    sp->state.var_state >= SPROM_OPCODE_VAR_STATE_OPEN,
		    ("invalid var state"));
		BHND_NV_ASSERT(sp->state.var.have_bind, ("invalid bind state"));

		binding_var = &sp->state.var;
		binding = &sp->state.var.bind;

		if (ipos >= nelem) {
			BHND_NV_LOG("output skip %u positioned "
			    "%zu beyond nelem %zu\n",
			    binding->skip_out, ipos, nelem);
			return (EINVAL);
		}

		/* Calculate input skip bytes for this binding */
		skip_in_bytes = binding->skip_in;
		error = sprom_opcode_apply_scale(&sp->state, &skip_in_bytes);
		if (error)
			return (error);

		/* Bind */
		offset = sp->state.offset;
		for (size_t i = 0; i < binding->count; i++) {
			/* Read the offset value, OR'ing with the current
			 * value of intv */
			error = bhnd_nvram_sprom_read_offset(sp, var,
			    binding_var->base_type,
			    offset,
			    binding_var->mask,
			    binding_var->shift,
			    &intv);
			if (error)
				return (error);

			/* If IGNALL1, record whether value does not have
			 * all bits set. */
			if (var->flags & BHND_NVRAM_VF_IGNALL1 &&
			    all_bits_set)
			{
				uint32_t all1;

				all1 = binding_var->mask;
				if (binding_var->shift > 0)
					all1 >>= binding_var->shift;
				else if (binding_var->shift < 0)
					all1 <<= -binding_var->shift;

				if ((intv.u32 & all1) != all1)
					all_bits_set = false;
			}

			/* Adjust input position; this was already verified to
			 * not overflow/underflow during SPROM opcode
			 * evaluation */
			if (binding->skip_in_negative) {
				offset -= skip_in_bytes;
			} else {
				offset += skip_in_bytes;
			}

			/* Skip writing to inp if additional bindings are
			 * required to fully populate intv */
			if (binding->skip_out == 0)
				continue;

			/* We use bhnd_nvram_value_coerce() to perform
			 * overflow-checked coercion from the widened
			 * uint32/int32 intv value to the requested output
			 * type */
			if (bhnd_nvram_is_signed_type(var_btype))
				intv_type = BHND_NVRAM_TYPE_INT32;
			else
				intv_type = BHND_NVRAM_TYPE_UINT32;

			/* Calculate address of the current element output
			 * position */
			ptr = (uint8_t *)inp + (iwidth * ipos);

			/* Perform coercion of the array element */
			nbyte = iwidth;
			error = bhnd_nvram_value_coerce(&intv, sizeof(intv),
			    intv_type, ptr, &nbyte, var_btype);
			if (error)
				return (error);

			/* Clear temporary state */
			intv.u32 = 0x0;

			/* Advance output position */
			if (SIZE_MAX - binding->skip_out < ipos) {
				BHND_NV_LOG("output skip %u would overflow "
				    "%zu\n", binding->skip_out, ipos);
				return (EINVAL);
			}

			ipos += binding->skip_out;
		}
	}

	/* Did we iterate all bindings until hitting end of the variable
	 * definition? */
	BHND_NV_ASSERT(error != 0, ("loop terminated early"));
	if (error != ENOENT) {
		return (error);
	}

	/* If marked IGNALL1 and all bits are set, treat variable as
	 * unavailable */
	if ((var->flags & BHND_NVRAM_VF_IGNALL1) && all_bits_set)
		return (ENOENT);


	/* Perform value coercion from our local representation */
	error = bhnd_nvram_val_init(&val, var->fmt, inp, ilen, var->type,
	    BHND_NVRAM_VAL_BORROW_DATA);
	if (error)
		return (error);

	error = bhnd_nvram_val_encode(&val, buf, len, otype);

	/* Clean up */
	bhnd_nvram_val_release(&val);
	return (error);
}

static const void *
bhnd_nvram_sprom_getvar_ptr(struct bhnd_nvram_data *nv, void *cookiep,
    size_t *len, bhnd_nvram_type *type)
{
	/* Unsupported */
	return (NULL);
}

static const char *
bhnd_nvram_sprom_getvar_name(struct bhnd_nvram_data *nv, void *cookiep)
{
	const struct bhnd_nvram_vardefn	*var;

	BHND_NV_ASSERT(cookiep != NULL, ("NULL variable cookiep"));

	var = SPROM_COOKIE_TO_NVRAM(cookiep);
	BHND_NV_ASSERT(var != NULL, ("invalid cookiep %p", cookiep));

	return (var->name);
}

/**
 * Initialize SPROM opcode evaluation state.
 * 
 * @param state The opcode state to be initialized.
 * @param layout The SPROM layout to be parsed by this instance.
 * 
 * 
 * @retval 0 success
 * @retval non-zero If initialization fails, a regular unix error code will be
 * returned.
 */
static int
sprom_opcode_state_init(struct sprom_opcode_state *state,
    const struct bhnd_sprom_layout *layout)
{
	memset(state, 0, sizeof(*state));

	state->layout = layout;
	state->input = layout->bindings;
	state->var_state = SPROM_OPCODE_VAR_STATE_NONE;

	bit_set(state->revs, layout->rev);

	return (0);
}

/**
 * Reset SPROM opcode evaluation state; future evaluation will be performed
 * starting at the first opcode.
 * 
 * @param state The opcode state to be reset.
 *
 * @retval 0 success
 * @retval non-zero If reset fails, a regular unix error code will be returned.
 */
static int
sprom_opcode_state_reset(struct sprom_opcode_state *state)
{
	return (sprom_opcode_state_init(state, state->layout));
}

/**
 * Reset SPROM opcode evaluation state and seek to the @p indexed position.
 * 
 * @param state The opcode state to be reset.
 * @param indexed The indexed location to which we'll seek the opcode state.
 */
static int
sprom_opcode_state_seek(struct sprom_opcode_state *state,
    struct sprom_opcode_idx *indexed)
{
	int error;

	BHND_NV_ASSERT(indexed->opcodes < state->layout->bindings_size,
	    ("index entry references invalid opcode position"));

	/* Reset state */
	if ((error = sprom_opcode_state_reset(state)))
		return (error);

	/* Seek to the indexed sprom opcode offset */
	state->input = state->layout->bindings + indexed->opcodes;

	/* Restore the indexed sprom data offset and VID */
	state->offset = indexed->offset;

	/* Restore the indexed sprom variable ID */
	if ((error = sprom_opcode_set_var(state, indexed->vid)))
		return (error);

	return (0);
}

/**
 * Set the current revision range for @p state. This also resets
 * variable state.
 * 
 * @param state The opcode state to update
 * @param start The first revision in the range.
 * @param end The last revision in the range.
 *
 * @retval 0 success
 * @retval non-zero If updating @p state fails, a regular unix error code will
 * be returned.
 */
static inline int
sprom_opcode_set_revs(struct sprom_opcode_state *state, uint8_t start,
    uint8_t end)
{
	int error;

	/* Validate the revision range */
	if (start > SPROM_OP_REV_MAX ||
	    end > SPROM_OP_REV_MAX ||
	    end < start)
	{
		SPROM_OP_BAD(state, "invalid revision range: %hhu-%hhu\n",
		    start, end);
		return (EINVAL);
	}

	/* Clear variable state */
	if ((error = sprom_opcode_clear_var(state)))
		return (error);

	/* Reset revision mask */
	memset(state->revs, 0x0, sizeof(state->revs));
	bit_nset(state->revs, start, end);

	return (0);
}

/**
 * Set the current variable's value mask for @p state.
 * 
 * @param state The opcode state to update
 * @param mask The mask to be set
 *
 * @retval 0 success
 * @retval non-zero If updating @p state fails, a regular unix error code will
 * be returned.
 */
static inline int
sprom_opcode_set_mask(struct sprom_opcode_state *state, uint32_t mask)
{
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "no open variable definition\n");
		return (EINVAL);
	}

	state->var.mask = mask;
	return (0);
}

/**
 * Set the current variable's value shift for @p state.
 * 
 * @param state The opcode state to update
 * @param shift The shift to be set
 *
 * @retval 0 success
 * @retval non-zero If updating @p state fails, a regular unix error code will
 * be returned.
 */
static inline int
sprom_opcode_set_shift(struct sprom_opcode_state *state, int8_t shift)
{
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "no open variable definition\n");
		return (EINVAL);
	}

	state->var.shift = shift;
	return (0);
}

/**
 * Register a new BIND/BINDN operation with @p state.
 * 
 * @param state The opcode state to update.
 * @param count The number of elements to be bound.
 * @param skip_in The number of input elements to skip after each bind.
 * @param skip_in_negative If true, the input skip should be subtracted from
 * the current offset after each bind. If false, the input skip should be
 * added.
 * @param skip_out The number of output elements to skip after each bind.
 * 
 * @retval 0 success
 * @retval EINVAL if a variable definition is not open.
 * @retval EINVAL if @p skip_in and @p count would trigger an overflow or
 * underflow when applied to the current input offset.
 * @retval ERANGE if @p skip_in would overflow uint32_t when multiplied by
 * @p count and the scale value.
 * @retval ERANGE if @p skip_out would overflow uint32_t when multiplied by
 * @p count and the scale value.
 * @retval non-zero If updating @p state otherwise fails, a regular unix error
 * code will be returned.
 */
static inline int
sprom_opcode_set_bind(struct sprom_opcode_state *state, uint8_t count,
    uint8_t skip_in, bool skip_in_negative, uint8_t skip_out)
{
	uint32_t	iskip_total;
	uint32_t	iskip_scaled;
	int		error;

	/* Must have an open variable */
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "no open variable definition\n");
		SPROM_OP_BAD(state, "BIND outside of variable definition\n");
		return (EINVAL);
	}

	/* Cannot overwite an existing bind definition */
	if (state->var.have_bind) {
		SPROM_OP_BAD(state, "BIND overwrites existing definition\n");
		return (EINVAL);
	}

	/* Must have a count of at least 1 */
	if (count == 0) {
		SPROM_OP_BAD(state, "BIND with zero count\n");
		return (EINVAL);
	}

	/* Scale skip_in by the current type width */
	iskip_scaled = skip_in;
	if ((error = sprom_opcode_apply_scale(state, &iskip_scaled)))
		return (error);

	/* Calculate total input bytes skipped: iskip_scaled * count) */
	if (iskip_scaled > 0 && UINT32_MAX / iskip_scaled < count) {
		SPROM_OP_BAD(state, "skip_in %hhu would overflow", skip_in);
		return (EINVAL);
	}

	iskip_total = iskip_scaled * count;

	/* Verify that the skip_in value won't under/overflow the current
	 * input offset. */
	if (skip_in_negative) {
		if (iskip_total > state->offset) {
			SPROM_OP_BAD(state, "skip_in %hhu would underflow "
			    "offset %u\n", skip_in, state->offset);
			return (EINVAL);
		}
	} else {
		if (UINT32_MAX - iskip_total < state->offset) {
			SPROM_OP_BAD(state, "skip_in %hhu would overflow "
			    "offset %u\n", skip_in, state->offset);
			return (EINVAL);
		}
	}

	/* Set the actual count and skip values */
	state->var.have_bind = true;
	state->var.bind.count = count;
	state->var.bind.skip_in = skip_in;
	state->var.bind.skip_out = skip_out;

	state->var.bind.skip_in_negative = skip_in_negative;

	/* Update total bind count for the current variable */
	state->var.bind_total++;

	return (0);
}


/**
 * Apply and clear the current opcode bind state, if any.
 * 
 * @param state The opcode state to update.
 * 
 * @retval 0 success
 * @retval non-zero If updating @p state otherwise fails, a regular unix error
 * code will be returned.
 */
static int
sprom_opcode_flush_bind(struct sprom_opcode_state *state)
{
	int		error;
	uint32_t	skip;

	/* Nothing to do? */
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN ||
	    !state->var.have_bind)
		return (0);

	/* Apply SPROM offset adjustment */
	if (state->var.bind.count > 0) {
		skip = state->var.bind.skip_in * state->var.bind.count;
		if ((error = sprom_opcode_apply_scale(state, &skip)))
			return (error);

		if (state->var.bind.skip_in_negative) {
			state->offset -= skip;
		} else {
			state->offset += skip;
		}
	}

	/* Clear bind state */
	memset(&state->var.bind, 0, sizeof(state->var.bind));
	state->var.have_bind = false;

	return (0);
}

/**
 * Set the current type to @p type, and reset type-specific
 * stream state.
 *
 * @param state The opcode state to update.
 * @param type The new type.
 * 
 * @retval 0 success
 * @retval EINVAL if @p vid is not a valid variable ID.
 */
static int
sprom_opcode_set_type(struct sprom_opcode_state *state, bhnd_nvram_type type)
{
	bhnd_nvram_type	base_type;
	size_t		width;
	uint32_t	mask;

	/* Must have an open variable definition */
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "type set outside variable definition\n");
		return (EINVAL);
	}

	/* Fetch type width for use as our scale value */
	width = bhnd_nvram_value_size(type, NULL, 0, 1);
	if (width == 0) {
		SPROM_OP_BAD(state, "unsupported variable-width type: %d\n",
		    type);
		return (EINVAL);
	} else if (width > UINT32_MAX) {
		SPROM_OP_BAD(state, "invalid type width %zu for type: %d\n",
		    width, type);
		return (EINVAL);
	}

	/* Determine default mask value for the element type */
	base_type = bhnd_nvram_base_type(type);
	switch (base_type) {
	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_CHAR:
		mask = UINT8_MAX;
		break;
	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_INT16:
		mask = UINT16_MAX;
		break;
	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_INT32:
		mask = UINT32_MAX;
		break;
	case BHND_NVRAM_TYPE_STRING:
		/* fallthrough (unused by SPROM) */
	default:
		SPROM_OP_BAD(state, "unsupported type: %d\n", type);
		return (EINVAL);
	}
	
	/* Update state */
	state->var.base_type = base_type;
	state->var.mask = mask;
	state->var.scale = (uint32_t)width;

	return (0);
}

/**
 * Clear current variable state, if any.
 * 
 * @param state The opcode state to update.
 */
static int
sprom_opcode_clear_var(struct sprom_opcode_state *state)
{
	if (state->var_state == SPROM_OPCODE_VAR_STATE_NONE)
		return (0);

	BHND_NV_ASSERT(state->var_state == SPROM_OPCODE_VAR_STATE_DONE,
	    ("incomplete variable definition"));
	BHND_NV_ASSERT(!state->var.have_bind, ("stale bind state"));

	memset(&state->var, 0, sizeof(state->var));
	state->var_state = SPROM_OPCODE_VAR_STATE_NONE;

	return (0);
}

/**
 * Set the current variable's array element count to @p nelem.
 *
 * @param state The opcode state to update.
 * @param nelem The new array length.
 * 
 * @retval 0 success
 * @retval EINVAL if no open variable definition exists.
 * @retval EINVAL if @p nelem is zero.
 * @retval ENXIO if @p nelem is greater than one, and the current variable does
 * not have an array type.
 * @retval ENXIO if @p nelem exceeds the array length of the NVRAM variable
 * definition.
 */
static int
sprom_opcode_set_nelem(struct sprom_opcode_state *state, uint8_t nelem)
{
	const struct bhnd_nvram_vardefn	*var;

	/* Must have a defined variable */
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "array length set without open variable "
		    "state");
		return (EINVAL);
	}

	/* Locate the actual variable definition */
	if ((var = bhnd_nvram_get_vardefn(state->vid)) == NULL) {
		SPROM_OP_BAD(state, "unknown variable ID: %zu\n", state->vid);
		return (EINVAL);
	}

	/* Must be greater than zero */
	if (nelem == 0) {
		SPROM_OP_BAD(state, "invalid nelem: %hhu\n", nelem);
		return (EINVAL);
	}

	/* If the variable is not an array-typed value, the array length
	 * must be 1 */
	if (!bhnd_nvram_is_array_type(var->type) && nelem != 1) {
		SPROM_OP_BAD(state, "nelem %hhu on non-array %zu\n", nelem,
		    state->vid);
		return (ENXIO);
	}
	
	/* Cannot exceed the variable's defined array length */
	if (nelem > var->nelem) {
		SPROM_OP_BAD(state, "nelem %hhu exceeds %zu length %hhu\n",
		    nelem, state->vid, var->nelem);
		return (ENXIO);
	}

	/* Valid length; update state */
	state->var.nelem = nelem;

	return (0);
}

/**
 * Set the current variable ID to @p vid, and reset variable-specific
 * stream state.
 *
 * @param state The opcode state to update.
 * @param vid The new variable ID.
 * 
 * @retval 0 success
 * @retval EINVAL if @p vid is not a valid variable ID.
 */
static int
sprom_opcode_set_var(struct sprom_opcode_state *state, size_t vid)
{
	const struct bhnd_nvram_vardefn	*var;
	int				 error;

	BHND_NV_ASSERT(state->var_state == SPROM_OPCODE_VAR_STATE_NONE,
	    ("overwrite of open variable definition"));

	/* Locate the variable definition */
	if ((var = bhnd_nvram_get_vardefn(vid)) == NULL) {
		SPROM_OP_BAD(state, "unknown variable ID: %zu\n", vid);
		return (EINVAL);
	}

	/* Update vid and var state */
	state->vid = vid;
	state->var_state = SPROM_OPCODE_VAR_STATE_OPEN;

	/* Initialize default variable record values */
	memset(&state->var, 0x0, sizeof(state->var));

	/* Set initial base type */
	if ((error = sprom_opcode_set_type(state, var->type)))
		return (error);

	/* Set default array length */
	if ((error = sprom_opcode_set_nelem(state, var->nelem)))
		return (error);

	return (0);
}

/**
 * Mark the currently open variable definition as complete.
 * 
 * @param state The opcode state to update.
 *
 * @retval 0 success
 * @retval EINVAL if no incomplete open variable definition exists.
 */
static int
sprom_opcode_end_var(struct sprom_opcode_state *state)
{
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "no open variable definition\n");
		return (EINVAL);
	}

	state->var_state = SPROM_OPCODE_VAR_STATE_DONE;
	return (0);
}

/**
 * Apply the current scale to @p value.
 * 
 * @param state The SPROM opcode state.
 * @param[in,out] value The value to scale
 * 
 * @retval 0 success
 * @retval EINVAL if no open variable definition exists.
 * @retval EINVAL if applying the current scale would overflow.
 */
static int
sprom_opcode_apply_scale(struct sprom_opcode_state *state, uint32_t *value)
{
	/* Must have a defined variable (and thus, scale) */
	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN) {
		SPROM_OP_BAD(state, "scaled value encoded without open "
		    "variable state");
		return (EINVAL);
	}

	/* Applying the scale value must not overflow */
	if (UINT32_MAX / state->var.scale < *value) {
		SPROM_OP_BAD(state, "cannot represent %" PRIu32 " * %" PRIu32
		    "\n", *value, state->var.scale);
		return (EINVAL);
	}

	*value = (*value) * state->var.scale;
	return (0);
}

/**
 * Read a SPROM_OP_DATA_* value from @p opcodes.
 * 
 * @param state The SPROM opcode state.
 * @param type The SROM_OP_DATA_* type to be read.
 * @param opval On success, the 32bit data representation. If @p type is signed,
 * the value will be appropriately sign extended and may be directly cast to
 * int32_t.
 * 
 * @retval 0 success
 * @retval non-zero If reading the value otherwise fails, a regular unix error
 * code will be returned.
 */
static int
sprom_opcode_read_opval32(struct sprom_opcode_state *state, uint8_t type,
   uint32_t *opval)
{
	const uint8_t	*p;
	int		 error;

	p = state->input;
	switch (type) {
	case SPROM_OP_DATA_I8:
		/* Convert to signed value first, then sign extend */
		*opval = (int32_t)(int8_t)(*p);
		p += 1;
		break;
	case SPROM_OP_DATA_U8:
		*opval = *p;
		p += 1;
		break;
	case SPROM_OP_DATA_U8_SCALED:
		*opval = *p;

		if ((error = sprom_opcode_apply_scale(state, opval)))
			return (error);

		p += 1;
		break;
	case SPROM_OP_DATA_U16:
		*opval = le16dec(p);
		p += 2;
		break;
	case SPROM_OP_DATA_U32:
		*opval = le32dec(p);
		p += 4;
		break;
	default:
		SPROM_OP_BAD(state, "unsupported data type: %hhu\n", type);
		return (EINVAL);
	}

	/* Update read address */
	state->input = p;

	return (0);
}

/**
 * Return true if our layout revision is currently defined by the SPROM
 * opcode state.
 * 
 * This may be used to test whether the current opcode stream state applies
 * to the layout that we are actually parsing.
 * 
 * A given opcode stream may cover multiple layout revisions, switching
 * between them prior to defining a set of variables.
 */
static inline bool
sprom_opcode_matches_layout_rev(struct sprom_opcode_state *state)
{
	return (bit_test(state->revs, state->layout->rev));
}

/**
 * When evaluating @p state and @p opcode, rewrite @p opcode and the current
 * evaluation state, as required.
 * 
 * If @p opcode is rewritten, it should be returned from
 * sprom_opcode_step() instead of the opcode parsed from @p state's opcode
 * stream.
 * 
 * If @p opcode remains unmodified, then sprom_opcode_step() should proceed
 * to standard evaluation.
 */
static int
sprom_opcode_rewrite_opcode(struct sprom_opcode_state *state, uint8_t *opcode)
{
	uint8_t	op;
	int	error;

	op = SPROM_OPCODE_OP(*opcode);
	switch (state->var_state) {
	case SPROM_OPCODE_VAR_STATE_NONE:
		/* No open variable definition */
		return (0);

	case SPROM_OPCODE_VAR_STATE_OPEN:
		/* Open variable definition; check for implicit closure. */

		/*
		 * If a variable definition contains no explicit bind
		 * instructions prior to closure, we must generate a DO_BIND
		 * instruction with count and skip values of 1.
		 */
		if (SPROM_OP_IS_VAR_END(op) &&
		    state->var.bind_total == 0)
		{
			uint8_t	count, skip_in, skip_out;
			bool	skip_in_negative;

			/* Create bind with skip_in/skip_out of 1, count of 1 */
			count = 1;
			skip_in = 1;
			skip_out = 1;
			skip_in_negative = false;

			error = sprom_opcode_set_bind(state, count, skip_in,
			    skip_in_negative, skip_out);
			if (error)
				return (error);

			/* Return DO_BIND */
			*opcode = SPROM_OPCODE_DO_BIND |
			    (0 << SPROM_OP_BIND_SKIP_IN_SIGN) |
			    (1 << SPROM_OP_BIND_SKIP_IN_SHIFT) |
			    (1 << SPROM_OP_BIND_SKIP_OUT_SHIFT);

			return (0);
		}

		/*
		 * If a variable is implicitly closed (e.g. by a new variable
		 * definition), we must generate a VAR_END instruction.
		 */
		if (SPROM_OP_IS_IMPLICIT_VAR_END(op)) {
			/* Mark as complete */
			if ((error = sprom_opcode_end_var(state)))
				return (error);

			/* Return VAR_END */
			*opcode = SPROM_OPCODE_VAR_END;
			return (0);
		}
		break;


	case SPROM_OPCODE_VAR_STATE_DONE:
		/* Previously completed variable definition. Discard variable
		 * state */
		return (sprom_opcode_clear_var(state));
	}

	/* Nothing to do */
	return (0);
}

/**
 * Evaluate one opcode from @p state.
 *
 * @param state The opcode state to be evaluated.
 * @param[out] opcode On success, the evaluated opcode
 * 
 * @retval 0 success
 * @retval ENOENT if EOF is reached
 * @retval non-zero if evaluation otherwise fails, a regular unix error
 * code will be returned.
 */
static int
sprom_opcode_step(struct sprom_opcode_state *state, uint8_t *opcode)
{
	int error;

	while (*state->input != SPROM_OPCODE_EOF) {
		uint32_t	val;
		uint8_t		op, rewrite, immd;

		/* Fetch opcode */
		*opcode = *state->input;
		op = SPROM_OPCODE_OP(*opcode);
		immd = SPROM_OPCODE_IMM(*opcode);

		/* Clear any existing bind state */
		if ((error = sprom_opcode_flush_bind(state)))
			return (error);

		/* Insert local opcode based on current state? */
		rewrite = *opcode;
		if ((error = sprom_opcode_rewrite_opcode(state, &rewrite)))
			return (error);

		if (rewrite != *opcode) {
			/* Provide rewritten opcode */
			*opcode = rewrite;

			/* We must keep evaluating until we hit a state
			 * applicable to the SPROM revision we're parsing */
			if (!sprom_opcode_matches_layout_rev(state))
				continue;

			return (0);
		}

		/* Advance input */
		state->input++;

		switch (op) {
		case SPROM_OPCODE_VAR_IMM:
			if ((error = sprom_opcode_set_var(state, immd)))
				return (error);
			break;

		case SPROM_OPCODE_VAR_REL_IMM:
			error = sprom_opcode_set_var(state, state->vid + immd);
			if (error)
				return (error);
			break;

		case SPROM_OPCODE_VAR:
			error = sprom_opcode_read_opval32(state, immd, &val);
			if (error)
				return (error);

			if ((error = sprom_opcode_set_var(state, val)))
				return (error);

			break;

		case SPROM_OPCODE_VAR_END:
			if ((error = sprom_opcode_end_var(state)))
				return (error);
			break;

		case SPROM_OPCODE_NELEM:
			immd = *state->input;
			if ((error = sprom_opcode_set_nelem(state, immd)))
				return (error);

			state->input++;
			break;

		case SPROM_OPCODE_DO_BIND:
		case SPROM_OPCODE_DO_BINDN: {
			uint8_t	count, skip_in, skip_out;
			bool	skip_in_negative;

			/* Fetch skip arguments */
			skip_in = (immd & SPROM_OP_BIND_SKIP_IN_MASK) >>
			    SPROM_OP_BIND_SKIP_IN_SHIFT;

			skip_in_negative =
			    ((immd & SPROM_OP_BIND_SKIP_IN_SIGN) != 0);

			skip_out = (immd & SPROM_OP_BIND_SKIP_OUT_MASK) >>
			      SPROM_OP_BIND_SKIP_OUT_SHIFT;

			/* Fetch count argument (if any) */
			if (op == SPROM_OPCODE_DO_BINDN) {
				/* Count is provided as trailing U8 */
				count = *state->input;
				state->input++;
			} else {
				count = 1;
			}

			/* Set BIND state */
			error = sprom_opcode_set_bind(state, count, skip_in,
			    skip_in_negative, skip_out);
			if (error)
				return (error);

			break;
		}
		case SPROM_OPCODE_DO_BINDN_IMM: {
			uint8_t	count, skip_in, skip_out;
			bool	skip_in_negative;

			/* Implicit skip_in/skip_out of 1, count encoded as immd
			 * value */
			count = immd;
			skip_in = 1;
			skip_out = 1;
			skip_in_negative = false;

			error = sprom_opcode_set_bind(state, count, skip_in,
			    skip_in_negative, skip_out);
			if (error)
				return (error);
			break;
		}

		case SPROM_OPCODE_REV_IMM:
			if ((error = sprom_opcode_set_revs(state, immd, immd)))
				return (error);
			break;

		case SPROM_OPCODE_REV_RANGE: {
			uint8_t range;
			uint8_t rstart, rend;

			/* Revision range is encoded in next byte, as
			 * { uint8_t start:4, uint8_t end:4 } */
			range = *state->input;
			rstart = (range & SPROM_OP_REV_START_MASK) >>
			    SPROM_OP_REV_START_SHIFT;
			rend = (range & SPROM_OP_REV_END_MASK) >>
			    SPROM_OP_REV_END_SHIFT;

			/* Update revision bitmask */
			error = sprom_opcode_set_revs(state, rstart, rend);
			if (error)
				return (error);

			/* Advance input */
			state->input++;
			break;
		}
		case SPROM_OPCODE_MASK_IMM:
			if ((error = sprom_opcode_set_mask(state, immd)))
				return (error);
			break;

		case SPROM_OPCODE_MASK:
			error = sprom_opcode_read_opval32(state, immd, &val);
			if (error)
				return (error);

			if ((error = sprom_opcode_set_mask(state, val)))
				return (error);
			break;

		case SPROM_OPCODE_SHIFT_IMM:
			if ((error = sprom_opcode_set_shift(state, immd * 2)))
				return (error);
			break;

		case SPROM_OPCODE_SHIFT: {
			int8_t shift;

			if (immd == SPROM_OP_DATA_I8) {
				shift = (int8_t)(*state->input);
			} else if (immd == SPROM_OP_DATA_U8) {
				val = *state->input;
				if (val > INT8_MAX) {
					SPROM_OP_BAD(state, "invalid shift "
					    "value: %#x\n", val);
				}

				shift = val;
			} else {
				SPROM_OP_BAD(state, "unsupported shift data "
				    "type: %#hhx\n", immd);
				return (EINVAL);
			}

			if ((error = sprom_opcode_set_shift(state, shift)))
				return (error);

			state->input++;
			break;
		}
		case SPROM_OPCODE_OFFSET_REL_IMM:
			/* Fetch unscaled relative offset */
			val = immd;

			/* Apply scale */
			if ((error = sprom_opcode_apply_scale(state, &val)))
				return (error);
	
			/* Adding val must not overflow our offset */
			if (UINT32_MAX - state->offset < val) {
				BHND_NV_LOG("offset out of range\n");
				return (EINVAL);
			}

			/* Adjust offset */
			state->offset += val;
			break;
		case SPROM_OPCODE_OFFSET:
			error = sprom_opcode_read_opval32(state, immd, &val);
			if (error)
				return (error);

			state->offset = val;
			break;

		case SPROM_OPCODE_TYPE:
			/* Type follows as U8 */
			immd = *state->input;
			state->input++;

			/* fall through */
		case SPROM_OPCODE_TYPE_IMM:
			switch (immd) {
			case BHND_NVRAM_TYPE_UINT8:
			case BHND_NVRAM_TYPE_UINT16:
			case BHND_NVRAM_TYPE_UINT32:
			case BHND_NVRAM_TYPE_UINT64:
			case BHND_NVRAM_TYPE_INT8:
			case BHND_NVRAM_TYPE_INT16:
			case BHND_NVRAM_TYPE_INT32:
			case BHND_NVRAM_TYPE_INT64:
			case BHND_NVRAM_TYPE_CHAR:
			case BHND_NVRAM_TYPE_STRING:
				error = sprom_opcode_set_type(state,
				    (bhnd_nvram_type)immd);
				if (error)
					return (error);
				break;
			default:
				BHND_NV_LOG("unrecognized type %#hhx\n", immd);
				return (EINVAL);
			}
			break;

		default:
			BHND_NV_LOG("unrecognized opcode %#hhx\n", *opcode);
			return (EINVAL);
		}

		/* We must keep evaluating until we hit a state applicable to
		 * the SPROM revision we're parsing */
		if (sprom_opcode_matches_layout_rev(state))
			return (0);
	}

	/* End of opcode stream */
	return (ENOENT);
}

/**
 * Reset SPROM opcode evaluation state, seek to the @p indexed position,
 * and perform complete evaluation of the variable's opcodes.
 * 
 * @param state The opcode state to be to be evaluated.
 * @param indexed The indexed variable location.
 *
 * @retval 0 success
 * @retval non-zero If evaluation fails, a regular unix error code will be
 * returned.
 */
static int
sprom_opcode_parse_var(struct sprom_opcode_state *state,
    struct sprom_opcode_idx *indexed)
{
	uint8_t	opcode;
	int	error;

	/* Seek to entry */
	if ((error = sprom_opcode_state_seek(state, indexed)))
		return (error);

	/* Parse full variable definition */
	while ((error = sprom_opcode_step(state, &opcode)) == 0) {
		/* Iterate until VAR_END */
		if (SPROM_OPCODE_OP(opcode) != SPROM_OPCODE_VAR_END)
			continue;

		BHND_NV_ASSERT(state->var_state == SPROM_OPCODE_VAR_STATE_DONE,
		    ("incomplete variable definition"));

		return (0);
	}

	/* Error parsing definition */
	return (error);
}

/**
 * Evaluate @p state until the next variable definition is found.
 * 
 * @param state The opcode state to be evaluated.
 * 
 * @retval 0 success
 * @retval ENOENT if no additional variable definitions are available.
 * @retval non-zero if evaluation otherwise fails, a regular unix error
 * code will be returned.
 */
static int
sprom_opcode_next_var(struct sprom_opcode_state *state)
{
	uint8_t	opcode;
	int	error;

	/* Step until we hit a variable opcode */
	while ((error = sprom_opcode_step(state, &opcode)) == 0) {
		switch (SPROM_OPCODE_OP(opcode)) {
		case SPROM_OPCODE_VAR:
		case SPROM_OPCODE_VAR_IMM:
		case SPROM_OPCODE_VAR_REL_IMM:
			BHND_NV_ASSERT(
			    state->var_state == SPROM_OPCODE_VAR_STATE_OPEN,
			    ("missing variable definition"));

			return (0);
		default:
			continue;
		}
	}

	/* Reached EOF, or evaluation failed */
	return (error);
}

/**
 * Evaluate @p state until the next binding for the current variable definition
 * is found.
 * 
 * @param state The opcode state to be evaluated.
 * 
 * @retval 0 success
 * @retval ENOENT if no additional binding opcodes are found prior to reaching
 * a new variable definition, or the end of @p state's binding opcodes.
 * @retval non-zero if evaluation otherwise fails, a regular unix error
 * code will be returned.
 */
static int
sprom_opcode_next_binding(struct sprom_opcode_state *state)
{
	uint8_t	opcode;
	int	error;

	if (state->var_state != SPROM_OPCODE_VAR_STATE_OPEN)
		return (EINVAL);

	/* Step until we hit a bind opcode, or a new variable */
	while ((error = sprom_opcode_step(state, &opcode)) == 0) {
		switch (SPROM_OPCODE_OP(opcode)) {
		case SPROM_OPCODE_DO_BIND:
		case SPROM_OPCODE_DO_BINDN:
		case SPROM_OPCODE_DO_BINDN_IMM:
			/* Found next bind */
			BHND_NV_ASSERT(
			    state->var_state == SPROM_OPCODE_VAR_STATE_OPEN,
			    ("missing variable definition"));
			BHND_NV_ASSERT(state->var.have_bind, ("missing bind"));

			return (0);

		case SPROM_OPCODE_VAR_END:
			/* No further binding opcodes */ 
			BHND_NV_ASSERT(
			    state->var_state == SPROM_OPCODE_VAR_STATE_DONE,
			    ("variable definition still available"));
			return (ENOENT);
		}
	}

	/* Not found, or evaluation failed */
	return (error);
}
