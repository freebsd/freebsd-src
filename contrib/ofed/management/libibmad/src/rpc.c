/*
 * Copyright (c) 2004-2006 Voltaire Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#include <infiniband/umad.h>
#include "mad.h"

#define MAX_CLASS 256

struct ibmad_port {
	int port_id;  /* file descriptor returned by umad_open() */
	int class_agents[MAX_CLASS]; /* class2agent mapper */
};

int ibdebug;

static int mad_portid = -1;
static int iberrs;

static int madrpc_retries = MAD_DEF_RETRIES;
static int def_madrpc_timeout = MAD_DEF_TIMEOUT_MS;
static void *save_mad;
static int save_mad_len = 256;

#undef DEBUG
#define DEBUG	if (ibdebug)	IBWARN
#define ERRS	if (iberrs || ibdebug)	IBWARN

#define MAD_TID(mad)	(*((uint64_t *)((char *)(mad) + 8)))

void
madrpc_show_errors(int set)
{
	iberrs = set;
}

void
madrpc_save_mad(void *madbuf, int len)
{
	save_mad = madbuf;
	save_mad_len = len;
}

int
madrpc_set_retries(int retries)
{
	if (retries > 0)
		madrpc_retries = retries;
	return madrpc_retries;
}

int
madrpc_set_timeout(int timeout)
{
	def_madrpc_timeout = timeout;
	return 0;
}

int
madrpc_def_timeout(void)
{
	return def_madrpc_timeout;
}

int
madrpc_portid(void)
{
	return mad_portid;
}

static int
_do_madrpc(int port_id, void *sndbuf, void *rcvbuf, int agentid, int len,
	   int timeout)
{
	uint32_t trid; /* only low 32 bits */
	int retries;
	int length, status;

	if (!timeout)
		timeout = def_madrpc_timeout;

	if (ibdebug > 1) {
		IBWARN(">>> sending: len %d pktsz %zu", len, umad_size() + len);
		xdump(stderr, "send buf\n", sndbuf, umad_size() + len);
	}

	if (save_mad) {
		memcpy(save_mad, umad_get_mad(sndbuf),
		       save_mad_len < len ? save_mad_len : len);
		save_mad = 0;
	}

	trid = mad_get_field64(umad_get_mad(sndbuf), 0, IB_MAD_TRID_F);

	for (retries = 0; retries < madrpc_retries; retries++) {
		if (retries) {
			ERRS("retry %d (timeout %d ms)", retries, timeout);
		}

		length = len;
		if (umad_send(port_id, agentid, sndbuf, length, timeout, 0) < 0) {
			IBWARN("send failed; %s", strerror(errno));
			return -1;
		}

		/* Use same timeout on receive side just in case */
		/* send packet is lost somewhere. */
		do {
			if (umad_recv(port_id, rcvbuf, &length, timeout) < 0) {
				IBWARN("recv failed: %s", strerror(errno));
				return -1;
			}

			if (ibdebug > 1) {
				IBWARN("rcv buf:");
				xdump(stderr, "rcv buf\n", umad_get_mad(rcvbuf), IB_MAD_SIZE);
			}
		} while ((uint32_t)mad_get_field64(umad_get_mad(rcvbuf), 0, IB_MAD_TRID_F) != trid);

		status = umad_status(rcvbuf);
		if (!status)
			return length;		/* done */
		if (status == ENOMEM)
			return length;
	}

	ERRS("timeout after %d retries, %d ms", retries, timeout * retries);
	return -1;
}

void *
mad_rpc(const void *port_id, ib_rpc_t *rpc, ib_portid_t *dport, void *payload,
	void *rcvdata)
{
	const struct ibmad_port *p = port_id;
	int status, len;
	uint8_t sndbuf[1024], rcvbuf[1024], *mad;

	len = 0;
	memset(sndbuf, 0, umad_size() + IB_MAD_SIZE);

	if ((len = mad_build_pkt(sndbuf, rpc, dport, 0, payload)) < 0)
		return 0;

	if ((len = _do_madrpc(p->port_id, sndbuf, rcvbuf,
			      p->class_agents[rpc->mgtclass],
			      len, rpc->timeout)) < 0) {
		IBWARN("_do_madrpc failed; dport (%s)", portid2str(dport));
		return 0;
	}

	mad = umad_get_mad(rcvbuf);

	if ((status = mad_get_field(mad, 0, IB_DRSMP_STATUS_F)) != 0) {
		ERRS("MAD completed with error status 0x%x; dport (%s)",
			status, portid2str(dport));
		return 0;
	}

	if (ibdebug) {
		IBWARN("data offs %d sz %d", rpc->dataoffs, rpc->datasz);
		xdump(stderr, "mad data\n", mad + rpc->dataoffs, rpc->datasz);
	}

	if (rcvdata)
		memcpy(rcvdata, mad + rpc->dataoffs, rpc->datasz);

	return rcvdata;
}

void *
mad_rpc_rmpp(const void *port_id, ib_rpc_t *rpc, ib_portid_t *dport,
	     ib_rmpp_hdr_t *rmpp, void *data)
{
	const struct ibmad_port *p = port_id;
	int status, len;
	uint8_t sndbuf[1024], rcvbuf[1024], *mad;

	memset(sndbuf, 0, umad_size() + IB_MAD_SIZE);

	DEBUG("rmpp %p data %p", rmpp, data);

	if ((len = mad_build_pkt(sndbuf, rpc, dport, rmpp, data)) < 0)
		return 0;

	if ((len = _do_madrpc(p->port_id, sndbuf, rcvbuf,
			      p->class_agents[rpc->mgtclass],
			      len, rpc->timeout)) < 0) {
		IBWARN("_do_madrpc failed; dport (%s)", portid2str(dport));
		return 0;
	}

	mad = umad_get_mad(rcvbuf);

	if ((status = mad_get_field(mad, 0, IB_MAD_STATUS_F)) != 0) {
		ERRS("MAD completed with error status 0x%x; dport (%s)",
			status, portid2str(dport));
		return 0;
	}

	if (ibdebug) {
		IBWARN("data offs %d sz %d", rpc->dataoffs, rpc->datasz);
		xdump(stderr, "rmpp mad data\n", mad + rpc->dataoffs,
		      rpc->datasz);
	}

	if (rmpp) {
		rmpp->flags = mad_get_field(mad, 0, IB_SA_RMPP_FLAGS_F);
		if ((rmpp->flags & 0x3) &&
		    mad_get_field(mad, 0, IB_SA_RMPP_VERS_F) != 1) {
			IBWARN("bad rmpp version");
			return 0;
		}
		rmpp->type = mad_get_field(mad, 0, IB_SA_RMPP_TYPE_F);
		rmpp->status = mad_get_field(mad, 0, IB_SA_RMPP_STATUS_F);
		DEBUG("rmpp type %d status %d", rmpp->type, rmpp->status);
		rmpp->d1.u = mad_get_field(mad, 0, IB_SA_RMPP_D1_F);
		rmpp->d2.u = mad_get_field(mad, 0, IB_SA_RMPP_D2_F);
	}

	if (data)
		memcpy(data, mad + rpc->dataoffs, rpc->datasz);

	rpc->recsz = mad_get_field(mad, 0, IB_SA_ATTROFFS_F);

	return data;
}

void *
madrpc(ib_rpc_t *rpc, ib_portid_t *dport, void *payload, void *rcvdata)
{
	struct ibmad_port port;

	port.port_id = mad_portid;
	port.class_agents[rpc->mgtclass] = mad_class_agent(rpc->mgtclass);
	return mad_rpc(&port, rpc, dport, payload, rcvdata);
}

void *
madrpc_rmpp(ib_rpc_t *rpc, ib_portid_t *dport, ib_rmpp_hdr_t *rmpp, void *data)
{
	struct ibmad_port port;

	port.port_id = mad_portid;
	port.class_agents[rpc->mgtclass] = mad_class_agent(rpc->mgtclass);
	return mad_rpc_rmpp(&port, rpc, dport, rmpp, data);
}

static pthread_mutex_t rpclock = PTHREAD_MUTEX_INITIALIZER;

void
madrpc_lock(void)
{
	pthread_mutex_lock(&rpclock);
}

void
madrpc_unlock(void)
{
	pthread_mutex_unlock(&rpclock);
}

void
madrpc_init(char *dev_name, int dev_port, int *mgmt_classes, int num_classes)
{
	if (umad_init() < 0)
		IBPANIC("can't init UMAD library");

	if ((mad_portid = umad_open_port(dev_name, dev_port)) < 0)
		IBPANIC("can't open UMAD port (%s:%d)", dev_name, dev_port);

	if (num_classes >= MAX_CLASS)
		IBPANIC("too many classes %d requested", num_classes);

	while (num_classes--) {
		int rmpp_version = 0;
		int mgmt = *mgmt_classes++;

		if (mgmt == IB_SA_CLASS)
			rmpp_version = 1;
		if (mad_register_client(mgmt, rmpp_version) < 0)
			IBPANIC("client_register for mgmt class %d failed", mgmt);
	}
}

void *
mad_rpc_open_port(char *dev_name, int dev_port,
		  int *mgmt_classes, int num_classes)
{
	struct ibmad_port *p;
	int port_id;

	if (num_classes >= MAX_CLASS) {
		IBWARN("too many classes %d requested", num_classes);
		errno = EINVAL;
		return NULL;
	}

	if (umad_init() < 0) {
		IBWARN("can't init UMAD library");
		errno = ENODEV;
		return NULL;
	}

	p = malloc(sizeof(*p));
	if (!p) {
		errno = ENOMEM;
		return NULL;
	}
	memset(p, 0, sizeof(*p));

	if ((port_id = umad_open_port(dev_name, dev_port)) < 0) {
		IBWARN("can't open UMAD port (%s:%d)", dev_name, dev_port);
		if (!errno)
			errno = EIO;
		free(p);
		return NULL;
	}

	while (num_classes--) {
		int rmpp_version = 0;
		int mgmt = *mgmt_classes++;
		int agent;

		if (mgmt == IB_SA_CLASS)
			rmpp_version = 1;
		if (mgmt < 0 || mgmt >= MAX_CLASS ||
		    (agent = mad_register_port_client(port_id, mgmt,
						      rmpp_version)) < 0) {
			IBWARN("client_register for mgmt %d failed", mgmt);
			if(!errno)
				errno = EINVAL;
			umad_close_port(port_id);
  			free(p);
  			return NULL;
		}
		p->class_agents[mgmt] = agent;
	}

	p->port_id = port_id;
	return p;
}

void
mad_rpc_close_port(void *port_id)
{
	struct ibmad_port *p = port_id;

	umad_close_port(p->port_id);
	free(p);
}

uint8_t *
sa_call(void *rcvbuf, ib_portid_t *portid, ib_sa_call_t *sa, unsigned timeout)
{
	struct ibmad_port port;

	port.port_id = mad_portid;
	port.class_agents[IB_SA_CLASS] = mad_class_agent(IB_SA_CLASS);
	return sa_rpc_call(&port, rcvbuf, portid, sa, timeout);
}
