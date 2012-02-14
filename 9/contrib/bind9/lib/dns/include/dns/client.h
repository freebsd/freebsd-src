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

/* $Id: client.h,v 1.3 2009-09-02 23:48:02 tbox Exp $ */

#ifndef DNS_CLIENT_H
#define DNS_CLIENT_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 *
 * \brief
 * The DNS client module provides convenient programming interfaces to various
 * DNS services, such as name resolution with or without DNSSEC validation or
 * dynamic DNS update.  This module is primarily expected to be used by other
 * applications than BIND9-related ones that need such advanced DNS features.
 *
 * MP:
 *\li	In the typical usage of this module, application threads will not share
 *	the same data structures created and manipulated in this module.
 *	However, the module still ensures appropriate synchronization of such
 *	data structures.
 *
 * Resources:
 *\li	TBS
 *
 * Security:
 *\li	This module does not handle any low-level data directly, and so no
 *	security issue specific to this module is anticipated.
 */

#include <isc/event.h>
#include <isc/sockaddr.h>

#include <dns/tsig.h>
#include <dns/types.h>

#include <dst/dst.h>

typedef enum {
	updateop_none = 0,
	updateop_add = 1,
	updateop_delete = 2,
	updateop_exist = 3,
	updateop_notexist = 4,
	updateop_max = 5
} dns_client_updateop_t;

ISC_LANG_BEGINDECLS

/***
 *** Types
 ***/

/*%
 * Optional flags for dns_client_create(x).
 */
/*%< Enable caching resolution results (experimental). */
#define DNS_CLIENTCREATEOPT_USECACHE	0x8000

/*%
 * Optional flags for dns_client_(start)resolve.
 */
/*%< Disable DNSSEC validation. */
#define DNS_CLIENTRESOPT_NODNSSEC	0x01
/*%< Allow running external context. */
#define DNS_CLIENTRESOPT_ALLOWRUN	0x02

/*%
 * Optional flags for dns_client_(start)request.
 */
/*%< Allow running external context. */
#define DNS_CLIENTREQOPT_ALLOWRUN	0x01

/*%
 * A dns_clientresevent_t is sent when name resolution performed by a client
 * completes.  'result' stores the result code of the entire resolution
 * procedure.  'vresult' specifically stores the result code of DNSSEC
 * validation if it is performed.  When name resolution successfully completes,
 * 'answerlist' is typically non empty, containing answer names along with
 * RRsets.  It is the receiver's responsibility to free this list by calling
 * dns_client_freeresanswer() before freeing the event structure.
 */
typedef struct dns_clientresevent {
	ISC_EVENT_COMMON(struct dns_clientresevent);
	isc_result_t	result;
	isc_result_t	vresult;
	dns_namelist_t	answerlist;
} dns_clientresevent_t;		/* too long? */

/*%
 * Status of a dynamic update procedure.
 */
typedef enum {
	dns_clientupdatestate_prepare,	/*%< no updates have been sent */
	dns_clientupdatestate_sent,	/*%< updates were sent, no response */
	dns_clientupdatestate_done	/*%< update was sent and succeeded */
} dns_clientupdatestate_t;

/*%
 * A dns_clientreqevent_t is sent when a DNS request is completed by a client.
 * 'result' stores the result code of the entire transaction.
 * If the transaction is successfully completed but the response packet cannot
 * be parsed, 'result' will store the result code of dns_message_parse().
 * If the response packet is received, 'rmessage' will contain the response
 * message, whether it is successfully parsed or not.
 */
typedef struct dns_clientreqevent {
	ISC_EVENT_COMMON(struct dns_clientreqevent);
	isc_result_t	result;
	dns_message_t	*rmessage;
} dns_clientreqevent_t;		/* too long? */

/*%
 * A dns_clientupdateevent_t is sent when dynamic update performed by a client
 * completes.  'result' stores the result code of the entire update procedure.
 * 'state' specifies the status of the update procedure when this event is
 * sent.  This can be used as a hint by the receiver to determine whether
 * the update attempt was ever made.  In particular, if the state is
 * dns_clientupdatestate_prepare, the receiver can be sure that the requested
 * update was not applied.
 */
typedef struct dns_clientupdateevent {
	ISC_EVENT_COMMON(struct dns_clientupdateevent);
	isc_result_t		result;
	dns_clientupdatestate_t	state;
} dns_clientupdateevent_t;	/* too long? */

isc_result_t
dns_client_create(dns_client_t **clientp, unsigned int options);

isc_result_t
dns_client_createx(isc_mem_t *mctx, isc_appctx_t *actx, isc_taskmgr_t *taskmgr,
		   isc_socketmgr_t *socketmgr, isc_timermgr_t *timermgr,
		   unsigned int options, dns_client_t **clientp);
/*%<
 * Create a DNS client.  These functions create a new client object with
 * minimal internal resources such as the default 'view' for the IN class and
 * IPv4/IPv6 dispatches for the view.
 *
 * dns_client_createx() takes 'manager' arguments so that the caller can
 * control the behavior of the client through the underlying event framework.
 * On the other hand, dns_client_create() simplifies the interface and creates
 * the managers internally.  A DNS client object created via
 * dns_client_create() is expected to be used by an application that only needs
 * simple synchronous services or by a thread-based application.
 *
 * If the DNS_CLIENTCREATEOPT_USECACHE flag is set in 'options',
 * dns_client_create(x) will create a cache database with the view.
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	'actx' is a valid application context.
 *
 *\li	'taskmgr' is a valid task manager.
 *
 *\li	'socketmgr' is a valid socket manager.
 *
 *\li	'timermgr' is a valid timer manager.
 *
 *\li	clientp != NULL && *clientp == NULL.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

void
dns_client_destroy(dns_client_t **clientp);
/*%<
 * Destroy 'client'.
 *
 * Requires:
 *
 *\li	'*clientp' is a valid client.
 *
 * Ensures:
 *
 *\li	*clientp == NULL.
 */

isc_result_t
dns_client_setservers(dns_client_t *client, dns_rdataclass_t rdclass,
		      dns_name_t *namespace, isc_sockaddrlist_t *addrs);
/*%<
 * Specify a list of addresses of recursive name servers that the client will
 * use for name resolution.  A view for the 'rdclass' class must be created
 * beforehand.  If 'namespace' is non NULL, the specified server will be used
 * if and only if the query name is a subdomain of 'namespace'.  When servers
 * for multiple 'namespace's are provided, and a query name is covered by
 * more than one 'namespace', the servers for the best (longest) matching
 * namespace will be used.  If 'namespace' is NULL, it works as if
 * dns_rootname (.) were specified.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'namespace' is NULL or a valid name.
 *
 *\li	'addrs' != NULL.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

isc_result_t
dns_client_clearservers(dns_client_t *client, dns_rdataclass_t rdclass,
			dns_name_t *namespace);
/*%<
 * Remove configured recursive name servers for the 'rdclass' and 'namespace'
 * from the client.  See the description of dns_client_setservers() for
 * the requirements about 'rdclass' and 'namespace'.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'namespace' is NULL or a valid name.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

isc_result_t
dns_client_resolve(dns_client_t *client, dns_name_t *name,
		   dns_rdataclass_t rdclass, dns_rdatatype_t type,
		   unsigned int options, dns_namelist_t *namelist);

isc_result_t
dns_client_startresolve(dns_client_t *client, dns_name_t *name,
			dns_rdataclass_t rdclass, dns_rdatatype_t type,
			unsigned int options, isc_task_t *task,
			isc_taskaction_t action, void *arg,
			dns_clientrestrans_t **transp);
/*%<
 * Perform name resolution for 'name', 'rdclass', and 'type'.
 *
 * If any trusted keys are configured and the query name is considered to
 * belong to a secure zone, these functions also validate the responses
 * using DNSSEC by default.  If the DNS_CLIENTRESOPT_NODNSSEC flag is set
 * in 'options', DNSSEC validation is disabled regardless of the configured
 * trusted keys or the query name.
 *
 * dns_client_resolve() provides a synchronous service.  This function starts
 * name resolution internally and blocks until it completes.  On success,
 * 'namelist' will contain a list of answer names, each of which has
 * corresponding RRsets.  The caller must provide a valid empty list, and
 * is responsible for freeing the list content via dns_client_freeresanswer().
 * If the name resolution fails due to an error in DNSSEC validation,
 * dns_client_resolve() returns the result code indicating the validation
 * error. Otherwise, it returns the result code of the entire resolution
 * process, either success or failure.
 *
 * It is typically expected that the client object passed to
 * dns_client_resolve() was created via dns_client_create() and has its own
 * managers and contexts.  However, if the DNS_CLIENTRESOPT_ALLOWRUN flag is
 * set in 'options', this function performs the synchronous service even if
 * it does not have its own manager and context structures.
 *
 * dns_client_startresolve() is an asynchronous version of dns_client_resolve()
 * and does not block.  When name resolution is completed, 'action' will be
 * called with the argument of a 'dns_clientresevent_t' object, which contains
 * the resulting list of answer names (on success).  On return, '*transp' is
 * set to an opaque transaction ID so that the caller can cancel this
 * resolution process.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'addrs' != NULL.
 *
 *\li	'name' is a valid name.
 *
 *\li	'namelist' != NULL and is not empty.
 *
 *\li	'task' is a valid task.
 *
 *\li	'transp' != NULL && *transp == NULL;
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

void
dns_client_cancelresolve(dns_clientrestrans_t *trans);
/*%<
 * Cancel an ongoing resolution procedure started via
 * dns_client_startresolve().
 *
 * Notes:
 *
 *\li	If the resolution procedure has not completed, post its CLIENTRESDONE
 *	event with a result code of #ISC_R_CANCELED.
 *
 * Requires:
 *
 *\li	'trans' is a valid transaction ID.
 */

void
dns_client_destroyrestrans(dns_clientrestrans_t **transp);
/*%<
 * Destroy name resolution transaction state identified by '*transp'.
 *
 * Requires:
 *
 *\li	'*transp' is a valid transaction ID.
 *
 *\li	The caller has received the CLIENTRESDONE event (either because the
 *	resolution completed or because dns_client_cancelresolve() was called).
 *
 * Ensures:
 *
 *\li	*transp == NULL.
 */

void
dns_client_freeresanswer(dns_client_t *client, dns_namelist_t *namelist);
/*%<
 * Free resources allocated for the content of 'namelist'.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'namelist' != NULL.
 */

isc_result_t
dns_client_addtrustedkey(dns_client_t *client, dns_rdataclass_t rdclass,
			 dns_name_t *keyname, isc_buffer_t *keydatabuf);
/*%<
 * Add a DNSSEC trusted key for the 'rdclass' class.  A view for the 'rdclass'
 * class must be created beforehand.  'keyname' is the DNS name of the key,
 * and 'keydatabuf' stores the resource data of the key.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'keyname' is a valid name.
 *
 *\li	'keydatabuf' is a valid buffer.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

isc_result_t
dns_client_request(dns_client_t *client, dns_message_t *qmessage,
		   dns_message_t *rmessage, isc_sockaddr_t *server,
		   unsigned int options, unsigned int parseoptions,
		   dns_tsec_t *tsec, unsigned int timeout,
		   unsigned int udptimeout, unsigned int udpretries);

isc_result_t
dns_client_startrequest(dns_client_t *client, dns_message_t *qmessage,
			dns_message_t *rmessage, isc_sockaddr_t *server,
			unsigned int options, unsigned int parseoptions,
			dns_tsec_t *tsec, unsigned int timeout,
			unsigned int udptimeout, unsigned int udpretries,
			isc_task_t *task, isc_taskaction_t action, void *arg,
			dns_clientreqtrans_t **transp);

/*%<
 * Send a DNS request containig a query message 'query' to 'server'.
 *
 * 'parseoptions' will be used when the response packet is parsed, and will be
 * passed to dns_message_parse() via dns_request_getresponse().  See
 * dns_message_parse() for more details.
 *
 * 'tsec' is a transaction security object containing, e.g. a TSIG key for
 * authenticating the request/response transaction.  This is optional and can
 * be NULL, in which case this library performs the transaction  without any
 * transaction authentication.
 *
 * 'timeout', 'udptimeout', and 'udpretries' are passed to
 * dns_request_createvia3().  See dns_request_createvia3() for more details.
 *
 * dns_client_request() provides a synchronous service.  This function sends
 * the request and blocks until a response is received.  On success,
 * 'rmessage' will contain the response message.  The caller must provide a
 * valid initialized message.
 *
 * It is usually expected that the client object passed to
 * dns_client_request() was created via dns_client_create() and has its own
 * managers and contexts.  However, if the DNS_CLIENTREQOPT_ALLOWRUN flag is
 * set in 'options', this function performs the synchronous service even if
 * it does not have its own manager and context structures.
 *
 * dns_client_startrequest() is an asynchronous version of dns_client_request()
 * and does not block.  When the transaction is completed, 'action' will be
 * called with the argument of a 'dns_clientreqevent_t' object, which contains
 * the response message (on success).  On return, '*transp' is set to an opaque
 * transaction ID so that the caller can cancel this request.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'qmessage' and 'rmessage' are valid initialized message.
 *
 *\li	'server' is a valid socket address structure.
 *
 *\li	'task' is a valid task.
 *
 *\li	'transp' != NULL && *transp == NULL;
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 *
 *\li	Any result that dns_message_parse() can return.
 */

void
dns_client_cancelrequest(dns_clientreqtrans_t *transp);
/*%<
 * Cancel an ongoing DNS request procedure started via
 * dns_client_startrequest().
 *
 * Notes:
 *
 *\li	If the request procedure has not completed, post its CLIENTREQDONE
 *	event with a result code of #ISC_R_CANCELED.
 *
 * Requires:
 *
 *\li	'trans' is a valid transaction ID.
 */

void
dns_client_destroyreqtrans(dns_clientreqtrans_t **transp);
/*%
 * Destroy DNS request transaction state identified by '*transp'.
 *
 * Requires:
 *
 *\li	'*transp' is a valid transaction ID.
 *
 *\li	The caller has received the CLIENTREQDONE event (either because the
 *	request completed or because dns_client_cancelrequest() was called).
 *
 * Ensures:
 *
 *\li	*transp == NULL.
 */

isc_result_t
dns_client_update(dns_client_t *client, dns_rdataclass_t rdclass,
		  dns_name_t *zonename, dns_namelist_t *prerequisites,
		  dns_namelist_t *updates, isc_sockaddrlist_t *servers,
		  dns_tsec_t *tsec, unsigned int options);

isc_result_t
dns_client_startupdate(dns_client_t *client, dns_rdataclass_t rdclass,
		       dns_name_t *zonename, dns_namelist_t *prerequisites,
		       dns_namelist_t *updates, isc_sockaddrlist_t *servers,
		       dns_tsec_t *tsec, unsigned int options,
		       isc_task_t *task, isc_taskaction_t action, void *arg,
		       dns_clientupdatetrans_t **transp);
/*%<
 * Perform DNS dynamic update for 'updates' of the 'rdclass' class with
 * optional 'prerequisites'.
 *
 * 'updates' are a list of names with associated RRsets to be updated.
 *
 * 'prerequisites' are a list of names with associated RRsets corresponding to
 * the prerequisites of the updates.  This is optional and can be NULL, in
 * which case the prerequisite section of the update message will be empty.
 *
 * Both 'updates' and 'prerequisites' must be constructed as specified in
 * RFC2136.
 *
 * 'zonename' is the name of the zone in which the updated names exist.
 * This is optional and can be NULL.  In this case, these functions internally
 * identify the appropriate zone through some queries for the SOA RR starting
 * with the first name in prerequisites or updates.
 *
 * 'servers' is a list of authoritative servers to which the update message
 * should be sent.  This is optional and can be NULL.  In this case, these
 * functions internally identify the appropriate primary server name and its
 * addresses through some queries for the SOA RR (like the case of zonename)
 * and supplemental A/AAAA queries for the server name.
 * Note: The client module generally assumes the given addresses are of the
 * primary server of the corresponding zone.  It will work even if a secondary
 * server address is specified as long as the server allows update forwarding,
 * it is generally discouraged to include secondary server addresses unless
 * there's strong reason to do so.
 *
 * 'tsec' is a transaction security object containing, e.g. a TSIG key for
 * authenticating the update transaction (and the supplemental query/response
 * transactions if the server is specified).  This is optional and can be
 * NULL, in which case the library tries the update without any transaction
 * authentication.
 *
 * dns_client_update() provides a synchronous service.  This function blocks
 * until the entire update procedure completes, including the additional
 * queries when necessary.
 *
 * dns_client_startupdate() is an asynchronous version of dns_client_update().
 * It immediately returns (typically with *transp being set to a non-NULL
 * pointer), and performs the update procedure through a set of internal
 * events.  All transactions including the additional query exchanges are
 * performed as a separate event, so none of these events cause blocking
 * operation.  When the update procedure completes, the specified function
 * 'action' will be called with the argument of a 'dns_clientupdateevent_t'
 * structure.  On return, '*transp' is set to an opaque transaction ID so that
 * the caller can cancel this update process.
 *
 * Notes:
 *\li	No options are currently defined.
 *
 * Requires:
 *
 *\li	'client' is a valid client.
 *
 *\li	'updates' != NULL.
 *
 *\li	'task' is a valid task.
 *
 *\li	'transp' != NULL && *transp == NULL;
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

void
dns_client_cancelupdate(dns_clientupdatetrans_t *trans);
/*%<
 * Cancel an ongoing dynamic update procedure started via
 * dns_client_startupdate().
 *
 * Notes:
 *
 *\li	If the update procedure has not completed, post its UPDATEDONE
 *	event with a result code of #ISC_R_CANCELED.
 *
 * Requires:
 *
 *\li	'trans' is a valid transaction ID.
 */

void
dns_client_destroyupdatetrans(dns_clientupdatetrans_t **transp);
/*%<
 * Destroy dynamic update transaction identified by '*transp'.
 *
 * Requires:
 *
 *\li	'*transp' is a valid transaction ID.
 *
 *\li	The caller has received the UPDATEDONE event (either because the
 *	update completed or because dns_client_cancelupdate() was called).
 *
 * Ensures:
 *
 *\li	*transp == NULL.
 */

isc_result_t
dns_client_updaterec(dns_client_updateop_t op, dns_name_t *owner,
		     dns_rdatatype_t type, dns_rdata_t *source,
		     dns_ttl_t ttl, dns_name_t *target,
		     dns_rdataset_t *rdataset, dns_rdatalist_t *rdatalist,
		     dns_rdata_t *rdata, isc_mem_t *mctx);
/*%<
 * TBD
 */

void
dns_client_freeupdate(dns_name_t **namep);
/*%<
 * TBD
 */

isc_mem_t *
dns_client_mctx(dns_client_t *client);

ISC_LANG_ENDDECLS

#endif /* DNS_CLIENT_H */
