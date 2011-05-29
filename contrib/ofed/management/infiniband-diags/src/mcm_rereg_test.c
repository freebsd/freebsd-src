/*
 * Copyright (c) 2006 Voltaire, Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <infiniband/umad.h>
#include <infiniband/mad.h>

#define info(fmt, arg...) fprintf(stderr, "INFO: " fmt, ##arg )
#define err(fmt, arg...) fprintf(stderr, "ERR: " fmt, ##arg )
#ifdef NOISY_DEBUG
#define dbg(fmt, arg...) fprintf(stderr, "DBG: " fmt, ##arg )
#else
#define dbg(fmt, arg...)
#endif

#define TMO 100

/* Multicast Member Record Component Masks */
#define IB_MCR_COMPMASK_MGID        (1ULL<<0)
#define IB_MCR_COMPMASK_PORT_GID    (1ULL<<1)
#define IB_MCR_COMPMASK_QKEY        (1ULL<<2)
#define IB_MCR_COMPMASK_MLID        (1ULL<<3)
#define IB_MCR_COMPMASK_MTU_SEL     (1ULL<<4)
#define IB_MCR_COMPMASK_MTU         (1ULL<<5)
#define IB_MCR_COMPMASK_TCLASS      (1ULL<<6)
#define IB_MCR_COMPMASK_PKEY        (1ULL<<7)
#define IB_MCR_COMPMASK_RATE_SEL    (1ULL<<8)
#define IB_MCR_COMPMASK_RATE        (1ULL<<9)
#define IB_MCR_COMPMASK_LIFE_SEL    (1ULL<<10)
#define IB_MCR_COMPMASK_LIFE        (1ULL<<11)
#define IB_MCR_COMPMASK_SL          (1ULL<<12)
#define IB_MCR_COMPMASK_FLOW        (1ULL<<13)
#define IB_MCR_COMPMASK_HOP         (1ULL<<14)
#define IB_MCR_COMPMASK_SCOPE       (1ULL<<15)
#define IB_MCR_COMPMASK_JOIN_STATE  (1ULL<<16)
#define IB_MCR_COMPMASK_PROXY       (1ULL<<17)

static ibmad_gid_t mgid_ipoib = {
	0xff, 0x12, 0x40, 0x1b, 0xff, 0xff, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
};

uint64_t build_mcm_rec(uint8_t *data, ibmad_gid_t mgid, ibmad_gid_t port_gid)
{
	memset(data, 0, IB_SA_DATA_SIZE);
	mad_set_array(data, 0, IB_SA_MCM_MGID_F, mgid);
	mad_set_array(data, 0, IB_SA_MCM_PORTGID_F, port_gid);
	mad_set_field(data, 0, IB_SA_MCM_JOIN_STATE_F, 1);

	return IB_MCR_COMPMASK_MGID|IB_MCR_COMPMASK_PORT_GID|
		IB_MCR_COMPMASK_JOIN_STATE;
}

static void build_mcm_rec_umad(void *umad, ib_portid_t *dport, int method,
			       uint64_t comp_mask, uint8_t *data)
{
	ib_rpc_t rpc;

	memset(&rpc, 0, sizeof(rpc));
	rpc.mgtclass = IB_SA_CLASS;
	rpc.method = method;
	rpc.attr.id = IB_SA_ATTR_MCRECORD;
	rpc.attr.mod = 0; // ???
	rpc.mask = comp_mask;
	rpc.datasz = IB_SA_DATA_SIZE;
	rpc.dataoffs = IB_SA_DATA_OFFS;

	mad_build_pkt(umad, &rpc, dport, NULL, data);
}

static int rereg_send(int port, int agent, ib_portid_t *dport,
		      uint8_t *umad, int len, int method, ibmad_gid_t port_gid)
{
	uint8_t data[IB_SA_DATA_SIZE];
	uint64_t comp_mask;

	comp_mask = build_mcm_rec(data, mgid_ipoib, port_gid);

	build_mcm_rec_umad(umad, dport, method, comp_mask, data);
	if(umad_send(port, agent, umad, len, TMO, 0) < 0) {
		err("umad_send leave failed: %s\n", strerror(errno));
		return -1;
	}
	dbg("umad_send %d: tid = 0x%016" PRIx64 "\n", method,
	    mad_get_field64(umad_get_mad(umad), 0, IB_MAD_TRID_F));

	return 0;
}

static int rereg_port_gid(int port, int agent, ib_portid_t *dport,
			  uint8_t *umad, int len, ibmad_gid_t port_gid)
{
	uint8_t data[IB_SA_DATA_SIZE];
	uint64_t comp_mask;

	comp_mask = build_mcm_rec(data, mgid_ipoib, port_gid);

	build_mcm_rec_umad(umad, dport, IB_MAD_METHOD_DELETE,
			   comp_mask, data);
	if(umad_send(port, agent, umad, len, TMO, 0) < 0) {
		err("umad_send leave failed: %s\n", strerror(errno));
		return -1;
	}
	dbg("umad_send leave: tid = 0x%016" PRIx64 "\n",
	    mad_get_field64(umad_get_mad(umad), 0, IB_MAD_TRID_F));

	build_mcm_rec_umad(umad, dport, IB_MAD_METHOD_SET,
			   comp_mask, data);
	if(umad_send(port, agent, umad, len, TMO, 0) < 0) {
		err("umad_send join failed: %s\n", strerror(errno));
		return -1;
	}
	dbg("umad_send join: tid = 0x%016" PRIx64 "\n",
	    mad_get_field64(umad_get_mad(umad), 0, IB_MAD_TRID_F));

	return 0;
}

struct guid_trid {
	ibmad_gid_t gid;
	uint64_t guid;
	uint64_t trid;
};

static int rereg_send_all(int port, int agent, ib_portid_t *dport,
			  struct guid_trid *list, unsigned cnt)
{
	uint8_t *umad;
	int len = umad_size() + 256;
	int i, ret;

	info("rereg_send_all... cnt = %u\n", cnt);

	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

	for (i = 0; i < cnt; i++) {
		ret = rereg_port_gid(port, agent, dport, umad, len, list[i].gid);
		if (ret < 0) {
			err("rereg_send_all: rereg_port_gid 0x%016" PRIx64
			    " failed\n", list[i].guid);
			continue;
		}
		list[i].trid = mad_get_field64(umad_get_mad(umad), 0,
					       IB_MAD_TRID_F);
	}

	info("rereg_send_all: sent %u requests\n", cnt*2);

	free(umad);

	return 0;
}

#if 0
static int rereg_mcm_rec_send(int port, int agent, ib_portid_t *dport, int cnt)
{
	ib_portid_t portid;
	ibmad_gid_t port_gid;
	uint8_t *umad;
	int len, ret = 0;

	ib_resolve_self(&portid, NULL, &port_gid);

	len = umad_size() + 256;
	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

	while(cnt--) {
		if (!rereg_port_gid(port, agent, dport, umad, len, port_gid))
			ret += 2;
	}

	free(umad);

	return ret;
}
#endif

static int rereg_recv(int port, int agent, ib_portid_t *dport,
		      uint8_t *umad, int length, int tmo)
{
	int ret, retry = 0;
	int len = length;

	while((ret = umad_recv(port, umad, &len, tmo)) < 0 &&
	      errno == ETIMEDOUT) {
		if (retry++ > 3)
			return 0;
	}
	if (ret < 0) {
		err("umad_recv %d failed: %s\n", ret, strerror(errno));
		return -1;
	}
	dbg("umad_recv (retries %d), tid = 0x%016" PRIx64 ": len = %d, status = %d\n",
	    retry,
	    mad_get_field64(umad_get_mad(umad), 0, IB_MAD_TRID_F),
	    len, umad_status(umad));

	return 1;
}

static int rereg_recv_all(int port, int agent, ib_portid_t *dport,
			  struct guid_trid *list, unsigned cnt)
{
	uint8_t *umad, *mad;
	int len = umad_size() + 256;
	uint64_t trid;
	unsigned n, method, status;
	int i;

	info("rereg_recv_all...\n");

	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

	n = 0;
	while (rereg_recv(port, agent, dport, umad, len, TMO) > 0) {
		dbg("rereg_recv_all: done %d\n", n);
		n++;
		mad = umad_get_mad(umad);

		method = mad_get_field(mad, 0, IB_MAD_METHOD_F);
		status = mad_get_field(mad, 0, IB_MAD_STATUS_F);

		if (status)
			dbg("MAD status %x, method %x\n", status, method);

		if (status &&
		    (method&0x7f) == (IB_MAD_METHOD_GET_RESPONSE&0x7f)) {
			trid = mad_get_field64(mad, 0, IB_MAD_TRID_F);
			for (i = 0; i < cnt; i++)
				if (trid == list[i].trid)
					break;
			if (i == cnt) {
				err("cannot find trid 0x%016" PRIx64 "\n",
				    trid);
				continue;
			}
			info("guid 0x%016" PRIx64 ": method = %x status = %x. Resending\n",
			     ntohll(list[i].guid), method, status);
			rereg_port_gid(port, agent, dport, umad, len,
				       list[i].gid);
			list[i].trid = mad_get_field64(umad_get_mad(umad), 0,
						       IB_MAD_TRID_F);
		}
	}

	info("rereg_recv_all: got %u responses\n", n);

	free(umad);
	return 0;
}

static int rereg_query_all(int port, int agent, ib_portid_t *dport,
			   struct guid_trid *list, unsigned cnt)
{
	uint8_t *umad, *mad;
	int len = umad_size() + 256;
	unsigned method, status;
	int i, ret;

	info("rereg_query_all...\n");

	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

	for ( i = 0; i < cnt; i++ ) {
		ret = rereg_send(port, agent, dport, umad, len,
				 IB_MAD_METHOD_GET, list[i].gid);
		if (ret < 0) {
			err("query_all: rereg_send failed.\n");
			continue;
		}

		ret = rereg_recv(port, agent, dport, umad, len, TMO);
		if (ret < 0) {
			err("query_all: rereg_recv failed.\n");
			continue;
		}

		mad = umad_get_mad(umad);

		method = mad_get_field(mad, 0, IB_MAD_METHOD_F);
		status = mad_get_field(mad, 0, IB_MAD_STATUS_F);

		if (status)
			info("guid 0x%016" PRIx64 ": status %x, method %x\n",
			     ntohll(list[i].guid), status, method);
	}

	info("rereg_query_all: %u queried.\n", cnt);

	free(umad);
	return 0;
}

#if 0
static int rereg_mcm_rec_recv(int port, int agent, int cnt)
{
	uint8_t *umad, *mad;
	int len = umad_size() + 256;
	int i;

	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

	for ( i = 0; i < cnt; i++ ) {
		int retry;
		retry = 0;
		while (umad_recv(port, umad, &len, TMO) < 0 &&
		       errno == ETIMEDOUT)
			if (retry++ > 3) {
				err("umad_recv %d failed: %s\n",
				    i, strerror(errno));
				free(umad);
				return -1;
			}
		dbg("umad_recv %d (retries %d), tid = 0x%016" PRIx64 ": len = %d, status = %d\n",
		    i, retry,
		    mad_get_field64(umad_get_mad(umad), 0, IB_MAD_TRID_F),
		    len, umad_status(umad));
		mad = umad_get_mad(umad);
	}

	free(umad);
	return 0;
}
#endif

#define MAX_CLIENTS 50

static int rereg_and_test_port(char *guid_file, int port, int agent, ib_portid_t *dport, int timeout)
{
	char line[256];
	FILE *f;
	ibmad_gid_t port_gid;
	uint64_t prefix = htonll(0xfe80000000000000llu);
	uint64_t guid = htonll(0x0002c90200223825llu);
	struct guid_trid *list;
	int i = 0;

	list = calloc(MAX_CLIENTS, sizeof(*list));
	if (!list) {
		err("cannot alloc mem for guid/trid list: %s\n", strerror(errno));
		return -1;
	}

	f = fopen(guid_file, "r");
	if (!f) {
		err("cannot open %s: %s\n", guid_file, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), f)) {
		guid = strtoull(line, NULL, 0);
		guid = htonll(guid);
		memcpy(&port_gid[0], &prefix, 8);
		memcpy(&port_gid[8], &guid, 8);

		list[i].guid = guid;
		memcpy(list[i].gid, port_gid, sizeof(list[i].gid));
		list[i].trid = 0;
		if (++i >= MAX_CLIENTS)
			break;
	}
	fclose(f);

	rereg_send_all(port, agent, dport, list, i);
	rereg_recv_all(port, agent, dport, list, i);

	rereg_query_all(port, agent, dport, list, i);

	free(list);
	return 0;
}

int main(int argc, char **argv)
{
	char *guid_file = "port_guids.list";
	int mgmt_classes[2] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS};
	ib_portid_t dport_id;
	int port, agent;
	uint8_t *umad, *mad;
	int len;

	if (argc > 1)
		guid_file = argv[1];

	madrpc_init(NULL, 0, mgmt_classes, 2);

#if 1
	ib_resolve_smlid(&dport_id, TMO);
#else
	memset(&dport_id, 0, sizeof(dport_id));
	dport_id.lid = 1;
#endif
	dport_id.qp = 1;
	if (!dport_id.qkey)
		dport_id.qkey = IB_DEFAULT_QP1_QKEY;


	len = umad_size() + 256;
	umad = calloc(1, len);
	if (!umad) {
		err("cannot alloc mem for umad: %s\n", strerror(errno));
		return -1;
	}

#if 1
	port = madrpc_portid();
#else
	ret = umad_init();

	port = umad_open_port(NULL, 0);
	if (port < 0) {
		err("umad_open_port failed: %s\n", strerror(errno));
		return port;
	}
#endif

	agent = umad_register(port, IB_SA_CLASS, 2, 0, NULL);

#if 0
	int cnt;
	cnt = rereg_mcm_rec_send(port, agent, &dport_id, cnt);

	rereg_recv_all(port, agent, &dport_id);
#else
	rereg_and_test_port(guid_file, port, agent, &dport_id, TMO);
#endif
	mad = umad_get_mad(umad);

	free(umad);
	umad_unregister(port, agent);
	umad_close_port(port);
	umad_done();

	return 0;
}
