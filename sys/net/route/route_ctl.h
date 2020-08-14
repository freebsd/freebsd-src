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
 * This header file contains public functions and structures used for
 * routing table manipulations.
 */

#ifndef	_NET_ROUTE_ROUTE_CTL_H_
#define	_NET_ROUTE_ROUTE_CTL_H_

struct rib_cmd_info {
	uint8_t			rc_cmd;		/* RTM_ADD|RTM_DEL|RTM_CHANGE */
	uint8_t			spare[3];
	uint32_t		rc_nh_weight;	/* new nhop weight */
	struct rtentry		*rc_rt;		/* Target entry */
	struct nhop_object	*rc_nh_old;	/* Target nhop OR mpath */
	struct nhop_object	*rc_nh_new;	/* Target nhop OR mpath */
};


int rib_add_route(uint32_t fibnum, struct rt_addrinfo *info,
  struct rib_cmd_info *rc);
int rib_del_route(uint32_t fibnum, struct rt_addrinfo *info,
  struct rib_cmd_info *rc);
int rib_change_route(uint32_t fibnum, struct rt_addrinfo *info,
  struct rib_cmd_info *rc);
int rib_action(uint32_t fibnum, int action, struct rt_addrinfo *info,
  struct rib_cmd_info *rc);

int rib_add_redirect(u_int fibnum, struct sockaddr *dst,
  struct sockaddr *gateway, struct sockaddr *author, struct ifnet *ifp,
  int flags, int expire_sec);

typedef int rt_walktree_f_t(struct rtentry *, void *);
void rib_walk(int af, u_int fibnum, rt_walktree_f_t *wa_f, void *arg);
void rib_walk_del(u_int fibnum, int family, rt_filter_f_t *filter_f,
  void *arg, bool report);

typedef void rt_setwarg_t(struct rib_head *, uint32_t, int, void *);
void rt_foreach_fib_walk(int af, rt_setwarg_t *, rt_walktree_f_t *, void *);
void rt_foreach_fib_walk_del(int af, rt_filter_f_t *filter_f, void *arg);

enum rib_subscription_type {
	RIB_NOTIFY_IMMEDIATE,
	RIB_NOTIFY_DELAYED
};

struct rib_subscription;
typedef void rib_subscription_cb_t(struct rib_head *rnh, struct rib_cmd_info *rc,
    void *arg);

struct rib_subscription *rib_subscribe(uint32_t fibnum, int family,
    rib_subscription_cb_t *f, void *arg, enum rib_subscription_type type,
    bool waitok);
struct rib_subscription *rib_subscribe_internal(struct rib_head *rnh,
    rib_subscription_cb_t *f, void *arg, enum rib_subscription_type type,
    bool waitok);
int rib_unsibscribe(uint32_t fibnum, int family, struct rib_subscription *rs);

#endif

