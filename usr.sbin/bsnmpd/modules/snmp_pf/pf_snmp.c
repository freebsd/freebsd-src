/*-
 * Copyright (c) 2005 Philip Paeps <philip@FreeBSD.org>
 * All rights reserved.
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

#include <bsnmp/snmpmod.h>

#include <net/pfvar.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "pf_oid.h"
#include "pf_tree.h"

struct lmodule *module;

static int dev = -1;
static int started;
static uint32_t pf_tick;

static struct pf_status pfs;

enum { IN, OUT };
enum { IPV4, IPV6 };
enum { PASS, BLOCK };

#define PFI_IFTYPE_GROUP	0
#define PFI_IFTYPE_INSTANCE	1
#define PFI_IFTYPE_DETACHED	2

struct pfi_entry {
	struct pfi_if	pfi;
	u_int		index;
	TAILQ_ENTRY(pfi_entry) link;
};
TAILQ_HEAD(pfi_table, pfi_entry);

static struct pfi_table pfi_table;
static time_t pfi_table_age;
static int pfi_table_count;

#define PFI_TABLE_MAXAGE	5

struct pft_entry {
	struct pfr_tstats pft;
	u_int		index;
	TAILQ_ENTRY(pft_entry) link;
};
TAILQ_HEAD(pft_table, pft_entry);

static struct pft_table pft_table;
static time_t pft_table_age;
static int pft_table_count;

#define PFT_TABLE_MAXAGE	5

struct pfq_entry {
	struct pf_altq	altq;
	u_int		index;
	TAILQ_ENTRY(pfq_entry) link;
};
TAILQ_HEAD(pfq_table, pfq_entry);

static struct pfq_table pfq_table;
static time_t pfq_table_age;
static int pfq_table_count;

#define PFQ_TABLE_MAXAGE	5

/* Forward declarations */
static int pfi_refresh(void);
static int pfq_refresh(void);
static int pfs_refresh(void);
static int pft_refresh(void);
static struct pfi_entry * pfi_table_find(u_int idx);
static struct pfq_entry * pfq_table_find(u_int idx);
static struct pft_entry * pft_table_find(u_int idx);

int
pf_status(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	time_t		runtime;
	unsigned char	str[128];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if (pfs_refresh() == -1)
			return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfStatusRunning:
			    val->v.uint32 = pfs.running;
			    break;
			case LEAF_pfStatusRuntime:
			    runtime = (pfs.since > 0) ?
				time(NULL) - pfs.since : 0;
			    val->v.uint32 = runtime * 100;
			    break;
			case LEAF_pfStatusDebug:
			    val->v.uint32 = pfs.debug;
			    break;
			case LEAF_pfStatusHostId:
			    sprintf(str, "0x%08x", ntohl(pfs.hostid));
			    return (string_get(val, str, strlen(str)));

			default:
			    return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_counter(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if (pfs_refresh() == -1)
			return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfCounterMatch:
				val->v.counter64 = pfs.counters[PFRES_MATCH];
				break;
			case LEAF_pfCounterBadOffset:
				val->v.counter64 = pfs.counters[PFRES_BADOFF];
				break;
			case LEAF_pfCounterFragment:
				val->v.counter64 = pfs.counters[PFRES_FRAG];
				break;
			case LEAF_pfCounterShort:
				val->v.counter64 = pfs.counters[PFRES_SHORT];
				break;
			case LEAF_pfCounterNormalize:
				val->v.counter64 = pfs.counters[PFRES_NORM];
				break;
			case LEAF_pfCounterMemDrop:
				val->v.counter64 = pfs.counters[PFRES_MEMORY];
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_statetable(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if (pfs_refresh() == -1)
			return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfStateTableCount:
				val->v.uint32 = pfs.states;
				break;
			case LEAF_pfStateTableSearches:
				val->v.counter64 =
				    pfs.fcounters[FCNT_STATE_SEARCH];
				break;
			case LEAF_pfStateTableInserts:
				val->v.counter64 =
				    pfs.fcounters[FCNT_STATE_INSERT];
				break;
			case LEAF_pfStateTableRemovals:
				val->v.counter64 =
				    pfs.fcounters[FCNT_STATE_REMOVALS];
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_srcnodes(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if (pfs_refresh() == -1)
			return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfSrcNodesCount:
				val->v.uint32 = pfs.src_nodes;
				break;
			case LEAF_pfSrcNodesSearches:
				val->v.counter64 =
				    pfs.scounters[SCNT_SRC_NODE_SEARCH];
				break;
			case LEAF_pfSrcNodesInserts:
				val->v.counter64 =
				    pfs.scounters[SCNT_SRC_NODE_INSERT];
				break;
			case LEAF_pfSrcNodesRemovals:
				val->v.counter64 =
				    pfs.scounters[SCNT_SRC_NODE_REMOVALS];
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_limits(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t		which = val->var.subs[sub - 1];
	struct pfioc_limit	pl;

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		bzero(&pl, sizeof(struct pfioc_limit));

		switch (which) {
			case LEAF_pfLimitsStates:
				pl.index = PF_LIMIT_STATES;
				break;
			case LEAF_pfLimitsSrcNodes:
				pl.index = PF_LIMIT_SRC_NODES;
				break;
			case LEAF_pfLimitsFrags:
				pl.index = PF_LIMIT_FRAGS;
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		if (ioctl(dev, DIOCGETLIMIT, &pl)) {
			syslog(LOG_ERR, "pf_limits(): ioctl(): %s",
			    strerror(errno));
			return (SNMP_ERR_GENERR);
		}

		val->v.uint32 = pl.limit;

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_timeouts(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	struct pfioc_tm	pt;

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		bzero(&pt, sizeof(struct pfioc_tm));

		switch (which) {
			case LEAF_pfTimeoutsTcpFirst:
				pt.timeout = PFTM_TCP_FIRST_PACKET;
				break;
			case LEAF_pfTimeoutsTcpOpening:
				pt.timeout = PFTM_TCP_OPENING;
				break;
			case LEAF_pfTimeoutsTcpEstablished:
				pt.timeout = PFTM_TCP_ESTABLISHED;
				break;
			case LEAF_pfTimeoutsTcpClosing:
				pt.timeout = PFTM_TCP_CLOSING;
				break;
			case LEAF_pfTimeoutsTcpFinWait:
				pt.timeout = PFTM_TCP_FIN_WAIT;
				break;
			case LEAF_pfTimeoutsTcpClosed:
				pt.timeout = PFTM_TCP_CLOSED;
				break;
			case LEAF_pfTimeoutsUdpFirst:
				pt.timeout = PFTM_UDP_FIRST_PACKET;
				break;
			case LEAF_pfTimeoutsUdpSingle:
				pt.timeout = PFTM_UDP_SINGLE;
				break;
			case LEAF_pfTimeoutsUdpMultiple:
				pt.timeout = PFTM_UDP_MULTIPLE;
				break;
			case LEAF_pfTimeoutsIcmpFirst:
				pt.timeout = PFTM_ICMP_FIRST_PACKET;
				break;
			case LEAF_pfTimeoutsIcmpError:
				pt.timeout = PFTM_ICMP_ERROR_REPLY;
				break;
			case LEAF_pfTimeoutsOtherFirst:
				pt.timeout = PFTM_OTHER_FIRST_PACKET;
				break;
			case LEAF_pfTimeoutsOtherSingle:
				pt.timeout = PFTM_OTHER_SINGLE;
				break;
			case LEAF_pfTimeoutsOtherMultiple:
				pt.timeout = PFTM_OTHER_MULTIPLE;
				break;
			case LEAF_pfTimeoutsFragment:
				pt.timeout = PFTM_FRAG;
				break;
			case LEAF_pfTimeoutsInterval:
				pt.timeout = PFTM_INTERVAL;
				break;
			case LEAF_pfTimeoutsAdaptiveStart:
				pt.timeout = PFTM_ADAPTIVE_START;
				break;
			case LEAF_pfTimeoutsAdaptiveEnd:
				pt.timeout = PFTM_ADAPTIVE_END;
				break;
			case LEAF_pfTimeoutsSrcNode:
				pt.timeout = PFTM_SRC_NODE;
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		if (ioctl(dev, DIOCGETTIMEOUT, &pt)) {
			syslog(LOG_ERR, "pf_timeouts(): ioctl(): %s",
			    strerror(errno));
			return (SNMP_ERR_GENERR);
		}

		val->v.integer = pt.seconds;

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_logif(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	unsigned char	str[IFNAMSIZ];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if (pfs_refresh() == -1)
			return (SNMP_ERR_GENERR);

		switch (which) {
	 		case LEAF_pfLogInterfaceName:
				strlcpy(str, pfs.ifname, sizeof str);
				return (string_get(val, str, strlen(str)));
			case LEAF_pfLogInterfaceIp4BytesIn:
				val->v.counter64 = pfs.bcounters[IPV4][IN];
				break;
			case LEAF_pfLogInterfaceIp4BytesOut:
				val->v.counter64 = pfs.bcounters[IPV4][OUT];
				break;
			case LEAF_pfLogInterfaceIp4PktsInPass:
				val->v.counter64 =
				    pfs.pcounters[IPV4][IN][PF_PASS];
				break;
			case LEAF_pfLogInterfaceIp4PktsInDrop:
				val->v.counter64 =
				    pfs.pcounters[IPV4][IN][PF_DROP];
				break;
			case LEAF_pfLogInterfaceIp4PktsOutPass:
				val->v.counter64 =
				    pfs.pcounters[IPV4][OUT][PF_PASS];
				break;
			case LEAF_pfLogInterfaceIp4PktsOutDrop:
				val->v.counter64 =
				    pfs.pcounters[IPV4][OUT][PF_DROP];
				break;
			case LEAF_pfLogInterfaceIp6BytesIn:
				val->v.counter64 = pfs.bcounters[IPV6][IN];
				break;
			case LEAF_pfLogInterfaceIp6BytesOut:
				val->v.counter64 = pfs.bcounters[IPV6][OUT];
				break;
			case LEAF_pfLogInterfaceIp6PktsInPass:
				val->v.counter64 =
				    pfs.pcounters[IPV6][IN][PF_PASS];
				break;
			case LEAF_pfLogInterfaceIp6PktsInDrop:
				val->v.counter64 =
				    pfs.pcounters[IPV6][IN][PF_DROP];
				break;
			case LEAF_pfLogInterfaceIp6PktsOutPass:
				val->v.counter64 =
				    pfs.pcounters[IPV6][OUT][PF_PASS];
				break;
			case LEAF_pfLogInterfaceIp6PktsOutDrop:
				val->v.counter64 =
				    pfs.pcounters[IPV6][OUT][PF_DROP];
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_interfaces(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if ((time(NULL) - pfi_table_age) > PFI_TABLE_MAXAGE)
			if (pfi_refresh() == -1)
			    return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfInterfacesIfNumber:
				val->v.uint32 = pfi_table_count;
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_iftable(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	struct pfi_entry *e = NULL;

	switch (op) {
		case SNMP_OP_SET:
			return (SNMP_ERR_NOT_WRITEABLE);
		case SNMP_OP_GETNEXT:
			if ((e = NEXT_OBJECT_INT(&pfi_table,
			    &val->var, sub)) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			val->var.len = sub + 1;
			val->var.subs[sub] = e->index;
			break;
		case SNMP_OP_GET:
			if (val->var.len - sub != 1)
				return (SNMP_ERR_NOSUCHNAME);
			if ((e = pfi_table_find(val->var.subs[sub])) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			break;

		case SNMP_OP_COMMIT:
		case SNMP_OP_ROLLBACK:
		default:
			abort();
	}

	if ((time(NULL) - pfi_table_age) > PFI_TABLE_MAXAGE)
		pfi_refresh();

	switch (which) {
		case LEAF_pfInterfacesIfDescr:
			return (string_get(val, e->pfi.pfif_name, -1));
		case LEAF_pfInterfacesIfType:
			val->v.integer = PFI_IFTYPE_INSTANCE;
			break;
		case LEAF_pfInterfacesIfTZero:
			val->v.uint32 =
			    (time(NULL) - e->pfi.pfif_tzero) * 100;
			break;
		case LEAF_pfInterfacesIfRefsState:
			val->v.uint32 = e->pfi.pfif_states;
			break;
		case LEAF_pfInterfacesIfRefsRule:
			val->v.uint32 = e->pfi.pfif_rules;
			break;
		case LEAF_pfInterfacesIf4BytesInPass:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV4][IN][PASS];
			break;
		case LEAF_pfInterfacesIf4BytesInBlock:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV4][IN][BLOCK];
			break;
		case LEAF_pfInterfacesIf4BytesOutPass:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV4][OUT][PASS];
			break;
		case LEAF_pfInterfacesIf4BytesOutBlock:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV4][OUT][BLOCK];
			break;
		case LEAF_pfInterfacesIf4PktsInPass:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV4][IN][PASS];
			break;
		case LEAF_pfInterfacesIf4PktsInBlock:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV4][IN][BLOCK];
			break;
		case LEAF_pfInterfacesIf4PktsOutPass:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV4][OUT][PASS];
			break;
		case LEAF_pfInterfacesIf4PktsOutBlock:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV4][OUT][BLOCK];
			break;
		case LEAF_pfInterfacesIf6BytesInPass:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV6][IN][PASS];
			break;
		case LEAF_pfInterfacesIf6BytesInBlock:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV6][IN][BLOCK];
			break;
		case LEAF_pfInterfacesIf6BytesOutPass:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV6][OUT][PASS];
			break;
		case LEAF_pfInterfacesIf6BytesOutBlock:
			val->v.counter64 =
			    e->pfi.pfif_bytes[IPV6][OUT][BLOCK];
			break;
		case LEAF_pfInterfacesIf6PktsInPass:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV6][IN][PASS];
			break;
		case LEAF_pfInterfacesIf6PktsInBlock:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV6][IN][BLOCK];
			break;
		case LEAF_pfInterfacesIf6PktsOutPass:
			val->v.counter64 =
			    e->pfi.pfif_packets[IPV6][OUT][PASS];
			break;
		case LEAF_pfInterfacesIf6PktsOutBlock:
			val->v.counter64 = 
			    e->pfi.pfif_packets[IPV6][OUT][BLOCK];
			break;

		default:
			return (SNMP_ERR_NOSUCHNAME);
	}

	return (SNMP_ERR_NOERROR);
}

int
pf_tables(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if ((time(NULL) - pft_table_age) > PFT_TABLE_MAXAGE)
			if (pft_refresh() == -1)
			    return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfTablesTblNumber:
				val->v.uint32 = pft_table_count;
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
}

int
pf_tbltable(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	struct pft_entry *e = NULL;

	switch (op) {
		case SNMP_OP_SET:
			return (SNMP_ERR_NOT_WRITEABLE);
		case SNMP_OP_GETNEXT:
			if ((e = NEXT_OBJECT_INT(&pft_table,
			    &val->var, sub)) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			val->var.len = sub + 1;
			val->var.subs[sub] = e->index;
			break;
		case SNMP_OP_GET:
			if (val->var.len - sub != 1)
				return (SNMP_ERR_NOSUCHNAME);
			if ((e = pft_table_find(val->var.subs[sub])) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			break;

		case SNMP_OP_COMMIT:
		case SNMP_OP_ROLLBACK:
		default:
			abort();
	}

	if ((time(NULL) - pft_table_age) > PFT_TABLE_MAXAGE)
		pft_refresh();

	switch (which) {
		case LEAF_pfTablesTblDescr:
			return (string_get(val, e->pft.pfrts_name, -1));
		case LEAF_pfTablesTblCount:
			val->v.integer = e->pft.pfrts_cnt;
			break;
		case LEAF_pfTablesTblTZero:
			val->v.uint32 =
			    (time(NULL) - e->pft.pfrts_tzero) * 100;
			break;
		case LEAF_pfTablesTblRefsAnchor:
			val->v.integer =
			    e->pft.pfrts_refcnt[PFR_REFCNT_ANCHOR];
			break;
		case LEAF_pfTablesTblRefsRule:
			val->v.integer =
			    e->pft.pfrts_refcnt[PFR_REFCNT_RULE];
			break;
		case LEAF_pfTablesTblEvalMatch:
			val->v.counter64 = e->pft.pfrts_match;
			break;
		case LEAF_pfTablesTblEvalNoMatch:
			val->v.counter64 = e->pft.pfrts_nomatch;
			break;
		case LEAF_pfTablesTblBytesInPass:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_IN][PFR_OP_PASS];
			break;
		case LEAF_pfTablesTblBytesInBlock:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_IN][PFR_OP_BLOCK];
			break;
		case LEAF_pfTablesTblBytesInXPass:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_IN][PFR_OP_XPASS];
			break;
		case LEAF_pfTablesTblBytesOutPass:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_OUT][PFR_OP_PASS];
			break;
		case LEAF_pfTablesTblBytesOutBlock:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_OUT][PFR_OP_BLOCK];
			break;
		case LEAF_pfTablesTblBytesOutXPass:
			val->v.counter64 =
			    e->pft.pfrts_bytes[PFR_DIR_OUT][PFR_OP_XPASS];
			break;
		case LEAF_pfTablesTblPktsInPass:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_IN][PFR_OP_PASS];
			break;
		case LEAF_pfTablesTblPktsInBlock:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_IN][PFR_OP_BLOCK];
			break;
		case LEAF_pfTablesTblPktsInXPass:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_IN][PFR_OP_XPASS];
			break;
		case LEAF_pfTablesTblPktsOutPass:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_OUT][PFR_OP_PASS];
			break;
		case LEAF_pfTablesTblPktsOutBlock:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_OUT][PFR_OP_BLOCK];
			break;
		case LEAF_pfTablesTblPktsOutXPass:
			val->v.counter64 =
			    e->pft.pfrts_packets[PFR_DIR_OUT][PFR_OP_XPASS];
			break;

		default:
			return (SNMP_ERR_NOSUCHNAME);
	}

	return (SNMP_ERR_NOERROR);
}

int
pf_tbladdr(struct snmp_context __unused *ctx, struct snmp_value __unused *val,
	u_int __unused sub, u_int __unused vindex, enum snmp_op __unused op)
{
	return (SNMP_ERR_GENERR);
}

int
pf_altq(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];

	if (op == SNMP_OP_SET)
		return (SNMP_ERR_NOT_WRITEABLE);

	if (op == SNMP_OP_GET) {
		if ((time(NULL) - pfq_table_age) > PFQ_TABLE_MAXAGE)
			if (pfq_refresh() == -1)
			    return (SNMP_ERR_GENERR);

		switch (which) {
			case LEAF_pfAltqQueueNumber:
				val->v.uint32 = pfq_table_count;
				break;

			default:
				return (SNMP_ERR_NOSUCHNAME);
		}

		return (SNMP_ERR_NOERROR);
	}

	abort();
	return (SNMP_ERR_GENERR);
}	

int
pf_altqq(struct snmp_context __unused *ctx, struct snmp_value *val,
	u_int sub, u_int __unused vindex, enum snmp_op op)
{
	asn_subid_t	which = val->var.subs[sub - 1];
	struct pfq_entry *e = NULL;

	switch (op) {
		case SNMP_OP_SET:
			return (SNMP_ERR_NOT_WRITEABLE);
		case SNMP_OP_GETNEXT:
			if ((e = NEXT_OBJECT_INT(&pfq_table,
			    &val->var, sub)) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			val->var.len = sub + 1;
			val->var.subs[sub] = e->index;
			break;
		case SNMP_OP_GET:
			if (val->var.len - sub != 1)
				return (SNMP_ERR_NOSUCHNAME);
			if ((e = pfq_table_find(val->var.subs[sub])) == NULL)
				return (SNMP_ERR_NOSUCHNAME);
			break;

		case SNMP_OP_COMMIT:
		case SNMP_OP_ROLLBACK:
		default:
			abort();
	}

	if ((time(NULL) - pfq_table_age) > PFQ_TABLE_MAXAGE)
		pfq_refresh();

	switch (which) {
		case LEAF_pfAltqQueueDescr:
			return (string_get(val, e->altq.qname, -1));
		case LEAF_pfAltqQueueParent:
			return (string_get(val, e->altq.parent, -1));
		case LEAF_pfAltqQueueScheduler:
			val->v.integer = e->altq.scheduler;
			break;
		case LEAF_pfAltqQueueBandwidth:
			val->v.uint32 = e->altq.bandwidth;
			break;
		case LEAF_pfAltqQueuePriority:
			val->v.integer = e->altq.priority;
			break;
		case LEAF_pfAltqQueueLimit:
			val->v.integer = e->altq.qlimit;
			break;
		
		default:
			return (SNMP_ERR_NOSUCHNAME);
	}

	return (SNMP_ERR_NOERROR);
}	

static struct pfi_entry *
pfi_table_find(u_int idx)
{
	struct pfi_entry *e;

	TAILQ_FOREACH(e, &pfi_table, link)
		if (e->index == idx)
			return (e);
	return (NULL);
}

static struct pfq_entry *
pfq_table_find(u_int idx)
{
	struct pfq_entry *e;
	TAILQ_FOREACH(e, &pfq_table, link)
		if (e->index == idx)
			return (e);
	return (NULL);
}

static struct pft_entry *
pft_table_find(u_int idx)
{
	struct pft_entry *e;

	TAILQ_FOREACH(e, &pft_table, link)
		if (e->index == idx)
			return (e);
	return (NULL);
}

static int
pfi_refresh(void)
{
	struct pfioc_iface io;
	struct pfi_if *p;
	struct pfi_entry *e;
	int i, numifs = 1;

	if (started && this_tick <= pf_tick)
		return (0);

	while (!TAILQ_EMPTY(&pfi_table)) {
		e = TAILQ_FIRST(&pfi_table);
		TAILQ_REMOVE(&pfi_table, e, link);
		free(e);
	}

	bzero(&io, sizeof(io));
	p = malloc(sizeof(struct pfi_if));
	io.pfiio_flags = PFI_FLAG_INSTANCE;
	io.pfiio_esize = sizeof(struct pfi_if);

	for (;;) {
		p = realloc(p, numifs * sizeof(struct pfi_if));
		io.pfiio_size = numifs;
		io.pfiio_buffer = p;

		if (ioctl(dev, DIOCIGETIFACES, &io)) {
			syslog(LOG_ERR, "pfi_refresh(): ioctl(): %s",
			    strerror(errno));
			return (-1);
		}

		if (numifs >= io.pfiio_size)
			break;

		numifs = io.pfiio_size;
	}

	for (i = 0; i < numifs; i++) {
		e = malloc(sizeof(struct pfi_entry));
		e->index = i + 1;
		memcpy(&e->pfi, p+i, sizeof(struct pfi_if));
		TAILQ_INSERT_TAIL(&pfi_table, e, link);
	}

	pfi_table_age = time(NULL);
	pfi_table_count = numifs;
	pf_tick = this_tick;

	free(p);
	return (0);
}

static int
pfq_refresh(void)
{
	struct pfioc_altq pa;
	struct pfq_entry *e;
	int i, numqs, ticket;

	if (started && this_tick <= pf_tick)
		return (0);

	while (!TAILQ_EMPTY(&pfq_table)) {
		e = TAILQ_FIRST(&pfq_table);
		TAILQ_REMOVE(&pfq_table, e, link);
		free(e);
	}

	bzero(&pa, sizeof(pa));
	
	if (ioctl(dev, DIOCGETALTQS, &pa)) {
		syslog(LOG_ERR, "pfq_refresh: ioctl(DIOCGETALTQS): %s",
		    strerror(errno));
		return (-1);
	}

	numqs = pa.nr;
	ticket = pa.ticket;

	for (i = 0; i < numqs; i++) {
		e = malloc(sizeof(struct pfq_entry));
		pa.ticket = ticket;
		pa.nr = i;

		if (ioctl(dev, DIOCGETALTQ, &pa)) {
			syslog(LOG_ERR, "pfq_refresh(): "
			    "ioctl(DIOCGETALTQ): %s",
			    strerror(errno));
			return (-1);
		}

		if (pa.altq.qid > 0) {
			memcpy(&e->altq, &pa.altq, sizeof(struct pf_altq));
			e->index = pa.altq.qid;
			pfq_table_count = i;
			TAILQ_INSERT_TAIL(&pfq_table, e, link);
		}
	}
	
	pfq_table_age = time(NULL);
	pf_tick = this_tick;

	return (0);
}

static int
pfs_refresh(void)
{
	if (started && this_tick <= pf_tick)
		return (0);

	bzero(&pfs, sizeof(struct pf_status));

	if (ioctl(dev, DIOCGETSTATUS, &pfs)) {
		syslog(LOG_ERR, "pfs_refresh(): ioctl(): %s",
		    strerror(errno));
		return (-1);
	}

	pf_tick = this_tick;
	return (0);
}

static int
pft_refresh(void)
{
	struct pfioc_table io;
	struct pfr_tstats *t;
	struct pft_entry *e;
	int i, numtbls = 1;

	if (started && this_tick <= pf_tick)
		return (0);

	while (!TAILQ_EMPTY(&pft_table)) {
		e = TAILQ_FIRST(&pft_table);
		TAILQ_REMOVE(&pft_table, e, link);
		free(e);
	}

	bzero(&io, sizeof(io));
	t = malloc(sizeof(struct pfr_tstats));
	io.pfrio_esize = sizeof(struct pfr_tstats);

	for (;;) {
		t = realloc(t, numtbls * sizeof(struct pfr_tstats));
		io.pfrio_size = numtbls;
		io.pfrio_buffer = t;

		if (ioctl(dev, DIOCRGETTSTATS, &io)) {
			syslog(LOG_ERR, "pft_refresh(): ioctl(): %s",
			    strerror(errno));
			return (-1);
		}

		if (numtbls >= io.pfrio_size)
			break;

		numtbls = io.pfrio_size;
	}

	for (i = 0; i < numtbls; i++) {
		e = malloc(sizeof(struct pfr_tstats));
		e->index = i + 1;
		memcpy(&e->pft, t+i, sizeof(struct pfr_tstats));
		TAILQ_INSERT_TAIL(&pft_table, e, link);
	}

	pft_table_age = time(NULL);
	pft_table_count = numtbls;
	pf_tick = this_tick;

	free(t);
	return (0);
}

/*
 * Implement the bsnmpd module interface
 */
static int
pf_init(struct lmodule *mod, int __unused argc, char __unused *argv[])
{
	module = mod;

	if ((dev = open("/dev/pf", O_RDONLY)) == -1) {
		syslog(LOG_ERR, "pf_init(): open(): %s\n",
		    strerror(errno));
		return (-1);
	}

	/* Prepare internal state */
	TAILQ_INIT(&pfi_table);
	TAILQ_INIT(&pfq_table);
	TAILQ_INIT(&pft_table);

	pfi_refresh();
	pfq_refresh();
	pfs_refresh();
	pft_refresh();

	started = 1;

	return (0);
}

static int
pf_fini(void)
{
	struct pfi_entry *i1, *i2;
	struct pfq_entry *q1, *q2;
	struct pft_entry *t1, *t2;

	/* Empty the list of interfaces */
	i1 = TAILQ_FIRST(&pfi_table);
	while (i1 != NULL) {
		i2 = TAILQ_NEXT(i1, link);
		free(i1);
		i1 = i2;
	}

	/* List of queues */
	q1 = TAILQ_FIRST(&pfq_table);
	while (q1 != NULL) {
		q2 = TAILQ_NEXT(q1, link);
		free(q1);
		q1 = q2;
	}

	/* And the list of tables */
	t1 = TAILQ_FIRST(&pft_table);
	while (t1 != NULL) {
		t2 = TAILQ_NEXT(t1, link);
		free(t1);
		t1 = t2;
	}

	close(dev);
	return (0);
}

static void
pf_dump(void)
{
	pfi_refresh();
	pfq_refresh();
	pft_refresh();

	syslog(LOG_ERR, "Dump: pfi_table_age = %jd",
	    (intmax_t)pfi_table_age);
	syslog(LOG_ERR, "Dump: pfi_table_count = %d",
	    pfi_table_count);
	
	syslog(LOG_ERR, "Dump: pfq_table_age = %jd",
	    (intmax_t)pfq_table_age);
	syslog(LOG_ERR, "Dump: pfq_table_count = %d",
	    pfq_table_count);

	syslog(LOG_ERR, "Dump: pft_table_age = %jd",
	    (intmax_t)pft_table_age);

	syslog(LOG_ERR, "Dump: pft_table_count = %d",
	    pft_table_count);
}

const struct snmp_module config = {
	.comment = "This module implements a MIB for the pf packet filter.",
	.init =		pf_init,
	.fini =		pf_fini,
	.tree =		pf_ctree,
	.dump =		pf_dump,
	.tree_size =	pf_CTREE_SIZE,
};
