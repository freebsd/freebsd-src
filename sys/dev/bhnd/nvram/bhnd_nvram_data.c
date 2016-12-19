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


#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/_inttypes.h>

#else /* !_KERNEL */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#endif /* _KERNEL */

#include "bhnd_nvram_private.h"
#include "bhnd_nvram_io.h"

#include "bhnd_nvram_datavar.h"
#include "bhnd_nvram_data.h"

/**
 * Return a human-readable description for the given NVRAM data class.
 * 
 * @param cls The NVRAM class.
 */
const char *
bhnd_nvram_data_class_desc(bhnd_nvram_data_class *cls)
{
	return (cls->desc);
}

/**
 * Probe to see if this NVRAM data class class supports the data mapped by the
 * given I/O context, returning a BHND_NVRAM_DATA_PROBE probe result.
 *
 * @param cls The NVRAM class.
 * @param io An I/O context mapping the NVRAM data.
 *
 * @retval 0 if this is the only possible NVRAM data class for @p io.
 * @retval negative if the probe succeeds, a negative value should be returned;
 * the class returning the highest negative value should be selected to handle
 * NVRAM parsing.
 * @retval ENXIO If the NVRAM format is not handled by @p cls.
 * @retval positive if an error occurs during probing, a regular unix error
 * code should be returned.
 */
int
bhnd_nvram_data_probe(bhnd_nvram_data_class *cls, struct bhnd_nvram_io *io)
{
	return (cls->op_probe(io));
}

/**
 * Probe to see if an NVRAM data class in @p classes supports parsing
 * of the data mapped by @p io, returning the parsed data in @p data.
 * 
 * The caller is responsible for deallocating the returned instance via
 * bhnd_nvram_data_release().
 * 
 * @param[out] data On success, the parsed NVRAM data instance.
 * @param io An I/O context mapping the NVRAM data to be copied and parsed.
 * @param classes An array of NVRAM data classes to be probed, or NULL to
 * probe the default supported set.
 * @param num_classes The number of NVRAM data classes in @p classes.
 * 
 * @retval 0 success
 * @retval ENXIO if no class is found capable of parsing @p io.
 * @retval non-zero if an error otherwise occurs during allocation,
 * initialization, or parsing of the NVRAM data, a regular unix error code
 * will be returned.
 */
int
bhnd_nvram_data_probe_classes(struct bhnd_nvram_data **data,
    struct bhnd_nvram_io *io, bhnd_nvram_data_class *classes[],
    size_t num_classes)
{
	bhnd_nvram_data_class	*cls;
	int			 error, prio, result;

	cls = NULL;
	prio = 0;
	*data = NULL;

	/* If class array is NULL, default to our linker set */
	if (classes == NULL) {
		classes = SET_BEGIN(bhnd_nvram_data_class_set);
		num_classes = SET_COUNT(bhnd_nvram_data_class_set);
	}

	/* Try to find the best data class capable of parsing io */
	for (size_t i = 0; i < num_classes; i++) {
		bhnd_nvram_data_class *next_cls;

		next_cls = classes[i];

		/* Try to probe */
		result = bhnd_nvram_data_probe(next_cls, io);

		/* The parser did not match if an error was returned */
		if (result > 0)
			continue;

		/* Lower priority than previous match; keep
		 * searching */
		if (cls != NULL && result <= prio)
			continue;

		/* Drop any previously parsed data */
		if (*data != NULL) {
			bhnd_nvram_data_release(*data);
			*data = NULL;
		}

		/* If this is a 'maybe' match, attempt actual parsing to
		 * verify that this does in fact match */
		if (result <= BHND_NVRAM_DATA_PROBE_MAYBE) {
			/* If parsing fails, keep searching */
			error = bhnd_nvram_data_new(next_cls, data, io);
			if (error)
				continue;
		}

		/* Record best new match */
		prio = result;
		cls = next_cls;


		/* Terminate search immediately on
		 * BHND_NVRAM_DATA_PROBE_SPECIFIC */
		if (result == BHND_NVRAM_DATA_PROBE_SPECIFIC)
			break;
	}

	/* If no match, return error */
	if (cls == NULL)
		return (ENXIO);

	/* If the NVRAM data was not parsed above, do so now */
	if (*data == NULL) {
		if ((error = bhnd_nvram_data_new(cls, data, io)))
			return (error);
	}

	return (0);
}

/**
 * Allocate and initialize a new instance of data class @p cls, copying and
 * parsing NVRAM data from @p io.
 *
 * The caller is responsible for releasing the returned parser instance
 * reference via bhnd_nvram_data_release().
 * 
 * @param cls If non-NULL, the data class to be allocated. If NULL,
 * bhnd_nvram_data_probe_classes() will be used to determine the data format.
 * @param[out] nv On success, a pointer to the newly allocated NVRAM data instance.
 * @param io An I/O context mapping the NVRAM data to be copied and parsed.
 * 
 * @retval 0 success
 * @retval non-zero if an error occurs during allocation or initialization, a
 * regular unix error code will be returned.
 */
int
bhnd_nvram_data_new(bhnd_nvram_data_class *cls, struct bhnd_nvram_data **nv,
    struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_data	*data;
	int			 error;

	/* If NULL, try to identify the appropriate class */
	if (cls == NULL)
		return (bhnd_nvram_data_probe_classes(nv, io, NULL, 0));

	/* Allocate new instance */
	BHND_NV_ASSERT(sizeof(struct bhnd_nvram_data) <= cls->size,
	    ("instance size %zu less than minimum %zu", cls->size,
	     sizeof(struct bhnd_nvram_data)));

	data = bhnd_nv_calloc(1, cls->size);
	data->cls = cls;
	refcount_init(&data->refs, 1);

	/* Let the class handle initialization */
	if ((error = cls->op_new(data, io))) {
		bhnd_nv_free(data);
		return (error);
	}

	*nv = data;
	return (0);
}

/**
 * Retain and return a reference to the given data instance.
 * 
 * @param nv The reference to be retained.
 */
struct bhnd_nvram_data *
bhnd_nvram_data_retain(struct bhnd_nvram_data *nv)
{
	refcount_acquire(&nv->refs);
	return (nv);
}

/**
 * Release a reference to the given data instance.
 *
 * If this is the last reference, the data instance and its associated
 * resources will be freed.
 * 
 * @param nv The reference to be released.
 */
void
bhnd_nvram_data_release(struct bhnd_nvram_data *nv)
{
	if (!refcount_release(&nv->refs))
		return;

	/* Free any internal resources */
	nv->cls->op_free(nv);

	/* Free the instance allocation */
	bhnd_nv_free(nv);
}

/**
 * Return a pointer to @p nv's data class.
 * 
 * @param nv The NVRAM data instance to be queried.
 */
bhnd_nvram_data_class *
bhnd_nvram_data_get_class(struct bhnd_nvram_data *nv)
{
	return (nv->cls);
}

/**
 * Return the number of variables in @p nv.
 * 
 * @param nv The NVRAM data to be queried.
 */
size_t
bhnd_nvram_data_count(struct bhnd_nvram_data *nv)
{
	return (nv->cls->op_count(nv));
}

/**
 * Compute the size of the serialized form of @p nv.
 *
 * Serialization may be performed via bhnd_nvram_data_serialize().
 *
 * @param	nv	The NVRAM data to be queried.
 * @param[out]	len	On success, will be set to the computed size.
 * 
 * @retval 0		success
 * @retval non-zero	if computing the serialized size otherwise fails, a
 *			regular unix error code will be returned.
 */
int
bhnd_nvram_data_size(struct bhnd_nvram_data *nv, size_t *len)
{
	return (nv->cls->op_size(nv, len));
}

/**
 * Serialize the NVRAM data to @p buf, using the NVRAM data class' native
 * format.
 * 
 * The resulting serialization may be reparsed with @p nv's BHND NVRAM data
 * class.
 * 
 * @param		nv	The NVRAM data to be serialized.
 * @param[out]		buf	On success, the serialed NVRAM data will be
 *				written to this buffer. This argment may be
 *				NULL if the value is not desired.
 * @param[in,out]	len	The capacity of @p buf. On success, will be set
 *				to the actual length of the serialized data.
 *
 * @retval 0		success
 * @retval ENOMEM	If @p buf is non-NULL and a buffer of @p len is too
 *			small to hold the serialized data.
 * @retval non-zero	If serialization otherwise fails, a regular unix error
 *			code will be returned.
 */
int
bhnd_nvram_data_serialize(struct bhnd_nvram_data *nv,
    void *buf, size_t *len)
{
	return (nv->cls->op_serialize(nv, buf, len));
}

/**
 * Return the capability flags (@see BHND_NVRAM_DATA_CAP_*) for @p nv.
 *
 * @param	nv	The NVRAM data to be queried.
 */
uint32_t
bhnd_nvram_data_caps(struct bhnd_nvram_data *nv)
{
	return (nv->cls->op_caps(nv));
}

/**
 * Iterate over @p nv, returning the names of subsequent variables.
 * 
 * @param		nv	The NVRAM data to be iterated.
 * @param[in,out]	cookiep	A pointer to a cookiep value previously returned
 *				by bhnd_nvram_data_next(), or a NULL value to
 *				begin iteration.
 * 
 * @return Returns the next variable name, or NULL if there are no more
 * variables defined in @p nv.
 */
const char *
bhnd_nvram_data_next(struct bhnd_nvram_data *nv, void **cookiep)
{
	return (nv->cls->op_next(nv, cookiep));
}

/**
 * Search @p nv for a named variable, returning the variable's opaque reference
 * if found, or NULL if unavailable.
 *
 * The BHND_NVRAM_DATA_CAP_INDEXED capability flag will be returned by
 * bhnd_nvram_data_caps() if @p nv supports effecient name-based
 * lookups.
 *
 * @param	nv	The NVRAM data to search.
 * @param	name	The name to search for.
 *
 * @retval non-NULL	If @p name is found, the opaque cookie value will be
 *			returned.
 * @retval NULL		If @p name is not found.
 */
void *
bhnd_nvram_data_find(struct bhnd_nvram_data *nv, const char *name)
{
	return (nv->cls->op_find(nv, name));
}

/**
 * A generic implementation of bhnd_nvram_data_find().
 *
 * This implementation will use bhnd_nvram_data_next() to perform a
 * simple O(n) case-insensitve search for @p name.
 */
void *
bhnd_nvram_data_generic_find(struct bhnd_nvram_data *nv, const char *name)
{
	const char	*next;
	void		*cookiep;

	cookiep = NULL;
	while ((next = bhnd_nvram_data_next(nv, &cookiep))) {
		if (strcasecmp(name, next) == 0)
			return (cookiep);
	}

	/* Not found */
	return (NULL);
}

/**
 * Read a variable and decode as @p type.
 *
 * @param		nv	The NVRAM data.
 * @param		cookiep	An NVRAM variable cookie previously returned
 *				via bhnd_nvram_data_next() or
 *				bhnd_nvram_data_find().
 * @param[out]		buf	On success, the requested value will be written
 *				to this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	len	The capacity of @p buf. On success, will be set
 *				to the actual size of the requested value.
 * @param		type	The data type to be written to @p buf.
 *
 * @retval 0		success
 * @retval ENOMEM	If @p buf is non-NULL and a buffer of @p len is too
 *			small to hold the requested value.
 * @retval EFTYPE	If the variable data cannot be coerced to @p type.
 * @retval ERANGE	If value coercion would overflow @p type.
 */
int
bhnd_nvram_data_getvar(struct bhnd_nvram_data *nv, void *cookiep, void *buf,
    size_t *len, bhnd_nvram_type type)
{
	return (nv->cls->op_getvar(nv, cookiep, buf, len, type));
}


/**
 * A generic implementation of bhnd_nvram_data_getvar().
 * 
 * This implementation will call bhnd_nvram_data_getvar_ptr() to fetch
 * a pointer to the variable data and perform data coercion on behalf
 * of the caller.
 *
 * If a variable definition for the requested variable is available via
 * bhnd_nvram_find_vardefn(), the definition will be used to provide
 * formatting hints to bhnd_nvram_coerce_value().
 */
int
bhnd_nvram_data_generic_rp_getvar(struct bhnd_nvram_data *nv, void *cookiep,
    void *outp, size_t *olen, bhnd_nvram_type otype)
{
	bhnd_nvram_val			 val;
	const struct bhnd_nvram_vardefn	*vdefn;
	const bhnd_nvram_val_fmt	*fmt;
	const char			*name;
	const void			*vptr;
	bhnd_nvram_type			 vtype;
	size_t				 vlen;
	int				 error;

	BHND_NV_ASSERT(bhnd_nvram_data_caps(nv) & BHND_NVRAM_DATA_CAP_READ_PTR,
	    ("instance does not advertise READ_PTR support"));

	/* Fetch pointer to our variable data */
	vptr = bhnd_nvram_data_getvar_ptr(nv, cookiep, &vlen, &vtype);
	if (vptr == NULL)
		return (EINVAL);

	/* Use the NVRAM string support */
	switch (vtype) {
	case BHND_NVRAM_TYPE_STRING:
	case BHND_NVRAM_TYPE_STRING_ARRAY:
		fmt = &bhnd_nvram_val_bcm_string_fmt;
		break;
	default:
		fmt = NULL;
	}

	/* Check the variable definition table for a matching entry; if
	 * it exists, use it to populate the value format. */
	name = bhnd_nvram_data_getvar_name(nv, cookiep);
	vdefn = bhnd_nvram_find_vardefn(name);
	if (vdefn != NULL)
		fmt = vdefn->fmt;

	/* Attempt value coercion */
	error = bhnd_nvram_val_init(&val, fmt, vptr, vlen, vtype,
	    BHND_NVRAM_VAL_BORROW_DATA);
	if (error)
		return (error);

	error = bhnd_nvram_val_encode(&val, outp, olen, otype);

	/* Clean up */
	bhnd_nvram_val_release(&val);
	return (error);
}

/**
 * If available and supported by the NVRAM data instance, return a reference
 * to the internal buffer containing an entry's variable data,
 * 
 * Note that string values may not be NUL terminated.
 *
 * @param		nv	The NVRAM data.
 * @param		cookiep	An NVRAM variable cookie previously returned
 *				via bhnd_nvram_data_next() or
 *				bhnd_nvram_data_find().
 * @param[out]		len	On success, will be set to the actual size of
 *				the requested value.
 * @param[out]		type	The data type of the entry data.
 *
 * @retval non-NULL	success
 * @retval NULL		if direct data access is unsupported by @p nv, or
 *			unavailable for @p cookiep.
 */
const void *
bhnd_nvram_data_getvar_ptr(struct bhnd_nvram_data *nv, void *cookiep,
    size_t *len, bhnd_nvram_type *type)
{
	return (nv->cls->op_getvar_ptr(nv, cookiep, len, type));
}


/**
 * Return the variable name associated with a given @p cookiep.
 * @param		nv	The NVRAM data to be iterated.
 * @param[in,out]	cookiep	A pointer to a cookiep value previously returned
 *				via bhnd_nvram_data_next() or
 *				bhnd_nvram_data_find().
 *
 * @return Returns the variable's name.
 */
const char *
bhnd_nvram_data_getvar_name(struct bhnd_nvram_data *nv, void *cookiep)
{
	return (nv->cls->op_getvar_name(nv, cookiep));
}
