/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: context.c,v 1.3 2009/09/02 23:48:02 tbox Exp $ */

#include <config.h>

#include <isc/app.h>
#include <isc/lib.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/thread.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/lib.h>

#include <irs/context.h>
#include <irs/dnsconf.h>
#include <irs/resconf.h>

#define IRS_CONTEXT_MAGIC		ISC_MAGIC('I', 'R', 'S', 'c')
#define IRS_CONTEXT_VALID(c)		ISC_MAGIC_VALID(c, IRS_CONTEXT_MAGIC)

#ifndef RESOLV_CONF
/*% location of resolve.conf */
#define RESOLV_CONF "/etc/resolv.conf"
#endif

#ifndef DNS_CONF
/*% location of dns.conf */
#define DNS_CONF "/etc/dns.conf"
#endif

#ifndef ISC_PLATFORM_USETHREADS
irs_context_t *irs_g_context = NULL;
#else
static isc_boolean_t thread_key_initialized = ISC_FALSE;
static isc_mutex_t thread_key_mutex;
static isc_thread_key_t irs_context_key;
static isc_once_t once = ISC_ONCE_INIT;
#endif


struct irs_context {
	/*
	 * An IRS context is a thread-specific object, and does not need to
	 * be locked.
	 */
	unsigned int			magic;
	isc_mem_t			*mctx;
	isc_appctx_t			*actx;
	isc_taskmgr_t			*taskmgr;
	isc_task_t			*task;
	isc_socketmgr_t			*socketmgr;
	isc_timermgr_t			*timermgr;
	dns_client_t			*dnsclient;
	irs_resconf_t			*resconf;
	irs_dnsconf_t			*dnsconf;
};

static void
ctxs_destroy(isc_mem_t **mctxp, isc_appctx_t **actxp,
	     isc_taskmgr_t **taskmgrp, isc_socketmgr_t **socketmgrp,
	     isc_timermgr_t **timermgrp)
{
	if (taskmgrp != NULL)
		isc_taskmgr_destroy(taskmgrp);

	if (timermgrp != NULL)
		isc_timermgr_destroy(timermgrp);

	if (socketmgrp != NULL)
		isc_socketmgr_destroy(socketmgrp);

	if (actxp != NULL)
		isc_appctx_destroy(actxp);

	if (mctxp != NULL)
		isc_mem_destroy(mctxp);
}

static isc_result_t
ctxs_init(isc_mem_t **mctxp, isc_appctx_t **actxp,
	  isc_taskmgr_t **taskmgrp, isc_socketmgr_t **socketmgrp,
	  isc_timermgr_t **timermgrp)
{
	isc_result_t result;

	result = isc_mem_create(0, 0, mctxp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_appctx_create(*mctxp, actxp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_taskmgr_createinctx(*mctxp, *actxp, 1, 0, taskmgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_socketmgr_createinctx(*mctxp, *actxp, socketmgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	result = isc_timermgr_createinctx(*mctxp, *actxp, timermgrp);
	if (result != ISC_R_SUCCESS)
		goto fail;

	return (ISC_R_SUCCESS);

 fail:
	ctxs_destroy(mctxp, actxp, taskmgrp, socketmgrp, timermgrp);

	return (result);
}

#ifdef ISC_PLATFORM_USETHREADS
static void
free_specific_context(void *arg) {
	irs_context_t *context = arg;

	irs_context_destroy(&context);

	isc_thread_key_setspecific(irs_context_key, NULL);
}

static void
thread_key_mutex_init(void) {
	RUNTIME_CHECK(isc_mutex_init(&thread_key_mutex) == ISC_R_SUCCESS);
}

static isc_result_t
thread_key_init() {
	isc_result_t result;

	result = isc_once_do(&once, thread_key_mutex_init);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (!thread_key_initialized) {
		LOCK(&thread_key_mutex);

		if (!thread_key_initialized &&
		    isc_thread_key_create(&irs_context_key,
					  free_specific_context) != 0) {
			result = ISC_R_FAILURE;
		} else
			thread_key_initialized = ISC_TRUE;

		UNLOCK(&thread_key_mutex);
	}

	return (result);
}
#endif /* ISC_PLATFORM_USETHREADS */

isc_result_t
irs_context_get(irs_context_t **contextp) {
	irs_context_t *context;
	isc_result_t result;

	REQUIRE(contextp != NULL && *contextp == NULL);

#ifndef ISC_PLATFORM_USETHREADS
	if (irs_g_context == NULL) {
		result = irs_context_create(&irs_g_context);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	context = irs_g_context;
#else
	result = thread_key_init();
	if (result != ISC_R_SUCCESS)
		return (result);

	context = isc_thread_key_getspecific(irs_context_key);
	if (context == NULL) {
		result = irs_context_create(&context);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = isc_thread_key_setspecific(irs_context_key, context);
		if (result != ISC_R_SUCCESS) {
			irs_context_destroy(&context);
			return (result);
		}
	}
#endif /* ISC_PLATFORM_USETHREADS */

	*contextp = context;

	return (ISC_R_SUCCESS);
}

isc_result_t
irs_context_create(irs_context_t **contextp) {
	isc_result_t result;
	irs_context_t *context;
	isc_appctx_t *actx = NULL;
	isc_mem_t *mctx = NULL;
	isc_taskmgr_t *taskmgr = NULL;
	isc_socketmgr_t *socketmgr = NULL;
	isc_timermgr_t *timermgr = NULL;
	dns_client_t *client = NULL;
	isc_sockaddrlist_t *nameservers;
	irs_dnsconf_dnskeylist_t *trustedkeys;
	irs_dnsconf_dnskey_t *trustedkey;

	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS)
		return (result);

	result = ctxs_init(&mctx, &actx, &taskmgr, &socketmgr, &timermgr);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = isc_app_ctxstart(actx);
	if (result != ISC_R_SUCCESS) {
		ctxs_destroy(&mctx, &actx, &taskmgr, &socketmgr, &timermgr);
		return (result);
	}

	context = isc_mem_get(mctx, sizeof(*context));
	if (context == NULL) {
		ctxs_destroy(&mctx, &actx, &taskmgr, &socketmgr, &timermgr);
		return (ISC_R_NOMEMORY);
	}

	context->mctx = mctx;
	context->actx = actx;
	context->taskmgr = taskmgr;
	context->socketmgr = socketmgr;
	context->timermgr = timermgr;
	context->resconf = NULL;
	context->dnsconf = NULL;
	context->task = NULL;
	result = isc_task_create(taskmgr, 0, &context->task);
	if (result != ISC_R_SUCCESS)
		goto fail;

	/* Create a DNS client object */
	result = dns_client_createx(mctx, actx, taskmgr, socketmgr, timermgr,
				    0, &client);
	if (result != ISC_R_SUCCESS)
		goto fail;
	context->dnsclient = client;

	/* Read resolver configuration file */
	result = irs_resconf_load(mctx, RESOLV_CONF, &context->resconf);
	if (result != ISC_R_SUCCESS)
		goto fail;
	/* Set nameservers */
	nameservers = irs_resconf_getnameservers(context->resconf);
	result = dns_client_setservers(client, dns_rdataclass_in, NULL,
				       nameservers);
	if (result != ISC_R_SUCCESS)
		goto fail;

	/* Read advanced DNS configuration (if any) */
	result = irs_dnsconf_load(mctx, DNS_CONF, &context->dnsconf);
	if (result != ISC_R_SUCCESS)
		goto fail;
	trustedkeys = irs_dnsconf_gettrustedkeys(context->dnsconf);
	for (trustedkey = ISC_LIST_HEAD(*trustedkeys);
	     trustedkey != NULL;
	     trustedkey = ISC_LIST_NEXT(trustedkey, link)) {
		result = dns_client_addtrustedkey(client, dns_rdataclass_in,
						  trustedkey->keyname,
						  trustedkey->keydatabuf);
		if (result != ISC_R_SUCCESS)
			goto fail;
	}

	context->magic = IRS_CONTEXT_MAGIC;
	*contextp = context;

	return (ISC_R_SUCCESS);

  fail:
	if (context->task != NULL)
		isc_task_detach(&context->task);
	if (context->resconf != NULL)
		irs_resconf_destroy(&context->resconf);
	if (context->dnsconf != NULL)
		irs_dnsconf_destroy(&context->dnsconf);
	if (client != NULL)
		dns_client_destroy(&client);
	ctxs_destroy(NULL, &actx, &taskmgr, &socketmgr, &timermgr);
	isc_mem_putanddetach(&mctx, context, sizeof(*context));

	return (result);
}

void
irs_context_destroy(irs_context_t **contextp) {
	irs_context_t *context;

	REQUIRE(contextp != NULL);
	context = *contextp;
	REQUIRE(IRS_CONTEXT_VALID(context));

	isc_task_detach(&context->task);
	irs_dnsconf_destroy(&context->dnsconf);
	irs_resconf_destroy(&context->resconf);
	dns_client_destroy(&context->dnsclient);

	ctxs_destroy(NULL, &context->actx, &context->taskmgr,
		     &context->socketmgr, &context->timermgr);

	context->magic = 0;

	isc_mem_putanddetach(&context->mctx, context, sizeof(*context));

	*contextp = NULL;

#ifndef ISC_PLATFORM_USETHREADS
	irs_g_context = NULL;
#else
	(void)isc_thread_key_setspecific(irs_context_key, NULL);
#endif
}

isc_mem_t *
irs_context_getmctx(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->mctx);
}

isc_appctx_t *
irs_context_getappctx(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->actx);
}

isc_taskmgr_t *
irs_context_gettaskmgr(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->taskmgr);
}

isc_timermgr_t *
irs_context_gettimermgr(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->timermgr);
}

isc_task_t *
irs_context_gettask(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->task);
}

dns_client_t *
irs_context_getdnsclient(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->dnsclient);
}

irs_resconf_t *
irs_context_getresconf(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->resconf);
}

irs_dnsconf_t *
irs_context_getdnsconf(irs_context_t *context) {
	REQUIRE(IRS_CONTEXT_VALID(context));

	return (context->dnsconf);
}
