/*
 * Copyright (C) 2004-2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* $Id: log.c,v 1.46.334.3 2009-01-07 01:50:14 jinmei Exp $ */

/*! \file */

#include <config.h>

#include <isc/result.h>

#include <isccfg/log.h>

#include <named/log.h>

#ifndef ISC_FACILITY
#define ISC_FACILITY LOG_DAEMON
#endif

/*%
 * When adding a new category, be sure to add the appropriate
 * \#define to <named/log.h> and to update the list in
 * bin/check/check-tool.c.
 */
static isc_logcategory_t categories[] = {
	{ "",		 		0 },
	{ "client",	 		0 },
	{ "network",	 		0 },
	{ "update",	 		0 },
	{ "queries",	 		0 },
	{ "unmatched",	 		0 },
	{ "update-security",		0 },
	{ "query-errors",		0 },
	{ NULL, 			0 }
};

/*%
 * When adding a new module, be sure to add the appropriate
 * \#define to <dns/log.h>.
 */
static isc_logmodule_t modules[] = {
	{ "main",	 		0 },
	{ "client",	 		0 },
	{ "server",		 	0 },
	{ "query",		 	0 },
	{ "interfacemgr",	 	0 },
	{ "update",	 		0 },
	{ "xfer-in",	 		0 },
	{ "xfer-out",	 		0 },
	{ "notify",	 		0 },
	{ "control",	 		0 },
	{ "lwresd",	 		0 },
	{ NULL, 			0 }
};

isc_result_t
ns_log_init(isc_boolean_t safe) {
	isc_result_t result;
	isc_logconfig_t *lcfg = NULL;

	ns_g_categories = categories;
	ns_g_modules = modules;

	/*
	 * Setup a logging context.
	 */
	result = isc_log_create(ns_g_mctx, &ns_g_lctx, &lcfg);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * named-checktool.c:setup_logging() needs to be kept in sync.
	 */
	isc_log_registercategories(ns_g_lctx, ns_g_categories);
	isc_log_registermodules(ns_g_lctx, ns_g_modules);
	isc_log_setcontext(ns_g_lctx);
	dns_log_init(ns_g_lctx);
	dns_log_setcontext(ns_g_lctx);
	cfg_log_init(ns_g_lctx);

	if (safe)
		result = ns_log_setsafechannels(lcfg);
	else
		result = ns_log_setdefaultchannels(lcfg);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = ns_log_setdefaultcategory(lcfg);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	return (ISC_R_SUCCESS);

 cleanup:
	isc_log_destroy(&ns_g_lctx);
	isc_log_setcontext(NULL);
	dns_log_setcontext(NULL);

	return (result);
}

isc_result_t
ns_log_setdefaultchannels(isc_logconfig_t *lcfg) {
	isc_result_t result;
	isc_logdestination_t destination;

	/*
	 * By default, the logging library makes "default_debug" log to
	 * stderr.  In BIND, we want to override this and log to named.run
	 * instead, unless the -g option was given.
	 */
	if (! ns_g_logstderr) {
		destination.file.stream = NULL;
		destination.file.name = "named.run";
		destination.file.versions = ISC_LOG_ROLLNEVER;
		destination.file.maximum_size = 0;
		result = isc_log_createchannel(lcfg, "default_debug",
					       ISC_LOG_TOFILE,
					       ISC_LOG_DYNAMIC,
					       &destination,
					       ISC_LOG_PRINTTIME|
					       ISC_LOG_DEBUGONLY);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

#if ISC_FACILITY != LOG_DAEMON
	destination.facility = ISC_FACILITY;
	result = isc_log_createchannel(lcfg, "default_syslog",
				       ISC_LOG_TOSYSLOG, ISC_LOG_INFO,
				       &destination, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
#endif

	/*
	 * Set the initial debug level.
	 */
	isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);

	result = ISC_R_SUCCESS;

 cleanup:
	return (result);
}

isc_result_t
ns_log_setsafechannels(isc_logconfig_t *lcfg) {
	isc_result_t result;
#if ISC_FACILITY != LOG_DAEMON
	isc_logdestination_t destination;
#endif

	if (! ns_g_logstderr) {
		result = isc_log_createchannel(lcfg, "default_debug",
					       ISC_LOG_TONULL,
					       ISC_LOG_DYNAMIC,
					       NULL, 0);
		if (result != ISC_R_SUCCESS)
			goto cleanup;

		/*
		 * Setting the debug level to zero should get the output
		 * discarded a bit faster.
		 */
		isc_log_setdebuglevel(ns_g_lctx, 0);
	} else {
		isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);
	}

#if ISC_FACILITY != LOG_DAEMON
	destination.facility = ISC_FACILITY;
	result = isc_log_createchannel(lcfg, "default_syslog",
				       ISC_LOG_TOSYSLOG, ISC_LOG_INFO,
				       &destination, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
#endif

	result = ISC_R_SUCCESS;

 cleanup:
	return (result);
}

isc_result_t
ns_log_setdefaultcategory(isc_logconfig_t *lcfg) {
	isc_result_t result;

	if (! ns_g_logstderr) {
		result = isc_log_usechannel(lcfg, "default_syslog",
					    ISC_LOGCATEGORY_DEFAULT, NULL);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	result = isc_log_usechannel(lcfg, "default_debug",
				    ISC_LOGCATEGORY_DEFAULT, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = ISC_R_SUCCESS;

 cleanup:
	return (result);
}

isc_result_t
ns_log_setunmatchedcategory(isc_logconfig_t *lcfg) {
	isc_result_t result;

	result = isc_log_usechannel(lcfg, "null",
				    NS_LOGCATEGORY_UNMATCHED, NULL);
	return (result);
}

void
ns_log_shutdown(void) {
	isc_log_destroy(&ns_g_lctx);
	isc_log_setcontext(NULL);
	dns_log_setcontext(NULL);
}
