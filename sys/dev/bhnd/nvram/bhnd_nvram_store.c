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
#include <sys/hash.h>
#include <sys/queue.h>

#ifdef _KERNEL

#include <sys/ctype.h>
#include <sys/systm.h>

#include <machine/_inttypes.h>

#else /* !_KERNEL */

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
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

static int			 bhnd_nvstore_parse_data(
				     struct bhnd_nvram_store *sc);

static int			 bhnd_nvstore_parse_path_entries(
				     struct bhnd_nvram_store *sc);

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

	BHND_NVSTORE_LOCK_INIT(sc);
	BHND_NVSTORE_LOCK(sc);

	/* Initialize path hash table */
	sc->num_paths = 0;
	for (size_t i = 0; i < nitems(sc->paths); i++)
		LIST_INIT(&sc->paths[i]);

	/* Initialize alias hash table */
	sc->num_aliases = 0;
	for (size_t i = 0; i < nitems(sc->aliases); i++)
		LIST_INIT(&sc->aliases[i]);

	/* Retain the NVRAM data */
	sc->data = bhnd_nvram_data_retain(data);
	sc->data_caps = bhnd_nvram_data_caps(data);

	/* Register required root path */
	error = bhnd_nvstore_register_path(sc, BHND_NVSTORE_ROOT_PATH,
	    BHND_NVSTORE_ROOT_PATH_LEN);
	if (error)
		goto cleanup;

	sc->root_path = bhnd_nvstore_get_path(sc, BHND_NVSTORE_ROOT_PATH,
	    BHND_NVSTORE_ROOT_PATH_LEN);
	BHND_NV_ASSERT(sc->root_path, ("missing root path"));

	/* Parse all variables vended by our backing NVRAM data instance,
	 * generating all path entries, alias entries, and variable indexes */
	if ((error = bhnd_nvstore_parse_data(sc)))
		goto cleanup;

	*store = sc;

	BHND_NVSTORE_UNLOCK(sc);
	return (0);

cleanup:
	BHND_NVSTORE_UNLOCK(sc);
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
	
	/* Clean up alias hash table */
	for (size_t i = 0; i < nitems(sc->aliases); i++) {
		bhnd_nvstore_alias *alias, *anext;
		LIST_FOREACH_SAFE(alias, &sc->aliases[i], na_link, anext)
			bhnd_nv_free(alias);
	}

	/* Clean up path hash table */
	for (size_t i = 0; i < nitems(sc->paths); i++) {
		bhnd_nvstore_path *path, *pnext;
		LIST_FOREACH_SAFE(path, &sc->paths[i], np_link, pnext)
			bhnd_nvstore_path_free(path);
	}

	if (sc->data != NULL)
		bhnd_nvram_data_release(sc->data);


	BHND_NVSTORE_LOCK_DESTROY(sc);
	bhnd_nv_free(sc);
}

/**
 * Parse all variables vended by our backing NVRAM data instance,
 * generating all path entries, alias entries, and variable indexes.
 * 
 * @param	sc	The NVRAM store instance to be initialized with
 *			paths, aliases, and data parsed from its backing
 *			data.
 *
 * @retval 0		success
 * @retval non-zero	if an error occurs during parsing, a regular unix error
 *			code will be returned.
 */
static int
bhnd_nvstore_parse_data(struct bhnd_nvram_store *sc)
{
	const char	*name;
	void		*cookiep;
	int		 error;

	/* Parse and register all device paths and path aliases. This enables
	 * resolution of _forward_ references to device paths aliases when
	 * scanning variable entries below */
	if ((error = bhnd_nvstore_parse_path_entries(sc)))
		return (error);

	/* Calculate the per-path variable counts, and report dangling alias
	 * references as an error. */
	cookiep = NULL;
	while ((name = bhnd_nvram_data_next(sc->data, &cookiep))) {
		bhnd_nvstore_path	*path;
		bhnd_nvstore_name_info	 info;

		/* Parse the name info */
		error = bhnd_nvstore_parse_name_info(name,
		    BHND_NVSTORE_NAME_INTERNAL, sc->data_caps, &info);
		if (error)
			return (error);

		switch (info.type) {
		case BHND_NVSTORE_VAR:
			/* Fetch referenced path */
			path = bhnd_nvstore_var_get_path(sc, &info);
			if (path == NULL) {
				BHND_NV_LOG("variable '%s' has dangling "
					    "path reference\n", name);
				return (EFTYPE);
			}

			/* Increment path variable count */
			if (path->num_vars == SIZE_MAX) {
				BHND_NV_LOG("more than SIZE_MAX variables in "
				    "path %s\n", path->path_str);
				return (EFTYPE);
			}
			path->num_vars++;
			break;

		case BHND_NVSTORE_ALIAS_DECL:
			/* Skip -- path alias already parsed and recorded */
			break;
		}
	}

	/* If the backing NVRAM data instance vends only a single root ("/")
	 * path, we may be able to skip generating an index for the root
	 * path */
	if (sc->num_paths == 1) {
		bhnd_nvstore_path *path;

		/* If the backing instance provides its own name-based lookup
		 * indexing, we can skip generating a duplicate here */
		if (sc->data_caps & BHND_NVRAM_DATA_CAP_INDEXED)
			return (0);

		/* If the sole root path contains fewer variables than the
		 * minimum indexing threshhold, we do not need to generate an
		 * index */
		path = bhnd_nvstore_get_root_path(sc);
		if (path->num_vars < BHND_NV_IDX_VAR_THRESHOLD)
			return (0);
	}

	/* Allocate per-path index instances */
	for (size_t i = 0; i < nitems(sc->paths); i++) {
		bhnd_nvstore_path	*path;

		LIST_FOREACH(path, &sc->paths[i], np_link) {
			path->index = bhnd_nvstore_index_new(path->num_vars);
			if (path->index == NULL)
				return (ENOMEM);
		}
	}

	/* Populate per-path indexes */
	cookiep = NULL;
	while ((name = bhnd_nvram_data_next(sc->data, &cookiep))) {
		bhnd_nvstore_name_info	 info;
		bhnd_nvstore_path	*path;

		/* Parse the name info */
		error = bhnd_nvstore_parse_name_info(name,
		    BHND_NVSTORE_NAME_INTERNAL, sc->data_caps, &info);
		if (error)
			return (error);

		switch (info.type) {
		case BHND_NVSTORE_VAR:
			/* Fetch referenced path */
			path = bhnd_nvstore_var_get_path(sc, &info);
			BHND_NV_ASSERT(path != NULL,
			    ("dangling path reference"));

			/* Append to index */
			error = bhnd_nvstore_index_append(sc, path->index,
			    cookiep);
			if (error)
				return (error);
			break;

		case BHND_NVSTORE_ALIAS_DECL:
			/* Skip */
			break;
		}
	}

	/* Prepare indexes for querying */
	for (size_t i = 0; i < nitems(sc->paths); i++) {
		bhnd_nvstore_path	*path;

		LIST_FOREACH(path, &sc->paths[i], np_link) {
			error = bhnd_nvstore_index_prepare(sc, path->index);
			if (error)
				return (error);
		}
	}

	return (0);
}


/**
 * Parse and register path and path alias entries for all declarations found in
 * the NVRAM data backing @p nvram.
 * 
 * @param sc		The NVRAM store instance.
 *
 * @retval 0		success
 * @retval non-zero	If parsing fails, a regular unix error code will be
 *			returned.
 */
static int
bhnd_nvstore_parse_path_entries(struct bhnd_nvram_store *sc)
{
	const char	*name;
	void		*cookiep;
	int		 error;

	BHND_NVSTORE_LOCK_ASSERT(sc, MA_OWNED);

	/* Skip path registration if the data source does not support device
	 * paths. */
	if (!(sc->data_caps & BHND_NVRAM_DATA_CAP_DEVPATHS)) {
		BHND_NV_ASSERT(sc->root_path != NULL, ("missing root path"));
		return (0);
	}

	/* Otherwise, parse and register all paths and path aliases */
	cookiep = NULL;
	while ((name = bhnd_nvram_data_next(sc->data, &cookiep))) {
		bhnd_nvstore_name_info info;

		/* Parse the name info */
		error = bhnd_nvstore_parse_name_info(name,
		    BHND_NVSTORE_NAME_INTERNAL, sc->data_caps, &info);
		if (error)
			return (error);

		/* Register the path */
		error = bhnd_nvstore_var_register_path(sc, &info, cookiep);
		if (error) {
			BHND_NV_LOG("failed to register path for %s: %d\n",
			    name, error);
			return (error);
		}
	}

	return (0);
}
/**
 * Read an NVRAM variable.
 *
 * @param		sc	The NVRAM parser state.
 * @param		name	The NVRAM variable name.
 * @param[out]		outp	On success, the requested value will be written
 *				to this buffer. This argment may be NULL if
 *				the value is not desired.
 * @param[in,out]	olen	The capacity of @p outp. On success, will be set
 *				to the actual size of the requested value.
 * @param		otype	The requested data type to be written to
 *				@p outp.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable was not found.
 * @retval ENOMEM	If @p outp is non-NULL and a buffer of @p olen is too
 *			small to hold the requested value.
 * @retval non-zero	If reading @p name otherwise fails, a regular unix
 *			error code will be returned.
  */
int
bhnd_nvram_store_getvar(struct bhnd_nvram_store *sc, const char *name,
    void *outp, size_t *olen, bhnd_nvram_type otype)
{
	bhnd_nvstore_name_info	 info;
	bhnd_nvstore_path	*path;
	bhnd_nvram_prop		*prop;
	void			*cookiep;
	int			 error;

	BHND_NVSTORE_LOCK(sc);

	/* Parse the variable name */
	error = bhnd_nvstore_parse_name_info(name, BHND_NVSTORE_NAME_EXTERNAL,
	    sc->data_caps, &info);
	if (error)
		goto finished;

	/* Fetch the variable's enclosing path entry */
	if ((path = bhnd_nvstore_var_get_path(sc, &info)) == NULL) {
		error = ENOENT;
		goto finished;
	}

	/* Search uncommitted updates first */
	prop = bhnd_nvstore_path_get_update(sc, path, info.name);
	if (prop != NULL) {
		if (bhnd_nvram_prop_is_null(prop)) {
			/* NULL denotes a pending deletion */
			error = ENOENT;
		} else {
			error = bhnd_nvram_prop_encode(prop, outp, olen, otype);
		}
		goto finished;
	}

	/* Search the backing NVRAM data */
	cookiep = bhnd_nvstore_path_data_lookup(sc, path, info.name);
	if (cookiep != NULL) {
		/* Found in backing store */
		error = bhnd_nvram_data_getvar(sc->data, cookiep, outp, olen,
		     otype);
		goto finished;
	}

	/* Not found */
	error = ENOENT;

finished:
	BHND_NVSTORE_UNLOCK(sc);
	return (error);
}

/**
 * Common bhnd_nvram_store_set*() and bhnd_nvram_store_unsetvar()
 * implementation.
 * 
 * If @p value is NULL, the variable will be marked for deletion.
 */
static int
bhnd_nvram_store_setval_common(struct bhnd_nvram_store *sc, const char *name,
    bhnd_nvram_val *value)
{
	bhnd_nvstore_path	*path;
	bhnd_nvstore_name_info	 info;
	int			 error;

	BHND_NVSTORE_LOCK_ASSERT(sc, MA_OWNED);

	/* Parse the variable name */
	error = bhnd_nvstore_parse_name_info(name, BHND_NVSTORE_NAME_EXTERNAL,
	    sc->data_caps, &info);
	if (error)
		return (error);

	/* Fetch the variable's enclosing path entry */
	if ((path = bhnd_nvstore_var_get_path(sc, &info)) == NULL)
		return (error);

	/* Register the update entry */
	return (bhnd_nvstore_path_register_update(sc, path, info.name, value));
}

/**
 * Set an NVRAM variable.
 * 
 * @param	sc	The NVRAM parser state.
 * @param	name	The NVRAM variable name.
 * @param	value	The new value.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable @p name was not found.
 * @retval EINVAL	If @p value is invalid.
 */
int
bhnd_nvram_store_setval(struct bhnd_nvram_store *sc, const char *name,
    bhnd_nvram_val *value)
{
	int error;

	BHND_NVSTORE_LOCK(sc);
	error = bhnd_nvram_store_setval_common(sc, name, value);
	BHND_NVSTORE_UNLOCK(sc);

	return (error);
}

/**
 * Set an NVRAM variable.
 * 
 * @param		sc	The NVRAM parser state.
 * @param		name	The NVRAM variable name.
 * @param[out]		inp	The new value.
 * @param[in,out]	ilen	The size of @p inp.
 * @param		itype	The data type of @p inp.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable @p name was not found.
 * @retval EINVAL	If the new value is invalid.
 * @retval EINVAL	If @p name is read-only.
 */
int
bhnd_nvram_store_setvar(struct bhnd_nvram_store *sc, const char *name,
    const void *inp, size_t ilen, bhnd_nvram_type itype)
{
	bhnd_nvram_val	val;
	int		error;

	error = bhnd_nvram_val_init(&val, NULL, inp, ilen, itype,
	    BHND_NVRAM_VAL_FIXED|BHND_NVRAM_VAL_BORROW_DATA);
	if (error) {
		BHND_NV_LOG("error initializing value: %d\n", error);
		return (EINVAL);
	}

	BHND_NVSTORE_LOCK(sc);
	error = bhnd_nvram_store_setval_common(sc, name, &val);
	BHND_NVSTORE_UNLOCK(sc);

	bhnd_nvram_val_release(&val);

	return (error);
}

/**
 * Unset an NVRAM variable.
 * 
 * @param		sc	The NVRAM parser state.
 * @param		name	The NVRAM variable name.
 *
 * @retval 0		success
 * @retval ENOENT	The requested variable @p name was not found.
 * @retval EINVAL	If @p name is read-only.
 */
int
bhnd_nvram_store_unsetvar(struct bhnd_nvram_store *sc, const char *name)
{
	int error;

	BHND_NVSTORE_LOCK(sc);
	error = bhnd_nvram_store_setval_common(sc, name, BHND_NVRAM_VAL_NULL);
	BHND_NVSTORE_UNLOCK(sc);

	return (error);
}
