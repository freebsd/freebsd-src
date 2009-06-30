/*-
 * Copyright (c) 2007-2009
 *	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009 Lawrence Stewart <lstewart@freebsd.org>
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
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/cc.h>
#include <netinet/cc_module.h>
#include <netinet/vinet.h>

/*
 * List of available cc algorithms on the current system. First element
 * is used as the system default CC algorithm.
 */
struct cc_head cc_list = STAILQ_HEAD_INITIALIZER(cc_list); 

/* Protects the cc_list TAILQ */
struct rwlock cc_list_lock;

/*
 * Set the default CC algorithm to new_default. The default is identified
 * by being the first element in the cc_list TAILQ.
 */
static void
cc_set_default(struct cc_algo *new_default)
{
	CC_LIST_WLOCK_ASSERT();

	/*
	 * Make the requested system default CC
	 * algorithm the first element in the list
	 * if it isn't already
	 */
	if (new_default != CC_DEFAULT()) {
		STAILQ_REMOVE(&cc_list, new_default, cc_algo, entries);
		STAILQ_INSERT_HEAD(&cc_list, new_default, entries);
	}
}

/*
 * Sysctl handler to show and change the default CC algorithm.
 */
static int
cc_default_algo(SYSCTL_HANDLER_ARGS)
{
	struct cc_algo *funcs;
	int error = 0, found = 0;

	if (req->newptr == NULL) {
		/* Just print the current default. */
		char default_cc[TCP_CA_NAME_MAX];
		CC_LIST_RLOCK();
		strlcpy(default_cc, CC_DEFAULT()->name, TCP_CA_NAME_MAX);
		CC_LIST_RUNLOCK();
		error = sysctl_handle_string(oidp, default_cc, 1, req);
	} else {
		/* Find algo with specified name and set it to default. */
		CC_LIST_WLOCK();
		STAILQ_FOREACH(funcs, &cc_list, entries) {
			if (strncmp((char *)req->newptr, funcs->name, TCP_CA_NAME_MAX) == 0) {
				found = 1;
				cc_set_default(funcs);
			}
		}
		CC_LIST_WUNLOCK();

		if (!found)
			return (ESRCH);

		error = sysctl_handle_string(oidp, arg1, arg2, req);
	}

	return (error);
}

/*
 * Sysctl handler to display the list of available CC algorithms.
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
	return (error);
}

/*
 * Initialise CC subsystem on system boot.
 */
void 
cc_init()
{
	CC_LIST_LOCK_INIT();
	STAILQ_INIT(&cc_list);
	/* Newreno must always be available as an algorithm. */
	cc_register_algo(&newreno_cc_algo);
}

/*
 * Returns non-zero on success, 0 on failure.
 */
int
cc_deregister_algo(struct cc_algo *remove_cc)
{
	struct cc_algo *funcs, *tmpfuncs;
	struct tcpcb *tp = NULL;
	struct inpcb *inp = NULL;
	int error = EPERM;

	/* Never allow newreno to be deregistered. */
	if (&newreno_cc_algo == remove_cc)
		return error;

	/* Remove algo from cc_list so that new connections can't use it. */
	CC_LIST_WLOCK();
	STAILQ_FOREACH_SAFE(funcs, &cc_list, entries, tmpfuncs) {
		if (funcs == remove_cc) {
			/*
			 * If we're removing the current system default,
			 * reset the default to newreno.
			 */
			if (strncmp(CC_DEFAULT()->name,
			    remove_cc->name,
			    TCP_CA_NAME_MAX) == 0)
				cc_set_default(&newreno_cc_algo);

			STAILQ_REMOVE(&cc_list, funcs, cc_algo, entries);
			error = 0;
			break;
		}
	}
	CC_LIST_WUNLOCK();

	if (!error) {
		/*
		 * Check all active control blocks and change any that are
		 * using this algorithm back to newreno. If the algorithm that
		 * was in use requires cleanup code to be run, call it.
		 */
		INP_INFO_RLOCK(&V_tcbinfo);
		LIST_FOREACH(inp, &V_tcb, inp_list) {
			INP_WLOCK(inp);
			/* Important to skip tcptw structs. */
			if (!(inp->inp_flags & INP_TIMEWAIT) &&
			    (tp = intotcpcb(inp)) != NULL) {
				/*
				 * By holding INP_WLOCK here, we are
				 * assured that the connection is not
				 * currently executing inside the CC
				 * module's functions i.e. it is safe to
				 * make the switch back to newreno.
				 */
				if (CC_ALGO(tp) == remove_cc) {
					tmpfuncs = CC_ALGO(tp);
					/* Newreno does not require any init. */
					CC_ALGO(tp) = &newreno_cc_algo;
					if (tmpfuncs->cb_destroy != NULL)
						tmpfuncs->cb_destroy(tp);
				}
			}
			INP_WUNLOCK(inp);
		}
		INP_INFO_RUNLOCK(&V_tcbinfo);
	}

	return (error);
}

/*
 * Returns 0 on success, non-zero on failure.
 */
int
cc_register_algo(struct cc_algo *add_cc)
{
	struct cc_algo *funcs;
	int error = 0;

	/*
	 * Iterate over list of registered CC algorithms and make sure
	 * we're not trying to add a duplicate.
	 */
	CC_LIST_WLOCK();
	STAILQ_FOREACH(funcs, &cc_list, entries) {
		if (funcs == add_cc ||
		    strncmp(funcs->name, add_cc->name, TCP_CA_NAME_MAX) == 0)
			error = EEXIST;
	}

	if (!error)
		STAILQ_INSERT_TAIL(&cc_list, add_cc, entries);

	CC_LIST_WUNLOCK();

	return (error);
}

/*
 * Handles kld related events. Returns 0 on success, non-zero on failure.
 */
int
cc_modevent(module_t mod, int event_type, void *data)
{
	int error = 0;
	struct cc_algo *algo = (struct cc_algo *)data;

	switch(event_type) {
		case MOD_LOAD:
			if (algo->mod_init != NULL)
				error = algo->mod_init();
			if (!error)
				error = cc_register_algo(algo);
			break;

		case MOD_QUIESCE:
			error = cc_deregister_algo(algo);
			if (!error && algo->mod_destroy != NULL)
				algo->mod_destroy();
			break;

		case MOD_SHUTDOWN:
		case MOD_UNLOAD:
			break;

		default:
			return EINVAL;
			break;
	}

	return (error);
}

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, cc, CTLFLAG_RW, NULL,
	"congestion control related settings");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, algorithm, CTLTYPE_STRING|CTLFLAG_RW,
	NULL, 0, cc_default_algo, "A",
	"default congestion control algorithm");

SYSCTL_PROC(_net_inet_tcp_cc, OID_AUTO, available, CTLTYPE_STRING|CTLFLAG_RD,
	NULL, 0, cc_list_available, "A",
	"list available congestion control algorithms");
