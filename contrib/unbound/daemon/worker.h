/*
 * daemon/worker.h - worker that handles a pending list of requests.
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
 * This file describes the worker structure that holds a list of 
 * pending requests and handles them.
 */

#ifndef DAEMON_WORKER_H
#define DAEMON_WORKER_H

#include "util/netevent.h"
#include "util/locks.h"
#include "util/alloc.h"
#include "util/data/msgreply.h"
#include "util/data/msgparse.h"
#include "daemon/stats.h"
#include "util/module.h"
struct listen_dnsport;
struct outside_network;
struct config_file;
struct daemon;
struct listen_port;
struct ub_randstate;
struct regional;
struct tube;
struct daemon_remote;

/** worker commands */
enum worker_commands {
	/** make the worker quit */
	worker_cmd_quit,
	/** obtain statistics */
	worker_cmd_stats,
	/** obtain statistics without statsclear */
	worker_cmd_stats_noreset,
	/** execute remote control command */
	worker_cmd_remote
};

/**
 * Structure holding working information for unbound.
 * Holds globally visible information.
 */
struct worker {
	/** the thread number (in daemon array). First in struct for debug. */
	int thread_num;
	/** global shared daemon structure */
	struct daemon* daemon;
	/** thread id */
	ub_thread_t thr_id;
	/** pipe, for commands for this worker */
	struct tube* cmd;
	/** the event base this worker works with */
	struct comm_base* base;
	/** the frontside listening interface where request events come in */
	struct listen_dnsport* front;
	/** the backside outside network interface to the auth servers */
	struct outside_network* back;
	/** ports to be used by this worker. */
	int* ports;
	/** number of ports for this worker */
	int numports;
	/** the signal handler */
	struct comm_signal* comsig;
	/** commpoint to listen to commands. */
	struct comm_point* cmd_com;
	/** timer for statistics */
	struct comm_timer* stat_timer;

	/** random() table for this worker. */
	struct ub_randstate* rndstate;
	/** do we need to restart or quit (on signal) */
	int need_to_exit;
	/** allocation cache for this thread */
	struct alloc_cache alloc;
	/** per thread statistics */
	struct server_stats stats;
	/** thread scratch regional */
	struct regional* scratchpad;

	/** module environment passed to modules, changed for this thread */
	struct module_env env;
};

/**
 * Create the worker structure. Bare bones version, zeroed struct,
 * with backpointers only. Use worker_init on it later.
 * @param daemon: the daemon that this worker thread is part of.
 * @param id: the thread number from 0.. numthreads-1.
 * @param ports: the ports it is allowed to use, array.
 * @param n: the number of ports.
 * @return: the new worker or NULL on alloc failure.
 */
struct worker* worker_create(struct daemon* daemon, int id, int* ports, int n);

/**
 * Initialize worker.
 * Allocates event base, listens to ports
 * @param worker: worker to initialize, created with worker_create.
 * @param cfg: configuration settings.
 * @param ports: list of shared query ports.
 * @param do_sigs: if true, worker installs signal handlers.
 * @return: false on error.
 */
int worker_init(struct worker* worker, struct config_file *cfg, 
	struct listen_port* ports, int do_sigs);

/**
 * Make worker work.
 */
void worker_work(struct worker* worker);

/**
 * Delete worker.
 */
void worker_delete(struct worker* worker);

/**
 * Send a command to a worker. Uses blocking writes.
 * @param worker: worker to send command to.
 * @param cmd: command to send.
 */
void worker_send_cmd(struct worker* worker, enum worker_commands cmd);

/**
 * Worker signal handler function. User argument is the worker itself.
 * @param sig: signal number.
 * @param arg: the worker (main worker) that handles signals.
 */
void worker_sighandler(int sig, void* arg);

/**
 * Worker service routine to send serviced queries to authoritative servers.
 * @param qname: query name. (host order)
 * @param qnamelen: length in bytes of qname, including trailing 0.
 * @param qtype: query type. (host order)
 * @param qclass: query class. (host order)
 * @param flags: host order flags word, with opcode and CD bit.
 * @param dnssec: if set, EDNS record will have DO bit set.
 * @param want_dnssec: signatures needed.
 * @param addr: where to.
 * @param addrlen: length of addr.
 * @param zone: wireformat dname of the zone.
 * @param zonelen: length of zone name.
 * @param q: wich query state to reactivate upon return.
 * @return: false on failure (memory or socket related). no query was
 *      sent.
 */
struct outbound_entry* worker_send_query(uint8_t* qname, size_t qnamelen, 
	uint16_t qtype, uint16_t qclass, uint16_t flags, int dnssec, 
	int want_dnssec, struct sockaddr_storage* addr, socklen_t addrlen,
	uint8_t* zone, size_t zonelen, struct module_qstate* q);

/** 
 * process control messages from the main thread. Frees the control 
 * command message.
 * @param tube: tube control message came on.
 * @param msg: message contents.  Is freed.
 * @param len: length of message.
 * @param error: if error (NETEVENT_*) happened.
 * @param arg: user argument
 */
void worker_handle_control_cmd(struct tube* tube, uint8_t* msg, size_t len,
	int error, void* arg);

/** handles callbacks from listening event interface */
int worker_handle_request(struct comm_point* c, void* arg, int error,
	struct comm_reply* repinfo);

/** process incoming replies from the network */
int worker_handle_reply(struct comm_point* c, void* arg, int error, 
	struct comm_reply* reply_info);

/** process incoming serviced query replies from the network */
int worker_handle_service_reply(struct comm_point* c, void* arg, int error, 
	struct comm_reply* reply_info);

/** cleanup the cache to remove all rrset IDs from it, arg is worker */
void worker_alloc_cleanup(void* arg);

/**
 * Init worker stats - includes server_stats_init, outside network and mesh.
 * @param worker: the worker to init
 */
void worker_stats_clear(struct worker* worker);

/** statistics timer callback handler */
void worker_stat_timer_cb(void* arg);

/** probe timer callback handler */
void worker_probe_timer_cb(void* arg);

/** start accept callback handler */
void worker_start_accept(void* arg);

/** stop accept callback handler */
void worker_stop_accept(void* arg);

#endif /* DAEMON_WORKER_H */
