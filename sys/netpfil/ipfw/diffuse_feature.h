/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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
 *
 * $FreeBSD$
 */

/*
 * DIFFUSE feature computation module parts.
 */

#ifndef _NETINET_IPFW_DIFFUSE_FEATURE_H_
#define _NETINET_IPFW_DIFFUSE_FEATURE_H_

struct di_cdata;
struct di_fdata;
struct mbuf;

/*
 * Descriptor for a feature. A feature is actually a list of related
 * statistics. Contains all function pointers for a given feature. This is
 * typically created when a module is loaded, and stored in a global list of
 * feature instances.
 */
struct di_feature_alg {
	const char	*name;		/* Feature name. */
	const int	type;		/* Unidirectional or bidirectional. */
	volatile int	ref_count;	/* Number of instances in the system */

	/*
	 * Init instance.
	 * param1: pointer to instance config
	 * param2: config from userspace
	 * return: non-zero on error
	 */
	int (*init_instance)(struct di_cdata *, struct di_oid *);

	/*
	 * Destroy instance.
	 * param1: pointer to instance config
	 * return: non-zero on error
	 */
	int (*destroy_instance)(struct di_cdata *);

	/*
	 * Init state for flow.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * return: non-zero on error
	 */
	int (*init_stats)(struct di_cdata *, struct di_fdata *);

	/*
	 * Destroy flow state.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * return: non-zero on error
	 */
	int (*destroy_stats)(struct di_cdata *, struct di_fdata *);

	/*
	 * Update feature, called for each packet.
	 * pre: the packet is an IPv4 or IPv6 packet and the caller has done a
	 *      pullup on the mbuf prior to calling.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * param3: packet (mbuf chain)
	 * param4: protocol on top of IP
	 * param5: pointer to upper layer protocol (e.g. UDP, TCP)
	 * param6: direction of the packet (MATCH_FORWARD or MATCH_REVERSE)
	 * return: non-zero on error
	 */
	int (*update_stats)(struct di_cdata *, struct di_fdata *, struct mbuf *,
	    int proto, void *ulp, int dir);

	/*
	 * Reset stats.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * return: non-zero on error
	 */
	int (*reset_stats)(struct di_cdata *, struct di_fdata *);

	/*
	 * Get feature statistics.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * param3: pointer to stats
	 * return: non-zero on error
	 */
        int (*get_stats)(struct di_cdata *, struct di_fdata *, int32_t **);

	/*
	 * Get one feature statistics.
	 * param1: pointer to instance config
	 * param2: pointer to flow work and stats data
	 * param3: which one
	 * param4: pointer to stat
	 * return: non-zero on error
	 */
	int (*get_stat)(struct di_cdata *, struct di_fdata *, int, int32_t *);

	/*
	 * Get names of statistics.
	 * param1: array of names
	 * return: non-zero on error
	 */
	int (*get_stat_names)(char **[]);

	/*
	 * Get configuration data.
	 * param1: pointer to instance config
	 * param2: pointer to configuration
	 * param3: only compute size (if 1)
	 * return: non-zero on error
	 */
	int (*get_conf)(struct di_cdata *, struct di_oid *, int);

	SLIST_ENTRY(di_feature_alg) next; /* Next feature in the list. */
};

#endif /* _NETINET_IPFW_DIFFUSE_FEATURE_H_ */
