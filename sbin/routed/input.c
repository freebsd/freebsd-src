/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.17 $"

#include "defs.h"

static void input(struct sockaddr_in *, struct interface *, struct interface *,
		  struct rip *, int);
static void input_route(struct interface *, naddr,
			naddr, naddr, naddr, struct netinfo *);
static int ck_passwd(struct interface *, struct rip *, void *,
		     naddr, struct msg_limit *);


/* process RIP input
 */
void
read_rip(int sock,
	 struct interface *sifp)
{
	static struct msg_limit  bad_name;
	struct sockaddr_in from;
	struct interface *aifp;
	int fromlen, cc;
	struct {
#ifdef USE_PASSIFNAME
		char	ifname[IFNAMSIZ];
#endif
		union pkt_buf pbuf;
	} inbuf;


	for (;;) {
		fromlen = sizeof(from);
		cc = recvfrom(sock, &inbuf, sizeof(inbuf), 0,
			      (struct sockaddr*)&from, &fromlen);
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				LOGERR("recvfrom(rip)");
			break;
		}
		if (fromlen != sizeof(struct sockaddr_in))
			logbad(1,"impossible recvfrom(rip) fromlen=%d",
			       fromlen);

		/* aifp is the "authenticated" interface via which the packet
		 *	arrived.  In fact, it is only the interface on which
		 *	the packet should have arrived based on is source
		 *	address.
		 * sifp is interface associated with the socket through which
		 *	the packet was received.
		 */
#ifdef USE_PASSIFNAME
		if ((cc -= sizeof(inbuf.ifname)) < 0)
			logbad(0,"missing USE_PASSIFNAME; only %d bytes",
			       cc+sizeof(inbuf.ifname));

		/* check the remote interfaces first */
		for (aifp = remote_if; aifp; aifp = aifp->int_rlink) {
			if (aifp->int_addr == from.sin_addr.s_addr)
				break;
		}
		if (aifp == 0) {
			aifp = ifwithname(inbuf.ifname, 0);
			if (aifp == 0) {
				/* maybe it is a new interface */
				ifinit();
				aifp = ifwithname(inbuf.ifname, 0);
				if (aifp == 0) {
					msglim(&bad_name, from.sin_addr.s_addr,
					       "impossible interface name"
					       " %.*s", IFNAMSIZ,
					       inbuf.ifname);
				}
			}

			/* If it came via the wrong interface, do not
			 * trust it.
			 */
			if (((aifp->int_if_flags & IFF_POINTOPOINT)
			     && aifp->int_dstaddr != from.sin_addr.s_addr)
			    || (!(aifp->int_if_flags & IFF_POINTOPOINT)
				&& !on_net(from.sin_addr.s_addr,
					   aifp->int_net, aifp->int_mask)))
				aifp = 0;
		}
#else
		aifp = iflookup(from.sin_addr.s_addr);
#endif
		if (sifp == 0)
			sifp = aifp;

		input(&from, sifp, aifp, &inbuf.pbuf.rip, cc);
	}
}


/* Process a RIP packet
 */
static void
input(struct sockaddr_in *from,		/* received from this IP address */
      struct interface *sifp,		/* interface of incoming socket */
      struct interface *aifp,		/* "authenticated" interface */
      struct rip *rip,
      int cc)
{
#	define FROM_NADDR from->sin_addr.s_addr
	static struct msg_limit use_auth, bad_len, bad_mask;
	static struct msg_limit  unk_router, bad_router, bad_nhop;

	struct rt_entry *rt;
	struct netinfo *n, *lim;
	struct interface *ifp1;
	naddr gate, mask, v1_mask, dst, ddst_h;
	struct auth_key *ap;
	int i;

	/* Notice when we hear from a remote gateway
	 */
	if (aifp != 0
	    && (aifp->int_state & IS_REMOTE))
		aifp->int_act_time = now.tv_sec;

	trace_rip("Recv", "from", from, sifp, rip, cc);

	if (rip->rip_vers == 0) {
		msglim(&bad_router, FROM_NADDR,
		       "RIP version 0, cmd %d, packet received from %s",
		       rip->rip_cmd, naddr_ntoa(FROM_NADDR));
		return;
	} else if (rip->rip_vers > RIPv2) {
		rip->rip_vers = RIPv2;
	}
	if (cc > OVER_MAXPACKETSIZE) {
		msglim(&bad_router, FROM_NADDR,
		       "packet at least %d bytes too long received from %s",
		       cc-MAXPACKETSIZE, naddr_ntoa(FROM_NADDR));
		return;
	}

	n = rip->rip_nets;
	lim = (struct netinfo *)((char*)rip + cc);

	/* Notice authentication.
	 * As required by section 4.2 in RFC 1723, discard authenticated
	 * RIPv2 messages, but only if configured for that silliness.
	 *
	 * RIPv2 authentication is lame.  Why authenticate queries?
	 * Why should a RIPv2 implementation with authentication disabled
	 * not be able to listen to RIPv2 packets with authenication, while
	 * RIPv1 systems will listen?  Crazy!
	 */
	if (!auth_ok
	    && rip->rip_vers == RIPv2
	    && n < lim && n->n_family == RIP_AF_AUTH) {
		msglim(&use_auth, FROM_NADDR,
		       "RIPv2 message with authentication from %s discarded",
		       naddr_ntoa(FROM_NADDR));
		return;
	}

	switch (rip->rip_cmd) {
	case RIPCMD_REQUEST:
		/* For mere requests, be a little sloppy about the source
		 */
		if (aifp == 0)
			aifp = sifp;

		/* Are we talking to ourself or a remote gateway?
		 */
		ifp1 = ifwithaddr(FROM_NADDR, 0, 1);
		if (ifp1) {
			if (ifp1->int_state & IS_REMOTE) {
				/* remote gateway */
				aifp = ifp1;
				if (check_remote(aifp)) {
					aifp->int_act_time = now.tv_sec;
					(void)if_ok(aifp, "remote ");
				}
			} else if (from->sin_port == htons(RIP_PORT)) {
				trace_pkt("    discard our own RIP request");
				return;
			}
		}

		/* did the request come from a router?
		 */
		if (from->sin_port == htons(RIP_PORT)) {
			/* yes, ignore the request if RIP is off so that
			 * the router does not depend on us.
			 */
			if (rip_sock < 0
			    || (aifp != 0
				&& IS_RIP_OUT_OFF(aifp->int_state))) {
				trace_pkt("    discard request while RIP off");
				return;
			}
		}

		/* According to RFC 1723, we should ignore unathenticated
		 * queries.  That is too silly to bother with.  Sheesh!
		 * Are forwarding tables supposed to be secret, when
		 * a bad guy can infer them with test traffic?  When RIP
		 * is still the most common router-discovery protocol
		 * and so hosts need to send queries that will be answered?
		 * What about `rtquery`?
		 * Maybe on firewalls you'd care, but not enough to
		 * give up the diagnostic facilities of remote probing.
		 */

		if (n >= lim) {
			msglim(&bad_len, FROM_NADDR, "empty request from %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (cc%sizeof(*n) != sizeof(struct rip)%sizeof(*n)) {
			msglim(&bad_len, FROM_NADDR,
			       "request of bad length (%d) from %s",
			       cc, naddr_ntoa(FROM_NADDR));
		}

		if (rip->rip_vers == RIPv2
		    && (aifp == 0 || (aifp->int_state & IS_NO_RIPV1_OUT))) {
			v12buf.buf->rip_vers = RIPv2;
			/* If we have a secret but it is a cleartext secret,
			 * do not disclose our secret unless the other guy
			 * already knows it.
			 */
			if (aifp != 0
			    && aifp->int_auth.type == RIP_AUTH_PW
			    && !ck_passwd(aifp,rip,lim,FROM_NADDR,&use_auth))
				ap = 0;
			else
				ap = find_auth(aifp);
		} else {
			v12buf.buf->rip_vers = RIPv1;
			ap = 0;
		}
		clr_ws_buf(&v12buf, ap, aifp);

		do {
			NTOHL(n->n_metric);

			/* A single entry with family RIP_AF_UNSPEC and
			 * metric HOPCNT_INFINITY means "all routes".
			 * We respond to routers only if we are acting
			 * as a supplier, or to anyone other than a router
			 * (i.e. a query).
			 */
			if (n->n_family == RIP_AF_UNSPEC
			    && n->n_metric == HOPCNT_INFINITY) {
				if (from->sin_port != htons(RIP_PORT)) {
					/* Answer a query from a utility
					 * program with all we know.
					 */
					supply(from, aifp, OUT_QUERY, 0,
					       rip->rip_vers, ap != 0);
					return;
				}

				/* A router trying to prime its tables.
				 * Filter the answer in the about same way
				 * broadcasts are filtered.
				 *
				 * Only answer a router if we are a supplier
				 * to keep an unwary host that is just starting
				 * from picking us as a router.  Respond with
				 * RIPv1 instead of RIPv2 if that is what we
				 * are broadcasting on the interface to keep
				 * the remote router from getting the wrong
				 * initial idea of the routes we send.
				 */
				if (aifp == 0) {
					trace_pkt("ignore distant router");
					return;
				}
				if (!supplier
				    || IS_RIP_OFF(aifp->int_state)) {
					trace_pkt("ignore; not supplying");
					return;
				}

				supply(from, aifp, OUT_UNICAST, 0,
				       (aifp->int_state&IS_NO_RIPV1_OUT)
				       ? RIPv2 : RIPv1,
				       ap != 0);
				return;
			}

			/* Ignore authentication */
			if (n->n_family == RIP_AF_AUTH)
				continue;

			if (n->n_family != RIP_AF_INET) {
				msglim(&bad_router, FROM_NADDR,
				       "request from %s for unsupported (af"
				       " %d) %s",
				       naddr_ntoa(FROM_NADDR),
				       ntohs(n->n_family),
				       naddr_ntoa(n->n_dst));
				return;
			}

			/* We are being asked about a specific destination.
			 */
			dst = n->n_dst;
			if (!check_dst(dst)) {
				msglim(&bad_router, FROM_NADDR,
				       "bad queried destination %s from %s",
				       naddr_ntoa(dst),
				       naddr_ntoa(FROM_NADDR));
				return;
			}

			/* decide what mask was intended */
			if (rip->rip_vers == RIPv1
			    || 0 == (mask = ntohl(n->n_mask))
			    || 0 != (ntohl(dst) & ~mask))
				mask = ripv1_mask_host(dst, aifp);

			/* try to find the answer */
			rt = rtget(dst, mask);
			if (!rt && dst != RIP_DEFAULT)
				rt = rtfind(n->n_dst);

			if (v12buf.buf->rip_vers != RIPv1)
				v12buf.n->n_mask = mask;
			if (rt == 0) {
				/* we do not have the answer */
				v12buf.n->n_metric = HOPCNT_INFINITY;
			} else {
				/* we have the answer, so compute the
				 * right metric and next hop.
				 */
				v12buf.n->n_family = RIP_AF_INET;
				v12buf.n->n_dst = dst;
				v12buf.n->n_metric = (rt->rt_metric+1
						      + ((aifp!=0)
							  ? aifp->int_metric
							  : 1));
				if (v12buf.n->n_metric > HOPCNT_INFINITY)
					v12buf.n->n_metric = HOPCNT_INFINITY;
				if (v12buf.buf->rip_vers != RIPv1) {
					v12buf.n->n_tag = rt->rt_tag;
					v12buf.n->n_mask = mask;
					if (aifp != 0
					    && on_net(rt->rt_gate,
						      aifp->int_net,
						      aifp->int_mask)
					    && rt->rt_gate != aifp->int_addr)
					    v12buf.n->n_nhop = rt->rt_gate;
				}
			}
			HTONL(v12buf.n->n_metric);

			/* Stop paying attention if we fill the output buffer.
			 */
			if (++v12buf.n >= v12buf.lim)
				break;
		} while (++n < lim);

		/* Send the answer about specific routes.
		 */
		if (ap != 0 && aifp->int_auth.type == RIP_AUTH_MD5)
			end_md5_auth(&v12buf, ap);

		if (from->sin_port != htons(RIP_PORT)) {
			/* query */
			(void)output(OUT_QUERY, from, aifp,
				     v12buf.buf,
				     ((char *)v12buf.n - (char*)v12buf.buf));
		} else if (supplier) {
			(void)output(OUT_UNICAST, from, aifp,
				     v12buf.buf,
				     ((char *)v12buf.n - (char*)v12buf.buf));
		} else {
			/* Only answer a router if we are a supplier
			 * to keep an unwary host that is just starting
			 * from picking us an a router.
			 */
			;
		}
		return;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
		/* verify message came from a privileged port */
		if (ntohs(from->sin_port) > IPPORT_RESERVED) {
			msglog("trace command from untrusted port on %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (aifp == 0) {
			msglog("trace command from unknown router %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (rip->rip_cmd == RIPCMD_TRACEON) {
			rip->rip_tracefile[cc-4] = '\0';
			trace_on((char*)rip->rip_tracefile, 0);
		} else {
			trace_off("tracing turned off by %s\n",
				  naddr_ntoa(FROM_NADDR));
		}
		return;

	case RIPCMD_RESPONSE:
		if (cc%sizeof(*n) != sizeof(struct rip)%sizeof(*n)) {
			msglim(&bad_len, FROM_NADDR,
			       "response of bad length (%d) from %s",
			       cc, naddr_ntoa(FROM_NADDR));
		}

		/* verify message came from a router */
		if (from->sin_port != ntohs(RIP_PORT)) {
			msglim(&bad_router, FROM_NADDR,
			       "    discard RIP response from unknown port"
			       " %d", from->sin_port);
			return;
		}

		if (rip_sock < 0) {
			trace_pkt("    discard response while RIP off");
			return;
		}

		/* Are we talking to ourself or a remote gateway?
		 */
		ifp1 = ifwithaddr(FROM_NADDR, 0, 1);
		if (ifp1) {
			if (ifp1->int_state & IS_REMOTE) {
				/* remote gateway */
				aifp = ifp1;
				if (check_remote(aifp)) {
					aifp->int_act_time = now.tv_sec;
					(void)if_ok(aifp, "remote ");
				}
			} else {
				trace_pkt("    discard our own RIP response");
				return;
			}
		}

		/* Accept routing packets from routers directly connected
		 * via broadcast or point-to-point networks, and from
		 * those listed in /etc/gateways.
		 */
		if (aifp == 0) {
			msglim(&unk_router, FROM_NADDR,
			       "   discard response from %s"
			       " via unexpected interface",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (IS_RIP_IN_OFF(aifp->int_state)) {
			trace_pkt("    discard RIPv%d response"
				  " via disabled interface %s",
				  rip->rip_vers, aifp->int_name);
			return;
		}

		if (n >= lim) {
			msglim(&bad_len, FROM_NADDR, "empty response from %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}

		if (((aifp->int_state & IS_NO_RIPV1_IN)
		     && rip->rip_vers == RIPv1)
		    || ((aifp->int_state & IS_NO_RIPV2_IN)
			&& rip->rip_vers != RIPv1)) {
			trace_pkt("    discard RIPv%d response",
				  rip->rip_vers);
			return;
		}

		/* Ignore routes via dead interface.
		 */
		if (aifp->int_state & IS_BROKE) {
			trace_pkt("%sdiscard response via broken interface %s",
				  aifp->int_name);
			return;
		}

		/* If the interface cares, ignore bad routers.
		 * Trace but do not log this problem because when it
		 * happens it happens a lot.
		 */
		if (aifp->int_state & IS_DISTRUST) {
			struct tgate *tg = tgates;
			while (tg->tgate_addr != FROM_NADDR) {
				tg = tg->tgate_next;
				if (tg == 0) {
					trace_pkt("    discard RIP response"
						  " from untrusted router %s",
						  naddr_ntoa(FROM_NADDR));
					return;
				}
			}
		}

		/* Authenticate the packet if we have a secret.
		 * If we do not, ignore the silliness in RFC 1723
		 * and accept it regardless.
		 */
		if (aifp->int_auth.type != RIP_AUTH_NONE
		    && rip->rip_vers != RIPv1) {
			if (!ck_passwd(aifp,rip,lim,FROM_NADDR,&use_auth))
				return;
		}

		do {
			if (n->n_family == RIP_AF_AUTH)
				continue;

			NTOHL(n->n_metric);
			dst = n->n_dst;
			if (n->n_family != RIP_AF_INET
			    && (n->n_family != RIP_AF_UNSPEC
				|| dst != RIP_DEFAULT)) {
				msglim(&bad_router, FROM_NADDR,
				       "route from %s to unsupported"
				       " address family=%d destination=%s",
				       naddr_ntoa(FROM_NADDR),
				       n->n_family,
				       naddr_ntoa(dst));
				continue;
			}
			if (!check_dst(dst)) {
				msglim(&bad_router, FROM_NADDR,
				       "bad destination %s from %s",
				       naddr_ntoa(dst),
				       naddr_ntoa(FROM_NADDR));
				return;
			}
			if (n->n_metric == 0
			    || n->n_metric > HOPCNT_INFINITY) {
				msglim(&bad_router, FROM_NADDR,
				       "bad metric %d from %s"
				       " for destination %s",
				       n->n_metric,
				       naddr_ntoa(FROM_NADDR),
				       naddr_ntoa(dst));
				return;
			}

			/* Notice the next-hop.
			 */
			gate = FROM_NADDR;
			if (n->n_nhop != 0) {
				if (rip->rip_vers == RIPv2) {
					n->n_nhop = 0;
				} else {
				    /* Use it only if it is valid. */
				    if (on_net(n->n_nhop,
					       aifp->int_net, aifp->int_mask)
					&& check_dst(n->n_nhop)) {
					    gate = n->n_nhop;
				    } else {
					    msglim(&bad_nhop, FROM_NADDR,
						   "router %s to %s"
						   " has bad next hop %s",
						   naddr_ntoa(FROM_NADDR),
						   naddr_ntoa(dst),
						   naddr_ntoa(n->n_nhop));
					    n->n_nhop = 0;
				    }
				}
			}

			if (rip->rip_vers == RIPv1
			    || 0 == (mask = ntohl(n->n_mask))) {
				mask = ripv1_mask_host(dst,aifp);
			} else if ((ntohl(dst) & ~mask) != 0) {
				msglim(&bad_mask, FROM_NADDR,
				       "router %s sent bad netmask"
				       " %#x with %s",
				       naddr_ntoa(FROM_NADDR),
				       mask,
				       naddr_ntoa(dst));
				continue;
			}
			if (rip->rip_vers == RIPv1)
				n->n_tag = 0;

			/* Adjust metric according to incoming interface..
			 */
			n->n_metric += aifp->int_metric;
			if (n->n_metric > HOPCNT_INFINITY)
				n->n_metric = HOPCNT_INFINITY;

			/* Recognize and ignore a default route we faked
			 * which is being sent back to us by a machine with
			 * broken split-horizon.
			 * Be a little more paranoid than that, and reject
			 * default routes with the same metric we advertised.
			 */
			if (aifp->int_d_metric != 0
			    && dst == RIP_DEFAULT
			    && n->n_metric >= aifp->int_d_metric)
				continue;

			/* We can receive aggregated RIPv2 routes that must
			 * be broken down before they are transmitted by
			 * RIPv1 via an interface on a subnet.
			 * We might also receive the same routes aggregated
			 * via other RIPv2 interfaces.
			 * This could cause duplicate routes to be sent on
			 * the RIPv1 interfaces.  "Longest matching variable
			 * length netmasks" lets RIPv2 listeners understand,
			 * but breaking down the aggregated routes for RIPv1
			 * listeners can produce duplicate routes.
			 *
			 * Breaking down aggregated routes here bloats
			 * the daemon table, but does not hurt the kernel
			 * table, since routes are always aggregated for
			 * the kernel.
			 *
			 * Notice that this does not break down network
			 * routes corresponding to subnets.  This is part
			 * of the defense against RS_NET_SYN.
			 */
			if (have_ripv1_out
			    && (((rt = rtget(dst,mask)) == 0
				 || !(rt->rt_state & RS_NET_SYN)))
			    && (v1_mask = ripv1_mask_net(dst,0)) > mask) {
				ddst_h = v1_mask & -v1_mask;
				i = (v1_mask & ~mask)/ddst_h;
				if (i >= 511) {
					/* Punt if we would have to generate
					 * an unreasonable number of routes.
					 */
#ifdef DEBUG
					msglog("accept %s from %s as 1"
					       " instead of %d routes",
					       addrname(dst,mask,0),
					       naddr_ntoa(FROM_NADDR),
					       i+1);
#endif
					i = 0;
				} else {
					mask = v1_mask;
				}
			} else {
				i = 0;
			}

			for (;;) {
				input_route(aifp, FROM_NADDR,
					    dst, mask, gate, n);
				if (i-- == 0)
					break;
				dst = htonl(ntohl(dst) + ddst_h);
			}
		} while (++n < lim);
		break;
	}
#undef FROM_NADDR
}


/* Process a single input route.
 */
static void
input_route(struct interface *ifp,
	    naddr from,
	    naddr dst,
	    naddr mask,
	    naddr gate,
	    struct netinfo *n)
{
	int i;
	struct rt_entry *rt;
	struct rt_spare *rts, *rts0;
	struct interface *ifp1;
	time_t new_time;


	/* See if the other guy is telling us to send our packets to him.
	 * Sometimes network routes arrive over a point-to-point link for
	 * the network containing the address(es) of the link.
	 *
	 * If our interface is broken, switch to using the other guy.
	 */
	ifp1 = ifwithaddr(dst, 1, 1);
	if (ifp1 != 0
	    && (!(ifp1->int_state & IS_BROKE)
		|| (ifp1->int_state & IS_PASSIVE)))
		return;

	/* Look for the route in our table.
	 */
	rt = rtget(dst, mask);

	/* Consider adding the route if we do not already have it.
	 */
	if (rt == 0) {
		/* Ignore unknown routes being poisoned.
		 */
		if (n->n_metric == HOPCNT_INFINITY)
			return;

		/* Ignore the route if it points to us */
		if (n->n_nhop != 0
		    && 0 != ifwithaddr(n->n_nhop, 1, 0))
			return;

		/* If something has not gone crazy and tried to fill
		 * our memory, accept the new route.
		 */
		if (total_routes < MAX_ROUTES)
			rtadd(dst, mask, gate, from, n->n_metric,
			      n->n_tag, 0, ifp);
		return;
	}

	/* We already know about the route.  Consider this update.
	 *
	 * If (rt->rt_state & RS_NET_SYN), then this route
	 * is the same as a network route we have inferred
	 * for subnets we know, in order to tell RIPv1 routers
	 * about the subnets.
	 *
	 * It is impossible to tell if the route is coming
	 * from a distant RIPv2 router with the standard
	 * netmask because that router knows about the entire
	 * network, or if it is a round-about echo of a
	 * synthetic, RIPv1 network route of our own.
	 * The worst is that both kinds of routes might be
	 * received, and the bad one might have the smaller
	 * metric.  Partly solve this problem by never
	 * aggregating into such a route.  Also keep it
	 * around as long as the interface exists.
	 */

	rts0 = rt->rt_spares;
	for (rts = rts0, i = NUM_SPARES; i != 0; i--, rts++) {
		if (rts->rts_router == from)
			break;
		/* Note the worst slot to reuse,
		 * other than the current slot.
		 */
		if (rts0 == rt->rt_spares
		    || BETTER_LINK(rt, rts0, rts))
			rts0 = rts;
	}
	if (i != 0) {
		/* Found the router
		 */
		int old_metric = rts->rts_metric;

		/* Keep poisoned routes around only long enough to pass
		 * the poison on.  Get a new timestamp for good routes.
		 */
		new_time =((old_metric == HOPCNT_INFINITY)
			   ? rts->rts_time
			   : now.tv_sec);

		/* If this is an update for the router we currently prefer,
		 * then note it.
		 */
		if (i == NUM_SPARES) {
			rtchange(rt,rt->rt_state, gate,rt->rt_router,
				 n->n_metric, n->n_tag, ifp, new_time, 0);
			/* If the route got worse, check for something better.
			 */
			if (n->n_metric > old_metric)
				rtswitch(rt, 0);
			return;
		}

		/* This is an update for a spare route.
		 * Finished if the route is unchanged.
		 */
		if (rts->rts_gate == gate
		    && old_metric == n->n_metric
		    && rts->rts_tag == n->n_tag) {
			rts->rts_time = new_time;
			return;
		}

	} else {
		/* The update is for a route we know about,
		 * but not from a familiar router.
		 *
		 * Ignore the route if it points to us.
		 */
		if (n->n_nhop != 0
		    && 0 != ifwithaddr(n->n_nhop, 1, 0))
			return;

		rts = rts0;

		/* Save the route as a spare only if it has
		 * a better metric than our worst spare.
		 * This also ignores poisoned routes (those
		 * received with metric HOPCNT_INFINITY).
		 */
		if (n->n_metric >= rts->rts_metric)
			return;

		new_time = now.tv_sec;
	}

	trace_upslot(rt, rts, gate, from, ifp, n->n_metric,n->n_tag, new_time);

	rts->rts_gate = gate;
	rts->rts_router = from;
	rts->rts_metric = n->n_metric;
	rts->rts_tag = n->n_tag;
	rts->rts_time = new_time;
	rts->rts_ifp = ifp;

	/* try to switch to a better route */
	rtswitch(rt, rts);
}


static int				/* 0 if bad */
ck_passwd(struct interface *aifp,
	  struct rip *rip,
	  void *lim,
	  naddr from,
	  struct msg_limit *use_authp)
{
#	define NA (rip->rip_auths)
#	define DAY (24*60*60)
	struct netauth *na2;
	struct auth_key *akp = aifp->int_auth.keys;
	MD5_CTX md5_ctx;
	u_char hash[RIP_AUTH_PW_LEN];
	int i;


	if ((void *)NA >= lim || NA->a_family != RIP_AF_AUTH) {
		msglim(use_authp, from, "missing password from %s",
		       naddr_ntoa(from));
		return 0;
	}

	if (NA->a_type != aifp->int_auth.type) {
		msglim(use_authp, from, "wrong type of password from %s",
		       naddr_ntoa(from));
		return 0;
	}

	if (NA->a_type == RIP_AUTH_PW) {
		/* accept any current cleartext password
		 */
		for (i = 0; i < MAX_AUTH_KEYS; i++, akp++) {
			if ((u_long)akp->start-DAY > (u_long)clk.tv_sec
			    || (u_long)akp->end+DAY < (u_long)clk.tv_sec)
				continue;

			if (!bcmp(NA->au.au_pw, akp->key, RIP_AUTH_PW_LEN))
				return 1;
		}

	} else {
		/* accept any current MD5 secret with the right key ID
		 */
		for (i = 0; i < MAX_AUTH_KEYS; i++, akp++) {
			if (NA->au.a_md5.md5_keyid == akp->keyid
			    && (u_long)akp->start-DAY <= (u_long)clk.tv_sec
			    && (u_long)akp->end+DAY >= (u_long)clk.tv_sec)
				break;
		}

		if (i < MAX_AUTH_KEYS) {
			na2 = (struct netauth *)((char *)(NA+1)
						 + NA->au.a_md5.md5_pkt_len);
			if (NA->au.a_md5.md5_pkt_len % sizeof(*NA) != 0
			    || lim < (void *)(na2+1)) {
				msglim(use_authp, from,
				       "bad MD5 RIP-II pkt length %d from %s",
				       NA->au.a_md5.md5_pkt_len,
				       naddr_ntoa(from));
				return 0;
			}
			MD5Init(&md5_ctx);
			MD5Update(&md5_ctx, (u_char *)NA,
				  (char *)na2->au.au_pw - (char *)NA);
			MD5Update(&md5_ctx,
				  (u_char *)akp->key, sizeof(akp->key));
			MD5Final(hash, &md5_ctx);
			if (na2->a_family == RIP_AF_AUTH
			    && na2->a_type == 1
			    && NA->au.a_md5.md5_auth_len == RIP_AUTH_PW_LEN
			    && !bcmp(hash, na2->au.au_pw, sizeof(hash)))
				return 1;
		}
	}

	msglim(use_authp, from, "bad password from %s",
	       naddr_ntoa(from));
	return 0;
#undef NA
}
