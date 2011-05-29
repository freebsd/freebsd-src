/*
 * Copyright (c) 2004,2005 Voltaire Inc.  All rights reserved.
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

#undef DEBUG
#define DEBUG	if (ibdebug)	IBWARN

#define MAX_CLASS	256
#define MAX_AGENTS	256

static int class_agent[MAX_CLASS];
static int agent_class[MAX_AGENTS];

static int
register_agent(int agent, int mclass)
{
	static int initialized;

	if (!initialized) {
		initialized++;
		memset(class_agent, 0xff, sizeof class_agent);
		memset(agent_class, 0xff, sizeof agent_class);
	}

	if (mclass < 0 || mclass >= MAX_CLASS ||
	    agent < 0 || agent >= MAX_AGENTS) {
		DEBUG("bad mgmt class %d or agent %d", mclass, agent);
		return -1;
	}

	class_agent[mclass] = agent;
	agent_class[agent] = mclass;

	return 0;
}

static int
mgmt_class_vers(int mgmt_class)
{
	if ((mgmt_class >= IB_VENDOR_RANGE1_START_CLASS &&
	     mgmt_class <= IB_VENDOR_RANGE1_END_CLASS) ||
	    (mgmt_class >= IB_VENDOR_RANGE2_START_CLASS &&
	     mgmt_class <= IB_VENDOR_RANGE2_END_CLASS))
		return 1;

	switch(mgmt_class) {
		case IB_SMI_CLASS:
		case IB_SMI_DIRECT_CLASS:
			return 1;
		case IB_SA_CLASS:
			return 2;
		case IB_PERFORMANCE_CLASS:
			return 1;
		case IB_DEVICE_MGMT_CLASS:
			return 1;
		case IB_CC_CLASS:
			return 2;
	}

	return 0;
}

int
mad_class_agent(int mgmt)
{
	if (mgmt < 1 || mgmt > MAX_CLASS)
		return -1;
	return class_agent[mgmt];
}

int
mad_agent_class(int agent)
{
	if (agent < 1 || agent > MAX_AGENTS)
		return -1;
	return agent_class[agent];
}

int
mad_register_port_client(int port_id, int mgmt, uint8_t rmpp_version)
{
	int vers, agent;

	if ((vers = mgmt_class_vers(mgmt)) <= 0) {
		DEBUG("Unknown class %d mgmt_class", mgmt);
		return -1;
	}
	if ((agent = umad_register(port_id, mgmt,
				   vers, rmpp_version, 0)) < 0) {
		DEBUG("Can't register agent for class %d", mgmt);
		return -1;
	}

	if (mgmt < 0 || mgmt >= MAX_CLASS || agent >= MAX_AGENTS) {
		DEBUG("bad mgmt class %d or agent %d", mgmt, agent);
		return -1;
	}

	return agent;
}

int
mad_register_client(int mgmt, uint8_t rmpp_version)
{
	int agent;

	agent = mad_register_port_client(madrpc_portid(), mgmt, rmpp_version);
	if (agent < 0)
		return agent;

	return register_agent(agent, mgmt);
}

int
mad_register_server(int mgmt, uint8_t rmpp_version,
		    long method_mask[], uint32_t class_oui)
{
	long class_method_mask[16/sizeof(long)];
	uint8_t oui[3];
	int agent, vers, mad_portid;

	if (method_mask)
		memcpy(class_method_mask, method_mask, sizeof class_method_mask);
	else
		memset(class_method_mask, 0xff, sizeof(class_method_mask));

	if ((mad_portid = madrpc_portid()) < 0)
		return -1;

	if (class_agent[mgmt] >= 0) {
		DEBUG("Class 0x%x already registered", mgmt);
		return -1;
	}
	if ((vers = mgmt_class_vers(mgmt)) <= 0) {
		DEBUG("Unknown class 0x%x mgmt_class", mgmt);
		return -1;
	}
	if (mgmt >= IB_VENDOR_RANGE2_START_CLASS &&
	    mgmt <= IB_VENDOR_RANGE2_END_CLASS) {
		oui[0] = (class_oui >> 16) & 0xff;
		oui[1] = (class_oui >> 8) & 0xff;
		oui[2] = class_oui & 0xff;
		if ((agent = umad_register_oui(mad_portid, mgmt, rmpp_version,
					       oui, class_method_mask)) < 0) {
			DEBUG("Can't register agent for class %d", mgmt);
			return -1;
		}
	} else if ((agent = umad_register(mad_portid, mgmt, vers, rmpp_version,
					  class_method_mask)) < 0) {
		DEBUG("Can't register agent for class %d", mgmt);
		return -1;
	}

	if (register_agent(agent, mgmt) < 0)
		return -1;

	return agent;
}
