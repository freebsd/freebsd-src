/*
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  Copyright (c) 1998 by the University of Southern California.
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and
 *  its documentation in source and binary forms for lawful
 *  non-commercial purposes and without fee is hereby granted, provided
 *  that the above copyright notice appear in all copies and that both
 *  the copyright notice and this permission notice appear in supporting
 *  documentation, and that any documentation, advertising materials,
 *  and other materials related to such distribution and use acknowledge
 *  that the software was developed by the University of Southern
 *  California and/or Information Sciences Institute.
 *  The name of the University of Southern California may not
 *  be used to endorse or promote products derived from this software
 *  without specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA DOES NOT MAKE ANY REPRESENTATIONS
 *  ABOUT THE SUITABILITY OF THIS SOFTWARE FOR ANY PURPOSE.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND 
 *  NON-INFRINGEMENT.
 *
 *  IN NO EVENT SHALL USC, OR ANY OTHER CONTRIBUTOR BE LIABLE FOR ANY
 *  SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN CONTRACT,
 *  TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN CONNECTION WITH,
 *  THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 */
/*
 *  Questions concerning this software should be directed to 
 *  Pavlin Ivanov Radoslavov (pavlin@catarina.usc.edu)
 *
 *  $Id: trace.c,v 1.5 1999/09/16 08:46:00 jinmei Exp $
 */
/*
 * Part of this program has been derived from mrouted.
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted".
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 * $FreeBSD: src/usr.sbin/pim6dd/trace.c,v 1.1.2.1 2000/07/15 07:36:30 kris Exp $
 */


#include "defs.h"
#include "trace.h"

/* TODO */
/*
 * Traceroute function which returns traceroute replies to the requesting
 * router. Also forwards the request to downstream routers.
 */
void
accept_mtrace(src, dst, group, ifindex, data, no, datalen)
	struct sockaddr_in6 *src;
	struct in6_addr *dst;
	struct in6_addr *group;
	int ifindex;
	char *data;
	u_int no;   /* promoted u_char */
	int datalen;
{
	u_char type;
	mrtentry_t *mrt;
	struct tr6_query *qry;
	struct tr6_resp  *resp;
	int vifi, ovifi;
	char *p;
	int rcount;
	int errcode = TR_NO_ERR;
	int resptype;
	struct timeval tp;
	struct sioc_mif_req6 mreq;
	struct in6_addr parent_address;
	struct sockaddr_in6 src_sa6 = {sizeof(src_sa6), AF_INET6};
	struct sockaddr_in6 dst_sa6 = {sizeof(dst_sa6), AF_INET6};
	struct sockaddr_in6 resp_sa6 = {sizeof(resp_sa6), AF_INET6};
	struct sockaddr_in6 grp_sa6 = {sizeof(grp_sa6), AF_INET6};
	struct sockaddr_in6 *sa_global;
#ifdef SM_ONLY
	rpentry_t *rpentry_ptr;
#endif 

	/* Remember qid across invocations */
	static u_int32 oqid = 0;

	/* timestamp the request/response */
	gettimeofday(&tp, 0);

	/*
	 * Check if it is a query or a response
	 */
	if (datalen == QLEN) {
		type = QUERY;
		IF_DEBUG(DEBUG_TRACE)
			log(LOG_DEBUG, 0, "Initial traceroute query rcvd "
			    "from %s to %s",
			    inet6_fmt(&src->sin6_addr),
			    inet6_fmt(dst));
	}
	else if ((datalen - QLEN) % RLEN == 0) {
		type = RESP;
		IF_DEBUG(DEBUG_TRACE)
			log(LOG_DEBUG, 0, "In-transit traceroute query rcvd "
			    "from %s to %s",
			    inet6_fmt(&src->sin6_addr),
			    inet6_fmt(dst));
		if (IN6_IS_ADDR_MULTICAST(dst)) {
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0, "Dropping multicast response");
			return;
		}
	}
	else {
		log(LOG_WARNING, 0, "%s from %s to %s",
		    "Non decipherable traceroute request recieved",
		    inet6_fmt(&src->sin6_addr), inet6_fmt(dst));
		return;
	}

	qry = (struct tr6_query *)data;
	src_sa6.sin6_addr = qry->tr_src;
	src_sa6.sin6_scope_id =
		(IN6_IS_ADDR_LINKLOCAL(&qry->tr_src)
		 || IN6_IS_ADDR_MC_LINKLOCAL(&qry->tr_src)) ? ifindex : 0;
	dst_sa6.sin6_addr = qry->tr_dst;
	dst_sa6.sin6_scope_id =
		(IN6_IS_ADDR_LINKLOCAL(&qry->tr_dst)
		 || IN6_IS_ADDR_MC_LINKLOCAL(&qry->tr_dst)) ? ifindex : 0;
	grp_sa6.sin6_addr = *group;
	grp_sa6.sin6_scope_id = 0;

	/*
	 * if it is a packet with all reports filled, drop it
	 */
	if ((rcount = (datalen - QLEN)/RLEN) == no) {
		IF_DEBUG(DEBUG_TRACE)
			log(LOG_DEBUG, 0, "packet with all reports filled in");
		return;
	}

	IF_DEBUG(DEBUG_TRACE) {
		log(LOG_DEBUG, 0, "s: %s g: %s d: %s ",
		    inet6_fmt(&qry->tr_src),
		    inet6_fmt(group), inet6_fmt(&qry->tr_dst));
		log(LOG_DEBUG, 0, "rhlim: %d rd: %s", qry->tr_rhlim,
		    inet6_fmt(&qry->tr_raddr));
		log(LOG_DEBUG, 0, "rcount:%d, qid:%06x", rcount, qry->tr_qid);
	}

	/* determine the routing table entry for this traceroute */
	mrt = find_route(&src_sa6, &grp_sa6, MRTF_SG | MRTF_WC | MRTF_PMBR,
			 DONT_CREATE);
	IF_DEBUG(DEBUG_TRACE) {
		if (mrt != (mrtentry_t *)NULL) {
			if (mrt->upstream != (pim_nbr_entry_t *)NULL)
				parent_address = mrt->upstream->address.sin6_addr;
			else
				parent_address = in6addr_any;
			log(LOG_DEBUG, 0,
			    "mrt parent mif: %d rtr: %s metric: %d",
			    mrt->incoming,
			    inet6_fmt(&parent_address), mrt->metric);
			/* TODO
			   log(LOG_DEBUG, 0, "mrt origin %s",
			   RT_FMT(rt, s1));
			*/
		} else
			log(LOG_DEBUG, 0, "...no route");
	}
    
	/*
	 * Query type packet - check if rte exists 
	 * Check if the query destination is a vif connected to me.
	 * and if so, whether I should start response back
	 */
	if (type == QUERY) {
		if (oqid == qry->tr_qid) {
			/*
			 * If the multicast router is a member of the group
			 * being queried, and the query is multicasted,
			 * then the router can recieve multiple copies of
			 * the same query.  If we have already replied to
			 * this traceroute, just ignore it this time.
			 *
			 * This is not a total solution, but since if this
			 * fails you only get N copies, N <= the number of
			 * interfaces on the router, it is not fatal.
			 */
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "ignoring duplicate traceroute packet");
			return;
		}

		if (mrt == (mrtentry_t *)NULL) {
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "Mcast traceroute: no route entry %s",
				    inet6_fmt(&qry->tr_src));
#if 0
			if (IN6_IS_ADDR_MULTICAST(dst))
				return;
#endif
		}
		vifi = find_vif_direct(&dst_sa6);
	
		if (vifi == NO_VIF) {
			/*
			 * The traceroute destination is not on one of
			 * my subnet vifs.
			 */
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "Destination %s not an interface",
				    inet6_fmt(&qry->tr_dst));
			if (IN6_IS_ADDR_MULTICAST(dst))
				return;
			errcode = TR_WRONG_IF;
		} else if (mrt != (mrtentry_t *)NULL &&
			   !IF_ISSET(vifi, &mrt->oifs)) {
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "Destination %s not on forwarding tree "
				    "for src %s",
				    inet6_fmt(&qry->tr_dst),
				    inet6_fmt(&qry->tr_src));
			if (IN6_IS_ADDR_MULTICAST(dst))
				return;
			errcode = TR_WRONG_IF;
		}
	}
	else {
		/*
		 * determine which interface the packet came in on
		 * RESP packets travel hop-by-hop so this either traversed
		 * a tunnel or came from a directly attached mrouter.
		 */
		if ((vifi = find_vif_direct(src)) == NO_VIF) {
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "Wrong interface for packet");
			errcode = TR_WRONG_IF;
		}
	}   
    
	/* Now that we've decided to send a response, save the qid */
	oqid = qry->tr_qid;

	IF_DEBUG(DEBUG_TRACE)
		log(LOG_DEBUG, 0, "Sending traceroute response");
    
	/* copy the packet to the sending buffer */
	p = mld6_send_buf + sizeof(struct mld6_hdr);
    
	bcopy(data, p, datalen);
    
	p += datalen;
    
	/*
	 * If there is no room to insert our reply, coopt the previous hop
	 * error indication to relay this fact.
	 */
	if (p + sizeof(struct tr6_resp) > mld6_send_buf + RECV_BUF_SIZE) {
		resp = (struct tr6_resp *)p - 1;
		resp->tr_rflags = TR_NO_SPACE;
		mrt = NULL;
		goto sendit;
	}

	/*
	 * fill in initial response fields
	 */
	resp = (struct tr6_resp *)p;
	bzero(resp, sizeof(struct tr6_resp));
	datalen += (RLEN + sizeof(struct mld6_hdr));

	resp->tr_qarr    = htonl(((tp.tv_sec + JAN_1970) << 16) + 
				 ((tp.tv_usec << 10) / 15625));

	resp->tr_rproto  = PROTO_PIM;
	resp->tr_outifid = (vifi == NO_VIF) ? TR_NO_VIF : htonl(vifi);
	resp->tr_rflags  = errcode;
	if ((sa_global = max_global_address()) == NULL)	/* impossible */
		log(LOG_ERR, 0, "acept_mtrace: max_global_address returns NULL");
	resp->tr_lcladdr = sa_global->sin6_addr;

	/*
	 * obtain # of packets out on interface
	 */
	mreq.mifi = vifi;
	if (vifi != NO_VIF &&
	    ioctl(udp_socket, SIOCGETMIFCNT_IN6, (char *)&mreq) >= 0)
		resp->tr_vifout  =  htonl(mreq.ocount);
	else
		resp->tr_vifout  =  0xffffffff;

	/*
	 * fill in scoping & pruning information
	 */
	/* TODO */
#if 0
	if (mrt != (mrtentry_t *)NULL)
		for (gt = rt->rt_groups; gt; gt = gt->gt_next) {
			if (gt->gt_mcastgrp >= group)
				break;
		}
	else
		gt = NULL;

	if (gt && gt->gt_mcastgrp == group) {
		struct stable *st;

		for (st = gt->gt_srctbl; st; st = st->st_next)
			if (qry->tr_src == st->st_origin)
				break;

		sg_req.src.s_addr = qry->tr_src;
		sg_req.grp.s_addr = group;
		if (st && st->st_ctime != 0 &&
		    ioctl(udp_socket, SIOCGETSGCNT, (char *)&sg_req) >= 0)
			resp->tr_pktcnt = htonl(sg_req.pktcnt + st->st_savpkt);
		else
			resp->tr_pktcnt = htonl(st ? st->st_savpkt : 0xffffffff);

		if (VIFM_ISSET(vifi, gt->gt_scope))
			resp->tr_rflags = TR_SCOPED;
		else if (gt->gt_prsent_timer)
			resp->tr_rflags = TR_PRUNED;
		else if (!VIFM_ISSET(vifi, gt->gt_grpmems))
			if (VIFM_ISSET(vifi, rt->rt_children) &&
			    NBRM_ISSETMASK(uvifs[vifi].uv_nbrmap,
					   rt->rt_subordinates)) /*XXX*/
				resp->tr_rflags = TR_OPRUNED;
			else
				resp->tr_rflags = TR_NO_FWD;
	} else {
		if (scoped_addr(vifi, group))
			resp->tr_rflags = TR_SCOPED;
		else if (rt && !VIFM_ISSET(vifi, rt->rt_children))
			resp->tr_rflags = TR_NO_FWD;
	}
#endif /* 0 */

	/*
	 *  if no rte exists, set NO_RTE error
	 */
	if (mrt == (mrtentry_t *)NULL) {
		src->sin6_addr = *dst;	/* the dst address of resp. pkt */
		resp->tr_inifid = TR_NO_VIF;
		resp->tr_rflags   = TR_NO_RTE;
		memset(&resp->tr_rmtaddr, 0, sizeof(struct in6_addr));
	} else {
		/* get # of packets in on interface */
		mreq.mifi = mrt->incoming;
		if (ioctl(udp_socket, SIOCGETMIFCNT_IN6, (char *)&mreq) >= 0)
			resp->tr_vifin = htonl(mreq.icount);
		else
			resp->tr_vifin = 0xffffffff;

		/*
		 * TODO
		 * MASK_TO_VAL(rt->rt_originmask, resp->tr_smask);
		 */
		resp->tr_inifid = htonl(mrt->incoming);
		if (mrt->upstream != (pim_nbr_entry_t *)NULL)
			parent_address = mrt->upstream->address.sin6_addr;
		else
			parent_address = in6addr_any;

		resp->tr_rmtaddr = parent_address;
		if (!IF_ISSET(vifi, &mrt->oifs)) {
			IF_DEBUG(DEBUG_TRACE)
				log(LOG_DEBUG, 0,
				    "Destination %s not on forwarding tree "
				    "for src %s",
				    inet6_fmt(&qry->tr_dst),
				    inet6_fmt(&qry->tr_src));
			resp->tr_rflags = TR_WRONG_IF;
		}
#if 0
		if (rt->rt_metric >= UNREACHABLE) {
			resp->tr_rflags = TR_NO_RTE;
			/* Hack to send reply directly */
			rt = NULL;
		}
#endif /* 0 */
	}

#ifdef SM_ONLY
	/*
	 * If we're the RP for the trace group, note it.
	 */
	rpentry_ptr = rp_match(&grp_sa6);
	if (rpentry_ptr && local_address(&rpentry_ptr->address) != NO_VIF)
		resp->tr_rflags = TR_RP;
#endif /* SM_ONLY */

  sendit:
	/*
	 * if metric is 1 or no. of reports is 1, send response to requestor
	 * else send to upstream router.  If the upstream router can't handle
	 * mtrace, set an error code and send to requestor anyway.
	 */
	IF_DEBUG(DEBUG_TRACE)
		log(LOG_DEBUG, 0, "rcount:%d, no:%d", rcount, no);

	ovifi = NO_VIF;		/* unspecified */
	if ((rcount + 1 == no) || (mrt == NULL) || (mrt->metric == 1)) {
		resptype = MLD6_MTRACE_RESP;
		resp_sa6.sin6_addr = qry->tr_raddr;
		if (IN6_IS_ADDR_LINKLOCAL(&resp_sa6.sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&resp_sa6.sin6_addr)) {
			if ((ovifi = find_vif_direct(&dst_sa6)) == NO_VIF) {
				log(LOG_INFO, 0,
				    "can't determine outgoing i/f for mtrace "
				    "response.");
				return;
			}
		}
	} else
		/* TODO */
	{
#if 0
		if (!can_mtrace(rt->rt_parent, rt->rt_gateway)) {
			resp_sa6.sin6_addr = qry->tr_raddr;
			resp->tr_rflags = TR_OLD_ROUTER;
			resptype = MLD6_MTRACE_RESP;
		} else
#endif /* 0 */
#ifdef SM_ONLY
		if (mrt->incoming &&
		    (uvifs[mrt->incoming].uv_flags & MIFF_REGISTER)) {
			log(LOG_DEBUG, 0,
			    "incoming i/f is for register. "
			    "Can't be forwarded anymore.");
				resp_sa6.sin6_addr = qry->tr_raddr;
				resptype = MLD6_MTRACE_RESP;
		} else
#endif /* SM_ONLY */
		{
			if (mrt->upstream != (pim_nbr_entry_t *)NULL)
				parent_address =
					mrt->upstream->address.sin6_addr;
			else
				parent_address = allrouters_group.sin6_addr;
			resp_sa6.sin6_addr = parent_address;
			ovifi = mrt->incoming;
			resptype = MLD6_MTRACE;
		}
	}

	if (IN6_IS_ADDR_MULTICAST(&resp_sa6.sin6_addr)) {
		struct sockaddr_in6 *sa6;

		/*
		 * Send the reply on a known multicast capable vif.
		 * If we don't have one, we can't source any
		 * multicasts anyway.
		 */
		if (IN6_IS_ADDR_MC_LINKLOCAL(&resp_sa6.sin6_addr)) {
			sa6 = &uvifs[ovifi].uv_linklocal->pa_addr;
			ifindex = uvifs[ovifi].uv_ifindex;
		}
		else {
			if (phys_vif != -1 &&
			    (sa6 = uv_global(phys_vif)) != NULL) {
				IF_DEBUG(DEBUG_TRACE)
					log(LOG_DEBUG, 0,
					    "Sending reply to %s from %s",
					    inet6_fmt(dst),
					    inet6_fmt(&sa6->sin6_addr));
				ifindex = uvifs[phys_vif].uv_ifindex;
			}
			else {
				log(LOG_INFO, 0, "No enabled phyints -- %s",
				    "dropping traceroute reply");
				return;
			}
		}
		k_set_hlim(mld6_socket, qry->tr_rhlim);
		send_mld6(resptype, no, sa6, &resp_sa6, group,
			  ifindex, 0, datalen, 0);
		k_set_hlim(mld6_socket, 1);
	} else {
		struct sockaddr_in6 *sa6 = NULL;
		ifindex = -1;	/* unspecified by default */

		if (IN6_IS_ADDR_LINKLOCAL(&resp_sa6.sin6_addr)) {
			/* ovifi must be valid in this case */
			ifindex = uvifs[ovifi].uv_ifindex;
			sa6 = &uvifs[ovifi].uv_linklocal->pa_addr;
		}

		IF_DEBUG(DEBUG_TRACE)
			log(LOG_DEBUG, 0, "Sending %s to %s from %s",
			    resptype == MLD6_MTRACE_RESP ?
			    "reply" : "request on",
			    inet6_fmt(dst),
			    sa6 ? inet6_fmt(&sa6->sin6_addr) : "unspecified");
	
		send_mld6(resptype, no, sa6, &resp_sa6, group, ifindex,
			  0, datalen, 0);
	}
	return;
}
