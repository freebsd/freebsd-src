/*
 * Copyright (C) 2004-2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: view.c,v 1.178.8.1 2011-03-11 06:47:06 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/file.h>
#include <isc/hash.h>
#include <isc/print.h>
#include <isc/sha2.h>
#include <isc/stats.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/task.h>
#include <isc/util.h>

#include <dns/acache.h>
#include <dns/acl.h>
#include <dns/adb.h>
#include <dns/cache.h>
#include <dns/db.h>
#include <dns/dlz.h>
#ifdef BIND9
#include <dns/dns64.h>
#endif
#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/forward.h>
#include <dns/keytable.h>
#include <dns/keyvalues.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/order.h>
#include <dns/peer.h>
#include <dns/rbt.h>
#include <dns/rdataset.h>
#include <dns/request.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/rpz.h>
#include <dns/stats.h>
#include <dns/tsig.h>
#include <dns/zone.h>
#include <dns/zt.h>

#define RESSHUTDOWN(v)	(((v)->attributes & DNS_VIEWATTR_RESSHUTDOWN) != 0)
#define ADBSHUTDOWN(v)	(((v)->attributes & DNS_VIEWATTR_ADBSHUTDOWN) != 0)
#define REQSHUTDOWN(v)	(((v)->attributes & DNS_VIEWATTR_REQSHUTDOWN) != 0)

#define DNS_VIEW_DELONLYHASH 111

static void resolver_shutdown(isc_task_t *task, isc_event_t *event);
static void adb_shutdown(isc_task_t *task, isc_event_t *event);
static void req_shutdown(isc_task_t *task, isc_event_t *event);

isc_result_t
dns_view_create(isc_mem_t *mctx, dns_rdataclass_t rdclass,
		const char *name, dns_view_t **viewp)
{
	dns_view_t *view;
	isc_result_t result;

	/*
	 * Create a view.
	 */

	REQUIRE(name != NULL);
	REQUIRE(viewp != NULL && *viewp == NULL);

	view = isc_mem_get(mctx, sizeof(*view));
	if (view == NULL)
		return (ISC_R_NOMEMORY);
	view->name = isc_mem_strdup(mctx, name);
	if (view->name == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup_view;
	}
	result = isc_mutex_init(&view->lock);
	if (result != ISC_R_SUCCESS)
		goto cleanup_name;

#ifdef BIND9
	view->zonetable = NULL;
	result = dns_zt_create(mctx, rdclass, &view->zonetable);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_zt_create() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
		goto cleanup_mutex;
	}
#endif
	view->secroots_priv = NULL;
	view->fwdtable = NULL;
	result = dns_fwdtable_create(mctx, &view->fwdtable);
	if (result != ISC_R_SUCCESS) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "dns_fwdtable_create() failed: %s",
				 isc_result_totext(result));
		result = ISC_R_UNEXPECTED;
		goto cleanup_zt;
	}

	view->acache = NULL;
	view->cache = NULL;
	view->cachedb = NULL;
	view->dlzdatabase = NULL;
	view->hints = NULL;
	view->resolver = NULL;
	view->adb = NULL;
	view->requestmgr = NULL;
	view->mctx = mctx;
	view->rdclass = rdclass;
	view->frozen = ISC_FALSE;
	view->task = NULL;
	result = isc_refcount_init(&view->references, 1);
	if (result != ISC_R_SUCCESS)
		goto cleanup_fwdtable;
	view->weakrefs = 0;
	view->attributes = (DNS_VIEWATTR_RESSHUTDOWN|DNS_VIEWATTR_ADBSHUTDOWN|
			    DNS_VIEWATTR_REQSHUTDOWN);
	view->statickeys = NULL;
	view->dynamickeys = NULL;
	view->matchclients = NULL;
	view->matchdestinations = NULL;
	view->matchrecursiveonly = ISC_FALSE;
	result = dns_tsigkeyring_create(view->mctx, &view->dynamickeys);
	if (result != ISC_R_SUCCESS)
		goto cleanup_references;
	view->peers = NULL;
	view->order = NULL;
	view->delonly = NULL;
	view->rootdelonly = ISC_FALSE;
	view->rootexclude = NULL;
	view->resstats = NULL;
	view->resquerystats = NULL;
	view->cacheshared = ISC_FALSE;
	ISC_LIST_INIT(view->dns64);
	view->dns64cnt = 0;

	/*
	 * Initialize configuration data with default values.
	 */
	view->recursion = ISC_TRUE;
	view->auth_nxdomain = ISC_FALSE; /* Was true in BIND 8 */
	view->additionalfromcache = ISC_TRUE;
	view->additionalfromauth = ISC_TRUE;
	view->enablednssec = ISC_TRUE;
	view->enablevalidation = ISC_TRUE;
	view->acceptexpired = ISC_FALSE;
	view->minimalresponses = ISC_FALSE;
	view->transfer_format = dns_one_answer;
	view->cacheacl = NULL;
	view->cacheonacl = NULL;
	view->queryacl = NULL;
	view->queryonacl = NULL;
	view->recursionacl = NULL;
	view->recursiononacl = NULL;
	view->sortlist = NULL;
	view->transferacl = NULL;
	view->notifyacl = NULL;
	view->updateacl = NULL;
	view->upfwdacl = NULL;
	view->denyansweracl = NULL;
	view->answeracl_exclude = NULL;
	view->denyanswernames = NULL;
	view->answernames_exclude = NULL;
	view->requestixfr = ISC_TRUE;
	view->provideixfr = ISC_TRUE;
	view->maxcachettl = 7 * 24 * 3600;
	view->maxncachettl = 3 * 3600;
	view->dstport = 53;
	view->preferred_glue = 0;
	view->flush = ISC_FALSE;
	view->dlv = NULL;
	view->maxudp = 0;
	view->v4_aaaa = dns_v4_aaaa_ok;
	view->v4_aaaa_acl = NULL;
	ISC_LIST_INIT(view->rpz_zones);
	dns_fixedname_init(&view->dlv_fixed);
	view->managed_keys = NULL;
#ifdef BIND9
	view->new_zone_file = NULL;
	view->new_zone_config = NULL;
	view->cfg_destroy = NULL;

	result = dns_order_create(view->mctx, &view->order);
	if (result != ISC_R_SUCCESS)
		goto cleanup_dynkeys;
#endif

	result = dns_peerlist_new(view->mctx, &view->peers);
	if (result != ISC_R_SUCCESS)
		goto cleanup_order;

	result = dns_aclenv_init(view->mctx, &view->aclenv);
	if (result != ISC_R_SUCCESS)
		goto cleanup_peerlist;

	ISC_LINK_INIT(view, link);
	ISC_EVENT_INIT(&view->resevent, sizeof(view->resevent), 0, NULL,
		       DNS_EVENT_VIEWRESSHUTDOWN, resolver_shutdown,
		       view, NULL, NULL, NULL);
	ISC_EVENT_INIT(&view->adbevent, sizeof(view->adbevent), 0, NULL,
		       DNS_EVENT_VIEWADBSHUTDOWN, adb_shutdown,
		       view, NULL, NULL, NULL);
	ISC_EVENT_INIT(&view->reqevent, sizeof(view->reqevent), 0, NULL,
		       DNS_EVENT_VIEWREQSHUTDOWN, req_shutdown,
		       view, NULL, NULL, NULL);
	view->magic = DNS_VIEW_MAGIC;

	*viewp = view;

	return (ISC_R_SUCCESS);

 cleanup_peerlist:
	dns_peerlist_detach(&view->peers);

 cleanup_order:
#ifdef BIND9
	dns_order_detach(&view->order);

 cleanup_dynkeys:
#endif
	dns_tsigkeyring_detach(&view->dynamickeys);

 cleanup_references:
	isc_refcount_destroy(&view->references);

 cleanup_fwdtable:
	dns_fwdtable_destroy(&view->fwdtable);

 cleanup_zt:
#ifdef BIND9
	dns_zt_detach(&view->zonetable);

 cleanup_mutex:
#endif
	DESTROYLOCK(&view->lock);

 cleanup_name:
	isc_mem_free(mctx, view->name);

 cleanup_view:
	isc_mem_put(mctx, view, sizeof(*view));

	return (result);
}

static inline void
destroy(dns_view_t *view) {
#ifdef BIND9
	dns_dns64_t *dns64;
#endif

	REQUIRE(!ISC_LINK_LINKED(view, link));
	REQUIRE(isc_refcount_current(&view->references) == 0);
	REQUIRE(view->weakrefs == 0);
	REQUIRE(RESSHUTDOWN(view));
	REQUIRE(ADBSHUTDOWN(view));
	REQUIRE(REQSHUTDOWN(view));

#ifdef BIND9
	if (view->order != NULL)
		dns_order_detach(&view->order);
#endif
	if (view->peers != NULL)
		dns_peerlist_detach(&view->peers);

	if (view->dynamickeys != NULL) {
		isc_result_t result;
		char template[20];
		char keyfile[20];
		FILE *fp = NULL;
		int n;

		n = snprintf(keyfile, sizeof(keyfile), "%s.tsigkeys",
			     view->name);
		if (n > 0 && (size_t)n < sizeof(keyfile)) {
			result = isc_file_mktemplate(keyfile, template,
						     sizeof(template));
			if (result == ISC_R_SUCCESS)
				(void)isc_file_openuniqueprivate(template, &fp);
		}
		if (fp == NULL)
			dns_tsigkeyring_detach(&view->dynamickeys);
		else {
			result = dns_tsigkeyring_dumpanddetach(
							&view->dynamickeys, fp);
			if (result == ISC_R_SUCCESS) {
				if (fclose(fp) == 0)
					result = isc_file_rename(template,
								 keyfile);
				if (result != ISC_R_SUCCESS)
					(void)remove(template);
			} else {
				(void)fclose(fp);
				(void)remove(template);
			}
		}
	}
	if (view->statickeys != NULL)
		dns_tsigkeyring_detach(&view->statickeys);
	if (view->adb != NULL)
		dns_adb_detach(&view->adb);
	if (view->resolver != NULL)
		dns_resolver_detach(&view->resolver);
#ifdef BIND9
	if (view->acache != NULL) {
		if (view->cachedb != NULL)
			dns_acache_putdb(view->acache, view->cachedb);
		dns_acache_detach(&view->acache);
	}
	dns_rpz_view_destroy(view);
#else
	INSIST(view->acache == NULL);
	INSIST(ISC_LIST_EMPTY(view->rpz_zones));
#endif
	if (view->requestmgr != NULL)
		dns_requestmgr_detach(&view->requestmgr);
	if (view->task != NULL)
		isc_task_detach(&view->task);
	if (view->hints != NULL)
		dns_db_detach(&view->hints);
	if (view->dlzdatabase != NULL)
		dns_dlzdestroy(&view->dlzdatabase);
	if (view->cachedb != NULL)
		dns_db_detach(&view->cachedb);
	if (view->cache != NULL)
		dns_cache_detach(&view->cache);
	if (view->matchclients != NULL)
		dns_acl_detach(&view->matchclients);
	if (view->matchdestinations != NULL)
		dns_acl_detach(&view->matchdestinations);
	if (view->cacheacl != NULL)
		dns_acl_detach(&view->cacheacl);
	if (view->cacheonacl != NULL)
		dns_acl_detach(&view->cacheonacl);
	if (view->queryacl != NULL)
		dns_acl_detach(&view->queryacl);
	if (view->queryonacl != NULL)
		dns_acl_detach(&view->queryonacl);
	if (view->recursionacl != NULL)
		dns_acl_detach(&view->recursionacl);
	if (view->recursiononacl != NULL)
		dns_acl_detach(&view->recursiononacl);
	if (view->sortlist != NULL)
		dns_acl_detach(&view->sortlist);
	if (view->transferacl != NULL)
		dns_acl_detach(&view->transferacl);
	if (view->notifyacl != NULL)
		dns_acl_detach(&view->notifyacl);
	if (view->updateacl != NULL)
		dns_acl_detach(&view->updateacl);
	if (view->upfwdacl != NULL)
		dns_acl_detach(&view->upfwdacl);
	if (view->denyansweracl != NULL)
		dns_acl_detach(&view->denyansweracl);
	if (view->v4_aaaa_acl != NULL)
		dns_acl_detach(&view->v4_aaaa_acl);
	if (view->answeracl_exclude != NULL)
		dns_rbt_destroy(&view->answeracl_exclude);
	if (view->denyanswernames != NULL)
		dns_rbt_destroy(&view->denyanswernames);
	if (view->answernames_exclude != NULL)
		dns_rbt_destroy(&view->answernames_exclude);
	if (view->delonly != NULL) {
		dns_name_t *name;
		int i;

		for (i = 0; i < DNS_VIEW_DELONLYHASH; i++) {
			name = ISC_LIST_HEAD(view->delonly[i]);
			while (name != NULL) {
				ISC_LIST_UNLINK(view->delonly[i], name, link);
				dns_name_free(name, view->mctx);
				isc_mem_put(view->mctx, name, sizeof(*name));
				name = ISC_LIST_HEAD(view->delonly[i]);
			}
		}
		isc_mem_put(view->mctx, view->delonly, sizeof(dns_namelist_t) *
			    DNS_VIEW_DELONLYHASH);
		view->delonly = NULL;
	}
	if (view->rootexclude != NULL) {
		dns_name_t *name;
		int i;

		for (i = 0; i < DNS_VIEW_DELONLYHASH; i++) {
			name = ISC_LIST_HEAD(view->rootexclude[i]);
			while (name != NULL) {
				ISC_LIST_UNLINK(view->rootexclude[i],
						name, link);
				dns_name_free(name, view->mctx);
				isc_mem_put(view->mctx, name, sizeof(*name));
				name = ISC_LIST_HEAD(view->rootexclude[i]);
			}
		}
		isc_mem_put(view->mctx, view->rootexclude,
			    sizeof(dns_namelist_t) * DNS_VIEW_DELONLYHASH);
		view->rootexclude = NULL;
	}
	if (view->resstats != NULL)
		isc_stats_detach(&view->resstats);
	if (view->resquerystats != NULL)
		dns_stats_detach(&view->resquerystats);
	if (view->secroots_priv != NULL)
		dns_keytable_detach(&view->secroots_priv);
#ifdef BIND9
	for (dns64 = ISC_LIST_HEAD(view->dns64);
	     dns64 != NULL;
	     dns64 = ISC_LIST_HEAD(view->dns64)) {
		dns_dns64_unlink(&view->dns64, dns64);
		dns_dns64_destroy(&dns64);
	}
	if (view->managed_keys != NULL)
		dns_zone_detach(&view->managed_keys);
	dns_view_setnewzones(view, ISC_FALSE, NULL, NULL);
#endif
	dns_fwdtable_destroy(&view->fwdtable);
	dns_aclenv_destroy(&view->aclenv);
	DESTROYLOCK(&view->lock);
	isc_refcount_destroy(&view->references);
	isc_mem_free(view->mctx, view->name);
	isc_mem_put(view->mctx, view, sizeof(*view));
}

/*
 * Return true iff 'view' may be freed.
 * The caller must be holding the view lock.
 */
static isc_boolean_t
all_done(dns_view_t *view) {

	if (isc_refcount_current(&view->references) == 0 &&
	    view->weakrefs == 0 &&
	    RESSHUTDOWN(view) && ADBSHUTDOWN(view) && REQSHUTDOWN(view))
		return (ISC_TRUE);

	return (ISC_FALSE);
}

void
dns_view_attach(dns_view_t *source, dns_view_t **targetp) {

	REQUIRE(DNS_VIEW_VALID(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	isc_refcount_increment(&source->references, NULL);

	*targetp = source;
}

static void
view_flushanddetach(dns_view_t **viewp, isc_boolean_t flush) {
	dns_view_t *view;
	unsigned int refs;
	isc_boolean_t done = ISC_FALSE;

	REQUIRE(viewp != NULL);
	view = *viewp;
	REQUIRE(DNS_VIEW_VALID(view));

	if (flush)
		view->flush = ISC_TRUE;
	isc_refcount_decrement(&view->references, &refs);
	if (refs == 0) {
		LOCK(&view->lock);
		if (!RESSHUTDOWN(view))
			dns_resolver_shutdown(view->resolver);
		if (!ADBSHUTDOWN(view))
			dns_adb_shutdown(view->adb);
		if (!REQSHUTDOWN(view))
			dns_requestmgr_shutdown(view->requestmgr);
#ifdef BIND9
		if (view->acache != NULL)
			dns_acache_shutdown(view->acache);
		if (view->flush)
			dns_zt_flushanddetach(&view->zonetable);
		else
			dns_zt_detach(&view->zonetable);
		if (view->managed_keys != NULL) {
			if (view->flush)
				dns_zone_flush(view->managed_keys);
			dns_zone_detach(&view->managed_keys);
		}
#endif
		done = all_done(view);
		UNLOCK(&view->lock);
	}

	*viewp = NULL;

	if (done)
		destroy(view);
}

void
dns_view_flushanddetach(dns_view_t **viewp) {
	view_flushanddetach(viewp, ISC_TRUE);
}

void
dns_view_detach(dns_view_t **viewp) {
	view_flushanddetach(viewp, ISC_FALSE);
}

#ifdef BIND9
static isc_result_t
dialup(dns_zone_t *zone, void *dummy) {
	UNUSED(dummy);
	dns_zone_dialup(zone);
	return (ISC_R_SUCCESS);
}

void
dns_view_dialup(dns_view_t *view) {
	REQUIRE(DNS_VIEW_VALID(view));
	(void)dns_zt_apply(view->zonetable, ISC_FALSE, dialup, NULL);
}
#endif

void
dns_view_weakattach(dns_view_t *source, dns_view_t **targetp) {

	REQUIRE(DNS_VIEW_VALID(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	LOCK(&source->lock);
	source->weakrefs++;
	UNLOCK(&source->lock);

	*targetp = source;
}

void
dns_view_weakdetach(dns_view_t **viewp) {
	dns_view_t *view;
	isc_boolean_t done = ISC_FALSE;

	REQUIRE(viewp != NULL);
	view = *viewp;
	REQUIRE(DNS_VIEW_VALID(view));

	LOCK(&view->lock);

	INSIST(view->weakrefs > 0);
	view->weakrefs--;
	done = all_done(view);

	UNLOCK(&view->lock);

	*viewp = NULL;

	if (done)
		destroy(view);
}

static void
resolver_shutdown(isc_task_t *task, isc_event_t *event) {
	dns_view_t *view = event->ev_arg;
	isc_boolean_t done;

	REQUIRE(event->ev_type == DNS_EVENT_VIEWRESSHUTDOWN);
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->task == task);

	UNUSED(task);

	LOCK(&view->lock);

	view->attributes |= DNS_VIEWATTR_RESSHUTDOWN;
	done = all_done(view);

	UNLOCK(&view->lock);

	isc_event_free(&event);

	if (done)
		destroy(view);
}

static void
adb_shutdown(isc_task_t *task, isc_event_t *event) {
	dns_view_t *view = event->ev_arg;
	isc_boolean_t done;

	REQUIRE(event->ev_type == DNS_EVENT_VIEWADBSHUTDOWN);
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->task == task);

	UNUSED(task);

	LOCK(&view->lock);

	view->attributes |= DNS_VIEWATTR_ADBSHUTDOWN;
	done = all_done(view);

	UNLOCK(&view->lock);

	isc_event_free(&event);

	if (done)
		destroy(view);
}

static void
req_shutdown(isc_task_t *task, isc_event_t *event) {
	dns_view_t *view = event->ev_arg;
	isc_boolean_t done;

	REQUIRE(event->ev_type == DNS_EVENT_VIEWREQSHUTDOWN);
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->task == task);

	UNUSED(task);

	LOCK(&view->lock);

	view->attributes |= DNS_VIEWATTR_REQSHUTDOWN;
	done = all_done(view);

	UNLOCK(&view->lock);

	isc_event_free(&event);

	if (done)
		destroy(view);
}

isc_result_t
dns_view_createresolver(dns_view_t *view,
			isc_taskmgr_t *taskmgr, unsigned int ntasks,
			isc_socketmgr_t *socketmgr,
			isc_timermgr_t *timermgr,
			unsigned int options,
			dns_dispatchmgr_t *dispatchmgr,
			dns_dispatch_t *dispatchv4,
			dns_dispatch_t *dispatchv6)
{
	isc_result_t result;
	isc_event_t *event;
	isc_mem_t *mctx = NULL;

	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);
	REQUIRE(view->resolver == NULL);

	result = isc_task_create(taskmgr, 0, &view->task);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_task_setname(view->task, "view", view);

	result = dns_resolver_create(view, taskmgr, ntasks, socketmgr,
				     timermgr, options, dispatchmgr,
				     dispatchv4, dispatchv6,
				     &view->resolver);
	if (result != ISC_R_SUCCESS) {
		isc_task_detach(&view->task);
		return (result);
	}
	event = &view->resevent;
	dns_resolver_whenshutdown(view->resolver, view->task, &event);
	view->attributes &= ~DNS_VIEWATTR_RESSHUTDOWN;

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS) {
		dns_resolver_shutdown(view->resolver);
		return (result);
	}

	result = dns_adb_create(mctx, view, timermgr, taskmgr, &view->adb);
	isc_mem_setname(mctx, "ADB", NULL);
	isc_mem_detach(&mctx);
	if (result != ISC_R_SUCCESS) {
		dns_resolver_shutdown(view->resolver);
		return (result);
	}
	event = &view->adbevent;
	dns_adb_whenshutdown(view->adb, view->task, &event);
	view->attributes &= ~DNS_VIEWATTR_ADBSHUTDOWN;

	result = dns_requestmgr_create(view->mctx, timermgr, socketmgr,
				      dns_resolver_taskmgr(view->resolver),
				      dns_resolver_dispatchmgr(view->resolver),
				      dns_resolver_dispatchv4(view->resolver),
				      dns_resolver_dispatchv6(view->resolver),
				      &view->requestmgr);
	if (result != ISC_R_SUCCESS) {
		dns_adb_shutdown(view->adb);
		dns_resolver_shutdown(view->resolver);
		return (result);
	}
	event = &view->reqevent;
	dns_requestmgr_whenshutdown(view->requestmgr, view->task, &event);
	view->attributes &= ~DNS_VIEWATTR_REQSHUTDOWN;

	return (ISC_R_SUCCESS);
}

void
dns_view_setcache(dns_view_t *view, dns_cache_t *cache) {
	dns_view_setcache2(view, cache, ISC_FALSE);
}

void
dns_view_setcache2(dns_view_t *view, dns_cache_t *cache, isc_boolean_t shared) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);

	view->cacheshared = shared;
	if (view->cache != NULL) {
#ifdef BIND9
		if (view->acache != NULL)
			dns_acache_putdb(view->acache, view->cachedb);
#endif
		dns_db_detach(&view->cachedb);
		dns_cache_detach(&view->cache);
	}
	dns_cache_attach(cache, &view->cache);
	dns_cache_attachdb(cache, &view->cachedb);
	INSIST(DNS_DB_VALID(view->cachedb));

#ifdef BIND9
	if (view->acache != NULL)
		dns_acache_setdb(view->acache, view->cachedb);
#endif
}

isc_boolean_t
dns_view_iscacheshared(dns_view_t *view) {
	REQUIRE(DNS_VIEW_VALID(view));

	return (view->cacheshared);
}

void
dns_view_sethints(dns_view_t *view, dns_db_t *hints) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);
	REQUIRE(view->hints == NULL);
	REQUIRE(dns_db_iszone(hints));

	dns_db_attach(hints, &view->hints);
}

void
dns_view_setkeyring(dns_view_t *view, dns_tsig_keyring_t *ring) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(ring != NULL);
	if (view->statickeys != NULL)
		dns_tsigkeyring_detach(&view->statickeys);
	dns_tsigkeyring_attach(ring, &view->statickeys);
}

void
dns_view_setdynamickeyring(dns_view_t *view, dns_tsig_keyring_t *ring) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(ring != NULL);
	if (view->dynamickeys != NULL)
		dns_tsigkeyring_detach(&view->dynamickeys);
	dns_tsigkeyring_attach(ring, &view->dynamickeys);
}

void
dns_view_getdynamickeyring(dns_view_t *view, dns_tsig_keyring_t **ringp) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(ringp != NULL && *ringp == NULL);
	if (view->dynamickeys != NULL)
		dns_tsigkeyring_attach(view->dynamickeys, ringp);
}

void
dns_view_restorekeyring(dns_view_t *view) {
	FILE *fp;
	char keyfile[20];
	int n;

	REQUIRE(DNS_VIEW_VALID(view));

	if (view->dynamickeys != NULL) {
		n = snprintf(keyfile, sizeof(keyfile), "%s.tsigkeys",
			     view->name);
		if (n > 0 && (size_t)n < sizeof(keyfile)) {
			fp = fopen(keyfile, "r");
			if (fp != NULL) {
				dns_keyring_restore(view->dynamickeys, fp);
				(void)fclose(fp);
			}
		}
	}
}

void
dns_view_setdstport(dns_view_t *view, in_port_t dstport) {
	REQUIRE(DNS_VIEW_VALID(view));
	view->dstport = dstport;
}

void
dns_view_freeze(dns_view_t *view) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);

	if (view->resolver != NULL) {
		INSIST(view->cachedb != NULL);
		dns_resolver_freeze(view->resolver);
	}
	view->frozen = ISC_TRUE;
}

#ifdef BIND9
void
dns_view_thaw(dns_view_t *view) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->frozen);

	view->frozen = ISC_FALSE;
}

isc_result_t
dns_view_addzone(dns_view_t *view, dns_zone_t *zone) {
	isc_result_t result;

	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);

	result = dns_zt_mount(view->zonetable, zone);

	return (result);
}
#endif

#ifdef BIND9
isc_result_t
dns_view_findzone(dns_view_t *view, dns_name_t *name, dns_zone_t **zonep) {
	isc_result_t result;

	REQUIRE(DNS_VIEW_VALID(view));

	result = dns_zt_find(view->zonetable, name, 0, NULL, zonep);
	if (result == DNS_R_PARTIALMATCH) {
		dns_zone_detach(zonep);
		result = ISC_R_NOTFOUND;
	}

	return (result);
}
#endif

isc_result_t
dns_view_find(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
	      isc_stdtime_t now, unsigned int options, isc_boolean_t use_hints,
	      dns_db_t **dbp, dns_dbnode_t **nodep, dns_name_t *foundname,
	      dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset) {
	return (dns_view_find2(view, name, type, now, options, use_hints,
			       ISC_FALSE, dbp, nodep, foundname, rdataset,
			       sigrdataset));
}

isc_result_t
dns_view_find2(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
	       isc_stdtime_t now, unsigned int options,
	       isc_boolean_t use_hints, isc_boolean_t use_static_stub,
	       dns_db_t **dbp, dns_dbnode_t **nodep, dns_name_t *foundname,
	       dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_db_t *db, *zdb;
	dns_dbnode_t *node, *znode;
	isc_boolean_t is_cache, is_staticstub_zone;
	dns_rdataset_t zrdataset, zsigrdataset;
	dns_zone_t *zone;

#ifndef BIND9
	UNUSED(use_hints);
	UNUSED(use_static_stub);
#endif

	/*
	 * Find an rdataset whose owner name is 'name', and whose type is
	 * 'type'.
	 */

	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->frozen);
	REQUIRE(type != dns_rdatatype_rrsig);
	REQUIRE(rdataset != NULL);  /* XXXBEW - remove this */
	REQUIRE(nodep == NULL || *nodep == NULL);

	/*
	 * Initialize.
	 */
	dns_rdataset_init(&zrdataset);
	dns_rdataset_init(&zsigrdataset);
	zdb = NULL;
	znode = NULL;

	/*
	 * Find a database to answer the query.
	 */
	zone = NULL;
	db = NULL;
	node = NULL;
	is_staticstub_zone = ISC_FALSE;
#ifdef BIND9
	result = dns_zt_find(view->zonetable, name, 0, NULL, &zone);
	if (zone != NULL && dns_zone_gettype(zone) == dns_zone_staticstub &&
	    !use_static_stub) {
		result = ISC_R_NOTFOUND;
	}
	if (result == ISC_R_SUCCESS || result == DNS_R_PARTIALMATCH) {
		result = dns_zone_getdb(zone, &db);
		if (result != ISC_R_SUCCESS && view->cachedb != NULL)
			dns_db_attach(view->cachedb, &db);
		else if (result != ISC_R_SUCCESS)
			goto cleanup;
		if (dns_zone_gettype(zone) == dns_zone_staticstub &&
		    dns_name_equal(name, dns_zone_getorigin(zone))) {
			is_staticstub_zone = ISC_TRUE;
		}
	} else if (result == ISC_R_NOTFOUND && view->cachedb != NULL)
		dns_db_attach(view->cachedb, &db);
#else
	result = ISC_R_NOTFOUND;
	if (view->cachedb != NULL)
		dns_db_attach(view->cachedb, &db);
#endif /* BIND9 */
	else
		goto cleanup;

	is_cache = dns_db_iscache(db);

 db_find:
	/*
	 * Now look for an answer in the database.
	 */
	result = dns_db_find(db, name, NULL, type, options,
			     now, &node, foundname, rdataset, sigrdataset);

	if (result == DNS_R_DELEGATION || result == ISC_R_NOTFOUND) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		if (node != NULL)
			dns_db_detachnode(db, &node);
		if (!is_cache) {
			dns_db_detach(&db);
			if (view->cachedb != NULL && !is_staticstub_zone) {
				/*
				 * Either the answer is in the cache, or we
				 * don't know it.
				 * Note that if the result comes from a
				 * static-stub zone we stop the search here
				 * (see the function description in view.h).
				 */
				is_cache = ISC_TRUE;
				dns_db_attach(view->cachedb, &db);
				goto db_find;
			}
		} else {
			/*
			 * We don't have the data in the cache.  If we've got
			 * glue from the zone, use it.
			 */
			if (dns_rdataset_isassociated(&zrdataset)) {
				dns_rdataset_clone(&zrdataset, rdataset);
				if (sigrdataset != NULL &&
				    dns_rdataset_isassociated(&zsigrdataset))
					dns_rdataset_clone(&zsigrdataset,
							   sigrdataset);
				result = DNS_R_GLUE;
				if (db != NULL)
					dns_db_detach(&db);
				dns_db_attach(zdb, &db);
				dns_db_attachnode(db, znode, &node);
				goto cleanup;
			}
		}
		/*
		 * We don't know the answer.
		 */
		result = ISC_R_NOTFOUND;
	} else if (result == DNS_R_GLUE) {
		if (view->cachedb != NULL && !is_staticstub_zone) {
			/*
			 * We found an answer, but the cache may be better.
			 * Remember what we've got and go look in the cache.
			 */
			is_cache = ISC_TRUE;
			dns_rdataset_clone(rdataset, &zrdataset);
			dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset)) {
				dns_rdataset_clone(sigrdataset, &zsigrdataset);
				dns_rdataset_disassociate(sigrdataset);
			}
			dns_db_attach(db, &zdb);
			dns_db_attachnode(zdb, node, &znode);
			dns_db_detachnode(db, &node);
			dns_db_detach(&db);
			dns_db_attach(view->cachedb, &db);
			goto db_find;
		}
		/*
		 * Otherwise, the glue is the best answer.
		 */
		result = ISC_R_SUCCESS;
	}

#ifdef BIND9
	if (result == ISC_R_NOTFOUND && use_hints && view->hints != NULL) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		if (db != NULL) {
			if (node != NULL)
				dns_db_detachnode(db, &node);
			dns_db_detach(&db);
		}
		result = dns_db_find(view->hints, name, NULL, type, options,
				     now, &node, foundname,
				     rdataset, sigrdataset);
		if (result == ISC_R_SUCCESS || result == DNS_R_GLUE) {
			/*
			 * We just used a hint.  Let the resolver know it
			 * should consider priming.
			 */
			dns_resolver_prime(view->resolver);
			dns_db_attach(view->hints, &db);
			result = DNS_R_HINT;
		} else if (result == DNS_R_NXRRSET) {
			dns_db_attach(view->hints, &db);
			result = DNS_R_HINTNXRRSET;
		} else if (result == DNS_R_NXDOMAIN)
			result = ISC_R_NOTFOUND;

		/*
		 * Cleanup if non-standard hints are used.
		 */
		if (db == NULL && node != NULL)
			dns_db_detachnode(view->hints, &node);
	}
#endif /* BIND9 */

 cleanup:
	if (dns_rdataset_isassociated(&zrdataset)) {
		dns_rdataset_disassociate(&zrdataset);
		if (dns_rdataset_isassociated(&zsigrdataset))
			dns_rdataset_disassociate(&zsigrdataset);
	}

	if (zdb != NULL) {
		if (znode != NULL)
			dns_db_detachnode(zdb, &znode);
		dns_db_detach(&zdb);
	}

	if (db != NULL) {
		if (node != NULL) {
			if (nodep != NULL)
				*nodep = node;
			else
				dns_db_detachnode(db, &node);
		}
		if (dbp != NULL)
			*dbp = db;
		else
			dns_db_detach(&db);
	} else
		INSIST(node == NULL);

#ifdef BIND9
	if (zone != NULL)
		dns_zone_detach(&zone);
#endif

	return (result);
}

isc_result_t
dns_view_simplefind(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
		    isc_stdtime_t now, unsigned int options,
		    isc_boolean_t use_hints,
		    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_fixedname_t foundname;

	dns_fixedname_init(&foundname);
	result = dns_view_find(view, name, type, now, options, use_hints,
			       NULL, NULL, dns_fixedname_name(&foundname),
			       rdataset, sigrdataset);
	if (result == DNS_R_NXDOMAIN) {
		/*
		 * The rdataset and sigrdataset of the relevant NSEC record
		 * may be returned, but the caller cannot use them because
		 * foundname is not returned by this simplified API.  We
		 * disassociate them here to prevent any misuse by the caller.
		 */
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
	} else if (result != ISC_R_SUCCESS &&
		   result != DNS_R_GLUE &&
		   result != DNS_R_HINT &&
		   result != DNS_R_NCACHENXDOMAIN &&
		   result != DNS_R_NCACHENXRRSET &&
		   result != DNS_R_NXRRSET &&
		   result != DNS_R_HINTNXRRSET &&
		   result != ISC_R_NOTFOUND) {
		if (dns_rdataset_isassociated(rdataset))
			dns_rdataset_disassociate(rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(sigrdataset))
			dns_rdataset_disassociate(sigrdataset);
		result = ISC_R_NOTFOUND;
	}

	return (result);
}

isc_result_t
dns_view_findzonecut(dns_view_t *view, dns_name_t *name, dns_name_t *fname,
		     isc_stdtime_t now, unsigned int options,
		     isc_boolean_t use_hints,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	return(dns_view_findzonecut2(view, name, fname, now, options,
				     use_hints, ISC_TRUE,
				     rdataset, sigrdataset));
}

isc_result_t
dns_view_findzonecut2(dns_view_t *view, dns_name_t *name, dns_name_t *fname,
		      isc_stdtime_t now, unsigned int options,
		      isc_boolean_t use_hints,	isc_boolean_t use_cache,
		      dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_db_t *db;
	isc_boolean_t is_cache, use_zone, try_hints;
	dns_zone_t *zone;
	dns_name_t *zfname;
	dns_rdataset_t zrdataset, zsigrdataset;
	dns_fixedname_t zfixedname;

	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(view->frozen);

	db = NULL;
	zone = NULL;
	use_zone = ISC_FALSE;
	try_hints = ISC_FALSE;
	zfname = NULL;

	/*
	 * Initialize.
	 */
	dns_fixedname_init(&zfixedname);
	dns_rdataset_init(&zrdataset);
	dns_rdataset_init(&zsigrdataset);

	/*
	 * Find the right database.
	 */
#ifdef BIND9
	result = dns_zt_find(view->zonetable, name, 0, NULL, &zone);
	if (result == ISC_R_SUCCESS || result == DNS_R_PARTIALMATCH)
		result = dns_zone_getdb(zone, &db);
#else
	result = ISC_R_NOTFOUND;
#endif
	if (result == ISC_R_NOTFOUND) {
		/*
		 * We're not directly authoritative for this query name, nor
		 * is it a subdomain of any zone for which we're
		 * authoritative.
		 */
		if (use_cache && view->cachedb != NULL) {
			/*
			 * We have a cache; try it.
			 */
			dns_db_attach(view->cachedb, &db);
		} else {
			/*
			 * Maybe we have hints...
			 */
			try_hints = ISC_TRUE;
			goto finish;
		}
	} else if (result != ISC_R_SUCCESS) {
		/*
		 * Something is broken.
		 */
		goto cleanup;
	}
	is_cache = dns_db_iscache(db);

 db_find:
	/*
	 * Look for the zonecut.
	 */
	if (!is_cache) {
		result = dns_db_find(db, name, NULL, dns_rdatatype_ns, options,
				     now, NULL, fname, rdataset, sigrdataset);
		if (result == DNS_R_DELEGATION)
			result = ISC_R_SUCCESS;
		else if (result != ISC_R_SUCCESS)
			goto cleanup;
		if (use_cache && view->cachedb != NULL && db != view->hints) {
			/*
			 * We found an answer, but the cache may be better.
			 */
			zfname = dns_fixedname_name(&zfixedname);
			result = dns_name_copy(fname, zfname, NULL);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
			dns_rdataset_clone(rdataset, &zrdataset);
			dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset)) {
				dns_rdataset_clone(sigrdataset, &zsigrdataset);
				dns_rdataset_disassociate(sigrdataset);
			}
			dns_db_detach(&db);
			dns_db_attach(view->cachedb, &db);
			is_cache = ISC_TRUE;
			goto db_find;
		}
	} else {
		result = dns_db_findzonecut(db, name, options, now, NULL,
					    fname, rdataset, sigrdataset);
		if (result == ISC_R_SUCCESS) {
			if (zfname != NULL &&
			    (!dns_name_issubdomain(fname, zfname) ||
			     (dns_zone_staticstub &&
			      dns_name_equal(fname, zfname)))) {
				/*
				 * We found a zonecut in the cache, but our
				 * zone delegation is better.
				 */
				use_zone = ISC_TRUE;
			}
		} else if (result == ISC_R_NOTFOUND) {
			if (zfname != NULL) {
				/*
				 * We didn't find anything in the cache, but we
				 * have a zone delegation, so use it.
				 */
				use_zone = ISC_TRUE;
			} else {
				/*
				 * Maybe we have hints...
				 */
				try_hints = ISC_TRUE;
			}
		} else {
			/*
			 * Something bad happened.
			 */
			goto cleanup;
		}
	}

 finish:
	if (use_zone) {
		if (dns_rdataset_isassociated(rdataset)) {
			dns_rdataset_disassociate(rdataset);
			if (sigrdataset != NULL &&
			    dns_rdataset_isassociated(sigrdataset))
				dns_rdataset_disassociate(sigrdataset);
		}
		result = dns_name_copy(zfname, fname, NULL);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		dns_rdataset_clone(&zrdataset, rdataset);
		if (sigrdataset != NULL &&
		    dns_rdataset_isassociated(&zrdataset))
			dns_rdataset_clone(&zsigrdataset, sigrdataset);
	} else if (try_hints && use_hints && view->hints != NULL) {
		/*
		 * We've found nothing so far, but we have hints.
		 */
		result = dns_db_find(view->hints, dns_rootname, NULL,
				     dns_rdatatype_ns, 0, now, NULL, fname,
				     rdataset, NULL);
		if (result != ISC_R_SUCCESS) {
			/*
			 * We can't even find the hints for the root
			 * nameservers!
			 */
			if (dns_rdataset_isassociated(rdataset))
				dns_rdataset_disassociate(rdataset);
			result = ISC_R_NOTFOUND;
		}
	}

 cleanup:
	if (dns_rdataset_isassociated(&zrdataset)) {
		dns_rdataset_disassociate(&zrdataset);
		if (dns_rdataset_isassociated(&zsigrdataset))
			dns_rdataset_disassociate(&zsigrdataset);
	}
	if (db != NULL)
		dns_db_detach(&db);
#ifdef BIND9
	if (zone != NULL)
		dns_zone_detach(&zone);
#endif

	return (result);
}

isc_result_t
dns_viewlist_find(dns_viewlist_t *list, const char *name,
		  dns_rdataclass_t rdclass, dns_view_t **viewp)
{
	dns_view_t *view;

	REQUIRE(list != NULL);

	for (view = ISC_LIST_HEAD(*list);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (strcmp(view->name, name) == 0 && view->rdclass == rdclass)
			break;
	}
	if (view == NULL)
		return (ISC_R_NOTFOUND);

	dns_view_attach(view, viewp);

	return (ISC_R_SUCCESS);
}

#ifdef BIND9
isc_result_t
dns_viewlist_findzone(dns_viewlist_t *list, dns_name_t *name,
		      isc_boolean_t allclasses, dns_rdataclass_t rdclass,
		      dns_zone_t **zonep)
{
	dns_view_t *view;
	isc_result_t result;
	dns_zone_t *zone1 = NULL, *zone2 = NULL;
	dns_zone_t **zp = NULL;;

	REQUIRE(list != NULL);
	for (view = ISC_LIST_HEAD(*list);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link)) {
		if (allclasses == ISC_FALSE && view->rdclass != rdclass)
			continue;

		/*
		 * If the zone is defined in more than one view,
		 * treat it as not found.
		 */
		zp = (zone1 == NULL) ? &zone1 : &zone2;
		result = dns_zt_find(view->zonetable, name, 0, NULL, zp);
		INSIST(result == ISC_R_SUCCESS ||
		       result == ISC_R_NOTFOUND ||
		       result == DNS_R_PARTIALMATCH);

		/* Treat a partial match as no match */
		if (result == DNS_R_PARTIALMATCH) {
			dns_zone_detach(zp);
			result = ISC_R_NOTFOUND;
			POST(result);
		}

		if (zone2 != NULL) {
			dns_zone_detach(&zone1);
			dns_zone_detach(&zone2);
			return (ISC_R_NOTFOUND);
		}
	}

	if (zone1 != NULL) {
		dns_zone_attach(zone1, zonep);
		dns_zone_detach(&zone1);
		return (ISC_R_SUCCESS);
	}

	return (ISC_R_NOTFOUND);
}

isc_result_t
dns_view_load(dns_view_t *view, isc_boolean_t stop) {

	REQUIRE(DNS_VIEW_VALID(view));

	return (dns_zt_load(view->zonetable, stop));
}

isc_result_t
dns_view_loadnew(dns_view_t *view, isc_boolean_t stop) {

	REQUIRE(DNS_VIEW_VALID(view));

	return (dns_zt_loadnew(view->zonetable, stop));
}
#endif /* BIND9 */

isc_result_t
dns_view_gettsig(dns_view_t *view, dns_name_t *keyname, dns_tsigkey_t **keyp)
{
	isc_result_t result;
	REQUIRE(keyp != NULL && *keyp == NULL);

	result = dns_tsigkey_find(keyp, keyname, NULL,
				  view->statickeys);
	if (result == ISC_R_NOTFOUND)
		result = dns_tsigkey_find(keyp, keyname, NULL,
					  view->dynamickeys);
	return (result);
}

isc_result_t
dns_view_getpeertsig(dns_view_t *view, isc_netaddr_t *peeraddr,
		     dns_tsigkey_t **keyp)
{
	isc_result_t result;
	dns_name_t *keyname = NULL;
	dns_peer_t *peer = NULL;

	result = dns_peerlist_peerbyaddr(view->peers, peeraddr, &peer);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_peer_getkey(peer, &keyname);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = dns_view_gettsig(view, keyname, keyp);
	return ((result == ISC_R_NOTFOUND) ? ISC_R_FAILURE : result);
}

isc_result_t
dns_view_checksig(dns_view_t *view, isc_buffer_t *source, dns_message_t *msg) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(source != NULL);

	return (dns_tsig_verify(source, msg, view->statickeys,
				view->dynamickeys));
}

#ifdef BIND9
isc_result_t
dns_view_dumpdbtostream(dns_view_t *view, FILE *fp) {
	isc_result_t result;

	REQUIRE(DNS_VIEW_VALID(view));

	(void)fprintf(fp, ";\n; Cache dump of view '%s'\n;\n", view->name);
	result = dns_master_dumptostream(view->mctx, view->cachedb, NULL,
					 &dns_master_style_cache, fp);
	if (result != ISC_R_SUCCESS)
		return (result);
	dns_adb_dump(view->adb, fp);
	dns_resolver_printbadcache(view->resolver, fp);
	return (ISC_R_SUCCESS);
}
#endif

isc_result_t
dns_view_flushcache(dns_view_t *view) {
	return (dns_view_flushcache2(view, ISC_FALSE));
}

isc_result_t
dns_view_flushcache2(dns_view_t *view, isc_boolean_t fixuponly) {
	isc_result_t result;

	REQUIRE(DNS_VIEW_VALID(view));

	if (view->cachedb == NULL)
		return (ISC_R_SUCCESS);
	if (!fixuponly) {
		result = dns_cache_flush(view->cache);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
#ifdef BIND9
	if (view->acache != NULL)
		dns_acache_putdb(view->acache, view->cachedb);
#endif
	dns_db_detach(&view->cachedb);
	dns_cache_attachdb(view->cache, &view->cachedb);
#ifdef BIND9
	if (view->acache != NULL)
		dns_acache_setdb(view->acache, view->cachedb);
	if (view->resolver != NULL)
		dns_resolver_flushbadcache(view->resolver, NULL);
#endif

	dns_adb_flush(view->adb);
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_view_flushname(dns_view_t *view, dns_name_t *name) {

	REQUIRE(DNS_VIEW_VALID(view));

	if (view->adb != NULL)
		dns_adb_flushname(view->adb, name);
	if (view->cache == NULL)
		return (ISC_R_SUCCESS);
	if (view->resolver != NULL)
		dns_resolver_flushbadcache(view->resolver, name);
	return (dns_cache_flushname(view->cache, name));
}

isc_result_t
dns_view_adddelegationonly(dns_view_t *view, dns_name_t *name) {
	isc_result_t result;
	dns_name_t *new;
	isc_uint32_t hash;

	REQUIRE(DNS_VIEW_VALID(view));

	if (view->delonly == NULL) {
		view->delonly = isc_mem_get(view->mctx,
					    sizeof(dns_namelist_t) *
					    DNS_VIEW_DELONLYHASH);
		if (view->delonly == NULL)
			return (ISC_R_NOMEMORY);
		for (hash = 0; hash < DNS_VIEW_DELONLYHASH; hash++)
			ISC_LIST_INIT(view->delonly[hash]);
	}
	hash = dns_name_hash(name, ISC_FALSE) % DNS_VIEW_DELONLYHASH;
	new = ISC_LIST_HEAD(view->delonly[hash]);
	while (new != NULL && !dns_name_equal(new, name))
		new = ISC_LIST_NEXT(new, link);
	if (new != NULL)
		return (ISC_R_SUCCESS);
	new = isc_mem_get(view->mctx, sizeof(*new));
	if (new == NULL)
		return (ISC_R_NOMEMORY);
	dns_name_init(new, NULL);
	result = dns_name_dup(name, view->mctx, new);
	if (result == ISC_R_SUCCESS)
		ISC_LIST_APPEND(view->delonly[hash], new, link);
	else
		isc_mem_put(view->mctx, new, sizeof(*new));
	return (result);
}

isc_result_t
dns_view_excludedelegationonly(dns_view_t *view, dns_name_t *name) {
	isc_result_t result;
	dns_name_t *new;
	isc_uint32_t hash;

	REQUIRE(DNS_VIEW_VALID(view));

	if (view->rootexclude == NULL) {
		view->rootexclude = isc_mem_get(view->mctx,
					    sizeof(dns_namelist_t) *
					    DNS_VIEW_DELONLYHASH);
		if (view->rootexclude == NULL)
			return (ISC_R_NOMEMORY);
		for (hash = 0; hash < DNS_VIEW_DELONLYHASH; hash++)
			ISC_LIST_INIT(view->rootexclude[hash]);
	}
	hash = dns_name_hash(name, ISC_FALSE) % DNS_VIEW_DELONLYHASH;
	new = ISC_LIST_HEAD(view->rootexclude[hash]);
	while (new != NULL && !dns_name_equal(new, name))
		new = ISC_LIST_NEXT(new, link);
	if (new != NULL)
		return (ISC_R_SUCCESS);
	new = isc_mem_get(view->mctx, sizeof(*new));
	if (new == NULL)
		return (ISC_R_NOMEMORY);
	dns_name_init(new, NULL);
	result = dns_name_dup(name, view->mctx, new);
	if (result == ISC_R_SUCCESS)
		ISC_LIST_APPEND(view->rootexclude[hash], new, link);
	else
		isc_mem_put(view->mctx, new, sizeof(*new));
	return (result);
}

isc_boolean_t
dns_view_isdelegationonly(dns_view_t *view, dns_name_t *name) {
	dns_name_t *new;
	isc_uint32_t hash;

	REQUIRE(DNS_VIEW_VALID(view));

	if (!view->rootdelonly && view->delonly == NULL)
		return (ISC_FALSE);

	hash = dns_name_hash(name, ISC_FALSE) % DNS_VIEW_DELONLYHASH;
	if (view->rootdelonly && dns_name_countlabels(name) <= 2) {
		if (view->rootexclude == NULL)
			return (ISC_TRUE);
		new = ISC_LIST_HEAD(view->rootexclude[hash]);
		while (new != NULL && !dns_name_equal(new, name))
			new = ISC_LIST_NEXT(new, link);
		if (new == NULL)
			return (ISC_TRUE);
	}

	if (view->delonly == NULL)
		return (ISC_FALSE);

	new = ISC_LIST_HEAD(view->delonly[hash]);
	while (new != NULL && !dns_name_equal(new, name))
		new = ISC_LIST_NEXT(new, link);
	if (new == NULL)
		return (ISC_FALSE);
	return (ISC_TRUE);
}

void
dns_view_setrootdelonly(dns_view_t *view, isc_boolean_t value) {
	REQUIRE(DNS_VIEW_VALID(view));
	view->rootdelonly = value;
}

isc_boolean_t
dns_view_getrootdelonly(dns_view_t *view) {
	REQUIRE(DNS_VIEW_VALID(view));
	return (view->rootdelonly);
}

#ifdef BIND9
isc_result_t
dns_view_freezezones(dns_view_t *view, isc_boolean_t value) {
	REQUIRE(DNS_VIEW_VALID(view));
	return (dns_zt_freezezones(view->zonetable, value));
}
#endif

void
dns_view_setresstats(dns_view_t *view, isc_stats_t *stats) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);
	REQUIRE(view->resstats == NULL);

	isc_stats_attach(stats, &view->resstats);
}

void
dns_view_getresstats(dns_view_t *view, isc_stats_t **statsp) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(statsp != NULL && *statsp == NULL);

	if (view->resstats != NULL)
		isc_stats_attach(view->resstats, statsp);
}

void
dns_view_setresquerystats(dns_view_t *view, dns_stats_t *stats) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(!view->frozen);
	REQUIRE(view->resquerystats == NULL);

	dns_stats_attach(stats, &view->resquerystats);
}

void
dns_view_getresquerystats(dns_view_t *view, dns_stats_t **statsp) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(statsp != NULL && *statsp == NULL);

	if (view->resquerystats != NULL)
		dns_stats_attach(view->resquerystats, statsp);
}

isc_result_t
dns_view_initsecroots(dns_view_t *view, isc_mem_t *mctx) {
	REQUIRE(DNS_VIEW_VALID(view));
	if (view->secroots_priv != NULL)
		dns_keytable_detach(&view->secroots_priv);
	return (dns_keytable_create(mctx, &view->secroots_priv));
}

isc_result_t
dns_view_getsecroots(dns_view_t *view, dns_keytable_t **ktp) {
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE(ktp != NULL && *ktp == NULL);
	if (view->secroots_priv == NULL)
		return (ISC_R_NOTFOUND);
	dns_keytable_attach(view->secroots_priv, ktp);
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_view_issecuredomain(dns_view_t *view, dns_name_t *name,
			 isc_boolean_t *secure_domain) {
	REQUIRE(DNS_VIEW_VALID(view));
	return (dns_keytable_issecuredomain(view->secroots_priv, name,
					    secure_domain));
}

void
dns_view_untrust(dns_view_t *view, dns_name_t *keyname,
		 dns_rdata_dnskey_t *dnskey, isc_mem_t *mctx)
{
	isc_result_t result;
	unsigned char data[4096];
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t buffer;
	dst_key_t *key = NULL;
	dns_keytable_t *sr = NULL;

	/*
	 * Clear the revoke bit, if set, so that the key will match what's
	 * in secroots now.
	 */
	dnskey->flags &= ~DNS_KEYFLAG_REVOKE;

	/* Convert dnskey to DST key. */
	isc_buffer_init(&buffer, data, sizeof(data));
	dns_rdata_fromstruct(&rdata, dnskey->common.rdclass,
			     dns_rdatatype_dnskey, dnskey, &buffer);
	result = dns_dnssec_keyfromrdata(keyname, &rdata, mctx, &key);
	if (result != ISC_R_SUCCESS)
		return;
	result = dns_view_getsecroots(view, &sr);
	if (result == ISC_R_SUCCESS) {
		dns_keytable_deletekeynode(sr, key);
		dns_keytable_detach(&sr);
	}
	dst_key_free(&key);
}

#define NZF ".nzf"

void
dns_view_setnewzones(dns_view_t *view, isc_boolean_t allow, void *cfgctx,
		     void (*cfg_destroy)(void **))
{
	REQUIRE(DNS_VIEW_VALID(view));
	REQUIRE((cfgctx != NULL && cfg_destroy != NULL) || !allow);

#ifdef BIND9
	if (view->new_zone_file != NULL) {
		isc_mem_free(view->mctx, view->new_zone_file);
		view->new_zone_file = NULL;
	}

	if (view->new_zone_config != NULL) {
		view->cfg_destroy(&view->new_zone_config);
		view->cfg_destroy = NULL;
	}

	if (allow) {
		char buffer[ISC_SHA256_DIGESTSTRINGLENGTH + sizeof(NZF)];
		isc_sha256_data((void *)view->name, strlen(view->name), buffer);
		/* Truncate the hash at 16 chars; full length is overkill */
		isc_string_printf(buffer + 16, sizeof(NZF), "%s", NZF);
		view->new_zone_file = isc_mem_strdup(view->mctx, buffer);
		view->new_zone_config = cfgctx;
		view->cfg_destroy = cfg_destroy;
	}
#else
	UNUSED(allow);
	UNUSED(cfgctx);
	UNUSED(cfg_destroy);
#endif
}
