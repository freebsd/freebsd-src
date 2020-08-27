/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
 * Contains various definitions shared between the parts of a routing subsystem.
 *
 * Header is not intended to be included by the code external to the
 * routing subsystem.
 */

#ifndef	_NET_ROUTE_SHARED_H_
#define	_NET_ROUTE_SHARED_H_

#include <vm/uma.h>

#ifdef	RTDEBUG
#define	DPRINTF(_fmt, ...)	printf("%s: " _fmt "\n", __func__ , ## __VA_ARGS__)
#else
#define	DPRINTF(_fmt, ...)
#endif

struct rib_head;

/* Nexhops */
void nhops_init(void);
int nhops_init_rib(struct rib_head *rh);
void nhops_destroy_rib(struct rib_head *rh);
void nhop_ref_object(struct nhop_object *nh);
int nhop_try_ref_object(struct nhop_object *nh);
int nhop_ref_any(struct nhop_object *nh);
void nhop_free_any(struct nhop_object *nh);

void nhop_set_type(struct nhop_object *nh, enum nhop_type nh_type);
void nhop_set_rtflags(struct nhop_object *nh, int rt_flags);

int nhop_create_from_info(struct rib_head *rnh, struct rt_addrinfo *info,
    struct nhop_object **nh_ret);
int nhop_create_from_nhop(struct rib_head *rnh, const struct nhop_object *nh_orig,
    struct rt_addrinfo *info, struct nhop_object **pnh_priv);

void nhops_update_ifmtu(struct rib_head *rh, struct ifnet *ifp, uint32_t mtu);
int nhops_dump_sysctl(struct rib_head *rh, struct sysctl_req *w);

/* subscriptions */
void rib_init_subscriptions(struct rib_head *rnh);
void rib_destroy_subscriptions(struct rib_head *rnh);

/* route */
struct rtentry *rt_unlinkrte(struct rib_head *rnh, struct rt_addrinfo *info,
    int *perror);

#endif

