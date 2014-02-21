/*
 * Copyright (c) 2004-2007 Voltaire Inc.  All rights reserved.
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
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include <infiniband/umad.h>
#include "mad.h"

#undef DEBUG
#define DEBUG 	if (ibdebug)	IBWARN

static uint8_t *
pma_query_via(void *rcvbuf, ib_portid_t *dest, int port,
	      unsigned timeout, unsigned id, const void *srcport)
{
	ib_rpc_t rpc = {0};
	int lid = dest->lid;

	DEBUG("lid %d port %d", lid, port);

	if (lid == -1) {
		IBWARN("only lid routed is supported");
		return 0;
	}

	rpc.mgtclass = IB_PERFORMANCE_CLASS;
	rpc.method = IB_MAD_METHOD_GET;
	rpc.attr.id = id;

	/* Same for attribute IDs */
	mad_set_field(rcvbuf, 0, IB_PC_PORT_SELECT_F, port);
	rpc.attr.mod = 0;
	rpc.timeout = timeout;
	rpc.datasz = IB_PC_DATA_SZ;
	rpc.dataoffs = IB_PC_DATA_OFFS;

	dest->qp = 1;
	if (!dest->qkey)
		dest->qkey = IB_DEFAULT_QP1_QKEY;

	if (srcport) {
		return mad_rpc(srcport, &rpc, dest, rcvbuf, rcvbuf);
	} else {
		return madrpc(&rpc, dest, rcvbuf, rcvbuf);
	}
}

uint8_t *
pma_query(void *rcvbuf, ib_portid_t *dest, int port, unsigned timeout, unsigned id)
{
	return pma_query_via(rcvbuf, dest, port, timeout, id, NULL);
}

uint8_t *
perf_classportinfo_query_via(void *rcvbuf, ib_portid_t *dest, int port,
			     unsigned timeout, const void *srcport)
{
	return pma_query_via(rcvbuf, dest, port, timeout, CLASS_PORT_INFO,
			     srcport);
}

uint8_t *
perf_classportinfo_query(void *rcvbuf, ib_portid_t *dest, int port, unsigned timeout)
{
	return pma_query(rcvbuf, dest, port, timeout, CLASS_PORT_INFO);
}

uint8_t *
port_performance_query_via(void *rcvbuf, ib_portid_t *dest, int port,
			   unsigned timeout, const void *srcport)
{
	return pma_query_via(rcvbuf, dest, port, timeout,
			     IB_GSI_PORT_COUNTERS, srcport);
}

uint8_t *
port_performance_query(void *rcvbuf, ib_portid_t *dest, int port, unsigned timeout)
{
	return pma_query(rcvbuf, dest, port, timeout, IB_GSI_PORT_COUNTERS);
}

static uint8_t *
performance_reset_via(void *rcvbuf, ib_portid_t *dest, int port, unsigned mask,
		      unsigned timeout, unsigned id, const void *srcport)
{
	ib_rpc_t rpc = {0};
	int lid = dest->lid;

	DEBUG("lid %d port %d mask 0x%x", lid, port, mask);

	if (lid == -1) {
		IBWARN("only lid routed is supported");
		return 0;
	}

	if (!mask)
		mask = ~0;

	rpc.mgtclass = IB_PERFORMANCE_CLASS;
	rpc.method = IB_MAD_METHOD_SET;
	rpc.attr.id = id;

	memset(rcvbuf, 0, IB_MAD_SIZE);

	/* Same for attribute IDs */
	mad_set_field(rcvbuf, 0, IB_PC_PORT_SELECT_F, port);
	mad_set_field(rcvbuf, 0, IB_PC_COUNTER_SELECT_F, mask);
	rpc.attr.mod = 0;
	rpc.timeout = timeout;
	rpc.datasz = IB_PC_DATA_SZ;
	rpc.dataoffs = IB_PC_DATA_OFFS;
	dest->qp = 1;
	if (!dest->qkey)
		dest->qkey = IB_DEFAULT_QP1_QKEY;

	if (srcport) {
		return mad_rpc(srcport, &rpc, dest, rcvbuf, rcvbuf);
	} else {
		return madrpc(&rpc, dest, rcvbuf, rcvbuf);
	}
}

static uint8_t *
performance_reset(void *rcvbuf, ib_portid_t *dest, int port, unsigned mask,
		  unsigned timeout, unsigned id)
{
	return performance_reset_via(rcvbuf, dest, port, mask, timeout,
				     id, NULL);
}

uint8_t *
port_performance_reset_via(void *rcvbuf, ib_portid_t *dest, int port,
			   unsigned mask, unsigned timeout, const void *srcport)
{
	return performance_reset_via(rcvbuf, dest, port, mask, timeout,
				     IB_GSI_PORT_COUNTERS, srcport);
}

uint8_t *
port_performance_reset(void *rcvbuf, ib_portid_t *dest, int port, unsigned mask,
		       unsigned timeout)
{
	return performance_reset(rcvbuf, dest, port, mask, timeout, IB_GSI_PORT_COUNTERS);
}

uint8_t *
port_performance_ext_query_via(void *rcvbuf, ib_portid_t *dest, int port,
			       unsigned timeout, const void *srcport)
{
	return pma_query_via(rcvbuf, dest, port, timeout,
			     IB_GSI_PORT_COUNTERS_EXT, srcport);
}

uint8_t *
port_performance_ext_query(void *rcvbuf, ib_portid_t *dest, int port, unsigned timeout)
{
	return pma_query(rcvbuf, dest, port, timeout, IB_GSI_PORT_COUNTERS_EXT);
}

uint8_t *
port_performance_ext_reset_via(void *rcvbuf, ib_portid_t *dest, int port,
			       unsigned mask, unsigned timeout,
			       const void *srcport)
{
	return performance_reset_via(rcvbuf, dest, port, mask, timeout,
				     IB_GSI_PORT_COUNTERS_EXT, srcport);
}

uint8_t *
port_performance_ext_reset(void *rcvbuf, ib_portid_t *dest, int port, unsigned mask,
			   unsigned timeout)
{
	return performance_reset(rcvbuf, dest, port, mask, timeout, IB_GSI_PORT_COUNTERS_EXT);
}

uint8_t *
port_samples_control_query_via(void *rcvbuf, ib_portid_t *dest, int port,
			       unsigned timeout, const void *srcport)
{
	return pma_query_via(rcvbuf, dest, port, timeout,
			     IB_GSI_PORT_SAMPLES_CONTROL, srcport);
}

uint8_t *
port_samples_control_query(void *rcvbuf, ib_portid_t *dest, int port, unsigned timeout)
{
	return pma_query(rcvbuf, dest, port, timeout, IB_GSI_PORT_SAMPLES_CONTROL);
}

uint8_t *
port_samples_result_query_via(void *rcvbuf, ib_portid_t *dest, int port,
			      unsigned timeout, const void *srcport)
{
	return pma_query_via(rcvbuf, dest, port, timeout,
			     IB_GSI_PORT_SAMPLES_RESULT, srcport);
}

uint8_t *
port_samples_result_query(void *rcvbuf, ib_portid_t *dest, int port,  unsigned timeout)
{
	return pma_query(rcvbuf, dest, port, timeout, IB_GSI_PORT_SAMPLES_RESULT);
}
