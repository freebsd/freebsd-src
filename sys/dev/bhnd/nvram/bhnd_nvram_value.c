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

#include <sys/param.h>
#include <sys/sbuf.h>

#ifdef _KERNEL

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/_inttypes.h>

#else /* !_KERNEL */

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#endif /* _KERNEL */

#include "bhnd_nvram_private.h"

#include "bhnd_nvram_valuevar.h"


static void	*bhnd_nvram_val_alloc_bytes(bhnd_nvram_val_t *value,
					    size_t ilen, bhnd_nvram_type itype,
					    uint32_t flags);
static int	 bhnd_nvram_val_set(bhnd_nvram_val_t *value, const void *inp,
				    size_t ilen, bhnd_nvram_type itype,
				    uint32_t flags);
static int	 bhnd_nvram_val_set_inline(bhnd_nvram_val_t *value,
		     const void *inp, size_t ilen, bhnd_nvram_type itype);

#define	BHND_NVRAM_VAL_INITIALIZER(_fmt, _storage)		\
	(bhnd_nvram_val_t) {					\
		.refs = 1,					\
		.val_storage = _storage,			\
		.fmt = _fmt,					\
		.data_storage = BHND_NVRAM_VAL_DATA_NONE,	\
	};


/** Assert that @p value's backing representation state has initialized
 *  as empty. */
#define	BHND_NVRAM_VAL_ASSERT_EMPTY(_value)			\
	BHND_NV_ASSERT(						\
	    value->data_storage == BHND_NVRAM_VAL_DATA_NONE &&	\
	    value->data_len == 0 &&				\
	    value->data.ptr == NULL,				\
	    ("previously initialized value"))

/* Common initialization support for bhnd_nvram_val_init() and
 * bhnd_nvram_val_new() */
static int
bhnd_nvram_val_init_common(bhnd_nvram_val_t *value, bhnd_nvram_val_storage_t
    val_storage, const bhnd_nvram_val_fmt_t *fmt, const void *inp, size_t ilen,
    bhnd_nvram_type itype, uint32_t flags)
{
	void		*outp;
	bhnd_nvram_type	 otype;
	size_t		 olen;
	int		 error;

	/* Determine expected data type, and allow the format to delegate to
	 * a new format instance */
	if (fmt != NULL && fmt->op_filter != NULL) {
		const bhnd_nvram_val_fmt_t *nfmt = fmt;

		/* Use the filter function to determine whether direct
		 * initialization from is itype permitted */
		error = fmt->op_filter(&nfmt, inp, ilen, itype);
		if (error)
			return (error);

		/* Retry initialization with new format? */
		if (nfmt != fmt) {
			return (bhnd_nvram_val_init_common(value, val_storage,
			    nfmt, inp, ilen, itype, flags));
		}

		/* Value can be initialized with provided input type */
		otype = itype;

	} else if (fmt != NULL) {
		/* Value must be initialized with the format's native
		 * type */
		otype = fmt->native_type;

	} else {
		/* No format specified; we can initialize directly from the
		 * input data, and we'll handle all format operations
		 * internally. */
		otype = itype;
	}

	/* Initialize value instance */
	*value = BHND_NVRAM_VAL_INITIALIZER(fmt, val_storage);

	/* If input data already in native format, init directly. */
	if (otype == itype) {
		error = bhnd_nvram_val_set(value, inp, ilen, itype, flags);
		if (error)
			return (error);

		return (0);
	}
	
	/* Determine size when encoded in native format */
	error = bhnd_nvram_value_coerce(inp, ilen, itype, NULL, &olen, otype);
	if (error)
		return (error);
	
	/* Fetch reference to (or allocate) an appropriately sized buffer */
	outp = bhnd_nvram_val_alloc_bytes(value, olen, otype, flags);
	if (outp == NULL)
		return (ENOMEM);
	
	/* Perform encode */
	error = bhnd_nvram_value_coerce(inp, ilen, itype, outp, &olen, otype);
	if (error)
		return (error);
	
	return (0);
}

/**
 * Initialize an externally allocated instance of @p value with @p fmt from the
 * given @p inp buffer of @p itype and @p ilen.
 *
 * On success, the caller owns a reference to @p value, and is responsible for
 * freeing any resources allocated for @p value via bhnd_nvram_val_release().
 *
 * @param	value	The externally allocated value instance to be
 *			initialized.
 * @param	fmt	The value's format, or NULL to use the default format
 *			for @p itype.
 * @param	inp	Input buffer.
 * @param	ilen	Input buffer length.
 * @param	itype	Input buffer type.
 * @param	flags	Value flags (see BHND_NVRAM_VAL_*).
 * 
 * @retval 0		success
 * @retval ENOMEM	If allocation fails.
 * @retval EFTYPE	If @p fmt initialization from @p itype is unsupported.
 * @retval EFAULT	if @p ilen is not correctly aligned for elements of
 *			@p itype.
 * @retval ERANGE	If value coercion would overflow (or underflow) the
 *			@p fmt representation.
 */
int
bhnd_nvram_val_init(bhnd_nvram_val_t *value, const bhnd_nvram_val_fmt_t *fmt,
    const void *inp, size_t ilen, bhnd_nvram_type itype, uint32_t flags)
{
	int error;

	error = bhnd_nvram_val_init_common(value, BHND_NVRAM_VAL_STORAGE_AUTO,
	    fmt, inp, ilen, itype, flags);
	if (error)
		bhnd_nvram_val_release(value);

	return (error);
}

/**
 * Allocate a value instance with @p fmt, and attempt to initialize its internal
 * representation from the given @p inp buffer of @p itype and @p ilen.
 *
 * On success, the caller owns a reference to @p value, and is responsible for
 * freeing any resources allocated for @p value via bhnd_nvram_val_release().
 *
 * @param[out]	value	On success, the allocated value instance.
 * @param	fmt	The value's format, or NULL to use the default format
 *			for @p itype.
 * @param	inp	Input buffer.
 * @param	ilen	Input buffer length.
 * @param	itype	Input buffer type.
 * @param	flags	Value flags (see BHND_NVRAM_VAL_*).
 * 
 * @retval 0		success
 * @retval ENOMEM	If allocation fails.
 * @retval EFTYPE	If @p fmt initialization from @p itype is unsupported.
 * @retval EFAULT	if @p ilen is not correctly aligned for elements of
 *			@p itype.
 * @retval ERANGE	If value coercion would overflow (or underflow) the
 *			@p fmt representation.
 */
int
bhnd_nvram_val_new(bhnd_nvram_val_t **value, const bhnd_nvram_val_fmt_t *fmt,
    const void *inp, size_t ilen, bhnd_nvram_type itype, uint32_t flags)
{
	int error;

	/* Allocate new instance */
	if ((*value = bhnd_nv_malloc(sizeof(**value))) == NULL)
		return (ENOMEM);

	/* Perform common initialization. */
	error = bhnd_nvram_val_init_common(*value,
	    BHND_NVRAM_VAL_STORAGE_DYNAMIC, fmt, inp, ilen, itype, flags);
	if (error) {
		/* Will also free() the value allocation */
		bhnd_nvram_val_release(*value);
	}

	return (error);
}

/**
 * Copy or retain a reference to @p value.
 * 
 * On success, the caller is responsible for freeing the result via
 * bhnd_nvram_val_release().
 * 
 * @param	value	The value to be copied (or retained).
 * 
 * @retval bhnd_nvram_val_t	if @p value was successfully copied or retained.
 * @retval NULL			if allocation failed.
 */
bhnd_nvram_val_t *
bhnd_nvram_val_copy(bhnd_nvram_val_t *value)
{
	bhnd_nvram_val_t	*result;
	const void		*bytes;
	bhnd_nvram_type		 type;
	size_t			 len;
	uint32_t		 flags;
	int			 error;

	/* If dynamically allocated, simply bump the reference count */
	if (value->val_storage == BHND_NVRAM_VAL_STORAGE_DYNAMIC) {
		refcount_acquire(&value->refs);
		return (value);
	}

	/* Otherwise, we need to perform an actual copy */
	BHND_NV_ASSERT(value->refs == 1, ("non-allocated value has "
	    "active refcount (%u)", value->refs));

	/* Compute the new value's flags based on the source value */
	switch (value->data_storage) {
	case BHND_NVRAM_VAL_DATA_NONE:
	case BHND_NVRAM_VAL_DATA_INLINE:
	case BHND_NVRAM_VAL_DATA_EXT_WEAK:
	case BHND_NVRAM_VAL_DATA_EXT_ALLOC:
		/* Copy the source data and permit additional allocation if the
		 * value cannot be represented inline */
		flags = BHND_NVRAM_VAL_COPY_DATA|BHND_NVRAM_VAL_DYNAMIC;
		break;
	case BHND_NVRAM_VAL_DATA_EXT_STATIC:
		flags = BHND_NVRAM_VAL_STATIC_DATA;
		break;
	default:
		BHND_NV_PANIC("invalid storage type: %d", value->data_storage);
	}

	/* Allocate new value copy */
	bytes = bhnd_nvram_val_bytes(value, &len, &type);
	error = bhnd_nvram_val_new(&result, value->fmt, bytes, len, type,
	    flags);
	if (error) {
		BHND_NV_LOG("copy failed: %d", error);
		return (NULL);
	}

	return (result);
}

/**
 * Release a reference to @p value.
 *
 * If this is the last reference, all associated resources will be freed.
 * 
 * @param	value	The value to be released.
 */
void
bhnd_nvram_val_release(bhnd_nvram_val_t *value)
{
	BHND_NV_ASSERT(value->refs >= 1, ("value over-released"));

	/* Drop reference */
	if (!refcount_release(&value->refs))
		return;

	/* Free allocated external representation data */
	if (value->data_storage == BHND_NVRAM_VAL_DATA_EXT_ALLOC)
		bhnd_nv_free(__DECONST(void *, value->data.ptr));

	/* Free instance if dynamically allocated */
	if (value->val_storage == BHND_NVRAM_VAL_STORAGE_DYNAMIC)
		bhnd_nv_free(value);
}

/**
 * Standard string/char array/char encoding implementation.
 *
 * Input type must be one of:
 * - BHND_NVRAM_TYPE_STRING
 * - BHND_NVRAM_TYPE_CHAR
 * - BHND_NVRAM_TYPE_CHAR_ARRAY
 */
static int
bhnd_nvram_val_encode_string(void *outp, size_t *olen, bhnd_nvram_type otype,
     const void *inp, size_t ilen, bhnd_nvram_type itype)
{
	const char	*cstr;
	bhnd_nvram_type	 otype_base;
	size_t		 cstr_size, cstr_len;
	size_t		 limit, nbytes;

	BHND_NV_ASSERT(
	    itype == BHND_NVRAM_TYPE_STRING ||
	    itype == BHND_NVRAM_TYPE_CHAR ||
	    itype == BHND_NVRAM_TYPE_CHAR_ARRAY,
	    ("unsupported type: %d", itype));

	cstr = inp;
	cstr_size = ilen;
	nbytes = 0;
	otype_base = bhnd_nvram_base_type(otype);

	/* Determine output byte limit */
	if (outp != NULL)
		limit = *olen;
	else
		limit = 0;

	/* Determine string length, minus trailing NUL (if any) */
	cstr_len = strnlen(cstr, cstr_size);

	/* Parse the field data */
	switch (otype) {
	case BHND_NVRAM_TYPE_CHAR:
	case BHND_NVRAM_TYPE_CHAR_ARRAY:
		/* String must contain exactly 1 non-terminating-NUL character
		 * to be represented as a single char */
		if (!bhnd_nvram_is_array_type(otype)) {
			if (cstr_len != 1)
				return (EFTYPE);
		}

		/* Copy out the characters directly (excluding trailing NUL) */
		for (size_t i = 0; i < cstr_len; i++) {
			if (limit > nbytes)
				*((uint8_t *)outp + nbytes) = cstr[i];
			nbytes++;
		}

		/* Provide required length */
		*olen = nbytes;
		if (limit < *olen && outp != NULL)
			return (ENOMEM);

		return (0);

	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_UINT8_ARRAY:
	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_UINT16_ARRAY:
	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_UINT32_ARRAY:
	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_UINT64_ARRAY:
	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_INT8_ARRAY:
	case BHND_NVRAM_TYPE_INT16:
	case BHND_NVRAM_TYPE_INT16_ARRAY:
	case BHND_NVRAM_TYPE_INT32:
	case BHND_NVRAM_TYPE_INT32_ARRAY:
	case BHND_NVRAM_TYPE_INT64:
	case BHND_NVRAM_TYPE_INT64_ARRAY: {
		const char	*p;
		size_t		 plen, parsed_len;
		int		 error;

		/* Trim leading/trailing whitespace */
		p = cstr;
		plen = bhnd_nvram_trim_field(&p, cstr_len, '\0');

		/* Try to parse the integer value */
		error = bhnd_nvram_parse_int(p, plen, 0, &parsed_len, outp,
		    olen, otype_base);
		if (error) {
			BHND_NV_DEBUG("error parsing '%.*s' as integer: %d\n",
			    BHND_NV_PRINT_WIDTH(plen), p, error);
			return (error);
		}

		/* Do additional bytes remain unparsed? */
		if (plen != parsed_len) {
			BHND_NV_DEBUG("error parsing '%.*s' as a single "
			    "integer value; trailing garbage '%.*s'\n",
			    BHND_NV_PRINT_WIDTH(plen), p,
			    BHND_NV_PRINT_WIDTH(plen-parsed_len), p+parsed_len);
			return (EFTYPE);
		}

		return (0);
	}

	case BHND_NVRAM_TYPE_STRING:
	case BHND_NVRAM_TYPE_STRING_ARRAY:
		/* Copy out the string representation as-is */
		*olen = cstr_size;

		/* Need additional space for trailing NUL? */
		if (cstr_len == cstr_size)
			(*olen)++;

		/* Skip output? */
		if (outp == NULL)
			return (0);

		/* Verify required length */
		if (limit < *olen)
			return (ENOMEM);

		/* Copy and NUL terminate */
		strncpy(outp, cstr, cstr_len);
		*((char *)outp + cstr_len) = '\0';

		return (0);
	}

	BHND_NV_PANIC("unknown type %s", bhnd_nvram_type_name(otype));
}

/**
 * Standard integer encoding implementation.
 */
static int
bhnd_nvram_val_encode_int(void *outp, size_t *olen, bhnd_nvram_type otype,
     const void *inp, size_t ilen, bhnd_nvram_type itype)
{
	bhnd_nvram_type	 otype_base;
	size_t		 limit, nbytes;
	bool		 itype_signed, otype_signed, otype_int;
	union {
		uint64_t	u64;
		int64_t		i64;
	} intv;

	BHND_NV_ASSERT(bhnd_nvram_is_int_type(itype), ("non-integer type"));

	/* Determine output byte limit */
	if (outp != NULL)
		limit = *olen;
	else
		limit = 0;

	/* Fetch output type info */
	otype_base = bhnd_nvram_base_type(otype);
	otype_int = bhnd_nvram_is_int_type(otype);
	otype_signed = bhnd_nvram_is_signed_type(otype_base);

	/*
	 * Promote integer value to a common 64-bit representation.
	 */
	switch (itype) {
	case BHND_NVRAM_TYPE_UINT8:
		if (ilen != sizeof(uint8_t))
			return (EFAULT);

		itype_signed = false;
		intv.u64 = *(const uint8_t *)inp;
		break;

	case BHND_NVRAM_TYPE_UINT16:
		if (ilen != sizeof(uint16_t))
			return (EFAULT);

		itype_signed = false;
		intv.u64 = *(const uint16_t *)inp;
		break;

	case BHND_NVRAM_TYPE_UINT32:
		if (ilen != sizeof(uint32_t))
			return (EFAULT);

		itype_signed = false;
		intv.u64 = *(const uint32_t *)inp;
		break;

	case BHND_NVRAM_TYPE_UINT64:
		if (ilen != sizeof(uint64_t))
			return (EFAULT);

		itype_signed = false;
		intv.u64 = *(const uint64_t *)inp;
		break;

	case BHND_NVRAM_TYPE_INT8:
		if (ilen != sizeof(int8_t))
			return (EFAULT);

		itype_signed = true;
		intv.i64 = *(const int8_t *)inp;
		break;

	case BHND_NVRAM_TYPE_INT16:
		if (ilen != sizeof(int16_t))
			return (EFAULT);

		itype_signed = true;
		intv.i64 = *(const int16_t *)inp;
		break;

	case BHND_NVRAM_TYPE_INT32:
		if (ilen != sizeof(int32_t))
			return (EFAULT);

		itype_signed = true;
		intv.i64 = *(const int32_t *)inp;
		break;

	case BHND_NVRAM_TYPE_INT64:
		if (ilen != sizeof(int32_t))
			return (EFAULT);

		itype_signed = true;
		intv.i64 = *(const int32_t *)inp;
		break;

	default:
		BHND_NV_PANIC("invalid type %d\n", itype);
	}

	/* Perform signed/unsigned conversion */
	if (itype_signed && otype_int && !otype_signed) {
		if (intv.i64 < 0) {
			/* Can't represent negative value */
			BHND_NV_LOG("cannot represent %" PRId64 " as %s\n",
			    intv.i64, bhnd_nvram_type_name(otype));

			return (ERANGE);
		}

		/* Convert to unsigned representation */
		intv.u64 = intv.i64;

	} else if (!itype_signed && otype_int && otype_signed) {
		/* Handle unsigned -> signed coercions */
		if (intv.u64 > INT64_MAX) {
			/* Can't represent positive value */
			BHND_NV_LOG("cannot represent %" PRIu64 " as %s\n",
			    intv.u64, bhnd_nvram_type_name(otype));
			return (ERANGE);
		}

		/* Convert to signed representation */
		intv.i64 = intv.u64;
	}

	/* Write output */
	switch (otype) {
	case BHND_NVRAM_TYPE_CHAR:
	case BHND_NVRAM_TYPE_CHAR_ARRAY:
	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_UINT8_ARRAY:
		if (intv.u64 > UINT8_MAX)
			return (ERANGE);

		nbytes = sizeof(uint8_t);
		if (limit >= nbytes)
			*((uint8_t *)outp) = (uint8_t)intv.u64;
		break;

	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_UINT16_ARRAY:
		if (intv.u64 > UINT16_MAX)
			return (ERANGE);

		nbytes = sizeof(uint16_t);
		if (limit >= nbytes)
			*((uint16_t *)outp) = (uint16_t)intv.u64;
		break;

	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_UINT32_ARRAY:
		if (intv.u64 > UINT32_MAX)
			return (ERANGE);

		nbytes = sizeof(uint32_t);
		if (limit >= nbytes)
			*((uint32_t *)outp) = (uint32_t)intv.u64;
		break;

	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_UINT64_ARRAY:
		nbytes = sizeof(uint64_t);
		if (limit >= nbytes)
			*((uint64_t *)outp) = intv.u64;
		break;

	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_INT8_ARRAY:
		if (intv.i64 < INT8_MIN || intv.i64 > INT8_MAX)
			return (ERANGE);

		nbytes = sizeof(int8_t);
		if (limit >= nbytes)
			*((int8_t *)outp) = (int8_t)intv.i64;
		break;

	case BHND_NVRAM_TYPE_INT16:
	case BHND_NVRAM_TYPE_INT16_ARRAY:
		if (intv.i64 < INT16_MIN || intv.i64 > INT16_MAX)
			return (ERANGE);

		nbytes = sizeof(int16_t);
		if (limit >= nbytes)
			*((int16_t *)outp) = (int16_t)intv.i64;
		break;

	case BHND_NVRAM_TYPE_INT32:
	case BHND_NVRAM_TYPE_INT32_ARRAY:
		if (intv.i64 < INT32_MIN || intv.i64 > INT32_MAX)
			return (ERANGE);

		nbytes = sizeof(int32_t);
		if (limit >= nbytes)
			*((int32_t *)outp) = (int32_t)intv.i64;
		break;

	case BHND_NVRAM_TYPE_INT64:
	case BHND_NVRAM_TYPE_INT64_ARRAY:
		nbytes = sizeof(int64_t);
		if (limit >= nbytes)
			*((int64_t *)outp) = intv.i64;
		break;

	case BHND_NVRAM_TYPE_STRING:
	case BHND_NVRAM_TYPE_STRING_ARRAY: {
		ssize_t len;
	
		/* Attempt to write the entry + NUL */
		if (otype_signed) {
			len = snprintf(outp, limit, "%" PRId64, intv.i64);
		} else {
			len = snprintf(outp, limit, "%" PRIu64, intv.u64);
		}

		if (len < 0) {
			BHND_NV_LOG("snprintf() failed: %zd\n", len);
			return (EFTYPE);
		}

		/* Set total length to the formatted string length, plus
		 * trailing NUL */
		nbytes = len + 1;
		break;
	}

	default:
		BHND_NV_LOG("unknown type %s\n", bhnd_nvram_type_name(otype));
		return (EFTYPE);
	}

	/* Provide required length */
	*olen = nbytes;
	if (limit < *olen) {
		if (outp == NULL)
			return (0);

		return (ENOMEM);
	}

	return (0);
}

/**
 * Encode the given @p value as @p otype, writing the result to @p outp.
 *
 * @param		value	The value to be encoded.
 * @param[out]		outp	On success, the value will be written to this 
 *				buffer. This argment may be NULL if the value is
 *				not desired.
 * @param[in,out]	olen	The capacity of @p outp. On success, will be set
 *				to the actual size of the requested value.
 * @param		otype	The data type to be written to @p outp.
 *
 * @retval 0		success
 * @retval ENOMEM	If the @p outp is non-NULL, and the provided @p olen
 *			is too small to hold the encoded value.
 * @retval EFTYPE	If value coercion from @p value to @p otype is
 *			impossible.
 * @retval ERANGE	If value coercion would overflow (or underflow) the
 *			a @p otype representation.
 */
int
bhnd_nvram_val_encode(bhnd_nvram_val_t *value, void *outp, size_t *olen,
    bhnd_nvram_type otype)
{
	/* Prefer format implementation */
	if (value->fmt != NULL && value->fmt->op_encode != NULL)
		return (value->fmt->op_encode(value, outp, olen, otype));

	return (bhnd_nvram_val_generic_encode(value, outp, olen, otype));
}

/**
 * Encode the given @p value's element as @p otype, writing the result to
 * @p outp.
 *
 * @param		inp	The element to be be encoded. Must be a value
 *				previously returned by bhnd_nvram_val_next()
 *				or bhnd_nvram_val_elem().
 * @param		ilen	The size of @p inp, as returned by
 *				bhnd_nvram_val_next() or bhnd_nvram_val_elem().
 * @param[out]		outp	On success, the value will be written to this 
 *				buffer. This argment may be NULL if the value is
 *				not desired.
 * @param[in,out]	olen	The capacity of @p outp. On success, will be set
 *				to the actual size of the requested value.
 * @param		otype	The data type to be written to @p outp.
 *
 * @retval 0		success
 * @retval ENOMEM	If the @p outp is non-NULL, and the provided @p olen
 *			is too small to hold the encoded value.
 * @retval EFTYPE	If value coercion from @p value to @p otype is
 *			impossible.
 * @retval ERANGE	If value coercion would overflow (or underflow) the
 *			a @p otype representation.
 */
int
bhnd_nvram_val_encode_elem(bhnd_nvram_val_t *value, const void *inp,
    size_t ilen, void *outp, size_t *olen, bhnd_nvram_type otype)
{
	/* Prefer format implementation */
	if (value->fmt != NULL && value->fmt->op_encode_elem != NULL) {
		return (value->fmt->op_encode_elem(value, inp, ilen, outp,
		    olen, otype));
	}

	return (bhnd_nvram_val_generic_encode_elem(value, inp, ilen, outp,
	    olen, otype));
}

/**
 * Return the type, size, and a pointer to the internal representation
 * of @p value.
 * 
 * @param	value	The value to be queried.
 * @param[out]	olen	Size of the returned data, in bytes.
 * @param[out]	otype	Data type.
 */
const void *
bhnd_nvram_val_bytes(bhnd_nvram_val_t *value, size_t *olen,
    bhnd_nvram_type *otype)
{
	/* Provide type and length */
	*otype = value->data_type;
	*olen = value->data_len;

	switch (value->data_storage) {
	case BHND_NVRAM_VAL_DATA_EXT_ALLOC:
	case BHND_NVRAM_VAL_DATA_EXT_STATIC:
	case BHND_NVRAM_VAL_DATA_EXT_WEAK:
		/* Return a pointer to external storage */
		return (value->data.ptr);

	case BHND_NVRAM_VAL_DATA_INLINE:
		/* Return a pointer to inline storage */
		return (&value->data);

	case BHND_NVRAM_VAL_DATA_NONE:
		BHND_NV_PANIC("uninitialized value");
	}

	BHND_NV_PANIC("unknown storage type: %d", value->data_storage);
}

/**
 * Iterate over all array elements in @p value.
 *
 * @param		value	The value to be iterated
 * @param		prev	A value pointer previously returned by
 *				bhnd_nvram_val_next() or bhnd_nvram_val_elem(),
 *				or NULL to begin iteration at the first element.
 * @param[in,out]	len	If prev is non-NULL, len must be a pointer
 *				to the length previously returned by
 *				bhnd_nvram_val_next() or bhnd_nvram_val_elem().
 *				On success, will be set to the next element's
 *				length, in bytes.
 *
 * @retval non-NULL	A borrowed reference to the element data.
 * @retval NULL		If the end of the element array is reached.
 */
const void *
bhnd_nvram_val_next(bhnd_nvram_val_t *value, const void *prev, size_t *len)
{
	/* Prefer the format implementation */
	if (value->fmt != NULL && value->fmt->op_next != NULL)
		return (value->fmt->op_next(value, prev, len));

	return (bhnd_nvram_val_generic_next(value, prev, len));
}

/**
 * Return value's element data type.
 *
 * @param	value	The value to be queried.
 */
bhnd_nvram_type
bhnd_nvram_val_elem_type(bhnd_nvram_val_t *value)
{
	return (bhnd_nvram_base_type(value->data_type));
}

/**
 * Return the total number of elements represented by @p value.
 */
size_t
bhnd_nvram_val_nelem(bhnd_nvram_val_t *value)
{
	const void	*bytes;
	bhnd_nvram_type	 type;
	size_t		 nelem, len;
	int		 error;

	/* Prefer format implementation */
	if (value->fmt != NULL && value->fmt->op_nelem != NULL)
		return (value->fmt->op_nelem(value));

	/*
	 * If a custom op_next() is defined, bhnd_nvram_value_nelem() almost
	 * certainly cannot produce a valid element count; it assumes a standard
	 * data format that may not apply when custom iteration is required.
	 *
	 * Instead, use bhnd_nvram_val_next() to parse the backing data and
	 * produce a total count.
	 */
	if (value->fmt != NULL && value->fmt->op_next != NULL) {
		const void *next;

		next = NULL;
		nelem = 0;
		while ((next = bhnd_nvram_val_next(value, next, &len)) != NULL)
			nelem++;

		return (nelem);
	}

	/* Otherwise, compute the standard element count */
	bytes = bhnd_nvram_val_bytes(value, &len, &type);
	if ((error = bhnd_nvram_value_nelem(type, bytes, len, &nelem))) {
		/* Should always succeed */
		BHND_NV_PANIC("error calculating element count for type '%s' "
		    "with length %zu: %d\n", bhnd_nvram_type_name(type), len,
		    error);
	}

	return (nelem);
}

/**
 * Generic implementation of bhnd_nvram_val_op_encode(), compatible with
 * all supported NVRAM data types.
 */
int
bhnd_nvram_val_generic_encode(bhnd_nvram_val_t *value, void *outp, size_t *olen,
    bhnd_nvram_type otype)
{
	const void	*inp;
	bhnd_nvram_type	 itype;
	size_t		 ilen;
	const void	*next;
	bhnd_nvram_type	 otype_base;
	size_t		 limit, nelem, nbytes;
	size_t		 next_len;
	int		 error;

	nbytes = 0;
	nelem = 0;
	otype_base = bhnd_nvram_base_type(otype);

	/*
	 * Normally, a rank polymorphic type like a character array would not
	 * be representable as a rank 1 type.
	 * 
	 * As a special-cased exception, we can support conversion directly
	 * from CHAR_ARRAY to STRING by treating the character array as a
	 * non-NUL-terminated string.
	 * 
	 * This conversion is isomorphic; we also support conversion directly
	 * from a STRING to a CHAR_ARRAY by the same mechanism.
	 */
	inp = bhnd_nvram_val_bytes(value, &ilen, &itype);
	if ((itype == BHND_NVRAM_TYPE_CHAR_ARRAY &&
	     otype == BHND_NVRAM_TYPE_STRING) ||
	    (itype == BHND_NVRAM_TYPE_STRING &&
	     otype == BHND_NVRAM_TYPE_CHAR_ARRAY))
	{
		return (bhnd_nvram_val_encode_elem(value, inp, ilen, outp, olen,
		    otype));
	}

	/*
	 * If both input and output are non-array types, try to encode them
	 * without performing element iteration.
	 */
	if (!bhnd_nvram_is_array_type(itype) &&
	    !bhnd_nvram_is_array_type(otype))
	{
		return (bhnd_nvram_val_encode_elem(value, inp, ilen, outp, olen,
		    otype));
	}

	/* Determine output byte limit */
	if (outp != NULL)
		limit = *olen;
	else
		limit = 0;

	/* Iterate over our array elements and encode as the requested
	 * type */
	next = NULL;
	while ((next = bhnd_nvram_val_next(value, next, &next_len))) {
		void			*elem_outp;
		size_t			 elem_nbytes;

		/* If the output type is not an array type, we can only encode
		 * one element */
		nelem++;
		if (nelem > 1 && !bhnd_nvram_is_array_type(otype)) {
			return (EFTYPE);
		}

		/* Determine output offset / limit */
		if (nbytes >= limit) {
			elem_nbytes = 0;
			elem_outp = NULL;
		} else {
			elem_nbytes = limit - nbytes;
			elem_outp = (uint8_t *)outp + nbytes;
		}

		/* Attempt encode */
		error = bhnd_nvram_val_encode_elem(value, next, next_len,
		    elem_outp, &elem_nbytes, otype_base);

		/* If encoding failed for any reason other than ENOMEM (which
		 * we'll detect and report below), return immediately */
		if (error && error != ENOMEM)
			return (error);

		/* Add to total length */
		if (SIZE_MAX - nbytes < elem_nbytes)
			return (EFTYPE); /* would overflow size_t */

		nbytes += elem_nbytes;
	}

	/* Provide the actual length */
	*olen = nbytes;

	/* If no output was requested, nothing left to do */
	if (outp == NULL)
		return (0);

	/* Otherwise, report a memory error if the output buffer was too
	 * small */
	if (limit < nbytes)
		return (ENOMEM);

	return (0);
}

/**
 * Generic implementation of bhnd_nvram_val_op_encode_elem(), compatible with
 * all supported NVRAM data types.
 */
int
bhnd_nvram_val_generic_encode_elem(bhnd_nvram_val_t *value, const void *inp,
    size_t ilen, void *outp, size_t *olen, bhnd_nvram_type otype)
{
	bhnd_nvram_type itype;

	itype = bhnd_nvram_val_elem_type(value);
	switch (itype) {
	case BHND_NVRAM_TYPE_STRING:
	case BHND_NVRAM_TYPE_CHAR:
	case BHND_NVRAM_TYPE_CHAR_ARRAY:
		return (bhnd_nvram_val_encode_string(outp, olen, otype, inp,
		    ilen, itype));

	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_INT16:
	case BHND_NVRAM_TYPE_INT32:
	case BHND_NVRAM_TYPE_INT64:
		return (bhnd_nvram_val_encode_int(outp, olen, otype, inp, ilen,
		    itype));
			
	default:
		BHND_NV_PANIC("missing encode_elem() implementation");
	}
}

/**
 * Generic implementation of bhnd_nvram_val_op_next(), compatible with
 * all supported NVRAM data types.
 */
const void *
bhnd_nvram_val_generic_next(bhnd_nvram_val_t *value, const void *prev,
    size_t *len)
{
	const uint8_t	*inp;
	const uint8_t	*next;
	bhnd_nvram_type	 itype;
	size_t		 ilen;
	size_t		 offset;

	/* Otherwise, default to iterating over the backing representation
	 * according to its native representation */
	inp = bhnd_nvram_val_bytes(value, &ilen, &itype);

	/* First element */
	if (prev == NULL) {
		/* Zero-length array? */
		if (ilen == 0)
			return (NULL);

		*len = bhnd_nvram_value_size(itype, inp, ilen, 1);
		return (inp);
	}

	/* Advance to next element */
	BHND_NV_ASSERT(prev >= (const void *)inp, ("invalid cookiep"));
	next = (const uint8_t *)prev + *len;
	offset = (size_t)(next - inp);

	if (offset >= ilen) {
		/* Hit end of the array */
		return (NULL);
	}

	/* Determine element size */
	*len = bhnd_nvram_value_size(itype, next, ilen - offset, 1);
	if (ilen - offset < *len)
		BHND_NV_PANIC("short element -- misaligned representation");

	return (next);
}

/**
 * Initialize the representation of @p value with @p ptr.
 * 
 * If @p value is an externally allocated instance and the representation
 * cannot be represented inline, the given data will not be copied, and @p ptr
 * must remain valid for the lifetime of @p value.
 *
 * Otherwise, @p value will be initialized with a copy of the @p ptr.
 * 
 * @param	value	The value to be initialized.
 * @param	inp	The external representation.
 * @param	ilen	The external representation length, in bytes.
 * @param	itype	The external representation's data type.
 * @param	flags	Value flags.
 * 
 * @retval 0		success.
 * @retval ENOMEM	if allocation fails
 * @retval EFTYPE	if @p itype is not an array type, and @p ilen is not
 *			equal to the size of a single element of @p itype.
 * @retval EFAULT	if @p ilen is not correctly aligned for elements of
 *			@p itype.
 */
static int
bhnd_nvram_val_set(bhnd_nvram_val_t *value, const void *inp, size_t ilen,
    bhnd_nvram_type itype, uint32_t flags)
{
	void	*bytes;

	BHND_NVRAM_VAL_ASSERT_EMPTY(value);

	/* Reference the external data */
	if ((flags & BHND_NVRAM_VAL_BORROW_DATA) ||
	    (flags & BHND_NVRAM_VAL_STATIC_DATA))
	{
		if (flags & BHND_NVRAM_VAL_BORROW_DATA)
			value->data_storage = BHND_NVRAM_VAL_DATA_EXT_WEAK;
		else
			value->data_storage = BHND_NVRAM_VAL_DATA_EXT_STATIC;

		value->data.ptr = inp;
		value->data_type = itype;
		value->data_len = ilen;
		return (0);
	}

	/* Fetch reference to (or allocate) an appropriately sized buffer */
	bytes = bhnd_nvram_val_alloc_bytes(value, ilen, itype, flags);
	if (bytes == NULL)
		return (ENOMEM);

	/* Copy data */
	memcpy(bytes, inp, ilen);

	return (0);
}

/**
 * Initialize the internal inline representation of @p value with a copy of
 * the data referenced by @p inp of @p itype.
 * 
 * If @p inp is NULL, @p itype and @p ilen will be validated, but no data will
 * be copied.
 *
 * @param	value	The value to be initialized.
 * @param	inp	The input data to be copied, or NULL to verify
 *			that data of @p ilen and @p itype can be represented
 *			inline.
 * @param	ilen	The size of the external buffer to be allocated.
 * @param	itype	The type of the external buffer to be allocated.
 * 
 * @retval 0		success
 * @retval ENOMEM	if @p ilen is too large to be represented inline.
 * @retval EFAULT	if @p ilen is not correctly aligned for elements of
 *			@p itype.
 */
static int
bhnd_nvram_val_set_inline(bhnd_nvram_val_t *value, const void *inp, size_t ilen,
    bhnd_nvram_type itype)
{
	BHND_NVRAM_VAL_ASSERT_EMPTY(value);

#define	NV_STORE_INIT_INLINE()	do {					\
	value->data_len = ilen;						\
} while(0)

#define	NV_STORE_INLINE(_type, _dest)	do {				\
	if (ilen != sizeof(_type))					\
		return (EFAULT);					\
									\
	if (inp != NULL) {						\
		value->data._dest[0] = *(const _type *)inp;		\
		NV_STORE_INIT_INLINE();					\
	}								\
} while (0)

#define	NV_COPY_ARRRAY_INLINE(_type, _dest)	do {		\
	if (ilen % sizeof(_type) != 0)				\
		return (EFAULT);				\
								\
	if (ilen > nitems(value->data. _dest))			\
		return (ENOMEM);				\
								\
	if (inp == NULL)					\
		return (0);					\
								\
	memcpy(&value->data._dest, inp, ilen);			\
	if (inp != NULL) {					\
		memcpy(&value->data._dest, inp, ilen);		\
		NV_STORE_INIT_INLINE();				\
	}							\
} while (0)

	/* Attempt to copy to inline storage */
	switch (itype) {
	case BHND_NVRAM_TYPE_CHAR:
		NV_STORE_INLINE(uint8_t, ch);
		return (0);

	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_INT8:
		NV_STORE_INLINE(uint8_t, u8);
		return (0);

	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_INT16:
		NV_STORE_INLINE(uint16_t, u16);
		return (0);

	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_INT32:
		NV_STORE_INLINE(uint32_t, u32);
		return (0);

	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_INT64:
		NV_STORE_INLINE(uint32_t, u32);
		return (0);

	case BHND_NVRAM_TYPE_CHAR_ARRAY:
		NV_COPY_ARRRAY_INLINE(uint8_t, ch);
		return (0);

	case BHND_NVRAM_TYPE_UINT8_ARRAY:
	case BHND_NVRAM_TYPE_INT8_ARRAY:
		NV_COPY_ARRRAY_INLINE(uint8_t, u8);
		return (0);

	case BHND_NVRAM_TYPE_UINT16_ARRAY:
	case BHND_NVRAM_TYPE_INT16_ARRAY:
		NV_COPY_ARRRAY_INLINE(uint16_t, u16);
		return (0);

	case BHND_NVRAM_TYPE_UINT32_ARRAY:
	case BHND_NVRAM_TYPE_INT32_ARRAY:
		NV_COPY_ARRRAY_INLINE(uint32_t, u32);
		return (0);

	case BHND_NVRAM_TYPE_UINT64_ARRAY:
	case BHND_NVRAM_TYPE_INT64_ARRAY:
		NV_COPY_ARRRAY_INLINE(uint64_t, u64);
		return (0);

	case BHND_NVRAM_TYPE_STRING:
	case BHND_NVRAM_TYPE_STRING_ARRAY:
		if (ilen > sizeof(value->data.ch))
			return (ENOMEM);

		if (inp != NULL) {
			memcpy(&value->data.ch, inp, ilen);
			NV_STORE_INIT_INLINE();
		}

		return (0);
	}

#undef	NV_STORE_INIT_INLINE
#undef	NV_STORE_INLINE
#undef	NV_COPY_ARRRAY_INLINE

	BHND_NV_PANIC("unknown data type %d", itype);
}

/**
 * Initialize the internal representation of @p value with a buffer allocation
 * of @p len and @p itype, returning a pointer to the allocated buffer.
 * 
 * If a buffer of @p len and @p itype can be represented inline, no
 * external buffer will be allocated, and instead a pointer to the inline
 * data representation will be returned.
 *
 * @param	value	The value to be initialized.
 * @param	ilen	The size of the external buffer to be allocated.
 * @param	itype	The type of the external buffer to be allocated.
 * @param	flags	Value flags.
 * 
 * @retval non-null	The newly allocated buffer.
 * @retval NULL		If allocation failed.
 * @retval NULL		If @p value is an externally allocated instance.
 */
static void *
bhnd_nvram_val_alloc_bytes(bhnd_nvram_val_t *value, size_t ilen,
    bhnd_nvram_type itype, uint32_t flags)
{
	void *ptr;

	BHND_NVRAM_VAL_ASSERT_EMPTY(value);

	/* Can we use inline storage? */
	if (bhnd_nvram_val_set_inline(value, NULL, ilen, itype) == 0) {
		BHND_NV_ASSERT(sizeof(value->data) >= ilen,
		    ("ilen exceeds inline storage"));

		value->data_type = itype;
		value->data_len = ilen;
		value->data_storage = BHND_NVRAM_VAL_DATA_INLINE;
		return (&value->data);
	}

	/* Is allocation permitted? */
	if (!(flags & BHND_NVRAM_VAL_DYNAMIC))
		return (NULL);

	/* Allocate external storage */
	if ((ptr = bhnd_nv_malloc(ilen)) == NULL)
		return (NULL);

	value->data.ptr = ptr;
	value->data_len = ilen;
	value->data_type = itype;
	value->data_storage = BHND_NVRAM_VAL_DATA_EXT_ALLOC;

	return (ptr);
}
