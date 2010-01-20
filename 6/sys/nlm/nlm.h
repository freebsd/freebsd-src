/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Authors: Doug Rabson <dfr@rabson.org>
 * Developed with Red Inc: Alfred Perlstein <alfred@freebsd.org>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NLM_NLM_H_
#define _NLM_NLM_H_

#ifdef _KERNEL

#ifdef _SYS_MALLOC_H_
MALLOC_DECLARE(M_NLM);
#endif

struct nlm_host;

/*
 * Copy a struct netobj.
 */ 
extern void nlm_copy_netobj(struct netobj *dst, struct netobj *src,
    struct malloc_type *type);

/*
 * Search for an existing NLM host that matches the given name
 * (typically the caller_name element of an nlm4_lock).  If none is
 * found, create a new host. If 'rqstp' is non-NULL, record the remote
 * address of the host so that we can call it back for async
 * responses.
 */
extern struct nlm_host *nlm_find_host_by_name(const char *name,
    struct svc_req *rqstp);

/*
 * Search for an existing NLM host that matches the given remote
 * address. If none is found, create a new host with the requested
 * address and remember 'vers' as the NLM protocol version to use for
 * that host.
 */
extern struct nlm_host *nlm_find_host_by_addr(const struct sockaddr *addr,
    int vers);

/*
 * Return an RPC client handle that can be used to talk to the NLM
 * running on the given host.
 */
extern CLIENT *nlm_host_get_rpc(struct nlm_host *host);

/*
 * Called when a host restarts.
 */
extern void nlm_sm_notify(nlm_sm_status *argp);

/*
 * Implementation for lock testing RPCs. Returns the NLM host that
 * matches the RPC arguments.
 */
extern struct nlm_host *nlm_do_test(nlm4_testargs *argp,
    nlm4_testres *result, struct svc_req *rqstp);

/*
 * Implementation for lock setting RPCs. Returns the NLM host that
 * matches the RPC arguments. If monitor is TRUE, set up an NSM
 * monitor for this host.
 */
extern struct nlm_host *nlm_do_lock(nlm4_lockargs *argp,
    nlm4_res *result, struct svc_req *rqstp, bool_t monitor); 

/*
 * Implementation for cancelling a pending lock request. Returns the
 * NLM host that matches the RPC arguments.
 */
extern struct nlm_host *nlm_do_cancel(nlm4_cancargs *argp,
    nlm4_res *result, struct svc_req *rqstp);

/*
 * Implementation for unlocking RPCs. Returns the NLM host that
 * matches the RPC arguments.
 */
extern struct nlm_host *nlm_do_unlock(nlm4_unlockargs *argp,
    nlm4_res *result, struct svc_req *rqstp);

/*
 * Free all locks associated with the hostname argp->name.
 */
extern void nlm_do_free_all(nlm4_notify *argp);

/*
 * Find an RPC transport that can be used to communicate with the
 * userland part of lockd.
 */
extern CLIENT *nlm_user_lockd(void);

#endif

#endif
