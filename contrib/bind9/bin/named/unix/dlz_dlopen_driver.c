/*
 * Copyright (C) 2011-2013  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dlz_dlopen_driver.c,v 1.1.4.6 2012/02/22 23:46:35 tbox Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <dns/log.h>
#include <dns/result.h>
#include <dns/dlz_dlopen.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/util.h>

#include <named/globals.h>

#include <dlz/dlz_dlopen_driver.h>

#ifdef ISC_DLZ_DLOPEN
static dns_sdlzimplementation_t *dlz_dlopen = NULL;


typedef struct dlopen_data {
	isc_mem_t *mctx;
	char *dl_path;
	char *dlzname;
	void *dl_handle;
	void *dbdata;
	unsigned int flags;
	isc_mutex_t lock;
	int version;
	isc_boolean_t in_configure;

	dlz_dlopen_version_t *dlz_version;
	dlz_dlopen_create_t *dlz_create;
	dlz_dlopen_findzonedb_t *dlz_findzonedb;
	dlz_dlopen_lookup_t *dlz_lookup;
	dlz_dlopen_authority_t *dlz_authority;
	dlz_dlopen_allnodes_t *dlz_allnodes;
	dlz_dlopen_allowzonexfr_t *dlz_allowzonexfr;
	dlz_dlopen_newversion_t *dlz_newversion;
	dlz_dlopen_closeversion_t *dlz_closeversion;
	dlz_dlopen_configure_t *dlz_configure;
	dlz_dlopen_ssumatch_t *dlz_ssumatch;
	dlz_dlopen_addrdataset_t *dlz_addrdataset;
	dlz_dlopen_subrdataset_t *dlz_subrdataset;
	dlz_dlopen_delrdataset_t *dlz_delrdataset;
	dlz_dlopen_destroy_t *dlz_destroy;
} dlopen_data_t;

/* Modules can choose whether they are lock-safe or not. */
#define MAYBE_LOCK(cd) \
	do { \
		if ((cd->flags & DNS_SDLZFLAG_THREADSAFE) == 0 && \
		    cd->in_configure == ISC_FALSE) \
			LOCK(&cd->lock); \
	} while (0)

#define MAYBE_UNLOCK(cd) \
	do { \
		if ((cd->flags & DNS_SDLZFLAG_THREADSAFE) == 0 && \
		    cd->in_configure == ISC_FALSE) \
			UNLOCK(&cd->lock); \
	} while (0)

/*
 * Log a message at the given level.
 */
static void dlopen_log(int level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	isc_log_vwrite(dns_lctx, DNS_LOGCATEGORY_DATABASE,
		       DNS_LOGMODULE_DLZ, ISC_LOG_DEBUG(level),
		       fmt, ap);
	va_end(ap);
}

/*
 * SDLZ methods
 */

static isc_result_t
dlopen_dlz_allnodes(const char *zone, void *driverarg, void *dbdata,
		    dns_sdlzallnodes_t *allnodes)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;


	UNUSED(driverarg);

	if (cd->dlz_allnodes == NULL) {
		return (ISC_R_NOPERM);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_allnodes(zone, cd->dbdata, allnodes);
	MAYBE_UNLOCK(cd);
	return (result);
}


static isc_result_t
dlopen_dlz_allowzonexfr(void *driverarg, void *dbdata, const char *name,
			const char *client)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);


	if (cd->dlz_allowzonexfr == NULL) {
		return (ISC_R_NOPERM);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_allowzonexfr(cd->dbdata, name, client);
	MAYBE_UNLOCK(cd);
	return (result);
}

static isc_result_t
dlopen_dlz_authority(const char *zone, void *driverarg, void *dbdata,
		   dns_sdlzlookup_t *lookup)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_authority == NULL) {
		return (ISC_R_NOTIMPLEMENTED);
	}

	MAYBE_LOCK(cd);
	result = cd->dlz_authority(zone, cd->dbdata, lookup);
	MAYBE_UNLOCK(cd);
	return (result);
}

static isc_result_t
dlopen_dlz_findzonedb(void *driverarg, void *dbdata, const char *name)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	MAYBE_LOCK(cd);
	result = cd->dlz_findzonedb(cd->dbdata, name);
	MAYBE_UNLOCK(cd);
	return (result);
}


static isc_result_t
dlopen_dlz_lookup(const char *zone, const char *name, void *driverarg,
		  void *dbdata, dns_sdlzlookup_t *lookup)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	MAYBE_LOCK(cd);
	result = cd->dlz_lookup(zone, name, cd->dbdata, lookup);
	MAYBE_UNLOCK(cd);
	return (result);
}

/*
 * Load a symbol from the library
 */
static void *
dl_load_symbol(dlopen_data_t *cd, const char *symbol, isc_boolean_t mandatory) {
	void *ptr = dlsym(cd->dl_handle, symbol);
	if (ptr == NULL && mandatory) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen: library '%s' is missing "
			   "required symbol '%s'", cd->dl_path, symbol);
	}
	return (ptr);
}

/*
 * Called at startup for each dlopen zone in named.conf
 */
static isc_result_t
dlopen_dlz_create(const char *dlzname, unsigned int argc, char *argv[],
		  void *driverarg, void **dbdata)
{
	dlopen_data_t *cd;
	isc_mem_t *mctx = NULL;
	isc_result_t result = ISC_R_FAILURE;
	int dlopen_flags = 0;

	UNUSED(driverarg);

	if (argc < 2) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen driver for '%s' needs a path to "
			   "the shared library", dlzname);
		return (ISC_R_FAILURE);
	}

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		return (result);

	cd = isc_mem_get(mctx, sizeof(*cd));
	if (cd == NULL) {
		isc_mem_destroy(&mctx);
		return (ISC_R_NOMEMORY);
	}
	memset(cd, 0, sizeof(*cd));

	cd->mctx = mctx;

	cd->dl_path = isc_mem_strdup(cd->mctx, argv[1]);
	if (cd->dl_path == NULL) {
		goto failed;
	}

	cd->dlzname = isc_mem_strdup(cd->mctx, dlzname);
	if (cd->dlzname == NULL) {
		goto failed;
	}

	/* Initialize the lock */
	result = isc_mutex_init(&cd->lock);
	if (result != ISC_R_SUCCESS)
		goto failed;

	/* Open the library */
	dlopen_flags = RTLD_NOW|RTLD_GLOBAL;

#ifdef RTLD_DEEPBIND
	/*
	 * If RTLD_DEEPBIND is available then use it. This can avoid
	 * issues with a module using a different version of a system
	 * library than one that bind9 uses. For example, bind9 may link
	 * to MIT kerberos, but the module may use Heimdal. If we don't
	 * use RTLD_DEEPBIND then we could end up with Heimdal functions
	 * calling MIT functions, which leads to bizarre results (usually
	 * a segfault).
	 */
	dlopen_flags |= RTLD_DEEPBIND;
#endif

	cd->dl_handle = dlopen(cd->dl_path, dlopen_flags);
	if (cd->dl_handle == NULL) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen failed to open library '%s' - %s",
			   cd->dl_path, dlerror());
		goto failed;
	}

	/* Find the symbols */
	cd->dlz_version = (dlz_dlopen_version_t *)
		dl_load_symbol(cd, "dlz_version", ISC_TRUE);
	cd->dlz_create = (dlz_dlopen_create_t *)
		dl_load_symbol(cd, "dlz_create", ISC_TRUE);
	cd->dlz_lookup = (dlz_dlopen_lookup_t *)
		dl_load_symbol(cd, "dlz_lookup", ISC_TRUE);
	cd->dlz_findzonedb = (dlz_dlopen_findzonedb_t *)
		dl_load_symbol(cd, "dlz_findzonedb", ISC_TRUE);

	if (cd->dlz_create == NULL ||
	    cd->dlz_lookup == NULL ||
	    cd->dlz_findzonedb == NULL)
	{
		/* We're missing a required symbol */
		goto failed;
	}

	cd->dlz_allowzonexfr = (dlz_dlopen_allowzonexfr_t *)
		dl_load_symbol(cd, "dlz_allowzonexfr", ISC_FALSE);
	cd->dlz_allnodes = (dlz_dlopen_allnodes_t *)
		dl_load_symbol(cd, "dlz_allnodes",
			       ISC_TF(cd->dlz_allowzonexfr != NULL));
	cd->dlz_authority = (dlz_dlopen_authority_t *)
		dl_load_symbol(cd, "dlz_authority", ISC_FALSE);
	cd->dlz_newversion = (dlz_dlopen_newversion_t *)
		dl_load_symbol(cd, "dlz_newversion", ISC_FALSE);
	cd->dlz_closeversion = (dlz_dlopen_closeversion_t *)
		dl_load_symbol(cd, "dlz_closeversion",
			       ISC_TF(cd->dlz_newversion != NULL));
	cd->dlz_configure = (dlz_dlopen_configure_t *)
		dl_load_symbol(cd, "dlz_configure", ISC_FALSE);
	cd->dlz_ssumatch = (dlz_dlopen_ssumatch_t *)
		dl_load_symbol(cd, "dlz_ssumatch", ISC_FALSE);
	cd->dlz_addrdataset = (dlz_dlopen_addrdataset_t *)
		dl_load_symbol(cd, "dlz_addrdataset", ISC_FALSE);
	cd->dlz_subrdataset = (dlz_dlopen_subrdataset_t *)
		dl_load_symbol(cd, "dlz_subrdataset", ISC_FALSE);
	cd->dlz_delrdataset = (dlz_dlopen_delrdataset_t *)
		dl_load_symbol(cd, "dlz_delrdataset", ISC_FALSE);
	cd->dlz_destroy = (dlz_dlopen_destroy_t *)
		dl_load_symbol(cd, "dlz_destroy", ISC_FALSE);

	/* Check the version of the API is the same */
	cd->version = cd->dlz_version(&cd->flags);
	if (cd->version != DLZ_DLOPEN_VERSION) {
		dlopen_log(ISC_LOG_ERROR,
			   "dlz_dlopen: incorrect version %d "
			   "should be %d in '%s'",
			   cd->version, DLZ_DLOPEN_VERSION, cd->dl_path);
		goto failed;
	}

	/*
	 * Call the library's create function. Note that this is an
	 * extended version of dlz create, with the addition of
	 * named function pointers for helper functions that the
	 * driver will need. This avoids the need for the backend to
	 * link the BIND9 libraries
	 */
	MAYBE_LOCK(cd);
	result = cd->dlz_create(dlzname, argc-1, argv+1,
				&cd->dbdata,
				"log", dlopen_log,
				"putrr", dns_sdlz_putrr,
				"putnamedrr", dns_sdlz_putnamedrr,
				"writeable_zone", dns_dlz_writeablezone,
				NULL);
	MAYBE_UNLOCK(cd);
	if (result != ISC_R_SUCCESS)
		goto failed;

	*dbdata = cd;

	return (ISC_R_SUCCESS);

failed:
	dlopen_log(ISC_LOG_ERROR, "dlz_dlopen of '%s' failed", dlzname);
	if (cd->dl_path != NULL)
		isc_mem_free(mctx, cd->dl_path);
	if (cd->dlzname != NULL)
		isc_mem_free(mctx, cd->dlzname);
	if (dlopen_flags != 0)
		(void) isc_mutex_destroy(&cd->lock);
#ifdef HAVE_DLCLOSE
	if (cd->dl_handle)
		dlclose(cd->dl_handle);
#endif
	isc_mem_put(mctx, cd, sizeof(*cd));
	isc_mem_destroy(&mctx);
	return (result);
}


/*
 * Called when bind is shutting down
 */
static void
dlopen_dlz_destroy(void *driverarg, void *dbdata) {
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_mem_t *mctx;

	UNUSED(driverarg);

	if (cd->dlz_destroy) {
		MAYBE_LOCK(cd);
		cd->dlz_destroy(cd->dbdata);
		MAYBE_UNLOCK(cd);
	}

	if (cd->dl_path)
		isc_mem_free(cd->mctx, cd->dl_path);
	if (cd->dlzname)
		isc_mem_free(cd->mctx, cd->dlzname);

#ifdef HAVE_DLCLOSE
	if (cd->dl_handle)
		dlclose(cd->dl_handle);
#endif

	(void) isc_mutex_destroy(&cd->lock);

	mctx = cd->mctx;
	isc_mem_put(mctx, cd, sizeof(*cd));
	isc_mem_destroy(&mctx);
}

/*
 * Called to start a transaction
 */
static isc_result_t
dlopen_dlz_newversion(const char *zone, void *driverarg, void *dbdata,
		      void **versionp)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_newversion == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_newversion(zone, cd->dbdata, versionp);
	MAYBE_UNLOCK(cd);
	return (result);
}

/*
 * Called to end a transaction
 */
static void
dlopen_dlz_closeversion(const char *zone, isc_boolean_t commit,
			void *driverarg, void *dbdata, void **versionp)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;

	UNUSED(driverarg);

	if (cd->dlz_newversion == NULL) {
		*versionp = NULL;
		return;
	}

	MAYBE_LOCK(cd);
	cd->dlz_closeversion(zone, commit, cd->dbdata, versionp);
	MAYBE_UNLOCK(cd);
}

/*
 * Called on startup to configure any writeable zones
 */
static isc_result_t
dlopen_dlz_configure(dns_view_t *view, void *driverarg, void *dbdata) {
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_configure == NULL)
		return (ISC_R_SUCCESS);

	MAYBE_LOCK(cd);
	cd->in_configure = ISC_TRUE;
	result = cd->dlz_configure(view, cd->dbdata);
	cd->in_configure = ISC_FALSE;
	MAYBE_UNLOCK(cd);

	return (result);
}


/*
 * Check for authority to change a name
 */
static isc_boolean_t
dlopen_dlz_ssumatch(const char *signer, const char *name, const char *tcpaddr,
		    const char *type, const char *key, isc_uint32_t keydatalen,
		    unsigned char *keydata, void *driverarg, void *dbdata)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_boolean_t ret;

	UNUSED(driverarg);

	if (cd->dlz_ssumatch == NULL)
		return (ISC_FALSE);

	MAYBE_LOCK(cd);
	ret = cd->dlz_ssumatch(signer, name, tcpaddr, type, key, keydatalen,
			       keydata, cd->dbdata);
	MAYBE_UNLOCK(cd);

	return (ret);
}


/*
 * Add an rdataset
 */
static isc_result_t
dlopen_dlz_addrdataset(const char *name, const char *rdatastr,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_addrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_addrdataset(name, rdatastr, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}

/*
 * Subtract an rdataset
 */
static isc_result_t
dlopen_dlz_subrdataset(const char *name, const char *rdatastr,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_subrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_subrdataset(name, rdatastr, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}

/*
  delete a rdataset
 */
static isc_result_t
dlopen_dlz_delrdataset(const char *name, const char *type,
		       void *driverarg, void *dbdata, void *version)
{
	dlopen_data_t *cd = (dlopen_data_t *) dbdata;
	isc_result_t result;

	UNUSED(driverarg);

	if (cd->dlz_delrdataset == NULL)
		return (ISC_R_NOTIMPLEMENTED);

	MAYBE_LOCK(cd);
	result = cd->dlz_delrdataset(name, type, cd->dbdata, version);
	MAYBE_UNLOCK(cd);

	return (result);
}


static dns_sdlzmethods_t dlz_dlopen_methods = {
	dlopen_dlz_create,
	dlopen_dlz_destroy,
	dlopen_dlz_findzonedb,
	dlopen_dlz_lookup,
	dlopen_dlz_authority,
	dlopen_dlz_allnodes,
	dlopen_dlz_allowzonexfr,
	dlopen_dlz_newversion,
	dlopen_dlz_closeversion,
	dlopen_dlz_configure,
	dlopen_dlz_ssumatch,
	dlopen_dlz_addrdataset,
	dlopen_dlz_subrdataset,
	dlopen_dlz_delrdataset
};
#endif

/*
 * Register driver with BIND
 */
isc_result_t
dlz_dlopen_init(isc_mem_t *mctx) {
#ifndef ISC_DLZ_DLOPEN
	UNUSED(mctx);
	return (ISC_R_NOTIMPLEMENTED);
#else
	isc_result_t result;

	dlopen_log(2, "Registering DLZ_dlopen driver");

	result = dns_sdlzregister("dlopen", &dlz_dlopen_methods, NULL,
				  DNS_SDLZFLAG_RELATIVEOWNER |
				  DNS_SDLZFLAG_THREADSAFE,
				  mctx, &dlz_dlopen);

	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_sdlzregister() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
	}

	return (result);
#endif
}


/*
 * Unregister the driver
 */
void
dlz_dlopen_clear(void) {
#ifdef ISC_DLZ_DLOPEN
	dlopen_log(2, "Unregistering DLZ_dlopen driver");
	if (dlz_dlopen != NULL)
		dns_sdlzunregister(&dlz_dlopen);
#endif
}
