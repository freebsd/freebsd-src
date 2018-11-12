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
#include <sys/bus.h>
#include <sys/ctype.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include "bhnd_nvram_parserreg.h"
#include "bhnd_nvram_parservar.h"

/*
 * BHND NVRAM Parser
 * 
 * Provides identification, decoding, and encoding of BHND NVRAM data.
 */

static const struct bhnd_nvram_ops *bhnd_nvram_find_ops(bhnd_nvram_format fmt);

static int	bhnd_nvram_find_var(struct bhnd_nvram *sc, const char *name,
		    const char **value, size_t *value_len);

static int	bhnd_nvram_keycmp(const char *lhs, size_t lhs_len,
		    const char *rhs, size_t rhs_len);
static int	bhnd_nvram_sort_idx(void *ctx, const void *lhs,
		    const void *rhs);
static int	bhnd_nvram_generate_index(struct bhnd_nvram *sc);

static int	bhnd_nvram_index_lookup(struct bhnd_nvram *sc,
		    struct bhnd_nvram_idx *idx, const char *name,
		    const char **env, size_t *len, const char **value,
		    size_t *value_len);
static int	bhnd_nvram_buffer_lookup(struct bhnd_nvram *sc,
		    const char *name, const char **env, size_t *env_len,
		    const char **value, size_t *value_len);

static bool	bhnd_nvram_bufptr_valid(struct bhnd_nvram *sc, const void *ptr,
		    size_t nbytes, bool log_error);

static int	bhnd_nvram_parse_env(struct bhnd_nvram *sc, const char *env,
		    size_t len, const char **key, size_t *key_len,
		    const char **val, size_t *val_len);

/**
 * Calculate the size of the NVRAM data in @p data.
 * 
 * @param		data	Pointer to NVRAM data to be parsed.
 * @param[in,out]	size	On input, the total size of @p data. On
 *				successful parsing of @p data, will be set to
 *				the parsed size (which may be larger).
 */
typedef int	(*bhnd_nvram_op_getsize)(const void *data, size_t *size);

/** Perform format-specific initialization. */
typedef int	(*bhnd_nvram_op_init)(struct bhnd_nvram *sc);

/** Initialize any format-specific default values. */
typedef int	(*bhnd_nvram_op_init_defaults)(struct bhnd_nvram *sc);
typedef int	(*bhnd_nvram_op_enum_buf)(struct bhnd_nvram *sc,
		    const char **env, size_t *len, const uint8_t *p,
		    uint8_t const **next);

/* FMT_BCM ops */
static int	bhnd_nvram_bcm_getsize(const void *data, size_t *size);
static int	bhnd_nvram_bcm_init(struct bhnd_nvram *sc);
static int	bhnd_nvram_bcm_init_defaults(struct bhnd_nvram *sc);
static int	bhnd_nvram_bcm_enum_buf(struct bhnd_nvram *sc, const char **env,
		    size_t *len, const uint8_t *p, uint8_t const **next);

/* FMT_TLV ops */
static int	bhnd_nvram_tlv_getsize(const void *data, size_t *size);
static int	bhnd_nvram_tlv_init(struct bhnd_nvram *sc);
static int	bhnd_nvram_tlv_enum_buf(struct bhnd_nvram *sc, const char **env,
		    size_t *len, const uint8_t *p, uint8_t const **next);
/* FMT_TXT ops */
static int	bhnd_nvram_txt_getsize(const void *data, size_t *size);
static int	bhnd_nvram_txt_init(struct bhnd_nvram *sc);
static int	bhnd_nvram_txt_enum_buf(struct bhnd_nvram *sc, const char **env,
		    size_t *len, const uint8_t *p, uint8_t const **next);

/**
 * Format-specific operations.
 */
struct bhnd_nvram_ops {
	bhnd_nvram_format		 fmt;		/**< nvram format */
	bhnd_nvram_op_getsize		 getsize;	/**< determine actual NVRAM size */
	bhnd_nvram_op_init		 init;		/**< format-specific initialization */
	bhnd_nvram_op_enum_buf		 enum_buf;	/**< enumerate backing buffer */
	bhnd_nvram_op_init_defaults	 init_defaults;	/**< populate any default values */
};

static const struct bhnd_nvram_ops bhnd_nvram_ops_table[] = {
	{ 
		BHND_NVRAM_FMT_BCM,
		bhnd_nvram_bcm_getsize,
		bhnd_nvram_bcm_init, 
		bhnd_nvram_bcm_enum_buf,
		bhnd_nvram_bcm_init_defaults
	},
	{
		BHND_NVRAM_FMT_TLV,
		bhnd_nvram_tlv_getsize,
		bhnd_nvram_tlv_init,
		bhnd_nvram_tlv_enum_buf,
		NULL
	},
	{
		BHND_NVRAM_FMT_BTXT,
		bhnd_nvram_txt_getsize,
		bhnd_nvram_txt_init,
		bhnd_nvram_txt_enum_buf,
		NULL
	},
};

#define	NVRAM_LOG(sc, fmt, ...)	do {			\
	if (sc->dev != NULL)					\
		device_printf(sc->dev, fmt, ##__VA_ARGS__);	\
	else							\
		printf("bhnd_nvram: " fmt, ##__VA_ARGS__);	\
} while (0)

/* Limit a size_t value to a suitable range for use as a printf string field
 * width */
#define	NVRAM_PRINT_WIDTH(_len)	\
	((_len) > NVRAM_VAL_MAX ? NVRAM_VAL_MAX : (int)(_len))

/* Is _c a field terminating/delimiting character? */
#define	nvram_is_ftermc(_c)	((_c) == '\0' || nvram_is_fdelim(_c))

/* Is _c a field delimiting character? */
#define	nvram_is_fdelim(_c)	((_c) == ',')

/**
 * Identify @p ident.
 * 
 * @param ident Initial header data to be used for identification.
 * @param expected Expected format against which @p ident will be tested.
 * 
 * @retval 0 If @p ident has the @p expected format.
 * @retval ENODEV If @p ident does not match @p expected.
 */
int
bhnd_nvram_parser_identify(const union bhnd_nvram_ident *ident,
    bhnd_nvram_format expected)
{
	uint32_t bcm_magic = le32toh(ident->bcm.magic);

	switch (expected) {
	case BHND_NVRAM_FMT_BCM:
		if (bcm_magic == NVRAM_MAGIC)
			return (0);

		return (ENODEV);
	case BHND_NVRAM_FMT_TLV:
		if (bcm_magic == NVRAM_MAGIC)
			return (ENODEV);

		if (ident->tlv.tag != NVRAM_TLV_TYPE_ENV)
			return (ENODEV);

		return (0);
	case BHND_NVRAM_FMT_BTXT:
		for (size_t i = 0; i < nitems(ident->btxt); i++) {
			char c = ident->btxt[i];
			if (!isprint(c) && !isspace(c))
				return (ENODEV);
		}
		return (0);
		break;
	default:
		printf("%s: unknown format: %d\n", __FUNCTION__, expected);
		return (ENODEV);
	}
}

/** Return the set of operations for @p fmt, if any */
static const struct bhnd_nvram_ops *
bhnd_nvram_find_ops(bhnd_nvram_format fmt)
{
	const struct bhnd_nvram_ops *ops;

	/* Fetch format-specific operation callbacks */
	for (size_t i = 0; i < nitems(bhnd_nvram_ops_table); i++) {
		ops = &bhnd_nvram_ops_table[i];

		if (ops->fmt != fmt)
			continue;

		/* found */
		return (ops);
	}

	return (NULL);
}

/**
 * Identify the NVRAM format at @p offset within @p r, verify the
 * CRC (if applicable), and allocate a local shadow copy of the NVRAM data.
 * 
 * After initialization, no reference to @p input will be held by the
 * NVRAM parser, and @p input may be safely deallocated.
 * 
 * @param[out] sc The NVRAM parser state to be initialized.
 * @param dev The parser's parent device, or NULL if none.
 * @param data NVRAM data to be parsed.
 * @param size Size of @p data.
 * @param fmt Required format of @p input.
 * 
 * @retval 0 success
 * @retval ENOMEM If internal allocation of NVRAM state fails.
 * @retval EINVAL If @p input parsing fails.
 */
int
bhnd_nvram_parser_init(struct bhnd_nvram *sc, device_t dev, const void *data,
    size_t size, bhnd_nvram_format fmt)
{
	int error;

	/* Initialize NVRAM state */
	memset(sc, 0, sizeof(*sc));

	sc->dev = dev;
	LIST_INIT(&sc->devpaths);

	/* Verify data format and init operation callbacks */
	if (size < sizeof(union bhnd_nvram_ident))
		return (EINVAL);

	error = bhnd_nvram_parser_identify(
	    (const union bhnd_nvram_ident *)data, fmt);
	if (error)
		return (error);

	if ((sc->ops = bhnd_nvram_find_ops(fmt)) == NULL) {
		NVRAM_LOG(sc, "unsupported format: %d\n", fmt);
		return (error);
	}

	/* Determine appropriate size for backing buffer */
	sc->buf_size = size;
	if ((error = sc->ops->getsize(data, &sc->buf_size)))
		return (error);

	if (sc->buf_size > size) {
		NVRAM_LOG(sc, "cannot parse %zu NVRAM bytes, would overrun "
		    "%zu byte input buffer\n", sc->buf_size, size);
		return (EINVAL);
	}

	/* Allocate and populate backing buffer */
	sc->buf = malloc(sc->buf_size, M_BHND_NVRAM, M_NOWAIT);
	if (sc->buf == NULL)
		return (ENOMEM);
	memcpy(sc->buf, data, sc->buf_size);

	/* Allocate default/pending variable hash tables */
	error = bhnd_nvram_varmap_init(&sc->defaults, NVRAM_SMALL_HASH_SIZE,
	    M_NOWAIT);
	if (error)
		goto cleanup;

	error = bhnd_nvram_varmap_init(&sc->pending, NVRAM_SMALL_HASH_SIZE,
	    M_NOWAIT);
	if (error)
		goto cleanup;

	/* Perform format-specific initialization */
	if ((error = sc->ops->init(sc)))
		goto cleanup;

	/* Generate all indices */
	if ((error = bhnd_nvram_generate_index(sc)))
		goto cleanup;

	/* Add any format-specific default values */
	if (sc->ops->init_defaults != NULL) {
		if ((error = sc->ops->init_defaults(sc)))
			goto cleanup;
	}

	return (0);

cleanup:
	bhnd_nvram_parser_fini(sc);
	return (error);
}


/**
 * Release all resources held by @p sc.
 * 
 * @param sc A NVRAM instance previously initialized via
 * bhnd_nvram_parser_init().
 */
void
bhnd_nvram_parser_fini(struct bhnd_nvram *sc)
{
	struct bhnd_nvram_devpath	*dpath, *dnext;

        LIST_FOREACH_SAFE(dpath, &sc->devpaths, dp_link, dnext) {
		free(dpath->path, M_BHND_NVRAM);
                free(dpath, M_BHND_NVRAM);
        }

        if (sc->defaults.table != NULL)
		bhnd_nvram_varmap_free(&sc->defaults);

	if (sc->pending.table != NULL)
		bhnd_nvram_varmap_free(&sc->pending);

	if (sc->idx != NULL)
		free(sc->idx, M_BHND_NVRAM);

	if (sc->buf != NULL)
		free(sc->buf, M_BHND_NVRAM);

}

/**
 * Identify the integer format of @p field.
 *
 * @param field Field to be identified.
 * @param field_len Length of @p field.
 * @param[out] base Integer base, or 0 if integer format unrecognized.
 * @param[out] negated True if integer is prefixed with negation sign.
 * 
 * @retval true if parsed successfully
 * @retval false if the format of @p field cannot be determined.
 */
static bool
bhnd_nvram_identify_intfmt(const char *field, size_t field_len, int *base,
    bool *negated)
{
	const char *p;

	/* Hex? */
	p = field;
	if (field_len > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		bool valid;

		/* Check all field characters */
		valid = true;
		for (p = field + 2; p - field < field_len; p++) {
			if (isxdigit(*p))
				continue;

			valid = false;
			break;
		}

		if (valid) {
			*base = 16;
			*negated = false;
			return (true);
		}
	}

	/* Decimal? */
	p = field;
	if (field_len >= 1 && (*p == '-' || isdigit(*p))) {
		bool		 valid;

		valid = true;
		*negated = false;
		for (p = field; p - field < field_len; p++) {
			if (p == field && *p == '-') {
				*negated = true;
				continue;
			}

			if (isdigit(*p))
				continue;

			valid = false;
			break;
		}

		if (valid) {
			*base = 10;
			return (true);
		}
	}

	/* No match */
	*base = 0;
	*negated = false;
	return (false);
}

/**
 * Search for a field delimiter or '\0' in @p value, returning the
 * size of the first field (not including its terminating character).
 * 
 * If no terminating character is found, @p value_len is returned.
 * 
 * @param value The value to be searched.
 * @param value_size The size of @p value.
 */
static size_t
bhnd_nvram_parse_field_len(const char *value, size_t value_size)
{
	for (const char *p = value; p - value < value_size; p++) {
		if (nvram_is_ftermc(*p))
			return (p - value);
	}

	return (value_size);
}

/* Parse a string NVRAM variable, writing the NUL-terminated result
 * to buf (if non-NULL). */
static int
bhnd_nvram_parse_strvar(const char *value, size_t value_len, char *buf,
    size_t *size)
{
	size_t str_len;
	size_t req_size;
	size_t max_size;

	if (buf != NULL)
		max_size = *size;
	else
		max_size = 0;


	/* Determine input and output sizes, including whether additional space
	 * is required for a trailing NUL */
	str_len = strnlen(value, value_len);
	if (str_len == value_len)
		req_size = str_len + 1;
	else
		req_size = value_len;

	/* Provide actual size to caller */
	*size = req_size;
	if (max_size < req_size) {
		if (buf != NULL)
			return (ENOMEM);
		else
			return (0);
	}

	/* Copy and NUL terminate output */
	memcpy(buf, value, str_len);
	buf[str_len] = '\0';
	return (0);
}

/**
 * Read an NVRAM variable.
 *
 * @param		sc	The NVRAM parser state.
 * @param		name	The NVRAM variable name.
 * @param[out]		buf	On success, the requested value will be written
 *				to this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	len	The capacity of @p buf. On success, will be set
 *				to the actual size of the requested value.
 * @param		type	The requested data type to be written to @p buf.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval ENOMEM	If @p buf is non-NULL and a buffer of @p len is too
 *			small to hold the requested value.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
int
bhnd_nvram_parser_getvar(struct bhnd_nvram *sc, const char *name, void *buf,
    size_t *len, bhnd_nvram_type type)
{
	char		*cstr, cstr_buf[NVRAM_VAL_MAX+1];
	const char	*val;
	size_t		 cstr_size;
	size_t		 limit, nbytes;
	size_t		 field_len, val_len;
	int		 error;

	/* Verify name validity */
	if (!bhnd_nvram_validate_name(name, strlen(name)))
		return (EINVAL);

	/* Fetch variable's string value */
	if ((error = bhnd_nvram_find_var(sc, name, &val, &val_len)))
		return (error);

	nbytes = 0;	
	if (buf != NULL)
		limit = *len;
	else
		limit = 0;

	/* Populate C string requests directly from the fetched value */
	if (type == BHND_NVRAM_TYPE_CSTR)
		return (bhnd_nvram_parse_strvar(val, val_len, buf, len));

	/* Determine actual string length. */
	val_len = strnlen(val, val_len);

	/* Try parsing as an octet string value (e.g. a MAC address) */
	if (bhnd_nvram_parse_octet_string(val, val_len, buf, len, type) == 0)
		return (0);

	/* Otherwise, we need a NUL-terminated copy of the string value
	 * for parsing */
	cstr_size = val_len + 1;
	if (cstr_size <= sizeof(cstr)) {
		/* prefer stack allocated buffer */
		cstr = cstr_buf;
	} else {
		cstr = malloc(cstr_size, M_BHND_NVRAM, M_NOWAIT);
		if (cstr == NULL)
			return (EFBIG);
	}

	/* Copy and NUL terminate */
	strncpy(cstr, val, val_len);
	cstr[val_len] = '\0';

	/* Parse */
	for (char *p = cstr; *p != '\0';) {
		char		*endp;
		int		 base;
		bool		 is_int, is_negated;
		union {
			unsigned long	u32;
			long		s32;
		} intv;

		/* Determine the field length */
		field_len = val_len - (p - cstr);
		field_len = bhnd_nvram_parse_field_len(p, field_len);

		/* Skip any leading whitespace */
		while (field_len > 0 && isspace(*p)) {
			p++;
			field_len--;
		}

		/* Empty field values cannot be parsed as a fixed
		 * data type */
		if (field_len == 0) {
			NVRAM_LOG(sc, "error: cannot parse empty string in "
			    "'%s'\n", cstr);
			return (EFTYPE);
		}

		/* Attempt to identify the integer format */
		is_int = bhnd_nvram_identify_intfmt(p, field_len, &base,
		    &is_negated);

		/* Extract the field data */
#define	NV_READ_INT(_ctype, _max, _min, _dest, _strto)	do {		\
	if (!is_int) {							\
		error = EFTYPE;						\
		goto finished;						\
	}								\
									\
	if (is_negated && _min == 0) {					\
		error = ERANGE;						\
		goto finished;						\
	}								\
									\
	_dest = _strto(p, &endp, base);					\
	if (endp == p || !nvram_is_ftermc(*endp)) {			\
		error = ERANGE;						\
		goto finished;						\
	}								\
									\
	if (_dest > _max || _dest < _min) {				\
		error = ERANGE;						\
		goto finished;						\
	}								\
									\
	if (limit > nbytes && limit - nbytes >= sizeof(_ctype))		\
		*((_ctype *)((uint8_t *)buf + nbytes)) = _dest;		\
									\
	nbytes += sizeof(_ctype);					\
} while(0)

		switch (type) {
		case BHND_NVRAM_TYPE_CHAR:
			/* Copy out the characters directly */
			for (size_t i = 0; i < field_len; i++) {
				if (limit > nbytes)
					*((char *)buf + nbytes) = p[i];
				nbytes++;
			}
			break;

		case BHND_NVRAM_TYPE_UINT8:
			NV_READ_INT(uint8_t, UINT8_MAX, 0, intv.u32, strtoul);
			break;

		case BHND_NVRAM_TYPE_UINT16:
			NV_READ_INT(uint16_t, UINT16_MAX, 0, intv.u32, strtoul);
			break;

		case BHND_NVRAM_TYPE_UINT32:
			NV_READ_INT(uint32_t, UINT32_MAX, 0, intv.u32, strtoul);
			break;

		case BHND_NVRAM_TYPE_INT8:
			NV_READ_INT(int8_t, INT8_MAX, INT8_MIN, intv.s32,
			    strtol);
			break;

		case BHND_NVRAM_TYPE_INT16:
			NV_READ_INT(int16_t, INT16_MAX, INT16_MIN, intv.s32,
			    strtol);
			break;

		case BHND_NVRAM_TYPE_INT32:
			NV_READ_INT(int32_t, INT32_MAX, INT32_MIN, intv.s32,
			    strtol);
			break;

		case BHND_NVRAM_TYPE_CSTR:	/* Must be handled above */
			/* fallthrough */
		default:
			NVRAM_LOG(sc, "unhandled NVRAM type: %d\n", type);
			error = ENXIO;
			goto finished;
		}

		/* Advance to next field, skip any trailing delimiter */
		p += field_len;
		if (nvram_is_fdelim(*p))
			p++;
	}

	error = 0;

finished:
	if (cstr != cstr_buf)
		free(cstr, M_BHND_NVRAM);

	return (error);
}

/**
 * Set an NVRAM variable.
 * 
 * @param		sc	The NVRAM parser state.
 * @param		name	The NVRAM variable name.
 * @param[out]		buf	The new value.
 * @param[in,out]	len	The size of @p buf.
 * @param		type	The data type of @p buf.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval EINVAL	If @p len does not match the expected variable size.
 */
int
bhnd_nvram_parser_setvar(struct bhnd_nvram *sc, const char *name,
    const void *buf, size_t len, bhnd_nvram_type type)
{
	/* Verify name validity */
	if (!bhnd_nvram_validate_name(name, strlen(name)))
		return (EINVAL);

	/* Verify buffer size alignment for the given type. If this is a
	 * variable width type, a width of 0 will always pass this check */
	if (len % bhnd_nvram_type_width(type) != 0)
		return (EINVAL);

	/* Determine string format (or directly add variable, if a C string) */
	switch (type) {
	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_INT16:
	case BHND_NVRAM_TYPE_INT32:
		// TODO: primitive type value support
		return (EOPNOTSUPP);

	case BHND_NVRAM_TYPE_CHAR:
	case BHND_NVRAM_TYPE_CSTR:
		return (bhnd_nvram_varmap_add(&sc->pending, name, buf, len));
	}

	return (0);
}

/**
 * Return true if @p ptr + nbytes falls within our backing buffer, false
 * otherwise.
 */
static bool
bhnd_nvram_bufptr_valid(struct bhnd_nvram *sc, const void *ptr, size_t nbytes,
    bool log_error)
{
	const uint8_t *p = ptr;

	if (p < sc->buf)
		goto failed;

	if (nbytes > sc->buf_size)
		goto failed;

	if (p - sc->buf > sc->buf_size - nbytes)
		goto failed;

	return (true);
	
failed:
	if (log_error)
		NVRAM_LOG(sc, "NVRAM record not readable at %p+%#zx (base=%p, "
		    "len=%zu)\n", p, nbytes, sc->buf, sc->buf_size);
	return (false);
}

/**
 * Parse a 'key=value' env string.
 */
static int
bhnd_nvram_parse_env(struct bhnd_nvram *sc, const char *env, size_t len,
    const char **key, size_t *key_len, const char **val, size_t *val_len)
{
	const char	*p;

	/* Key */
	if ((p = memchr(env, '=', len)) == NULL) {
		NVRAM_LOG(sc, "missing delim in '%.*s'\n",
		    NVRAM_PRINT_WIDTH(len), env);
		return (EINVAL);
	}

	*key = env;
	*key_len = p - env;

	/* Skip '=' */
	p++;

	/* Vaue */
	*val = p;
	*val_len = len - (p - env);

	return (0);
}

/**
 * Fetch a string pointer to @p name's value, if any.
 * 
 * @param	sc		The NVRAM parser state.
 * @param	name		The NVRAM variable name.
 * @param[out]	value		On success, a pointer to the variable's value
 *				string. The string may not be NUL terminated.
 * @param[out]	value_len	On success, the length of @p value, not
 *				including a terminating NUL (if any exists).
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
 */
static int
bhnd_nvram_find_var(struct bhnd_nvram *sc, const char *name, const char **value,
    size_t *value_len)
{
	struct bhnd_nvram_tuple	*t;
	bhnd_nvram_op_enum_buf	 enum_fn;
	const char		*env;
	size_t			 env_len;
	size_t			 name_len;
	int			 error;

	enum_fn = sc->ops->enum_buf;
	name_len = strlen(name);

	/*
	 * Search path:
	 * 
	 * - uncommitted changes
	 * - index lookup OR buffer scan
	 * - registered defaults
	 */

	/* Search uncommitted changes */
	t = bhnd_nvram_varmap_find(&sc->pending, name, name_len);
	if (t != NULL) {
		if (t->value != NULL) {
			/* Uncommited value exists, is not a deletion */
			*value = t->value;
			*value_len = t->value_len;
			return (0);
		} else {
			/* Value is marked for deletion. */
			error = ENOENT;
			goto failed;
		}
	}

	/* Search backing buffer. We the index if available; otherwise,
	 * perform a buffer scan */
	if (sc->idx != NULL) {
		error = bhnd_nvram_index_lookup(sc, sc->idx, name, &env,
		    &env_len, value, value_len);
	} else {
		error = bhnd_nvram_buffer_lookup(sc, name, &env, &env_len,
		    value, value_len);
	}

failed:
	/* If a parse error occured, we don't want to hide the issue by
	 * returning a default NVRAM value. Otherwise, look for a matching
	 * default. */
	if (error != ENOENT)
		return (error);

	t = bhnd_nvram_varmap_find(&sc->defaults, name, name_len);
	if (t != NULL) {
		*value = t->value;
		*value_len = t->value_len;
		return (0);
	}

	/* Not found, and no default value available */
	return (ENOENT);
}

/*
 * An strcmp()-compatible  lexical comparison implementation that
 * handles non-NUL-terminated strings.
 */
static int
bhnd_nvram_keycmp(const char *lhs, size_t lhs_len, const char *rhs,
    size_t rhs_len)
{
	int order;

	order = strncmp(lhs, rhs, ulmin(lhs_len, rhs_len));
	if (order == 0) {
		if (lhs_len < rhs_len)
			order = -1;
		else if (lhs_len > rhs_len)
			order = 1;
	}

	return (order);
}

/* sort function for bhnd_nvram_idx_entry values */
static int
bhnd_nvram_sort_idx(void *ctx, const void *lhs, const void *rhs)
{
	struct bhnd_nvram			*sc;
	const struct bhnd_nvram_idx_entry	*l_idx, *r_idx;
	const char				*l_str, *r_str;

	sc = ctx;
	l_idx = lhs;
	r_idx = rhs;

	/* Fetch string pointers */
	l_str = (char *)(sc->buf + l_idx->env_offset);
	r_str = (char *)(sc->buf + r_idx->env_offset);

	/* Perform comparison */
	return (bhnd_nvram_keycmp(l_str, l_idx->key_len, r_str,
	    r_idx->key_len));
}


/**
 * Generate all indices for the NVRAM data backing @p nvram.
 * 
 * @param sc		The NVRAM parser state.
 *
 * @retval 0		success
 * @retval non-zero	If indexing @p nvram fails, a regular unix
 *			error code will be returned.
 */
static int
bhnd_nvram_generate_index(struct bhnd_nvram *sc)
{
	bhnd_nvram_op_enum_buf	 enum_fn;
	const char		*key, *val;
	const char		*env;
	const uint8_t		*p;
	size_t			 env_len;
	size_t			 idx_bytes;
	size_t			 key_len, val_len;
	size_t			 num_records;
	int			 error;

	enum_fn = sc->ops->enum_buf;
	num_records = 0;

	/* Parse and register all device path aliases */
	p = NULL;
	while ((error = enum_fn(sc, &env, &env_len, p, &p)) == 0) {
		struct bhnd_nvram_devpath	*devpath;
		char				*eptr;
		char				 suffix[NVRAM_KEY_MAX+1];
		size_t				 suffix_len;
		u_long				 index;

		/* Hit EOF */
		if (env == NULL)
			break;

		num_records++;

		/* Skip string comparison if env_len < strlen(devpath) */
		if (env_len < NVRAM_DEVPATH_LEN)
			continue;

		/* Check for devpath prefix */
		if (strncmp(env, NVRAM_DEVPATH_STR, NVRAM_DEVPATH_LEN) != 0)
			continue;

		/* Split key and value */
		error = bhnd_nvram_parse_env(sc, env, env_len, &key,
		    &key_len, &val, &val_len);
		if (error)
			return (error);

		/* NUL terminate the devpath's suffix */
		if (key_len >= sizeof(suffix)) {
			NVRAM_LOG(sc, "variable '%.*s' exceeds NVRAM_KEY_MAX, "
			    "skipping devpath parsing\n",
			    NVRAM_PRINT_WIDTH(key_len), key);
			continue;
		} else {
			suffix_len = key_len - NVRAM_DEVPATH_LEN;
			if (suffix_len == 0)
				continue;

			strcpy(suffix, key+NVRAM_DEVPATH_LEN);
			suffix[suffix_len] = '\0';
		}

		/* Parse the index value */
		index = strtoul(suffix, &eptr, 10);
		if (eptr == suffix || *eptr != '\0') {
			NVRAM_LOG(sc, "invalid devpath variable '%.*s'\n",
			    NVRAM_PRINT_WIDTH(key_len), key);
			continue;
		}

		/* Register path alias */
		devpath = malloc(sizeof(*devpath), M_BHND_NVRAM, M_NOWAIT);
		if (devpath == NULL)
			return (ENOMEM);

		devpath->index = index;
		devpath->path = strndup(val, val_len, M_BHND_NVRAM);
		LIST_INSERT_HEAD(&sc->devpaths, devpath, dp_link);
	}

	if (error)
		return (error);

	/* Save record count */
	sc->num_buf_vars = num_records;

	/* Skip generating variable index if threshold is not met */
	if (sc->num_buf_vars < NVRAM_IDX_VAR_THRESH)
		return (0);

	/* Allocate and populate variable index */
	idx_bytes = sizeof(struct bhnd_nvram_idx) +
	    (sizeof(struct bhnd_nvram_idx_entry) * sc->num_buf_vars);
	sc->idx = malloc(idx_bytes, M_BHND_NVRAM, M_NOWAIT);
	if (sc->idx == NULL) {
		NVRAM_LOG(sc, "error allocating %zu byte index\n", idx_bytes);
		goto bad_index;
	}

	sc->idx->num_entries = sc->num_buf_vars;

	if (bootverbose) {
		NVRAM_LOG(sc, "allocated %zu byte index for %zu variables "
		    "in %zu bytes\n", idx_bytes, sc->num_buf_vars,
		    sc->buf_size);
	}

	p = NULL;
	for (size_t i = 0; i < sc->idx->num_entries; i++) {
		struct bhnd_nvram_idx_entry	*idx;
		size_t				 env_offset;
		size_t				 key_len, val_len;

		/* Fetch next record */
		if ((error = enum_fn(sc, &env, &env_len, p, &p)))
			return (error);

		/* Early EOF */
		if (env == NULL) {
			NVRAM_LOG(sc, "indexing failed, expected %zu records "
			    "(got %zu)\n", sc->idx->num_entries, i+1);
			goto bad_index;
		}
	
		/* Calculate env offset */
		env_offset = (const uint8_t *)env - (const uint8_t *)sc->buf;
		if (env_offset > NVRAM_IDX_OFFSET_MAX) {
			NVRAM_LOG(sc, "'%.*s' offset %#zx exceeds maximum "
			    "indexable value\n", NVRAM_PRINT_WIDTH(env_len),
			    env, env_offset);
			goto bad_index;
		}

		/* Split key and value */
		error = bhnd_nvram_parse_env(sc, env, env_len, &key, &key_len,
		    &val, &val_len);
		if (error)
			return (error);

		if (key_len > NVRAM_IDX_LEN_MAX) {
			NVRAM_LOG(sc, "key length %#zx at %#zx exceeds maximum "
			    "indexable value\n", key_len, env_offset);
			goto bad_index;
		}

		if (val_len > NVRAM_IDX_LEN_MAX) {
			NVRAM_LOG(sc, "value length %#zx for key '%.*s' "
			    "exceeds maximum indexable value\n", val_len,
			    NVRAM_PRINT_WIDTH(key_len), key);
			goto bad_index;
		}

		idx = &sc->idx->entries[i];
		idx->env_offset = env_offset;
		idx->key_len = key_len;
		idx->val_len = val_len;
	}

	/* Sort the index table */
	qsort_r(sc->idx->entries, sc->idx->num_entries,
	    sizeof(sc->idx->entries[0]), sc, bhnd_nvram_sort_idx);

	return (0);

bad_index:
	/* Fall back on non-indexed access */
	NVRAM_LOG(sc, "reverting to non-indexed variable lookup\n");
	if (sc->idx != NULL) {
		free(sc->idx, M_BHND_NVRAM);
		sc->idx = NULL;
	}

	return (0);
}


/**
 * Perform an index lookup of @p name.
 *
 * @param	sc		The NVRAM parser state.
 * @param	idx		The index to search.
 * @param	name		The variable to search for.
 * @param[out]	env		On success, the pointer to @p name within the
 *				backing buffer.
 * @param[out]	env_len		On success, the length of @p env.
 * @param[out]	value		On success, the pointer to @p name's value
 *				within the backing buffer.
 * @param[out]	value_len	On success, the length of @p value.
 * 
 * @retval 0 If @p name was found in the index.
 * @retval ENOENT If @p name was not found in the index.
 * @retval ENODEV If no index has been generated.
 */
static int
bhnd_nvram_index_lookup(struct bhnd_nvram *sc, struct bhnd_nvram_idx *idx,
    const char *name, const char **env, size_t *env_len, const char **value,
    size_t *value_len)
{
	struct bhnd_nvram_idx_entry	*idxe;
	const char			*idxe_key;
	size_t				 min, mid, max;
	size_t				 name_len;
	int				 order;

	if (idx->num_entries == 0)
		return (ENOENT);

	/*
	 * Locate the requested variable using a binary search.
	 */
	min = 0;
	mid = 0;
	max = idx->num_entries - 1;
	name_len = strlen(name);

	while (max >= min) {
		/* Select midpoint */
		mid = (min + max) / 2;
		idxe = &idx->entries[mid];

		/* Determine which side of the partition to search */
		idxe_key = (const char *) (sc->buf + idxe->env_offset);
		order = bhnd_nvram_keycmp(idxe_key, idxe->key_len, name,
		    name_len);

		if (order < 0) {
			/* Search upper partition */
			min = mid + 1;
		} else if (order > 0) {
			/* Search lower partition */
			max = mid - 1;
		} else if (order == 0) {
			/* Match found */
			*env = sc->buf + idxe->env_offset;
			*env_len = idxe->key_len + idxe->val_len + 1 /* '=' */;

			*value = *env + idxe->key_len + 1 /* '=' */;
			*value_len = idxe->val_len;

			return (0);
		}
	}

	/* Not found */
	return (ENOENT);
}


/**
 * Perform a unindexed search for an entry matching @p name in the backing
 * NVRAM data buffer.
 *
 * @param	sc		The NVRAM parser state.
 * @param	name		The variable to search for.
 * @param[out]	env		On success, the pointer to @p name within the
 *				backing buffer.
 * @param[out]	env_len		On success, the length of @p env.
 * @param[out]	value		On success, the pointer to @p name's value
 *				within the backing buffer.
 * @param[out]	value_len	On success, the length of @p value.
 * 
 * @retval 0 If @p name was found in the index.
 * @retval ENOENT If @p name was not found in the index.
 * @retval ENODEV If no index has been generated.
 */
static int
bhnd_nvram_buffer_lookup(struct bhnd_nvram *sc, const char *name,
    const char **env, size_t *env_len, const char **value, size_t *value_len)
{
	bhnd_nvram_op_enum_buf	 enum_fn;
	const uint8_t		*p;
	size_t			 name_len;
	int			 error;

	enum_fn = sc->ops->enum_buf;
	name_len = strlen(name);

	/* Iterate over all records in the backing buffer */
	p = NULL;
	while ((error = enum_fn(sc, env, env_len, p, &p)) == 0) {
		/* Hit EOF, not found */
		if (*env == NULL)
			return (ENOENT);

		/* Skip string comparison if env_len < strlen('key=') */
		if (*env_len < name_len + 1)
			continue;

		/* Skip string comparison if delimiter isn't found at
		* expected position */
		if (*(*env + name_len) != '=')
			continue;

		/* Check for match */
		if (strncmp(*env, name, name_len) == 0) {
			/* Found */
			*value = *env + name_len + 1;
			*value_len = *env_len - name_len - 1;
			return (0);
		};
	}

	return (error);
}

/* FMT_BCM NVRAM data size calculation */
static int
bhnd_nvram_bcm_getsize(const void *data, size_t *size)
{
	const struct bhnd_nvram_header *hdr;

	if (*size < sizeof(*hdr))
		return (EINVAL);

	hdr = (const struct bhnd_nvram_header *) data;
	*size = le32toh(hdr->size);
	return (0);
}

/* FMT_BCM-specific parser initialization */
static int
bhnd_nvram_bcm_init(struct bhnd_nvram *sc)
{
	const uint8_t	*p;
	uint32_t	 cfg0;
	uint8_t		 crc, valid;

	/* Validate CRC */
	if (sc->buf_size < NVRAM_CRC_SKIP)
		return (EINVAL);

	if (sc->buf_size < sizeof(struct bhnd_nvram_header))
		return (EINVAL);

	cfg0 = ((struct bhnd_nvram_header *)sc->buf)->cfg0;
	valid = (cfg0 & NVRAM_CFG0_CRC_MASK) >> NVRAM_CFG0_CRC_SHIFT;

	p = sc->buf;
	crc = bhnd_nvram_crc8(p + NVRAM_CRC_SKIP, sc->buf_size-NVRAM_CRC_SKIP,
	    BHND_NVRAM_CRC8_INITIAL);

	if (crc != valid) {
		NVRAM_LOG(sc, "warning: NVRAM CRC error (crc=%#hhx, "
		    "expected=%hhx)\n", crc, valid);
	}

	return (0);
}

/* Populate FMT_BCM-specific default values */
static int
bhnd_nvram_bcm_init_defaults(struct bhnd_nvram *sc)
{
	struct bhnd_nvram_header	*header;
	char				 vbuf[NVRAM_VAL_MAX];
	uint32_t			 value;
	int				 error;
	int				 nwrite;

	/* Verify that our header is readable */
	header = (struct bhnd_nvram_header *) sc->buf;
	if (!bhnd_nvram_bufptr_valid(sc, header, sizeof(*header), true))
		return (EINVAL);

	/* Extract a value value from the NVRAM header, format it, and
	 * register a new default variable tuple */
#define	NVRAM_BCM_HEADER_DEFAULT(_field, _name)	do {			\
	value = NVRAM_GET_BITS(le32toh(header->_field), _name);		\
	nwrite = snprintf(vbuf, sizeof(vbuf), _name ##_FMT, value);	\
	if (nwrite < 0 || nwrite >= sizeof(vbuf)) {			\
		NVRAM_LOG(sc, "%s: formatting '%s' failed: %d\n",	\
		    __FUNCTION__, _name ## _VAR, nwrite);		\
		return (ENXIO);						\
	}								\
	error = bhnd_nvram_varmap_add(&sc->defaults,			\
		_name ##_VAR, vbuf, strlen(vbuf));			\
									\
	if (error)							\
		return (error);						\
} while(0)

	NVRAM_BCM_HEADER_DEFAULT(cfg0,		NVRAM_CFG0_SDRAM_INIT);
	NVRAM_BCM_HEADER_DEFAULT(cfg1,		NVRAM_CFG1_SDRAM_CFG);
	NVRAM_BCM_HEADER_DEFAULT(cfg1,		NVRAM_CFG1_SDRAM_REFRESH);
	NVRAM_BCM_HEADER_DEFAULT(sdram_ncdl,	NVRAM_SDRAM_NCDL);

#undef	NVRAM_BCM_HEADER_DEFAULT

	return (0);
}


/* FMT_BCM record parsing */
static int
bhnd_nvram_bcm_enum_buf(struct bhnd_nvram *sc, const char **env, size_t *len,
    const uint8_t *p, uint8_t const **next)
{
	/* First record is found following the NVRAM header */
	if (p == NULL)
		p = sc->buf + sizeof(struct bhnd_nvram_header);

	if (!bhnd_nvram_bufptr_valid(sc, p, 1, true))
		return (EINVAL);

	/* EOF */
	if (*p == '\0') {
		*env = NULL;
		*len = 0;
		*next = p;
		return (0);
	}

	/* Provide pointer to env data */
	*env = p;
	*len = strnlen(p, sc->buf_size - (p - sc->buf));

	/* Advance to next entry and skip terminating NUL */
	p += *len;
	if (bhnd_nvram_bufptr_valid(sc, p, 1, false)) {
		p++;
	} else {
		NVRAM_LOG(sc, "warning: missing NVRAM termination record");
	}

	*next = p;
	return (0);
}

/* FMT_TLV NVRAM data size calculation */
static int
bhnd_nvram_tlv_getsize(const void *data, size_t *size)
{
	const uint8_t	*const start = data;
	size_t		 offset;
	uint16_t	 rlen;

	offset = 0;
	while (offset < *size) {
		uint8_t type;

		/* Fetch type */
		type = *(start+offset);

		/* EOF */
		if (type == NVRAM_TLV_TYPE_END) {
			*size = offset + 1;
			return (0);
		}

		if ((offset++) == *size)
			return (EINVAL);

		/* Determine record length */
		if (type & NVRAM_TLV_TF_U8_LEN) {
			rlen = *(start+offset);
		} else {
			rlen = *(start+offset) << 8;
			if ((offset++) == *size)
				return (EINVAL);
			rlen |= *(start+offset);
		}

		if ((offset++) >= *size)
			return (EINVAL);

		/* Advance to next entry */
		if (rlen > *size || *size - rlen < offset)
			return (EINVAL);

		offset += rlen;
	}

	/* EOF not found */
	return (EINVAL);
}

/* FMT_TLV-specific parser initialization */
static int
bhnd_nvram_tlv_init(struct bhnd_nvram *sc)
{
	return (0);
}

/* FMT_TLV record parsing */
static int
bhnd_nvram_tlv_enum_buf(struct bhnd_nvram *sc, const char **env, size_t *len,
    const uint8_t *p, uint8_t const **next)
{
	size_t		 rlen;
	uint8_t		 type;

	if (p == NULL)
		p = sc->buf;

	/* Fetch type */
	if (!bhnd_nvram_bufptr_valid(sc, p, 1, true))
		return (EINVAL);

	type = *p;

	/* EOF */
	if (type == NVRAM_TLV_TYPE_END) {
		*env = NULL;
		*len = 0;
		*next = p;
		return (0);
	}

	/* Determine record length */
	p++;
	if (type & NVRAM_TLV_TF_U8_LEN) {
		if (!bhnd_nvram_bufptr_valid(sc, p, 1, true))
			return (EINVAL);
	
		rlen = *p;
		p += 1;
	} else {
		if (!bhnd_nvram_bufptr_valid(sc, p, 2, true))
			return (EINVAL);
		rlen = (p[0] << 8) | (p[1]);
		p += 2;
	}

	/* Verify record readability */
	if (!bhnd_nvram_bufptr_valid(sc, p, rlen, true))
		return (EINVAL);

	/* Error on non-env records */
	if (type != NVRAM_TLV_TYPE_ENV) {
		NVRAM_LOG(sc, "unsupported NVRAM TLV tag: %#hhx\n", type);
		return (EINVAL);
	}

	/* Skip flag field */
	if (rlen < 1)
		return (EINVAL);
	p++;
	rlen--;

	/* Provide pointer to env data */
	*env = p;
	*len = strnlen(*env, rlen);

	/* Advance to next entry */
	*next = p + rlen;

	return (0);
}

/* FMT_BTXT NVRAM data size calculation */
static int
bhnd_nvram_txt_getsize(const void *data, size_t *size)
{
	*size = (strnlen(data, *size));
	return (0);
}

/* FMT_BTXT-specific parser initialization */
static int
bhnd_nvram_txt_init(struct bhnd_nvram *sc)
{
	return (0);
}

/* Seek past the next line ending (\r, \r\n, or \n) */
static const uint8_t *
bhnd_nvram_txt_seek_eol(struct bhnd_nvram *sc, const uint8_t *p)
{
	while (p < sc->buf + sc->buf_size) {
		switch (*p) {
		case '\r':
			/* \r\n */
			if (bhnd_nvram_bufptr_valid(sc, p, 1, false)) {
				if (*(p+1) == '\n')
					p++;
			}

			return (p+1);
		case '\n':
			return (p+1);
		default:
			p++;
			break;
		}
	}

	return (p);
}

/* Seek to the next valid key=value entry (or EOF) */
static const uint8_t *
bhnd_nvram_txt_seek_nextline(struct bhnd_nvram *sc, const uint8_t *p)
{
	/* Skip leading whitespace and comments */
	while (p < sc->buf + sc->buf_size) {
		if (isspace(*p)) {
			p++;
			continue;
		}
		
		if (*p == '#') {
			p = bhnd_nvram_txt_seek_eol(sc, p);
			continue;
		}
		
		break;
	}

	return (p);
}

/* FMT_BTXT record parsing */
static int
bhnd_nvram_txt_enum_buf(struct bhnd_nvram *sc, const char **env, size_t *len,
    const uint8_t *p, uint8_t const **next)
{
	const uint8_t	*startp;
	size_t		 line_len;

	if (p == NULL)
		p = sc->buf;

	/* Skip any leading whitespace and comments */
	p = bhnd_nvram_txt_seek_nextline(sc, p);

	/* EOF? */
	if (!bhnd_nvram_bufptr_valid(sc, p, 1, false)) {
		*env = NULL;
		*len = 0;
		*next = p;
		return (0);
	}

	/* Find record termination (EOL, or '#') */
	startp = p;
	while (p < sc->buf + sc->buf_size) {
		if (*p == '#' || *p == '\n' || *p == '\r')
			break;

		p++;
	}

	/* Calculate line length, check for EOF */
	line_len = p - startp;
	if (!bhnd_nvram_bufptr_valid(sc, p, 1, false)) {
		*env = NULL;
		*len = 0;
		*next = p;
		return (0);
	}

	/* Got env data; trim any tailing whitespace */
	*env = startp;
	*len = line_len;

	for (size_t i = 0; i < line_len && line_len > 0; i++) {
		char c = startp[line_len - i - 1];
		if (!isspace(c))
			break;

		*len -= 1;
	}

	/* Advance to next entry */
	p = bhnd_nvram_txt_seek_nextline(sc, p);
	
	*next = p;
	return (0);
}
