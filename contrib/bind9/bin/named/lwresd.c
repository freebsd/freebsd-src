/*
 * Copyright (C) 2004-2009, 2012, 2015  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
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

/* $Id: lwresd.c,v 1.60 2009/09/02 23:48:01 tbox Exp $ */

/*! \file
 * \brief
 * Main program for the Lightweight Resolver Daemon.
 *
 * To paraphrase the old saying about X11, "It's not a lightweight deamon
 * for resolvers, it's a deamon for lightweight resolvers".
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/list.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/print.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <dns/log.h>
#include <dns/result.h>
#include <dns/view.h>

#include <named/config.h>
#include <named/globals.h>
#include <named/log.h>
#include <named/lwaddr.h>
#include <named/lwresd.h>
#include <named/lwdclient.h>
#include <named/lwsearch.h>
#include <named/server.h>

#define LWRESD_MAGIC		ISC_MAGIC('L', 'W', 'R', 'D')
#define VALID_LWRESD(l)		ISC_MAGIC_VALID(l, LWRESD_MAGIC)

#define LWRESLISTENER_MAGIC	ISC_MAGIC('L', 'W', 'R', 'L')
#define VALID_LWRESLISTENER(l)	ISC_MAGIC_VALID(l, LWRESLISTENER_MAGIC)

/*!
 * The total number of clients we can handle will be NTASKS * NRECVS.
 */
#define NTASKS		2	/*%< tasks to create to handle lwres queries */
#define NRECVS		2	/*%< max clients per task */

typedef ISC_LIST(ns_lwreslistener_t) ns_lwreslistenerlist_t;

static ns_lwreslistenerlist_t listeners;
static isc_mutex_t listeners_lock;
static isc_once_t once = ISC_ONCE_INIT;


static void
initialize_mutex(void) {
	RUNTIME_CHECK(isc_mutex_init(&listeners_lock) == ISC_R_SUCCESS);
}


/*%
 * Wrappers around our memory management stuff, for the lwres functions.
 */
void *
ns__lwresd_memalloc(void *arg, size_t size) {
	return (isc_mem_get(arg, size));
}

void
ns__lwresd_memfree(void *arg, void *mem, size_t size) {
	isc_mem_put(arg, mem, size);
}


#define CHECK(op)						\
	do { result = (op);					\
		if (result != ISC_R_SUCCESS) goto cleanup;	\
	} while (0)

static isc_result_t
buffer_putstr(isc_buffer_t *b, const char *s) {
	unsigned int len = strlen(s);
	if (isc_buffer_availablelength(b) <= len)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(b, (const unsigned char *)s, len);
	return (ISC_R_SUCCESS);
}

/*
 * Convert a resolv.conf file into a config structure.
 */
isc_result_t
ns_lwresd_parseeresolvconf(isc_mem_t *mctx, cfg_parser_t *pctx,
			   cfg_obj_t **configp)
{
	char text[4096];
	char str[16];
	isc_buffer_t b;
	lwres_context_t *lwctx = NULL;
	lwres_conf_t *lwc = NULL;
	isc_sockaddr_t sa;
	isc_netaddr_t na;
	int i;
	isc_result_t result;
	lwres_result_t lwresult;

	lwctx = NULL;
	lwresult = lwres_context_create(&lwctx, mctx, ns__lwresd_memalloc,
					ns__lwresd_memfree,
					LWRES_CONTEXT_SERVERMODE);
	if (lwresult != LWRES_R_SUCCESS) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	lwresult = lwres_conf_parse(lwctx, lwresd_g_resolvconffile);
	if (lwresult != LWRES_R_SUCCESS) {
		result = DNS_R_SYNTAX;
		goto cleanup;
	}

	lwc = lwres_conf_get(lwctx);
	INSIST(lwc != NULL);

	isc_buffer_init(&b, text, sizeof(text));

	CHECK(buffer_putstr(&b, "options {\n"));

	/*
	 * Build the list of forwarders.
	 */
	if (lwc->nsnext > 0) {
		CHECK(buffer_putstr(&b, "\tforwarders {\n"));

		for (i = 0; i < lwc->nsnext; i++) {
			CHECK(lwaddr_sockaddr_fromlwresaddr(
							&sa,
							&lwc->nameservers[i],
							ns_g_port));
			isc_netaddr_fromsockaddr(&na, &sa);
			CHECK(buffer_putstr(&b, "\t\t"));
			CHECK(isc_netaddr_totext(&na, &b));
			CHECK(buffer_putstr(&b, ";\n"));
		}
		CHECK(buffer_putstr(&b, "\t};\n"));
	}

	/*
	 * Build the sortlist
	 */
	if (lwc->sortlistnxt > 0) {
		CHECK(buffer_putstr(&b, "\tsortlist {\n"));
		CHECK(buffer_putstr(&b, "\t\t{\n"));
		CHECK(buffer_putstr(&b, "\t\t\tany;\n"));
		CHECK(buffer_putstr(&b, "\t\t\t{\n"));
		for (i = 0; i < lwc->sortlistnxt; i++) {
			lwres_addr_t *lwaddr = &lwc->sortlist[i].addr;
			lwres_addr_t *lwmask = &lwc->sortlist[i].mask;
			unsigned int mask;

			CHECK(lwaddr_sockaddr_fromlwresaddr(&sa, lwmask, 0));
			isc_netaddr_fromsockaddr(&na, &sa);
			result = isc_netaddr_masktoprefixlen(&na, &mask);
			if (result != ISC_R_SUCCESS) {
				char addrtext[ISC_NETADDR_FORMATSIZE];
				isc_netaddr_format(&na, addrtext,
						   sizeof(addrtext));
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_LWRESD,
					      ISC_LOG_ERROR,
					      "processing sortlist: '%s' is "
					      "not a valid netmask",
					      addrtext);
				goto cleanup;
			}

			CHECK(lwaddr_sockaddr_fromlwresaddr(&sa, lwaddr, 0));
			isc_netaddr_fromsockaddr(&na, &sa);

			CHECK(buffer_putstr(&b, "\t\t\t\t"));
			CHECK(isc_netaddr_totext(&na, &b));
			snprintf(str, sizeof(str), "%u", mask);
			CHECK(buffer_putstr(&b, "/"));
			CHECK(buffer_putstr(&b, str));
			CHECK(buffer_putstr(&b, ";\n"));
		}
		CHECK(buffer_putstr(&b, "\t\t\t};\n"));
		CHECK(buffer_putstr(&b, "\t\t};\n"));
		CHECK(buffer_putstr(&b, "\t};\n"));
	}

	CHECK(buffer_putstr(&b, "};\n\n"));

	CHECK(buffer_putstr(&b, "lwres {\n"));

	/*
	 * Build the search path
	 */
	if (lwc->searchnxt > 0) {
		if (lwc->searchnxt > 0) {
			CHECK(buffer_putstr(&b, "\tsearch {\n"));
			for (i = 0; i < lwc->searchnxt; i++) {
				CHECK(buffer_putstr(&b, "\t\t\""));
				CHECK(buffer_putstr(&b, lwc->search[i]));
				CHECK(buffer_putstr(&b, "\";\n"));
			}
			CHECK(buffer_putstr(&b, "\t};\n"));
		}
	}

	/*
	 * Build the ndots line
	 */
	if (lwc->ndots != 1) {
		CHECK(buffer_putstr(&b, "\tndots "));
		snprintf(str, sizeof(str), "%u", lwc->ndots);
		CHECK(buffer_putstr(&b, str));
		CHECK(buffer_putstr(&b, ";\n"));
	}

	/*
	 * Build the listen-on line
	 */
	if (lwc->lwnext > 0) {
		CHECK(buffer_putstr(&b, "\tlisten-on {\n"));

		for (i = 0; i < lwc->lwnext; i++) {
			CHECK(lwaddr_sockaddr_fromlwresaddr(&sa,
							    &lwc->lwservers[i],
							    0));
			isc_netaddr_fromsockaddr(&na, &sa);
			CHECK(buffer_putstr(&b, "\t\t"));
			CHECK(isc_netaddr_totext(&na, &b));
			CHECK(buffer_putstr(&b, ";\n"));
		}
		CHECK(buffer_putstr(&b, "\t};\n"));
	}

	CHECK(buffer_putstr(&b, "};\n"));

#if 0
	printf("%.*s\n",
	       (int)isc_buffer_usedlength(&b),
	       (char *)isc_buffer_base(&b));
#endif

	lwres_conf_clear(lwctx);
	lwres_context_destroy(&lwctx);

	return (cfg_parse_buffer(pctx, &b, &cfg_type_namedconf, configp));

 cleanup:

	if (lwctx != NULL) {
		lwres_conf_clear(lwctx);
		lwres_context_destroy(&lwctx);
	}

	return (result);
}


/*
 * Handle lwresd manager objects
 */
isc_result_t
ns_lwdmanager_create(isc_mem_t *mctx, const cfg_obj_t *lwres,
		     ns_lwresd_t **lwresdp)
{
	ns_lwresd_t *lwresd;
	const char *vname;
	dns_rdataclass_t vclass;
	const cfg_obj_t *obj, *viewobj, *searchobj;
	const cfg_listelt_t *element;
	isc_result_t result;

	INSIST(lwresdp != NULL && *lwresdp == NULL);

	lwresd = isc_mem_get(mctx, sizeof(ns_lwresd_t));
	if (lwresd == NULL)
		return (ISC_R_NOMEMORY);

	lwresd->mctx = NULL;
	isc_mem_attach(mctx, &lwresd->mctx);
	lwresd->view = NULL;
	lwresd->search = NULL;
	lwresd->refs = 1;

	obj = NULL;
	(void)cfg_map_get(lwres, "ndots", &obj);
	if (obj != NULL)
		lwresd->ndots = cfg_obj_asuint32(obj);
	else
		lwresd->ndots = 1;

	RUNTIME_CHECK(isc_mutex_init(&lwresd->lock) == ISC_R_SUCCESS);

	lwresd->shutting_down = ISC_FALSE;

	viewobj = NULL;
	(void)cfg_map_get(lwres, "view", &viewobj);
	if (viewobj != NULL) {
		vname = cfg_obj_asstring(cfg_tuple_get(viewobj, "name"));
		obj = cfg_tuple_get(viewobj, "class");
		result = ns_config_getclass(obj, dns_rdataclass_in, &vclass);
		if (result != ISC_R_SUCCESS)
			goto fail;
	} else {
		vname = "_default";
		vclass = dns_rdataclass_in;
	}

	result = dns_viewlist_find(&ns_g_server->viewlist, vname, vclass,
				   &lwresd->view);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
			      "couldn't find view %s", vname);
		goto fail;
	}

	searchobj = NULL;
	(void)cfg_map_get(lwres, "search", &searchobj);
	if (searchobj != NULL) {
		lwresd->search = NULL;
		result = ns_lwsearchlist_create(lwresd->mctx,
						&lwresd->search);
		if (result != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
				      "couldn't create searchlist");
			goto fail;
		}
		for (element = cfg_list_first(searchobj);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			const cfg_obj_t *search;
			const char *searchstr;
			isc_buffer_t namebuf;
			dns_fixedname_t fname;
			dns_name_t *name;

			search = cfg_listelt_value(element);
			searchstr = cfg_obj_asstring(search);

			dns_fixedname_init(&fname);
			name = dns_fixedname_name(&fname);
			isc_buffer_constinit(&namebuf, searchstr,
					strlen(searchstr));
			isc_buffer_add(&namebuf, strlen(searchstr));
			result = dns_name_fromtext(name, &namebuf,
						   dns_rootname, 0, NULL);
			if (result != ISC_R_SUCCESS) {
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_LWRESD,
					      ISC_LOG_WARNING,
					      "invalid name %s in searchlist",
					      searchstr);
				continue;
			}

			result = ns_lwsearchlist_append(lwresd->search, name);
			if (result != ISC_R_SUCCESS) {
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_LWRESD,
					      ISC_LOG_WARNING,
					      "couldn't update searchlist");
				goto fail;
			}
		}
	}

	lwresd->magic = LWRESD_MAGIC;

	*lwresdp = lwresd;
	return (ISC_R_SUCCESS);

 fail:
	if (lwresd->view != NULL)
		dns_view_detach(&lwresd->view);
	if (lwresd->search != NULL)
		ns_lwsearchlist_detach(&lwresd->search);
	if (lwresd->mctx != NULL)
		isc_mem_detach(&lwresd->mctx);
	isc_mem_put(mctx, lwresd, sizeof(ns_lwresd_t));
	return (result);
}

void
ns_lwdmanager_attach(ns_lwresd_t *source, ns_lwresd_t **targetp) {
	INSIST(VALID_LWRESD(source));
	INSIST(targetp != NULL && *targetp == NULL);

	LOCK(&source->lock);
	source->refs++;
	UNLOCK(&source->lock);

	*targetp = source;
}

void
ns_lwdmanager_detach(ns_lwresd_t **lwresdp) {
	ns_lwresd_t *lwresd;
	isc_mem_t *mctx;
	isc_boolean_t done = ISC_FALSE;

	INSIST(lwresdp != NULL && *lwresdp != NULL);
	INSIST(VALID_LWRESD(*lwresdp));

	lwresd = *lwresdp;
	*lwresdp = NULL;

	LOCK(&lwresd->lock);
	INSIST(lwresd->refs > 0);
	lwresd->refs--;
	if (lwresd->refs == 0)
		done = ISC_TRUE;
	UNLOCK(&lwresd->lock);

	if (!done)
		return;

	dns_view_detach(&lwresd->view);
	if (lwresd->search != NULL)
		ns_lwsearchlist_detach(&lwresd->search);
	mctx = lwresd->mctx;
	lwresd->magic = 0;
	isc_mem_put(mctx, lwresd, sizeof(*lwresd));
	isc_mem_detach(&mctx);
}


/*
 * Handle listener objects
 */
void
ns_lwreslistener_attach(ns_lwreslistener_t *source,
			ns_lwreslistener_t **targetp)
{
	INSIST(VALID_LWRESLISTENER(source));
	INSIST(targetp != NULL && *targetp == NULL);

	LOCK(&source->lock);
	source->refs++;
	UNLOCK(&source->lock);

	*targetp = source;
}

void
ns_lwreslistener_detach(ns_lwreslistener_t **listenerp) {
	ns_lwreslistener_t *listener;
	isc_mem_t *mctx;
	isc_boolean_t done = ISC_FALSE;

	INSIST(listenerp != NULL && *listenerp != NULL);
	INSIST(VALID_LWRESLISTENER(*listenerp));

	listener = *listenerp;

	LOCK(&listener->lock);
	INSIST(listener->refs > 0);
	listener->refs--;
	if (listener->refs == 0)
		done = ISC_TRUE;
	UNLOCK(&listener->lock);

	if (!done)
		return;

	if (listener->manager != NULL)
		ns_lwdmanager_detach(&listener->manager);

	if (listener->sock != NULL)
		isc_socket_detach(&listener->sock);

	listener->magic = 0;
	mctx = listener->mctx;
	isc_mem_put(mctx, listener, sizeof(*listener));
	isc_mem_detach(&mctx);
	listenerp = NULL;
}

static isc_result_t
listener_create(isc_mem_t *mctx, ns_lwresd_t *lwresd,
		ns_lwreslistener_t **listenerp)
{
	ns_lwreslistener_t *listener;
	isc_result_t result;

	REQUIRE(listenerp != NULL && *listenerp == NULL);

	listener = isc_mem_get(mctx, sizeof(ns_lwreslistener_t));
	if (listener == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_mutex_init(&listener->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, listener, sizeof(ns_lwreslistener_t));
		return (result);
	}

	listener->magic = LWRESLISTENER_MAGIC;
	listener->refs = 1;

	listener->sock = NULL;

	listener->manager = NULL;
	ns_lwdmanager_attach(lwresd, &listener->manager);

	listener->mctx = NULL;
	isc_mem_attach(mctx, &listener->mctx);

	ISC_LINK_INIT(listener, link);
	ISC_LIST_INIT(listener->cmgrs);

	*listenerp = listener;
	return (ISC_R_SUCCESS);
}

static isc_result_t
listener_bind(ns_lwreslistener_t *listener, isc_sockaddr_t *address) {
	isc_socket_t *sock = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	int pf;

	pf = isc_sockaddr_pf(address);
	if ((pf == AF_INET && isc_net_probeipv4() != ISC_R_SUCCESS) ||
	    (pf == AF_INET6 && isc_net_probeipv6() != ISC_R_SUCCESS))
		return (ISC_R_FAMILYNOSUPPORT);

	listener->address = *address;

	if (isc_sockaddr_getport(&listener->address) == 0) {
		in_port_t port;
		port = lwresd_g_listenport;
		if (port == 0)
			port = LWRES_UDP_PORT;
		isc_sockaddr_setport(&listener->address, port);
	}

	sock = NULL;
	result = isc_socket_create(ns_g_socketmgr, pf,
				   isc_sockettype_udp, &sock);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
			      "failed to create lwres socket: %s",
			      isc_result_totext(result));
		return (result);
	}

	result = isc_socket_bind(sock, &listener->address,
				 ISC_SOCKET_REUSEADDRESS);
	if (result != ISC_R_SUCCESS) {
		char socktext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(&listener->address, socktext,
				    sizeof(socktext));
		isc_socket_detach(&sock);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
			      "failed to add lwres socket: %s: %s",
			      socktext, isc_result_totext(result));
		return (result);
	}
	listener->sock = sock;
	return (ISC_R_SUCCESS);
}

static void
listener_copysock(ns_lwreslistener_t *oldlistener,
		  ns_lwreslistener_t *newlistener)
{
	newlistener->address = oldlistener->address;
	isc_socket_attach(oldlistener->sock, &newlistener->sock);
}

static isc_result_t
listener_startclients(ns_lwreslistener_t *listener) {
	ns_lwdclientmgr_t *cm, *next;
	unsigned int i;
	isc_result_t result;

	/*
	 * Create the client managers.
	 */
	result = ISC_R_SUCCESS;
	for (i = 0; i < NTASKS && result == ISC_R_SUCCESS; i++)
		result = ns_lwdclientmgr_create(listener, NRECVS,
						ns_g_taskmgr);

	/*
	 * Ensure that we have created at least one.
	 */
	if (ISC_LIST_EMPTY(listener->cmgrs))
		return (result);

	/*
	 * Walk the list of clients and start each one up.
	 */
	LOCK(&listener->lock);
	cm = ISC_LIST_HEAD(listener->cmgrs);
	while (cm != NULL) {
		next = ISC_LIST_NEXT(cm, link);
		result = ns_lwdclient_startrecv(cm);
		if (result != ISC_R_SUCCESS)
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_LWRESD, ISC_LOG_ERROR,
				      "could not start lwres "
				      "client handler: %s",
				      isc_result_totext(result));
		cm = next;
	}
	UNLOCK(&listener->lock);

	return (ISC_R_SUCCESS);
}

static void
listener_shutdown(ns_lwreslistener_t *listener) {
	ns_lwdclientmgr_t *cm;

	cm = ISC_LIST_HEAD(listener->cmgrs);
	while (cm != NULL) {
		isc_task_shutdown(cm->task);
		cm = ISC_LIST_NEXT(cm, link);
	}
}

static isc_result_t
find_listener(isc_sockaddr_t *address, ns_lwreslistener_t **listenerp) {
	ns_lwreslistener_t *listener;

	INSIST(listenerp != NULL && *listenerp == NULL);

	for (listener = ISC_LIST_HEAD(listeners);
	     listener != NULL;
	     listener = ISC_LIST_NEXT(listener, link))
	{
		if (!isc_sockaddr_equal(address, &listener->address))
			continue;
		*listenerp = listener;
		return (ISC_R_SUCCESS);
	}
	return (ISC_R_NOTFOUND);
}

void
ns_lwreslistener_unlinkcm(ns_lwreslistener_t *listener, ns_lwdclientmgr_t *cm)
{
	REQUIRE(VALID_LWRESLISTENER(listener));

	LOCK(&listener->lock);
	ISC_LIST_UNLINK(listener->cmgrs, cm, link);
	UNLOCK(&listener->lock);
}

void
ns_lwreslistener_linkcm(ns_lwreslistener_t *listener, ns_lwdclientmgr_t *cm) {
	REQUIRE(VALID_LWRESLISTENER(listener));

	/*
	 * This does no locking, since it's called early enough that locking
	 * isn't needed.
	 */
	ISC_LIST_APPEND(listener->cmgrs, cm, link);
}

static isc_result_t
configure_listener(isc_sockaddr_t *address, ns_lwresd_t *lwresd,
		   isc_mem_t *mctx, ns_lwreslistenerlist_t *newlisteners)
{
	ns_lwreslistener_t *listener, *oldlistener = NULL;
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	isc_result_t result;

	(void)find_listener(address, &oldlistener);
	listener = NULL;
	result = listener_create(mctx, lwresd, &listener);
	if (result != ISC_R_SUCCESS) {
		isc_sockaddr_format(address, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, ISC_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
			      "lwres failed to configure %s: %s",
			      socktext, isc_result_totext(result));
		return (result);
	}

	/*
	 * If there's already a listener, don't rebind the socket.
	 */
	if (oldlistener == NULL) {
		result = listener_bind(listener, address);
		if (result != ISC_R_SUCCESS) {
			ns_lwreslistener_detach(&listener);
			return (ISC_R_SUCCESS);
		}
	} else
		listener_copysock(oldlistener, listener);

	result = listener_startclients(listener);
	if (result != ISC_R_SUCCESS) {
		isc_sockaddr_format(address, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, ISC_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_WARNING,
			      "lwres: failed to start %s: %s", socktext,
			      isc_result_totext(result));
		ns_lwreslistener_detach(&listener);
		return (ISC_R_SUCCESS);
	}

	if (oldlistener != NULL) {
		/*
		 * Remove the old listener from the old list and shut it down.
		 */
		ISC_LIST_UNLINK(listeners, oldlistener, link);
		listener_shutdown(oldlistener);
		ns_lwreslistener_detach(&oldlistener);
	} else {
		isc_sockaddr_format(address, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, ISC_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_NOTICE,
			      "lwres listening on %s", socktext);
	}

	ISC_LIST_APPEND(*newlisteners, listener, link);
	return (result);
}

isc_result_t
ns_lwresd_configure(isc_mem_t *mctx, const cfg_obj_t *config) {
	const cfg_obj_t *lwreslist = NULL;
	const cfg_obj_t *lwres = NULL;
	const cfg_obj_t *listenerslist = NULL;
	const cfg_listelt_t *element = NULL;
	ns_lwreslistener_t *listener;
	ns_lwreslistenerlist_t newlisteners;
	isc_result_t result;
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	isc_sockaddr_t *addrs = NULL;
	ns_lwresd_t *lwresd = NULL;
	isc_uint32_t count = 0;

	REQUIRE(mctx != NULL);
	REQUIRE(config != NULL);

	RUNTIME_CHECK(isc_once_do(&once, initialize_mutex) == ISC_R_SUCCESS);

	ISC_LIST_INIT(newlisteners);

	result = cfg_map_get(config, "lwres", &lwreslist);
	if (result != ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	LOCK(&listeners_lock);
	/*
	 * Run through the new lwres address list, noting sockets that
	 * are already being listened on and moving them to the new list.
	 *
	 * Identifying duplicates addr/port combinations is left to either
	 * the underlying config code, or to the bind attempt getting an
	 * address-in-use error.
	 */
	for (element = cfg_list_first(lwreslist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		in_port_t port;

		lwres = cfg_listelt_value(element);
		CHECK(ns_lwdmanager_create(mctx, lwres, &lwresd));

		port = lwresd_g_listenport;
		if (port == 0)
			port = LWRES_UDP_PORT;

		listenerslist = NULL;
		(void)cfg_map_get(lwres, "listen-on", &listenerslist);
		if (listenerslist == NULL) {
			struct in_addr localhost;
			isc_sockaddr_t address;

			localhost.s_addr = htonl(INADDR_LOOPBACK);
			isc_sockaddr_fromin(&address, &localhost, port);
			CHECK(configure_listener(&address, lwresd, mctx,
						 &newlisteners));
		} else {
			isc_uint32_t i;

			CHECK(ns_config_getiplist(config, listenerslist,
						  port, mctx, &addrs, &count));
			for (i = 0; i < count; i++)
				CHECK(configure_listener(&addrs[i], lwresd,
							 mctx, &newlisteners));
			ns_config_putiplist(mctx, &addrs, count);
		}
		ns_lwdmanager_detach(&lwresd);
	}

	/*
	 * Shutdown everything on the listeners list, and remove them from
	 * the list.  Then put all of the new listeners on it.
	 */

	while (!ISC_LIST_EMPTY(listeners)) {
		listener = ISC_LIST_HEAD(listeners);
		ISC_LIST_UNLINK(listeners, listener, link);

		isc_sockaddr_format(&listener->address,
				    socktext, sizeof(socktext));

		listener_shutdown(listener);
		ns_lwreslistener_detach(&listener);

		isc_log_write(ns_g_lctx, ISC_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_NOTICE,
			      "lwres no longer listening on %s", socktext);
	}

 cleanup:
	ISC_LIST_APPENDLIST(listeners, newlisteners, link);

	if (addrs != NULL)
		ns_config_putiplist(mctx, &addrs, count);

	if (lwresd != NULL)
		ns_lwdmanager_detach(&lwresd);

	UNLOCK(&listeners_lock);

	return (result);
}

void
ns_lwresd_shutdown(void) {
	ns_lwreslistener_t *listener;

	RUNTIME_CHECK(isc_once_do(&once, initialize_mutex) == ISC_R_SUCCESS);

	while (!ISC_LIST_EMPTY(listeners)) {
		listener = ISC_LIST_HEAD(listeners);
		ISC_LIST_UNLINK(listeners, listener, link);
		ns_lwreslistener_detach(&listener);
	}
}
