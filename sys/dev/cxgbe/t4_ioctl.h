/*-
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 *
 */

#ifndef __T4_IOCTL_H__
#define __T4_IOCTL_H__

#include <sys/types.h>
#include <net/ethernet.h>

/*
 * Ioctl commands specific to this driver.
 */
enum {
	T4_GETREG = 0x40,		/* read register */
	T4_SETREG,			/* write register */
	T4_REGDUMP,			/* dump of all registers */
	T4_GET_FILTER_MODE,		/* get global filter mode */
	T4_SET_FILTER_MODE,		/* set global filter mode */
	T4_GET_FILTER,			/* get information about a filter */
	T4_SET_FILTER,			/* program a filter */
	T4_DEL_FILTER,			/* delete a filter */
};

struct t4_reg {
	uint32_t addr;
	uint32_t size;
	uint64_t val;
};

#define T4_REGDUMP_SIZE  (160 * 1024)
struct t4_regdump {
	uint32_t version;
	uint32_t len; /* bytes */
	uint32_t *data;
};

/*
 * A hardware filter is some valid combination of these.
 */
#define T4_FILTER_IPv4		0x1	/* IPv4 packet */
#define T4_FILTER_IPv6		0x2	/* IPv6 packet */
#define T4_FILTER_IP_SADDR	0x4	/* Source IP address or network */
#define T4_FILTER_IP_DADDR	0x8	/* Destination IP address or network */
#define T4_FILTER_IP_SPORT	0x10	/* Source IP port */
#define T4_FILTER_IP_DPORT	0x20	/* Destination IP port */
#define T4_FILTER_FCoE		0x40	/* Fibre Channel over Ethernet packet */
#define T4_FILTER_PORT		0x80	/* Physical ingress port */
#define T4_FILTER_OVLAN		0x100	/* Outer VLAN ID */
#define T4_FILTER_IVLAN		0x200	/* Inner VLAN ID */
#define T4_FILTER_IP_TOS	0x400	/* IPv4 TOS/IPv6 Traffic Class */
#define T4_FILTER_IP_PROTO	0x800	/* IP protocol */
#define T4_FILTER_ETH_TYPE	0x1000	/* Ethernet Type */
#define T4_FILTER_MAC_IDX	0x2000	/* MPS MAC address match index */
#define T4_FILTER_MPS_HIT_TYPE	0x4000	/* MPS match type */
#define T4_FILTER_IP_FRAGMENT	0x8000	/* IP fragment */

/* Filter action */
enum {
	FILTER_PASS = 0,	/* default */
	FILTER_DROP,
	FILTER_SWITCH
};

/* 802.1q manipulation on FILTER_SWITCH */
enum {
	VLAN_NOCHANGE = 0,	/* default */
	VLAN_REMOVE,
	VLAN_INSERT,
	VLAN_REWRITE
};

/* MPS match type */
enum {
	UCAST_EXACT = 0,       /* exact unicast match */
	UCAST_HASH  = 1,       /* inexact (hashed) unicast match */
	MCAST_EXACT = 2,       /* exact multicast match */
	MCAST_HASH  = 3,       /* inexact (hashed) multicast match */
	PROMISC     = 4,       /* no match but port is promiscuous */
	HYPPROMISC  = 5,       /* port is hypervisor-promisuous + not bcast */
	BCAST       = 6,       /* broadcast packet */
};

/* Rx steering */
enum {
	DST_MODE_QUEUE,        /* queue is directly specified by filter */
	DST_MODE_RSS_QUEUE,    /* filter specifies RSS entry containing queue */
	DST_MODE_RSS,          /* queue selected by default RSS hash lookup */
	DST_MODE_FILT_RSS      /* queue selected by hashing in filter-specified
				  RSS subtable */
};

struct t4_filter_tuple {
	/*
	 * These are always available.
	 */
	uint8_t sip[16];	/* source IP address (IPv4 in [3:0]) */
	uint8_t dip[16];	/* destinatin IP address (IPv4 in [3:0]) */
	uint16_t sport;		/* source port */
	uint16_t dport;		/* destination port */

	/*
	 * A combination of these (upto 36 bits) is available.  TP_VLAN_PRI_MAP
	 * is used to select the global mode and all filters are limited to the
	 * set of fields allowed by the global mode.
	 */
	uint16_t ovlan;		/* outer VLAN */
	uint16_t ivlan;		/* inner VLAN */
	uint16_t ethtype;	/* Ethernet type */
	uint8_t  tos;		/* TOS/Traffic Type */
	uint8_t  proto;		/* protocol type */
	uint32_t fcoe:1;	/* FCoE packet */
	uint32_t iport:3;	/* ingress port */
	uint32_t matchtype:3;	/* MPS match type */
	uint32_t frag:1;	/* fragmentation extension header */
	uint32_t macidx:9;	/* exact match MAC index */
	uint32_t ivlan_vld:1;	/* inner VLAN valid */
	uint32_t ovlan_vld:1;	/* outer VLAN valid */
};

struct t4_filter_specification {
	uint32_t hitcnts:1;	/* count filter hits in TCB */
	uint32_t prio:1;	/* filter has priority over active/server */
	uint32_t type:1;	/* 0 => IPv4, 1 => IPv6 */
	uint32_t action:2;	/* drop, pass, switch */
	uint32_t rpttid:1;	/* report TID in RSS hash field */
	uint32_t dirsteer:1;	/* 0 => RSS, 1 => steer to iq */
	uint32_t iq:10;		/* ingress queue */
	uint32_t maskhash:1;	/* dirsteer=0: store RSS hash in TCB */
	uint32_t dirsteerhash:1;/* dirsteer=1: 0 => TCB contains RSS hash */
				/*             1 => TCB contains IQ ID */

	/*
	 * Switch proxy/rewrite fields.  An ingress packet which matches a
	 * filter with "switch" set will be looped back out as an egress
	 * packet -- potentially with some Ethernet header rewriting.
	 */
	uint32_t eport:2;	/* egress port to switch packet out */
	uint32_t newdmac:1;	/* rewrite destination MAC address */
	uint32_t newsmac:1;	/* rewrite source MAC address */
	uint32_t newvlan:2;	/* rewrite VLAN Tag */
	uint8_t dmac[ETHER_ADDR_LEN];	/* new destination MAC address */
	uint8_t smac[ETHER_ADDR_LEN];	/* new source MAC address */
	uint16_t vlan;		/* VLAN Tag to insert */

	/*
	 * Filter rule value/mask pairs.
	 */
	struct t4_filter_tuple val;
	struct t4_filter_tuple mask;
};

struct t4_filter {
	uint32_t idx;
	uint16_t l2tidx;
	uint16_t smtidx;
	uint64_t hits;
	struct t4_filter_specification fs;
};

#define CHELSIO_T4_GETREG	_IOWR('f', T4_GETREG, struct t4_reg)
#define CHELSIO_T4_SETREG	_IOW('f', T4_SETREG, struct t4_reg)
#define CHELSIO_T4_REGDUMP	_IOWR('f', T4_REGDUMP, struct t4_regdump)
#define CHELSIO_T4_GET_FILTER_MODE _IOWR('f', T4_GET_FILTER_MODE, uint32_t)
#define CHELSIO_T4_SET_FILTER_MODE _IOW('f', T4_SET_FILTER_MODE, uint32_t)
#define CHELSIO_T4_GET_FILTER	_IOWR('f', T4_GET_FILTER, struct t4_filter)
#define CHELSIO_T4_SET_FILTER	_IOW('f', T4_SET_FILTER, struct t4_filter)
#define CHELSIO_T4_DEL_FILTER	_IOW('f', T4_DEL_FILTER, struct t4_filter)
#endif
