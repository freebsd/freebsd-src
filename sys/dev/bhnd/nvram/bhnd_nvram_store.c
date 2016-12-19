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

#include <sys/queue.h>

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/systm.h>

#else /* !_KERNEL */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#endif /* _KERNEL */

#include "bhnd_nvram_private.h"
#include "bhnd_nvram_datavar.h"

#include "bhnd_nvram_storevar.h"

/*
 * BHND NVRAM Store
 *
 * Manages in-memory and persistent representations of NVRAM data.
 */

static int	 bhnd_nvram_sort_idx(void *ctx, const void *lhs,
		     const void *rhs);
static int	 bhnd_nvram_generate_index(struct bhnd_nvram_store *sc);
static void	*bhnd_nvram_index_lookup(struct bhnd_nvram_store *sc,
		     const char *name);

/**
 * Allocate and initialize a new NVRAM data store instance.
 *
 * The caller is responsible for deallocating the instance via
 * bhnd_nvram_store_free().
 * 
 * @param[out] store On success, a pointer to the newly allocated NVRAM data
 * instance.
 * @param data The NVRAM data to be managed by the returned NVRAM data store
 * instance.
 *
 * @retval 0 success
 * @retval non-zero if an error occurs during allocation or initialization, a
 * regular unix error code will be returned.
 */
int
bhnd_nvram_store_new(struct bhnd_nvram_store **store,
    struct bhnd_nvram_data *data)
{
	struct bhnd_nvram_store *sc;
	int			 error;

	/* Allocate new instance */
	sc = bhnd_nv_calloc(1, sizeof(*sc));
	if (sc == NULL)
		return (ENOMEM);

	LIST_INIT(&sc->paths);

	/* Retain the NVRAM data */
	sc->nv = bhnd_nvram_data_retain(data);

	/* Allocate uncommitted change list */
	sc->pending = nvlist_create(NV_FLAG_IGNORE_CASE);
	if (sc->pending == NULL) {
		error = ENOMEM;
		goto cleanup;
	}

	/* Generate all indices */
	if ((error = bhnd_nvram_generate_index(sc)))
		goto cleanup;

	BHND_NVSTORE_LOCK_INIT(sc);

	*store = sc;
	return (0);

cleanup:
	bhnd_nvram_store_free(sc);
	return (error);
}

/**
 * Allocate and initialize a new NVRAM data store instance, parsing the
 * NVRAM data from @p io.
 *
 * The caller is responsible for deallocating the instance via
 * bhnd_nvram_store_free().
 * 
 * The NVRAM data mapped by @p io will be copied, and @p io may be safely
 * deallocated after bhnd_nvram_store_new() returns.
 * 
 * @param[out] store On success, a pointer to the newly allocated NVRAM data
 * instance.
 * @param io An I/O context mapping the NVRAM data to be copied and parsed.
 * @param cls The NVRAM data class to be used when parsing @p io, or NULL
 * to perform runtime identification of the appropriate data class.
 *
 * @retval 0 success
 * @retval non-zero if an error occurs during allocation or initialization, a
 * regular unix error code will be returned.
 */
int
bhnd_nvram_store_parse_new(struct bhnd_nvram_store **store,
    struct bhnd_nvram_io *io, bhnd_nvram_data_class *cls)
{
	struct bhnd_nvram_data	*data;
	int			 error;


	/* Try to parse the data */
	if ((error = bhnd_nvram_data_new(cls, &data, io)))
		return (error);

	/* Try to create our new store instance */
	error = bhnd_nvram_store_new(store, data);
	bhnd_nvram_data_release(data);

	return (error);
}

/**
 * Free an NVRAM store instance, releasing all associated resources.
 * 
 * @param sc A store instance previously allocated via
 * bhnd_nvram_store_new().
 */
void
bhnd_nvram_store_free(struct bhnd_nvram_store *sc)
{
	struct bhnd_nvstore_path *dpath, *dnext;

        LIST_FOREACH_SAFE(dpath, &sc->paths, dp_link, dnext) {
		bhnd_nv_free(dpath->path);
                bhnd_nv_free(dpath);
        }

	if (sc->pending != NULL)
		nvlist_destroy(sc->pending);

	if (sc->idx != NULL)
		bhnd_nv_free(sc->idx);

	if (sc->nv != NULL)
		bhnd_nvram_data_release(sc->nv);

	BHND_NVSTORE_LOCK_DESTROY(sc);
	bhnd_nv_free(sc);
}

/**
 * Read an NVRAM variable.
 *
 * @param              sc      The NVRAM parser state.
 * @param              name    The NVRAM variable name.
 * @param[out]         buf     On success, the requested value will be written
 *                             to this buffer. This argment may be NULL if
 *                             the value is not desired.
 * @param[in,out]      len     The capacity of @p buf. On success, will be set
 *                             to the actual size of the requested value.
 * @param              type    The requested data type to be written to @p buf.
 *
 * @retval 0           success
 * @retval ENOENT      The requested variable was not found.
 * @retval ENOMEM      If @p buf is non-NULL and a buffer of @p len is too
 *                     small to hold the requested value.
 * @retval non-zero    If reading @p name otherwise fails, a regular unix
 *                     error code will be returned.
  */
int
bhnd_nvram_store_getvar(struct bhnd_nvram_store *sc, const char *name,
    void *buf, size_t *len, bhnd_nvram_type type)
{
	void		*cookiep;
	const void	*inp;
	size_t		 ilen;
	bhnd_nvram_type	 itype;
	int		 error;

	/*
	 * Search order:
	 *
	 * - uncommitted changes
	 * - index lookup OR buffer scan
	 */

	BHND_NVSTORE_LOCK(sc);

	/* Is variable marked for deletion? */
	if (nvlist_exists_null(sc->pending, name)) {
		BHND_NVSTORE_UNLOCK(sc);
		return (ENOENT);
	}

	/* Does an uncommitted value exist? */
	if (nvlist_exists_string(sc->pending, name)) {
		/* Uncommited value exists, is not a deletion */
		inp = nvlist_get_string(sc->pending, name);
		ilen = strlen(inp) + 1;
		itype = BHND_NVRAM_TYPE_STRING;

		/* Coerce borrowed data reference before releasing
		 * our lock. */
		error = bhnd_nvram_value_coerce(inp, ilen, itype, buf, len,
		    type);

		BHND_NVSTORE_UNLOCK(sc);

		return (error);
	} else if (nvlist_exists(sc->pending, name)) {
		BHND_NV_PANIC("invalid value type for pending change %s", name);
	}

	/* Fetch variable from parsed NVRAM data. */
	if ((cookiep = bhnd_nvram_index_lookup(sc, name)) == NULL) {
		BHND_NVSTORE_UNLOCK(sc);
		return (ENOENT);
	}

	/* Let the parser itself perform value coercion */
	error =  bhnd_nvram_data_getvar(sc->nv, cookiep, buf, len, type);
	BHND_NVSTORE_UNLOCK(sc);

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
bhnd_nvram_store_setvar(struct bhnd_nvram_store *sc, const char *name,
    const void *buf, size_t len, bhnd_nvram_type type)
{
	const char	*inp;
	char		 vbuf[512];

	/* Verify name validity */
	if (!bhnd_nvram_validate_name(name, strlen(name)))
		return (EINVAL);

	/* Verify buffer size alignment for the given type. If this is a
	 * variable width type, a width of 0 will always pass this check */
	if (len % bhnd_nvram_value_size(buf, len, type, 1) != 0)
		return (EINVAL);

	/* Determine string format (or directly add variable, if a C string) */
	switch (type) {
	case BHND_NVRAM_TYPE_UINT8:
	case BHND_NVRAM_TYPE_UINT16:
	case BHND_NVRAM_TYPE_UINT32:
	case BHND_NVRAM_TYPE_UINT64:
	case BHND_NVRAM_TYPE_INT8:
	case BHND_NVRAM_TYPE_INT16:
	case BHND_NVRAM_TYPE_INT32:
	case BHND_NVRAM_TYPE_INT64:
	case BHND_NVRAM_TYPE_NULL:
	case BHND_NVRAM_TYPE_DATA:
	case BHND_NVRAM_TYPE_BOOL:
	case BHND_NVRAM_TYPE_UINT8_ARRAY:
	case BHND_NVRAM_TYPE_UINT16_ARRAY:
	case BHND_NVRAM_TYPE_UINT32_ARRAY:
	case BHND_NVRAM_TYPE_UINT64_ARRAY:
	case BHND_NVRAM_TYPE_INT8_ARRAY:
	case BHND_NVRAM_TYPE_INT16_ARRAY:
	case BHND_NVRAM_TYPE_INT32_ARRAY:
	case BHND_NVRAM_TYPE_INT64_ARRAY:
	case BHND_NVRAM_TYPE_CHAR_ARRAY:
	case BHND_NVRAM_TYPE_STRING_ARRAY:
	case BHND_NVRAM_TYPE_BOOL_ARRAY:
		// TODO: non-char/string value support
		return (EOPNOTSUPP);

	case BHND_NVRAM_TYPE_CHAR:
	case BHND_NVRAM_TYPE_STRING:
		inp = buf;

		/* Must not exceed buffer size */
		if (len > sizeof(vbuf))
			return (EINVAL);

		/* Must have room for a trailing NUL */
		if (len == sizeof(vbuf) && inp[len-1] != '\0')
			return (EINVAL);

		/* Copy out the string value and append trailing NUL */
		strlcpy(vbuf, buf, len);
	
		/* Add to pending change list */
		BHND_NVSTORE_LOCK(sc);
		nvlist_add_string(sc->pending, name, vbuf);
		BHND_NVSTORE_UNLOCK(sc);
	}

	return (0);
}

/* sort function for bhnd_nvstore_index cookie values */
static int
bhnd_nvram_sort_idx(void *ctx, const void *lhs, const void *rhs)
{
	struct bhnd_nvram_store			*sc;
	const char				*l_str, *r_str;

	sc = ctx;

	/* Fetch string pointers from the cookiep values */
	l_str = bhnd_nvram_data_getvar_name(sc->nv, *(void * const *)lhs);
	r_str = bhnd_nvram_data_getvar_name(sc->nv, *(void * const *)rhs);

	/* Perform comparison */
	return (strcasecmp(l_str, r_str));
}

/**
 * Parse and register all device paths and path aliases in @p nvram.
 * 
 * @param sc		The NVRAM parser state.
 *
 * @retval 0		success
 * @retval non-zero	If registering device paths fails, a regular unix
 *			error code will be returned.
 */
static int
bhnd_nvram_register_devpaths(struct bhnd_nvram_store *sc)
{
	const char	*name;
	void		*cookiep;
	int		 error;

	/* Skip if backing parser does not support device paths */
	if (!(bhnd_nvram_data_caps(sc->nv) & BHND_NVRAM_DATA_CAP_DEVPATHS))
		return (0);

	/* Parse and register all device path aliases */
	cookiep = NULL;
	while ((name = bhnd_nvram_data_next(sc->nv, &cookiep))) {
		struct bhnd_nvstore_path	*devpath;
		const char			*suffix;
		char				*eptr;
		char				*path;
		size_t				 path_len;
		u_long				 index;

		path = NULL;

		/* Check for devpath prefix */
		if (strncmp(name, "devpath", strlen("devpath")) != 0)
			continue;

		/* Parse index value that should follow a 'devpath' prefix */
		suffix = name + strlen("devpath");
		index = strtoul(suffix, &eptr, 10);
		if (eptr == suffix || *eptr != '\0') {
			BHND_NV_LOG("invalid devpath variable '%s'\n", name);
			continue;
		}

		/* Determine path value length */
		error = bhnd_nvram_data_getvar(sc->nv, cookiep, NULL, &path_len,
		    BHND_NVRAM_TYPE_STRING);
		if (error)
			return (error);

		/* Allocate path buffer */
		if ((path = bhnd_nv_malloc(path_len)) == NULL)
			return (ENOMEM);

		/* Decode to our new buffer */
		error = bhnd_nvram_data_getvar(sc->nv, cookiep, path, &path_len,
		    BHND_NVRAM_TYPE_STRING);
		if (error) {
			bhnd_nv_free(path);
			return (error);
		}

		/* Register path alias */
		devpath = bhnd_nv_malloc(sizeof(*devpath));
		if (devpath == NULL) {
			bhnd_nv_free(path);
			return (ENOMEM);
		}

		devpath->index = index;
		devpath->path = path;
		LIST_INSERT_HEAD(&sc->paths, devpath, dp_link);
	}

	return (0);
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
bhnd_nvram_generate_index(struct bhnd_nvram_store *sc)
{
	const char	*name;
	void		*cookiep;
	size_t		 idx_bytes;
	size_t		 num_vars;
	int		 error;

	/* Parse and register all device path aliases */
	if ((error = bhnd_nvram_register_devpaths(sc)))
		return (error);

	/* Skip generating a variable index if threshold is not met ... */
	num_vars = bhnd_nvram_data_count(sc->nv);
	if (num_vars < NVRAM_IDX_VAR_THRESH)
		return (0);

	/* ... or if the backing data instance implements indexed lookup
	 * internally */
	if (bhnd_nvram_data_caps(sc->nv) & BHND_NVRAM_DATA_CAP_INDEXED)
		return (0);

	/* Allocate and populate variable index */
	idx_bytes = sizeof(struct bhnd_nvstore_index) +
	    (sizeof(void *) * num_vars);
	sc->idx = bhnd_nv_malloc(idx_bytes);
	if (sc->idx == NULL) {
		BHND_NV_LOG("error allocating %zu byte index\n", idx_bytes);
		goto bad_index;
	}

	sc->idx->num_cookiep = num_vars;

#ifdef _KERNEL
	if (bootverbose) {
		BHND_NV_LOG("allocated %zu byte index for %zu variables\n",
		    idx_bytes, num_vars);
	}
#endif /* _KERNEL */

	cookiep = NULL;
	for (size_t i = 0; i < sc->idx->num_cookiep; i++) {
		/* Fetch next entry */
		name = bhnd_nvram_data_next(sc->nv, &cookiep);

		/* Early EOF */
		if (name == NULL) {
			BHND_NV_LOG("indexing failed, expected %zu records "
			    "(got %zu)\n", sc->idx->num_cookiep, i+1);
			goto bad_index;
		}

		/* Save the variable's cookiep */
		sc->idx->cookiep[i] = cookiep;
	}

	/* Sort the index table */
	qsort_r(sc->idx->cookiep, sc->idx->num_cookiep,
	    sizeof(sc->idx->cookiep[0]), sc, bhnd_nvram_sort_idx);

	return (0);

bad_index:
	/* Fall back on non-indexed access */
	BHND_NV_LOG("reverting to non-indexed variable lookup\n");
	if (sc->idx != NULL) {
		bhnd_nv_free(sc->idx);
		sc->idx = NULL;
	}

	return (0);
}


/**
 * Perform an index lookup of @p name, returning the associated cookie
 * value, or NULL if the variable does not exist.
 *
 * @param	sc		The NVRAM parser state.
 * @param	name		The variable to search for.
 */
static void *
bhnd_nvram_index_lookup(struct bhnd_nvram_store *sc, const char *name)
{
	void		*cookiep;
	const char	*indexed_name;
	size_t		 min, mid, max;
	int		 order;

	BHND_NVSTORE_LOCK_ASSERT(sc, MA_OWNED);

	if (sc->idx == NULL || sc->idx->num_cookiep == 0)
		return (bhnd_nvram_data_find(sc->nv, name));

	/*
	 * Locate the requested variable using a binary search.
	 */
	BHND_NV_ASSERT(sc->idx->num_cookiep > 0,
	    ("empty array causes underflow"));
	min = 0;
	max = sc->idx->num_cookiep - 1;

	while (max >= min) {
		/* Select midpoint */
		mid = (min + max) / 2;
		cookiep = sc->idx->cookiep[mid];

		/* Determine which side of the partition to search */
		indexed_name = bhnd_nvram_data_getvar_name(sc->nv, cookiep);
		order = strcasecmp(indexed_name, name);

		if (order < 0) {
			/* Search upper partition */
			min = mid + 1;
		} else if (order > 0) {
			/* Search (non-empty) lower partition */
			if (mid == 0)
				break;
			max = mid - 1;
		} else if (order == 0) {
			/* Match found */
			return (cookiep);
		}
	}

	/* Not found */
	return (NULL);
}
