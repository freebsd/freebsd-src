/*
 * util/module.h - DNS handling module interface
 *
 * Copyright (c) 2007, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains the interface for DNS handling modules.
 */

#ifndef UTIL_MODULE_H
#define UTIL_MODULE_H
#include "util/storage/lruhash.h"
#include "util/data/msgreply.h"
#include "util/data/msgparse.h"
struct alloc_cache;
struct rrset_cache;
struct key_cache;
struct config_file;
struct slabhash;
struct query_info;
struct edns_data;
struct regional;
struct worker;
struct module_qstate;
struct ub_randstate;
struct mesh_area;
struct mesh_state;
struct val_anchors;
struct val_neg_cache;
struct iter_forwards;
struct iter_hints;

/** Maximum number of modules in operation */
#define MAX_MODULE 5

/**
 * Module environment.
 * Services and data provided to the module.
 */
struct module_env {
	/* --- data --- */
	/** config file with config options */
	struct config_file* cfg;
	/** shared message cache */
	struct slabhash* msg_cache;
	/** shared rrset cache */
	struct rrset_cache* rrset_cache;
	/** shared infrastructure cache (edns, lameness) */
	struct infra_cache* infra_cache;
	/** shared key cache */
	struct key_cache* key_cache;

	/* --- services --- */
	/** 
	 * Send serviced DNS query to server. UDP/TCP and EDNS is handled.
	 * operate() should return with wait_reply. Later on a callback 
	 * will cause operate() to be called with event timeout or reply.
	 * The time until a timeout is calculated from roundtrip timing,
	 * several UDP retries are attempted.
	 * @param qname: query name. (host order)
	 * @param qnamelen: length in bytes of qname, including trailing 0.
	 * @param qtype: query type. (host order)
	 * @param qclass: query class. (host order)
	 * @param flags: host order flags word, with opcode and CD bit.
	 * @param dnssec: if set, EDNS record will have bits set.
	 *	If EDNS_DO bit is set, DO bit is set in EDNS records.
	 *	If BIT_CD is set, CD bit is set in queries with EDNS records.
	 * @param want_dnssec: if set, the validator wants DNSSEC.  Without
	 * 	EDNS, the answer is likely to be useless for this domain.
	 * @param addr: where to.
	 * @param addrlen: length of addr.
	 * @param zone: delegation point name.
	 * @param zonelen: length of zone name.
	 * @param q: wich query state to reactivate upon return.
	 * @return: false on failure (memory or socket related). no query was
	 *	sent. Or returns an outbound entry with qsent and qstate set.
	 *	This outbound_entry will be used on later module invocations
	 *	that involve this query (timeout, error or reply).
	 */
	struct outbound_entry* (*send_query)(uint8_t* qname, size_t qnamelen, 
		uint16_t qtype, uint16_t qclass, uint16_t flags, int dnssec, 
		int want_dnssec, struct sockaddr_storage* addr, 
		socklen_t addrlen, uint8_t* zone, size_t zonelen,
		struct module_qstate* q);

	/**
	 * Detach-subqueries.
	 * Remove all sub-query references from this query state.
	 * Keeps super-references of those sub-queries correct.
	 * Updates stat items in mesh_area structure.
	 * @param qstate: used to find mesh state.
	 */
	void (*detach_subs)(struct module_qstate* qstate);

	/**
	 * Attach subquery.
	 * Creates it if it does not exist already.
	 * Keeps sub and super references correct.
	 * Updates stat items in mesh_area structure.
	 * Pass if it is priming query or not.
	 * return:
	 * o if error (malloc) happened.
	 * o need to initialise the new state (module init; it is a new state).
	 *   so that the next run of the query with this module is successful.
	 * o no init needed, attachment successful.
	 * 
	 * @param qstate: the state to find mesh state, and that wants to 
	 * 	receive the results from the new subquery.
	 * @param qinfo: what to query for (copied).
	 * @param qflags: what flags to use (RD, CD flag or not).
	 * @param prime: if it is a (stub) priming query.
	 * @param newq: If the new subquery needs initialisation, it is 
	 * 	returned, otherwise NULL is returned.
	 * @return: false on error, true if success (and init may be needed).
	 */ 
	int (*attach_sub)(struct module_qstate* qstate, 
		struct query_info* qinfo, uint16_t qflags, int prime, 
		struct module_qstate** newq);

	/**
	 * Kill newly attached sub. If attach_sub returns newq for 
	 * initialisation, but that fails, then this routine will cleanup and
	 * delete the fresly created sub.
	 * @param newq: the new subquery that is no longer needed.
	 * 	It is removed.
	 */
	void (*kill_sub)(struct module_qstate* newq);

	/**
	 * Detect if adding a dependency for qstate on name,type,class will
	 * create a dependency cycle.
	 * @param qstate: given mesh querystate.
	 * @param qinfo: query info for dependency. 
	 * @param flags: query flags of dependency, RD/CD flags.
	 * @param prime: if dependency is a priming query or not.
	 * @return true if the name,type,class exists and the given 
	 * 	qstate mesh exists as a dependency of that name. Thus 
	 * 	if qstate becomes dependent on name,type,class then a 
	 * 	cycle is created.
	 */
	int (*detect_cycle)(struct module_qstate* qstate, 
		struct query_info* qinfo, uint16_t flags, int prime);

	/** region for temporary usage. May be cleared after operate() call. */
	struct regional* scratch;
	/** buffer for temporary usage. May be cleared after operate() call. */
	ldns_buffer* scratch_buffer;
	/** internal data for daemon - worker thread. */
	struct worker* worker;
	/** mesh area with query state dependencies */
	struct mesh_area* mesh;
	/** allocation service */
	struct alloc_cache* alloc;
	/** random table to generate random numbers */
	struct ub_randstate* rnd;
	/** time in seconds, converted to integer */
	uint32_t* now;
	/** time in microseconds. Relatively recent. */
	struct timeval* now_tv;
	/** is validation required for messages, controls client-facing
	 * validation status (AD bits) and servfails */
	int need_to_validate;
	/** trusted key storage; these are the configured keys, if not NULL,
	 * otherwise configured by validator. These are the trust anchors,
	 * and are not primed and ready for validation, but on the bright
	 * side, they are read only memory, thus no locks and fast. */
	struct val_anchors* anchors;
	/** negative cache, configured by the validator. if not NULL,
	 * contains NSEC record lookup trees. */
	struct val_neg_cache* neg_cache;
	/** the 5011-probe timer (if any) */
	struct comm_timer* probe_timer;
	/** Mapping of forwarding zones to targets.
	 * iterator forwarder information. per-thread, created by worker */
	struct iter_forwards* fwds;
	/** 
	 * iterator forwarder information. per-thread, created by worker.
	 * The hints -- these aren't stored in the cache because they don't 
	 * expire. The hints are always used to "prime" the cache. Note 
	 * that both root hints and stub zone "hints" are stored in this 
	 * data structure. 
	 */
	struct iter_hints* hints;
	/** module specific data. indexed by module id. */
	void* modinfo[MAX_MODULE];
};

/**
 * External visible states of the module state machine 
 * Modules may also have an internal state.
 * Modules are supposed to run to completion or until blocked.
 */
enum module_ext_state {
	/** initial state - new query */
	module_state_initial = 0,
	/** waiting for reply to outgoing network query */
	module_wait_reply,
	/** module is waiting for another module */
	module_wait_module,
	/** module is waiting for another module; that other is restarted */
	module_restart_next,
	/** module is waiting for sub-query */
	module_wait_subquery,
	/** module could not finish the query */
	module_error,
	/** module is finished with query */
	module_finished
};

/**
 * Events that happen to modules, that start or wakeup modules.
 */
enum module_ev {
	/** new query */
	module_event_new = 0,
	/** query passed by other module */
	module_event_pass,
	/** reply inbound from server */
	module_event_reply,
	/** no reply, timeout or other error */
	module_event_noreply,
	/** reply is there, but capitalisation check failed */
	module_event_capsfail,
	/** next module is done, and its reply is awaiting you */
	module_event_moddone,
	/** error */
	module_event_error
};

/** 
 * Linked list of sockaddrs 
 * May be allocated such that only 'len' bytes of addr exist for the structure.
 */
struct sock_list {
	/** next in list */
	struct sock_list* next;
	/** length of addr */
	socklen_t len;
	/** sockaddr */
	struct sockaddr_storage addr;
};

/**
 * Module state, per query.
 */
struct module_qstate {
	/** which query is being answered: name, type, class */
	struct query_info qinfo;
	/** flags uint16 from query */
	uint16_t query_flags;
	/** if this is a (stub or root) priming query (with hints) */
	int is_priming;

	/** comm_reply contains server replies */
	struct comm_reply* reply;
	/** the reply message, with message for client and calling module */
	struct dns_msg* return_msg;
	/** the rcode, in case of error, instead of a reply message */
	int return_rcode;
	/** origin of the reply (can be NULL from cache, list for cnames) */
	struct sock_list* reply_origin;
	/** IP blacklist for queries */
	struct sock_list* blacklist;
	/** region for this query. Cleared when query process finishes. */
	struct regional* region;
	/** failure reason information if val-log-level is high */
	struct config_strlist* errinf;

	/** which module is executing */
	int curmod;
	/** module states */
	enum module_ext_state ext_state[MAX_MODULE];
	/** module specific data for query. indexed by module id. */
	void* minfo[MAX_MODULE];
	/** environment for this query */
	struct module_env* env;
	/** mesh related information for this query */
	struct mesh_state* mesh_info;
	/** how many seconds before expiry is this prefetched (0 if not) */
	uint32_t prefetch_leeway;
};

/** 
 * Module functionality block
 */
struct module_func_block {
	/** text string name of module */
	const char* name;

	/** 
	 * init the module. Called once for the global state.
	 * This is the place to apply settings from the config file.
	 * @param env: module environment.
	 * @param id: module id number.
	 * return: 0 on error
	 */
	int (*init)(struct module_env* env, int id);

	/**
	 * de-init, delete, the module. Called once for the global state.
	 * @param env: module environment.
	 * @param id: module id number.
	 */
	void (*deinit)(struct module_env* env, int id);

	/**
	 * accept a new query, or work further on existing query.
	 * Changes the qstate->ext_state to be correct on exit.
	 * @param ev: event that causes the module state machine to 
	 *	(re-)activate.
	 * @param qstate: the query state. 
	 *	Note that this method is not allowed to change the
	 *	query state 'identity', that is query info, qflags,
	 *	and priming status.
	 *	Attach a subquery to get results to a different query.
	 * @param id: module id number that operate() is called on. 
	 * @param outbound: if not NULL this event is due to the reply/timeout
	 *	or error on this outbound query.
	 * @return: if at exit the ext_state is:
	 *	o wait_module: next module is started. (with pass event).
	 *	o error or finished: previous module is resumed.
	 *	o otherwise it waits until that event happens (assumes
	 *	  the service routine to make subrequest or send message
	 *	  have been called.
	 */
	void (*operate)(struct module_qstate* qstate, enum module_ev event, 
		int id, struct outbound_entry* outbound);

	/**
	 * inform super querystate about the results from this subquerystate.
	 * Is called when the querystate is finished.  The method invoked is
	 * the one from the current module active in the super querystate.
	 * @param qstate: the query state that is finished.
	 *	Examine return_rcode and return_reply in the qstate.
	 * @param id: module id for this module.
	 *	This coincides with the current module for the super qstate.
	 * @param super: the super querystate that needs to be informed.
	 */
	void (*inform_super)(struct module_qstate* qstate, int id,
		struct module_qstate* super);

	/**
	 * clear module specific data
	 */
	void (*clear)(struct module_qstate* qstate, int id);

	/**
	 * How much memory is the module specific data using. 
	 * @param env: module environment.
	 * @param id: the module id.
	 * @return the number of bytes that are alloced.
	 */
	size_t (*get_mem)(struct module_env* env, int id);
};

/** 
 * Debug utility: module external qstate to string 
 * @param s: the state value.
 * @return descriptive string.
 */
const char* strextstate(enum module_ext_state s);

/** 
 * Debug utility: module event to string 
 * @param e: the module event value.
 * @return descriptive string.
 */
const char* strmodulevent(enum module_ev e);

#endif /* UTIL_MODULE_H */
