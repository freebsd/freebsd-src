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
 * Compatibility goo to allow kernel code to be compiled in userspace. Any file
 * including this file must ensure the contents of this file are only visible if
 * _KERNEL is not defined.
 */

#ifndef _NETINET_IPFW_DIFFUSE_USER_COMPAT_H_
#define _NETINET_IPFW_DIFFUSE_USER_COMPAT_H_

/* Mbuf for userspace. */
struct mbuf {
	uint8_t		*data;
	uint16_t	len;
};

/* From ip_fw_private.h */

struct ip_fw_args {
	struct mbuf     	*m;	/* The mbuf chain. */
	struct ipfw_flow_id	f_id;	/* Grabbed from IP header. */
};

/* Result for matching dynamic rules. */
enum {
	MATCH_REVERSE = 0,
	MATCH_FORWARD,
	MATCH_NONE,
	MATCH_UNKNOWN,
};

/* From diffuse_private.h */

struct di_feature {
	char			*name;	/* Instance name. */
	struct di_cdata		conf;
	struct di_feature_alg	*alg;	/* Feature algo ptr. */
};

struct di_ft_entry {
	struct di_ft_entry	*next;	/* Linked list of rules. */
	struct ip_fw		*rule;	/* Used to pass up the rule number. */
	struct ipfw_flow_id	id;	/* (masked) flow id. */
	uint64_t		pcnt;	/* Packet match counter. */
	uint64_t		bcnt;	/* Byte match counter. */
	uint32_t		expire;	/* Expire time. */
	uint32_t		bucket;	/* Which bucket in hash table. */
	uint32_t		state;	/* State of this rule (typically a
					 * combination of TCP flags). */
	uint8_t			ftype;	/* Bidir vs unidir, match limiting. */
	uint8_t			fcnt;	/* Number of features. */
	uint8_t			tcnt;	/* Number of class tags. */
	uint16_t		sample_int;	/* Sample interval. */
	uint32_t		sample_prob;	/* Sample probability. */
	uint16_t		pkts_after_last; /* Match limiting: packets n */
	struct di_feature	*features[DI_MAX_FEATURES]; /* Feature ptrs. */
	struct di_fdata		fwd_data[DI_MAX_FEATURES];
	struct di_fdata		bck_data[DI_MAX_FEATURES];
	uint64_t		flow_id;
	struct timeval		start_time; /* Timestamp of first packet. */
};

struct di_feature_stat_ptr {
        struct di_feature	*fptr;	/* Feature ptr. */
        uint8_t			sidx;	/* Statistic index. */
};

/* Redefined to get pointer to alg definition. */
/* XXX: Maybe better to call register function here? */
#define	DECLARE_DIFFUSE_FEATURE_MODULE(fname, fstruct)			\
struct di_feature_alg * diffuse_feature_##fname##_kmodule()		\
{									\
	return (fstruct);						\
}

#define	VNET_DEFINE(t, n)	t n
#define	VNET(n)			n

#define	DI_FT_LOCK_INIT()
#define	DI_FT_LOCK_DESTROY()
#define	DI_FT_RLOCK()
#define	DI_FT_WLOCK()
#define	DI_FT_UNLOCK()
#define	DI_FT_RLOCK_ASSERT()
#define	DI_FT_WLOCK_ASSERT()
#define	DI_FT_LOCK_ASSERT()

/* #define this if you need to compile kernel code in userspace. */
#ifdef KPI_USER_COMPAT

/* Functions hacks. */
#define	free(s, n)		free(s)
#define	getmicrouptime(t)	mygettimeofday(t, NULL)
#define	m_length(m, p) 		m->len
#define	malloc(s, n, f)		calloc(s, 1)
#define	microuptime(t)		mygettimeofday(t, NULL)
#define	mtod(m, p)		(p) m->data
#define	time_uptime		mytime(NULL)
#define	uma_zfree(z, p)		free(p, M_DIFFUSE)

time_t mytime(time_t *t);
int mygettimeofday(struct timeval *tp, struct timezone *tzp);

#endif /* KPI_USER_COMPAT */

#endif /* _NETINET_IPFW_DIFFUSE_USER_COMPAT_H_ */
