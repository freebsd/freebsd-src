/*
 * Copyright (c) 2001-2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Harti Brandt <harti@freebsd.org>
 *
 * Netgraph module to connect NATM interfaces to netgraph.
 *
 * $FreeBSD$
 */
#ifndef _NETGRAPH_ATM_NG_ATM_H
#define _NETGRAPH_ATM_NG_ATM_H

#define NG_ATM_NODE_TYPE "atm"
#define NGM_ATM_COOKIE	960802260

/* Netgraph control messages */
enum {
	NGM_ATM_GET_IFNAME = 1,		/* get the interface name */
	NGM_ATM_GET_CONFIG,		/* get configuration */
	NGM_ATM_GET_VCCS,		/* get a list of all active vccs */
	NGM_ATM_CPCS_INIT,		/* start the channel */
	NGM_ATM_CPCS_TERM,		/* stop the channel */
	NGM_ATM_GET_VCC,		/* get VCC config */
	NGM_ATM_GET_VCCID,		/* get VCC by VCI/VPI */
	NGM_ATM_GET_STATS,		/* get global statistics */

	/* messages from the node */
	NGM_ATM_CARRIER_CHANGE = 1000,	/* UNUSED: carrier changed */
	NGM_ATM_VCC_CHANGE,		/* permanent VCC changed */
	NGM_ATM_ACR_CHANGE,		/* ABR ACR has changed */
	NGM_ATM_IF_CHANGE,		/* interface state change */
};

/*
 * Hardware interface configuration
 */
struct ngm_atm_config {
	uint32_t	pcr;		/* peak cell rate */
	uint32_t	vpi_bits;	/* number of active VPI bits */
	uint32_t	vci_bits;	/* number of active VCI bits */
	uint32_t	max_vpcs;	/* maximum number of VPCs */
	uint32_t	max_vccs;	/* maximum number of VCCs */
};
#define NGM_ATM_CONFIG_INFO 					\
	{							\
	  { "pcr",	&ng_parse_uint32_type },		\
	  { "vpi_bits",	&ng_parse_uint32_type },		\
	  { "vci_bits",	&ng_parse_uint32_type },		\
	  { "max_vpcs",	&ng_parse_uint32_type },		\
	  { "max_vccs",	&ng_parse_uint32_type },		\
	  { NULL }						\
	}

/*
 * Information about an open VCC
 * See net/if_atm.h. Keep in sync.
 */
#define NGM_ATM_TPARAM_INFO 					\
	{							\
	  { "pcr",	&ng_parse_uint32_type },		\
	  { "scr",	&ng_parse_uint32_type },		\
	  { "mbs",	&ng_parse_uint32_type },		\
	  { "mcr",	&ng_parse_uint32_type },		\
	  { "icr",	&ng_parse_uint32_type },		\
	  { "tbe",	&ng_parse_uint32_type },		\
	  { "nrm",	&ng_parse_uint8_type },			\
	  { "trm",	&ng_parse_uint8_type },			\
	  { "adtf",	&ng_parse_uint16_type },		\
	  { "rif",	&ng_parse_uint8_type },			\
	  { "rdf",	&ng_parse_uint8_type },			\
	  { "cdf",	&ng_parse_uint8_type },			\
	  { NULL }						\
	}

#define NGM_ATM_VCC_INFO 					\
	{							\
	  { "flags",	&ng_parse_hint16_type },		\
	  { "vpi",	&ng_parse_uint16_type },		\
	  { "vci",	&ng_parse_uint16_type },		\
	  { "rmtu",	&ng_parse_uint16_type },		\
	  { "tmtu",	&ng_parse_uint16_type },		\
	  { "aal",	&ng_parse_uint8_type },			\
	  { "traffic",	&ng_parse_uint8_type },			\
	  { "tparam",	&ng_atm_tparam_type },			\
	  { NULL }						\
	}

#define NGM_ATM_VCCARRAY_INFO					\
	{							\
	  &ng_atm_vcc_type,					\
	  ng_atm_vccarray_getlen,				\
	  NULL							\
	}

#define NGM_ATM_VCCTABLE_INFO 					\
	{							\
	  { "count",	&ng_parse_uint32_type },		\
	  { "vccs",	&ng_atm_vccarray_type },		\
	  { NULL }						\
	}

/*
 * Structure to open a VCC.
 */
struct ngm_atm_cpcs_init {
	char		name[NG_HOOKLEN + 1];
	uint32_t	flags;		/* flags. (if_atm.h) */
	uint16_t	vci;		/* VCI to open */
	uint16_t	vpi;		/* VPI to open */
	uint16_t	rmtu;		/* Receive maximum CPCS size */
	uint16_t	tmtu;		/* Transmit maximum CPCS size */
	uint8_t		aal;		/* AAL type (if_atm.h) */
	uint8_t		traffic;	/* traffic type (if_atm.h) */
	uint32_t	pcr;		/* Peak cell rate */
	uint32_t	scr;		/* VBR: Sustainable cell rate */
	uint32_t	mbs;		/* VBR: Maximum burst rate */
	uint32_t	mcr;		/* UBR+: Minimum cell rate */
	uint32_t	icr;		/* ABR: Initial cell rate */
	uint32_t	tbe;		/* ABR: Transmit buffer exposure */
	uint8_t		nrm;		/* ABR: Nrm */
	uint8_t		trm;		/* ABR: Trm */
	uint16_t	adtf;		/* ABR: ADTF */
	uint8_t		rif;		/* ABR: RIF */
	uint8_t		rdf;		/* ABR: RDF */
	uint8_t		cdf;		/* ABR: CDF */
};

#define NGM_ATM_CPCS_INIT_INFO 					\
	{							\
	  { "name",	&ng_parse_hookbuf_type },		\
	  { "flags",	&ng_parse_hint32_type },		\
	  { "vci",	&ng_parse_uint16_type },		\
	  { "vpi",	&ng_parse_uint16_type },		\
	  { "rmtu",	&ng_parse_uint16_type },		\
	  { "tmtu",	&ng_parse_uint16_type },		\
	  { "aal",	&ng_parse_uint8_type },			\
	  { "traffic",	&ng_parse_uint8_type },			\
	  { "pcr",	&ng_parse_uint32_type },		\
	  { "scr",	&ng_parse_uint32_type },		\
	  { "mbs",	&ng_parse_uint32_type },		\
	  { "mcr",	&ng_parse_uint32_type },		\
	  { "icr",	&ng_parse_uint32_type },		\
	  { "tbe",	&ng_parse_uint32_type },		\
	  { "nrm",	&ng_parse_uint8_type },			\
	  { "trm",	&ng_parse_uint8_type },			\
	  { "adtf",	&ng_parse_uint16_type },		\
	  { "rif",	&ng_parse_uint8_type },			\
	  { "rdf",	&ng_parse_uint8_type },			\
	  { "cdf",	&ng_parse_uint8_type },			\
	  { NULL }						\
	}

/*
 * Structure to close a VCI without disconnecting the hook
 */
struct ngm_atm_cpcs_term {
	char		name[NG_HOOKLEN + 1];
};
#define NGM_ATM_CPCS_TERM_INFO 					\
	{							\
	  { "name",	&ng_parse_hookbuf_type },		\
	  { NULL }						\
	}

struct ngm_atm_stats {
	uint64_t	in_packets;
	uint64_t	in_errors;
	uint64_t	out_packets;
	uint64_t	out_errors;
};
#define NGM_ATM_STATS_INFO					\
	{							\
	  { "in_packets",	&ng_parse_uint64_type },	\
	  { "in_errors",	&ng_parse_uint64_type },	\
	  { "out_packets",	&ng_parse_uint64_type },	\
	  { "out_errors",	&ng_parse_uint64_type },	\
	  { NULL }						\
	}

struct ngm_atm_if_change {
	uint32_t	node;
	uint8_t		carrier;
	uint8_t		running;
};
#define NGM_ATM_IF_CHANGE_INFO 					\
	{							\
	  { "node",	&ng_parse_hint32_type },		\
	  { "carrier",	&ng_parse_uint8_type },			\
	  { "running",	&ng_parse_uint8_type },			\
	  { NULL }						\
	}

struct ngm_atm_vcc_change {
	uint32_t	node;
	uint16_t	vci;
	uint8_t		vpi;
	uint8_t		state;
};
#define NGM_ATM_VCC_CHANGE_INFO 				\
	{							\
	  { "node",	&ng_parse_hint32_type },		\
	  { "vci",	&ng_parse_uint16_type },		\
	  { "vpi",	&ng_parse_uint8_type },			\
	  { "state",	&ng_parse_uint8_type },			\
	  { NULL }						\
	}

struct ngm_atm_acr_change {
	uint32_t	node;
	uint16_t	vci;
	uint8_t		vpi;
	uint32_t	acr;
};
#define NGM_ATM_ACR_CHANGE_INFO					\
	{							\
	  { "node",	&ng_parse_hint32_type },		\
	  { "vci",	&ng_parse_uint16_type },		\
	  { "vpi",	&ng_parse_uint8_type },			\
	  { "acr",	&ng_parse_uint32_type },		\
	  { NULL }						\
	}

#endif /* _NETGRAPH_ATM_NG_ATM_H */
