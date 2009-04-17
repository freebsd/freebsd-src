/*-
 * Copyright (c) 2007-2009
 *	Swinburne University of Technology, Melbourne, Australia
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart and James Healy,
 * made possible in part by a grant from the Cisco University Research Program
 * Fund at Community Foundation Silicon Valley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <netinet/cc.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>

/* list of available cc algorithms on the current system */
struct cc_head cc_list = STAILQ_HEAD_INITIALIZER(cc_list); 

struct rwlock cc_list_lock;

/* the system wide default cc algorithm */
char cc_algorithm[TCP_CA_NAME_MAX];

/*
 * sysctl handler that allows the default cc algorithm for the system to be
 * viewed and changed
 */
static int
cc_default_algorithm(SYSCTL_HANDLER_ARGS)
{
	struct cc_algo *funcs;

	if (req->newptr == NULL)
		goto skip;

	CC_LIST_RLOCK();
	STAILQ_FOREACH(funcs, &cc_list, entries) {
		if (strncmp((char *)req->newptr, funcs->name, TCP_CA_NAME_MAX) == 0)
			goto reorder;
	}
	CC_LIST_RUNLOCK();

	return 1;

reorder:
	/*
	 * Make the selected system default cc algorithm
	 * the first element in the list if it isn't already
	 */
	CC_LIST_RUNLOCK();
	CC_LIST_WLOCK();
	if (funcs != STAILQ_FIRST(&cc_list)) {
		STAILQ_REMOVE(&cc_list, funcs, cc_algo, entries);
		STAILQ_INSERT_HEAD(&cc_list, funcs, entries);
	}
	CC_LIST_WUNLOCK();

skip:
	return sysctl_handle_string(oidp, arg1, arg2, req);
}

/*
 * sysctl handler that displays the available cc algorithms as a read
 * only value
 */
static int
cc_list_available(SYSCTL_HANDLER_ARGS)
{
	struct cc_algo *algo;
	int error = 0, first = 1;
	struct sbuf *s = NULL;

	if ((s = sbuf_new(NULL, NULL, TCP_CA_NAME_MAX, SBUF_AUTOEXTEND)) == NULL)
		return -1;

	CC_LIST_RLOCK();
	STAILQ_FOREACH(algo, &cc_list, entries) {
		error = sbuf_printf(s, (first) ? "%s" : ", %s", algo->name);
		if (error != 0)
			break;
		first = 0;
	}
	CC_LIST_RUNLOCK();

	if (!error) {
		sbuf_finish(s);
		error = sysctl_handle_string(oidp, sbuf_data(s), 1, req);
	}

	sbuf_delete(s);
	return error;
}

/*
 * Initialise cc on system boot
 */
void 
cc_init()
{
	/* initialise the lock that will protect read/write access to our linked list */
	CC_LIST_LOCK_INIT();

	/* initilize list of cc algorithms */
	STAILQ_INIT(&cc_list);

	/* add newreno to the list of available algorithms */
	cc_register_algorithm(&newreno_cc_algo);

	/* set newreno to the system default */
	strlcpy(cc_algorithm, newreno_cc_algo.name, TCP_CA_NAME_MAX);
}

/*
 * Returns 1 on success, 0 on failure
 */
int
cc_deregister_algorithm(struct cc_algo *remove_cc)
{
	struct cc_algo *funcs, *tmpfuncs;
	register struct tcpcb *tp = NULL;
	register struct inpcb *inp = NULL;
	int success = 0;

	/* remove the algorithm from the list available to the system */
	CC_LIST_RLOCK();
	STAILQ_FOREACH_SAFE(funcs, &cc_list, entries, tmpfuncs) {
		if (funcs == remove_cc) {
			if (CC_LIST_TRY_WLOCK()) {
				/* if this algorithm is the system default, reset the default to newreno */
				if (strncmp(cc_algorithm, remove_cc->name, TCP_CA_NAME_MAX) == 0)
					snprintf(cc_algorithm, TCP_CA_NAME_MAX, "%s", newreno_cc_algo.name);

				STAILQ_REMOVE(&cc_list, funcs, cc_algo, entries);
				success = 1;
				CC_LIST_W2RLOCK();
			}
			break;
		}
	}
	CC_LIST_RUNLOCK();

	if (success) {
		/*
		 * check all active control blocks and change any that are using this
		 * algorithm back to newreno. If the algorithm that was in use requires
		 * deinit code to be run, call it
		 */
		INP_INFO_RLOCK(&tcbinfo);
		LIST_FOREACH(inp, &tcb, inp_list) {
			/* skip tcptw structs */
			if (inp->inp_flags & INP_TIMEWAIT)
				continue;
			INP_WLOCK(inp);
			if ((tp = intotcpcb(inp)) != NULL) {
				if (strncmp(CC_ALGO(tp)->name, remove_cc->name, TCP_CA_NAME_MAX) == 0 ) {
					tmpfuncs = CC_ALGO(tp);
					CC_ALGO(tp) = &newreno_cc_algo;
					/*
					 * XXX: We should stall here until
					 * we're sure the tcb has stopped
					 * using the deregistered algo's functions...
					 * Not sure how to do that yet!
					 */
					if(CC_ALGO(tp)->init != NULL)
						CC_ALGO(tp)->init(tp);
					if (tmpfuncs->deinit != NULL)
						tmpfuncs->deinit(tp);
				}
			}
			INP_WUNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&tcbinfo);
	}

	return success;
}

int
cc_register_algorithm(struct cc_algo *add_cc)
{
	CC_LIST_WLOCK();
	STAILQ_INSERT_TAIL(&cc_list, add_cc, entries);
	CC_LIST_WUNLOCK();
	return 1;
}

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, cc, CTLFLAG_RW, NULL,
	"congestion control related settings");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, algorithm, CTLTYPE_STRING|CTLFLAG_RW,
	&cc_algorithm, sizeof(cc_algorithm), cc_default_algorithm, "A",
	"default congestion control algorithm");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, available, CTLTYPE_STRING|CTLFLAG_RD,
	NULL, 0, cc_list_available, "A",
	"list available congestion control algorithms");

