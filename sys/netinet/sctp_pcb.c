/*-
 * Copyright (c) 2001-2006, Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_pcb.c,v 1.38 2005/03/06 16:04:18 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_ipsec.h"
#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <sys/limits.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/in6_pcb.h>
#endif				/* INET6 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif				/* IPSEC */

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp.h>
#include <netinet/sctp_header.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_timer.h>


#ifdef SCTP_DEBUG
uint32_t sctp_debug_on = 0;

#endif				/* SCTP_DEBUG */


extern int sctp_pcbtblsize;
extern int sctp_hashtblsize;
extern int sctp_chunkscale;

struct sctp_epinfo sctppcbinfo;

/* FIX: we don't handle multiple link local scopes */
/* "scopeless" replacement IN6_ARE_ADDR_EQUAL */
int
SCTP6_ARE_ADDR_EQUAL(struct in6_addr *a, struct in6_addr *b)
{
	struct in6_addr tmp_a, tmp_b;

	/* use a copy of a and b */
	tmp_a = *a;
	tmp_b = *b;
	in6_clearscope(&tmp_a);
	in6_clearscope(&tmp_b);
	return (IN6_ARE_ADDR_EQUAL(&tmp_a, &tmp_b));
}


void
sctp_fill_pcbinfo(struct sctp_pcbinfo *spcb)
{
	/*
	 * We really don't need to lock this, but I will just because it
	 * does not hurt.
	 */
	SCTP_INP_INFO_RLOCK();
	spcb->ep_count = sctppcbinfo.ipi_count_ep;
	spcb->asoc_count = sctppcbinfo.ipi_count_asoc;
	spcb->laddr_count = sctppcbinfo.ipi_count_laddr;
	spcb->raddr_count = sctppcbinfo.ipi_count_raddr;
	spcb->chk_count = sctppcbinfo.ipi_count_chunk;
	spcb->readq_count = sctppcbinfo.ipi_count_readq;
	spcb->stream_oque = sctppcbinfo.ipi_count_strmoq;
	spcb->free_chunks = sctppcbinfo.ipi_free_chunks;

	SCTP_INP_INFO_RUNLOCK();
}


/*
 * Notes on locks for FreeBSD 5 and up. All association lookups that have a
 * definte ep, the INP structure is assumed to be locked for reading. If we
 * need to go find the INP (ususally when a **inp is passed) then we must
 * lock the INFO structure first and if needed lock the INP too. Note that if
 * we lock it we must
 *
 */


/*
 * Given a endpoint, look and find in its association list any association
 * with the "to" address given. This can be a "from" address, too, for
 * inbound packets. For outbound packets it is a true "to" address.
 */

static struct sctp_tcb *
sctp_tcb_special_locate(struct sctp_inpcb **inp_p, struct sockaddr *from,
    struct sockaddr *to, struct sctp_nets **netp)
{
	/**** ASSUMSES THE CALLER holds the INP_INFO_RLOCK */

	/*
	 * Note for this module care must be taken when observing what to is
	 * for. In most of the rest of the code the TO field represents my
	 * peer and the FROM field represents my address. For this module it
	 * is reversed of that.
	 */
	/*
	 * If we support the TCP model, then we must now dig through to see
	 * if we can find our endpoint in the list of tcp ep's.
	 */
	uint16_t lport, rport;
	struct sctppcbhead *ephead;
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;

	if ((to == NULL) || (from == NULL)) {
		return (NULL);
	}
	if (to->sa_family == AF_INET && from->sa_family == AF_INET) {
		lport = ((struct sockaddr_in *)to)->sin_port;
		rport = ((struct sockaddr_in *)from)->sin_port;
	} else if (to->sa_family == AF_INET6 && from->sa_family == AF_INET6) {
		lport = ((struct sockaddr_in6 *)to)->sin6_port;
		rport = ((struct sockaddr_in6 *)from)->sin6_port;
	} else {
		return NULL;
	}
	ephead = &sctppcbinfo.sctp_tcpephash[SCTP_PCBHASH_ALLADDR(
	    (lport + rport), sctppcbinfo.hashtcpmark)];
	/*
	 * Ok now for each of the guys in this bucket we must look and see:
	 * - Does the remote port match. - Does there single association's
	 * addresses match this address (to). If so we update p_ep to point
	 * to this ep and return the tcb from it.
	 */
	LIST_FOREACH(inp, ephead, sctp_hash) {
		if (lport != inp->sctp_lport) {
			continue;
		}
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		/* check to see if the ep has one of the addresses */
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
			/* We are NOT bound all, so look further */
			int match = 0;

			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {

				if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_PCB1) {
						printf("An ounce of prevention is worth a pound of cure\n");
					}
#endif
					continue;
				}
				if (laddr->ifa->ifa_addr == NULL) {
#ifdef SCTP_DEBUG
					if (sctp_debug_on & SCTP_DEBUG_PCB1) {
						printf("ifa with a NULL address\n");
					}
#endif
					continue;
				}
				if (laddr->ifa->ifa_addr->sa_family ==
				    to->sa_family) {
					/* see if it matches */
					struct sockaddr_in *intf_addr, *sin;

					intf_addr = (struct sockaddr_in *)
					    laddr->ifa->ifa_addr;
					sin = (struct sockaddr_in *)to;
					if (from->sa_family == AF_INET) {
						if (sin->sin_addr.s_addr ==
						    intf_addr->sin_addr.s_addr) {
							match = 1;
							break;
						}
					} else {
						struct sockaddr_in6 *intf_addr6;
						struct sockaddr_in6 *sin6;

						sin6 = (struct sockaddr_in6 *)
						    to;
						intf_addr6 = (struct sockaddr_in6 *)
						    laddr->ifa->ifa_addr;

						if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
						    &intf_addr6->sin6_addr)) {
							match = 1;
							break;
						}
					}
				}
			}
			if (match == 0) {
				/* This endpoint does not have this address */
				SCTP_INP_RUNLOCK(inp);
				continue;
			}
		}
		/*
		 * Ok if we hit here the ep has the address, does it hold
		 * the tcb?
		 */

		stcb = LIST_FIRST(&inp->sctp_asoc_list);
		if (stcb == NULL) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		SCTP_TCB_LOCK(stcb);
		if (stcb->rport != rport) {
			/* remote port does not match. */
			SCTP_TCB_UNLOCK(stcb);
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		/* Does this TCB have a matching address? */
		TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {

			if (net->ro._l_addr.sa.sa_family != from->sa_family) {
				/* not the same family, can't be a match */
				continue;
			}
			if (from->sa_family == AF_INET) {
				struct sockaddr_in *sin, *rsin;

				sin = (struct sockaddr_in *)&net->ro._l_addr;
				rsin = (struct sockaddr_in *)from;
				if (sin->sin_addr.s_addr ==
				    rsin->sin_addr.s_addr) {
					/* found it */
					if (netp != NULL) {
						*netp = net;
					}
					/* Update the endpoint pointer */
					*inp_p = inp;
					SCTP_INP_RUNLOCK(inp);
					return (stcb);
				}
			} else {
				struct sockaddr_in6 *sin6, *rsin6;

				sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
				rsin6 = (struct sockaddr_in6 *)from;
				if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
				    &rsin6->sin6_addr)) {
					/* found it */
					if (netp != NULL) {
						*netp = net;
					}
					/* Update the endpoint pointer */
					*inp_p = inp;
					SCTP_INP_RUNLOCK(inp);
					return (stcb);
				}
			}
		}
		SCTP_TCB_UNLOCK(stcb);
		SCTP_INP_RUNLOCK(inp);
	}
	return (NULL);
}

/*
 * rules for use
 *
 * 1) If I return a NULL you must decrement any INP ref cnt. 2) If I find an
 * stcb, both will be locked (locked_tcb and stcb) but decrement will be done
 * (if locked == NULL). 3) Decrement happens on return ONLY if locked ==
 * NULL.
 */

struct sctp_tcb *
sctp_findassociation_ep_addr(struct sctp_inpcb **inp_p, struct sockaddr *remote,
    struct sctp_nets **netp, struct sockaddr *local, struct sctp_tcb *locked_tcb)
{
	struct sctpasochead *head;
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;
	struct sctp_nets *net;
	uint16_t rport;

	inp = *inp_p;
	if (remote->sa_family == AF_INET) {
		rport = (((struct sockaddr_in *)remote)->sin_port);
	} else if (remote->sa_family == AF_INET6) {
		rport = (((struct sockaddr_in6 *)remote)->sin6_port);
	} else {
		return (NULL);
	}
	if (locked_tcb) {
		/*
		 * UN-lock so we can do proper locking here this occurs when
		 * called from load_addresses_from_init.
		 */
		SCTP_TCB_UNLOCK(locked_tcb);
	}
	SCTP_INP_INFO_RLOCK();
	if (inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) {
		/*
		 * Now either this guy is our listener or it's the
		 * connector. If it is the one that issued the connect, then
		 * it's only chance is to be the first TCB in the list. If
		 * it is the acceptor, then do the special_lookup to hash
		 * and find the real inp.
		 */
		if ((inp->sctp_socket) && (inp->sctp_socket->so_qlimit)) {
			/* to is peer addr, from is my addr */
			stcb = sctp_tcb_special_locate(inp_p, remote, local,
			    netp);
			if ((stcb != NULL) && (locked_tcb == NULL)) {
				/* we have a locked tcb, lower refcount */
				SCTP_INP_WLOCK(inp);
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
			}
			if ((locked_tcb != NULL) && (locked_tcb != stcb)) {
				SCTP_INP_RLOCK(locked_tcb->sctp_ep);
				SCTP_TCB_LOCK(locked_tcb);
				SCTP_INP_RUNLOCK(locked_tcb->sctp_ep);
			}
			SCTP_INP_INFO_RUNLOCK();
			return (stcb);
		} else {
			SCTP_INP_WLOCK(inp);
			if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
				goto null_return;
			}
			stcb = LIST_FIRST(&inp->sctp_asoc_list);
			if (stcb == NULL) {
				goto null_return;
			}
			SCTP_TCB_LOCK(stcb);
			if (stcb->rport != rport) {
				/* remote port does not match. */
				SCTP_TCB_UNLOCK(stcb);
				goto null_return;
			}
			/* now look at the list of remote addresses */
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
#ifdef INVARIANTS
				if (net == (TAILQ_NEXT(net, sctp_next))) {
					panic("Corrupt net list");
				}
#endif
				if (net->ro._l_addr.sa.sa_family !=
				    remote->sa_family) {
					/* not the same family */
					continue;
				}
				if (remote->sa_family == AF_INET) {
					struct sockaddr_in *sin, *rsin;

					sin = (struct sockaddr_in *)
					    &net->ro._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
				} else if (remote->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6, *rsin6;

					sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &rsin6->sin6_addr)) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}
	} else {
		SCTP_INP_WLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			goto null_return;
		}
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(rport,
		    inp->sctp_hashmark)];
		if (head == NULL) {
			goto null_return;
		}
		LIST_FOREACH(stcb, head, sctp_tcbhash) {
			if (stcb->rport != rport) {
				/* remote port does not match */
				continue;
			}
			/* now look at the list of remote addresses */
			SCTP_TCB_LOCK(stcb);
			TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
#ifdef INVARIANTS
				if (net == (TAILQ_NEXT(net, sctp_next))) {
					panic("Corrupt net list");
				}
#endif
				if (net->ro._l_addr.sa.sa_family !=
				    remote->sa_family) {
					/* not the same family */
					continue;
				}
				if (remote->sa_family == AF_INET) {
					struct sockaddr_in *sin, *rsin;

					sin = (struct sockaddr_in *)
					    &net->ro._l_addr;
					rsin = (struct sockaddr_in *)remote;
					if (sin->sin_addr.s_addr ==
					    rsin->sin_addr.s_addr) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
				} else if (remote->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6, *rsin6;

					sin6 = (struct sockaddr_in6 *)
					    &net->ro._l_addr;
					rsin6 = (struct sockaddr_in6 *)remote;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &rsin6->sin6_addr)) {
						/* found it */
						if (netp != NULL) {
							*netp = net;
						}
						if (locked_tcb == NULL) {
							SCTP_INP_DECR_REF(inp);
						} else if (locked_tcb != stcb) {
							SCTP_TCB_LOCK(locked_tcb);
						}
						SCTP_INP_WUNLOCK(inp);
						SCTP_INP_INFO_RUNLOCK();
						return (stcb);
					}
				}
			}
			SCTP_TCB_UNLOCK(stcb);
		}
	}
null_return:
	/* clean up for returning null */
	if (locked_tcb) {
		SCTP_TCB_LOCK(locked_tcb);
	}
	SCTP_INP_WUNLOCK(inp);
	SCTP_INP_INFO_RUNLOCK();
	/* not found */
	return (NULL);
}

/*
 * Find an association for a specific endpoint using the association id given
 * out in the COMM_UP notification
 */

struct sctp_tcb *
sctp_findassociation_ep_asocid(struct sctp_inpcb *inp, sctp_assoc_t asoc_id, int want_lock)
{
	/*
	 * Use my the assoc_id to find a endpoint
	 */
	struct sctpasochead *head;
	struct sctp_tcb *stcb;
	uint32_t id;

	if (asoc_id == 0 || inp == NULL) {
		return (NULL);
	}
	SCTP_INP_INFO_RLOCK();
	id = (uint32_t) asoc_id;
	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(id,
	    sctppcbinfo.hashasocmark)];
	if (head == NULL) {
		/* invalid id TSNH */
		SCTP_INP_INFO_RUNLOCK();
		return (NULL);
	}
	LIST_FOREACH(stcb, head, sctp_asocs) {
		SCTP_INP_RLOCK(stcb->sctp_ep);
		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			SCTP_INP_INFO_RUNLOCK();
			return (NULL);
		}
		if (stcb->asoc.assoc_id == id) {
			/* candidate */
			if (inp != stcb->sctp_ep) {
				/*
				 * some other guy has the same id active (id
				 * collision ??).
				 */
				SCTP_INP_RUNLOCK(stcb->sctp_ep);
				continue;
			}
			if (want_lock) {
				SCTP_TCB_LOCK(stcb);
			}
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			SCTP_INP_INFO_RUNLOCK();
			return (stcb);
		}
		SCTP_INP_RUNLOCK(stcb->sctp_ep);
	}
	/* Ok if we missed here, lets try the restart hash */
	head = &sctppcbinfo.sctp_restarthash[SCTP_PCBHASH_ASOC(id, sctppcbinfo.hashrestartmark)];
	if (head == NULL) {
		/* invalid id TSNH */
		SCTP_INP_INFO_RUNLOCK();
		return (NULL);
	}
	LIST_FOREACH(stcb, head, sctp_tcbrestarhash) {
		SCTP_INP_RLOCK(stcb->sctp_ep);
		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			SCTP_INP_INFO_RUNLOCK();
			return (NULL);
		}
		SCTP_TCB_LOCK(stcb);
		SCTP_INP_RUNLOCK(stcb->sctp_ep);
		if (stcb->asoc.assoc_id == id) {
			/* candidate */
			if (inp != stcb->sctp_ep) {
				/*
				 * some other guy has the same id active (id
				 * collision ??).
				 */
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			SCTP_INP_INFO_RUNLOCK();
			return (stcb);
		}
		SCTP_TCB_UNLOCK(stcb);
	}
	SCTP_INP_INFO_RUNLOCK();
	return (NULL);
}


static struct sctp_inpcb *
sctp_endpoint_probe(struct sockaddr *nam, struct sctppcbhead *head,
    uint16_t lport)
{
	struct sctp_inpcb *inp;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sctp_laddr *laddr;

	/*
	 * Endpoing probe expects that the INP_INFO is locked.
	 */
	if (nam->sa_family == AF_INET) {
		sin = (struct sockaddr_in *)nam;
		sin6 = NULL;
	} else if (nam->sa_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)nam;
		sin = NULL;
	} else {
		/* unsupported family */
		return (NULL);
	}
	if (head == NULL)
		return (NULL);
	LIST_FOREACH(inp, head, sctp_hash) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) &&
		    (inp->sctp_lport == lport)) {
			/* got it */
			if ((nam->sa_family == AF_INET) &&
			    (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
			    SCTP_IPV6_V6ONLY(inp)) {
				/* IPv4 on a IPv6 socket with ONLY IPv6 set */
				SCTP_INP_RUNLOCK(inp);
				continue;
			}
			/* A V6 address and the endpoint is NOT bound V6 */
			if (nam->sa_family == AF_INET6 &&
			    (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) == 0) {
				SCTP_INP_RUNLOCK(inp);
				continue;
			}
			SCTP_INP_RUNLOCK(inp);
			return (inp);
		}
		SCTP_INP_RUNLOCK(inp);
	}

	if ((nam->sa_family == AF_INET) &&
	    (sin->sin_addr.s_addr == INADDR_ANY)) {
		/* Can't hunt for one that has no address specified */
		return (NULL);
	} else if ((nam->sa_family == AF_INET6) &&
	    (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))) {
		/* Can't hunt for one that has no address specified */
		return (NULL);
	}
	/*
	 * ok, not bound to all so see if we can find a EP bound to this
	 * address.
	 */
	LIST_FOREACH(inp, head, sctp_hash) {
		SCTP_INP_RLOCK(inp);
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL)) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		/*
		 * Ok this could be a likely candidate, look at all of its
		 * addresses
		 */
		if (inp->sctp_lport != lport) {
			SCTP_INP_RUNLOCK(inp);
			continue;
		}
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("An ounce of prevention is worth a pound of cure\n");
				}
#endif
				continue;
			}
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("Ok laddr->ifa:%p is possible, ",
				    laddr->ifa);
			}
#endif
			if (laddr->ifa->ifa_addr == NULL) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("Huh IFA as an ifa_addr=NULL, ");
				}
#endif
				continue;
			}
			if (laddr->ifa->ifa_addr->sa_family == nam->sa_family) {
				/* possible, see if it matches */
				struct sockaddr_in *intf_addr;

				intf_addr = (struct sockaddr_in *)
				    laddr->ifa->ifa_addr;
				if (nam->sa_family == AF_INET) {
					if (sin->sin_addr.s_addr ==
					    intf_addr->sin_addr.s_addr) {
						SCTP_INP_RUNLOCK(inp);
						return (inp);
					}
				} else if (nam->sa_family == AF_INET6) {
					struct sockaddr_in6 *intf_addr6;

					intf_addr6 = (struct sockaddr_in6 *)
					    laddr->ifa->ifa_addr;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &intf_addr6->sin6_addr)) {
						SCTP_INP_RUNLOCK(inp);
						return (inp);
					}
				}
			}
		}
		SCTP_INP_RUNLOCK(inp);
	}
	return (NULL);
}


struct sctp_inpcb *
sctp_pcb_findep(struct sockaddr *nam, int find_tcp_pool, int have_lock)
{
	/*
	 * First we check the hash table to see if someone has this port
	 * bound with just the port.
	 */
	struct sctp_inpcb *inp;
	struct sctppcbhead *head;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	int lport;

	if (nam->sa_family == AF_INET) {
		sin = (struct sockaddr_in *)nam;
		lport = ((struct sockaddr_in *)nam)->sin_port;
	} else if (nam->sa_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)nam;
		lport = ((struct sockaddr_in6 *)nam)->sin6_port;
	} else {
		/* unsupported family */
		return (NULL);
	}
	/*
	 * I could cheat here and just cast to one of the types but we will
	 * do it right. It also provides the check against an Unsupported
	 * type too.
	 */
	/* Find the head of the ALLADDR chain */
	if (have_lock == 0) {
		SCTP_INP_INFO_RLOCK();

	}
	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];
	inp = sctp_endpoint_probe(nam, head, lport);

	/*
	 * If the TCP model exists it could be that the main listening
	 * endpoint is gone but there exists a connected socket for this guy
	 * yet. If so we can return the first one that we find. This may NOT
	 * be the correct one but the sctp_findassociation_ep_addr has
	 * further code to look at all TCP models.
	 */
	if (inp == NULL && find_tcp_pool) {
		unsigned int i;

		for (i = 0; i < sctppcbinfo.hashtblsize; i++) {
			/*
			 * This is real gross, but we do NOT have a remote
			 * port at this point depending on who is calling.
			 * We must therefore look for ANY one that matches
			 * our local port :/
			 */
			head = &sctppcbinfo.sctp_tcpephash[i];
			if (LIST_FIRST(head)) {
				inp = sctp_endpoint_probe(nam, head, lport);
				if (inp) {
					/* Found one */
					break;
				}
			}
		}
	}
	if (inp) {
		SCTP_INP_INCR_REF(inp);
	}
	if (have_lock == 0) {
		SCTP_INP_INFO_RUNLOCK();
	}
	return (inp);
}

/*
 * Find an association for an endpoint with the pointer to whom you want to
 * send to and the endpoint pointer. The address can be IPv4 or IPv6. We may
 * need to change the *to to some other struct like a mbuf...
 */
struct sctp_tcb *
sctp_findassociation_addr_sa(struct sockaddr *to, struct sockaddr *from,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, int find_tcp_pool)
{
	struct sctp_inpcb *inp;
	struct sctp_tcb *retval;

	SCTP_INP_INFO_RLOCK();
	if (find_tcp_pool) {
		if (inp_p != NULL) {
			retval = sctp_tcb_special_locate(inp_p, from, to, netp);
		} else {
			retval = sctp_tcb_special_locate(&inp, from, to, netp);
		}
		if (retval != NULL) {
			SCTP_INP_INFO_RUNLOCK();
			return (retval);
		}
	}
	inp = sctp_pcb_findep(to, 0, 1);
	if (inp_p != NULL) {
		*inp_p = inp;
	}
	SCTP_INP_INFO_RUNLOCK();

	if (inp == NULL) {
		return (NULL);
	}
	/*
	 * ok, we have an endpoint, now lets find the assoc for it (if any)
	 * we now place the source address or from in the to of the find
	 * endpoint call. Since in reality this chain is used from the
	 * inbound packet side.
	 */
	if (inp_p != NULL) {
		retval = sctp_findassociation_ep_addr(inp_p, from, netp, to, NULL);
	} else {
		retval = sctp_findassociation_ep_addr(&inp, from, netp, to, NULL);
	}
	return retval;
}


/*
 * This routine will grub through the mbuf that is a INIT or INIT-ACK and
 * find all addresses that the sender has specified in any address list. Each
 * address will be used to lookup the TCB and see if one exits.
 */
static struct sctp_tcb *
sctp_findassociation_special_addr(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_inpcb **inp_p, struct sctp_nets **netp,
    struct sockaddr *dest)
{
	struct sockaddr_in sin4;
	struct sockaddr_in6 sin6;
	struct sctp_paramhdr *phdr, parm_buf;
	struct sctp_tcb *retval;
	uint32_t ptype, plen;

	memset(&sin4, 0, sizeof(sin4));
	memset(&sin6, 0, sizeof(sin6));
	sin4.sin_len = sizeof(sin4);
	sin4.sin_family = AF_INET;
	sin4.sin_port = sh->src_port;
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = sh->src_port;

	retval = NULL;
	offset += sizeof(struct sctp_init_chunk);

	phdr = sctp_get_next_param(m, offset, &parm_buf, sizeof(parm_buf));
	while (phdr != NULL) {
		/* now we must see if we want the parameter */
		ptype = ntohs(phdr->param_type);
		plen = ntohs(phdr->param_length);
		if (plen == 0) {
			break;
		}
		if (ptype == SCTP_IPV4_ADDRESS &&
		    plen == sizeof(struct sctp_ipv4addr_param)) {
			/* Get the rest of the address */
			struct sctp_ipv4addr_param ip4_parm, *p4;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip4_parm, plen);
			if (phdr == NULL) {
				return (NULL);
			}
			p4 = (struct sctp_ipv4addr_param *)phdr;
			memcpy(&sin4.sin_addr, &p4->addr, sizeof(p4->addr));
			/* look it up */
			retval = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin4, netp, dest, NULL);
			if (retval != NULL) {
				return (retval);
			}
		} else if (ptype == SCTP_IPV6_ADDRESS &&
		    plen == sizeof(struct sctp_ipv6addr_param)) {
			/* Get the rest of the address */
			struct sctp_ipv6addr_param ip6_parm, *p6;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&ip6_parm, plen);
			if (phdr == NULL) {
				return (NULL);
			}
			p6 = (struct sctp_ipv6addr_param *)phdr;
			memcpy(&sin6.sin6_addr, &p6->addr, sizeof(p6->addr));
			/* look it up */
			retval = sctp_findassociation_ep_addr(inp_p,
			    (struct sockaddr *)&sin6, netp, dest, NULL);
			if (retval != NULL) {
				return (retval);
			}
		}
		offset += SCTP_SIZE32(plen);
		phdr = sctp_get_next_param(m, offset, &parm_buf,
		    sizeof(parm_buf));
	}
	return (NULL);
}


static struct sctp_tcb *
sctp_findassoc_by_vtag(struct sockaddr *from, uint32_t vtag,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp, uint16_t rport,
    uint16_t lport, int skip_src_check)
{
	/*
	 * Use my vtag to hash. If we find it we then verify the source addr
	 * is in the assoc. If all goes well we save a bit on rec of a
	 * packet.
	 */
	struct sctpasochead *head;
	struct sctp_nets *net;
	struct sctp_tcb *stcb;

	*netp = NULL;
	*inp_p = NULL;
	SCTP_INP_INFO_RLOCK();
	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(vtag,
	    sctppcbinfo.hashasocmark)];
	if (head == NULL) {
		/* invalid vtag */
		SCTP_INP_INFO_RUNLOCK();
		return (NULL);
	}
	LIST_FOREACH(stcb, head, sctp_asocs) {
		SCTP_INP_RLOCK(stcb->sctp_ep);
		if (stcb->sctp_ep->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
			SCTP_INP_RUNLOCK(stcb->sctp_ep);
			SCTP_INP_INFO_RUNLOCK();
			return (NULL);
		}
		SCTP_TCB_LOCK(stcb);
		SCTP_INP_RUNLOCK(stcb->sctp_ep);
		if (stcb->asoc.my_vtag == vtag) {
			/* candidate */
			if (stcb->rport != rport) {
				/*
				 * we could remove this if vtags are unique
				 * across the system.
				 */
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (stcb->sctp_ep->sctp_lport != lport) {
				/*
				 * we could remove this if vtags are unique
				 * across the system.
				 */
				SCTP_TCB_UNLOCK(stcb);
				continue;
			}
			if (skip_src_check) {
				*netp = NULL;	/* unknown */
				*inp_p = stcb->sctp_ep;
				SCTP_INP_INFO_RUNLOCK();
				return (stcb);
			}
			net = sctp_findnet(stcb, from);
			if (net) {
				/* yep its him. */
				*netp = net;
				SCTP_STAT_INCR(sctps_vtagexpress);
				*inp_p = stcb->sctp_ep;
				SCTP_INP_INFO_RUNLOCK();
				return (stcb);
			} else {
				/*
				 * not him, this should only happen in rare
				 * cases so I peg it.
				 */
				SCTP_STAT_INCR(sctps_vtagbogus);
			}
		}
		SCTP_TCB_UNLOCK(stcb);
	}
	SCTP_INP_INFO_RUNLOCK();
	return (NULL);
}

/*
 * Find an association with the pointer to the inbound IP packet. This can be
 * a IPv4 or IPv6 packet.
 */
struct sctp_tcb *
sctp_findassociation_addr(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_chunkhdr *ch,
    struct sctp_inpcb **inp_p, struct sctp_nets **netp)
{
	int find_tcp_pool;
	struct ip *iph;
	struct sctp_tcb *retval;
	struct sockaddr_storage to_store, from_store;
	struct sockaddr *to = (struct sockaddr *)&to_store;
	struct sockaddr *from = (struct sockaddr *)&from_store;
	struct sctp_inpcb *inp;


	iph = mtod(m, struct ip *);
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		struct sockaddr_in *from4;

		from4 = (struct sockaddr_in *)&from_store;
		bzero(from4, sizeof(*from4));
		from4->sin_family = AF_INET;
		from4->sin_len = sizeof(struct sockaddr_in);
		from4->sin_addr.s_addr = iph->ip_src.s_addr;
		from4->sin_port = sh->src_port;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;
		struct sockaddr_in6 *from6;

		ip6 = mtod(m, struct ip6_hdr *);
		from6 = (struct sockaddr_in6 *)&from_store;
		bzero(from6, sizeof(*from6));
		from6->sin6_family = AF_INET6;
		from6->sin6_len = sizeof(struct sockaddr_in6);
		from6->sin6_addr = ip6->ip6_src;
		from6->sin6_port = sh->src_port;
		/* Get the scopes in properly to the sin6 addr's */
		/* we probably don't need these operations */
		(void)sa6_recoverscope(from6);
		sa6_embedscope(from6, ip6_use_defzone);
	} else {
		/* Currently not supported. */
		return (NULL);
	}
	if (sh->v_tag) {
		/* we only go down this path if vtag is non-zero */
		retval = sctp_findassoc_by_vtag(from, ntohl(sh->v_tag),
		    inp_p, netp, sh->src_port, sh->dest_port, 0);
		if (retval) {
			return (retval);
		}
	}
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		struct sockaddr_in *to4;

		to4 = (struct sockaddr_in *)&to_store;
		bzero(to4, sizeof(*to4));
		to4->sin_family = AF_INET;
		to4->sin_len = sizeof(struct sockaddr_in);
		to4->sin_addr.s_addr = iph->ip_dst.s_addr;
		to4->sin_port = sh->dest_port;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;
		struct sockaddr_in6 *to6;

		ip6 = mtod(m, struct ip6_hdr *);
		to6 = (struct sockaddr_in6 *)&to_store;
		bzero(to6, sizeof(*to6));
		to6->sin6_family = AF_INET6;
		to6->sin6_len = sizeof(struct sockaddr_in6);
		to6->sin6_addr = ip6->ip6_dst;
		to6->sin6_port = sh->dest_port;
		/* Get the scopes in properly to the sin6 addr's */
		/* we probably don't need these operations */
		(void)sa6_recoverscope(to6);
		sa6_embedscope(to6, ip6_use_defzone);
	}
	find_tcp_pool = 0;
	/*
	 * FIX FIX?, I think we only need to look in the TCP pool if its an
	 * INIT or COOKIE-ECHO, We really don't need to find it that way if
	 * its a INIT-ACK or COOKIE_ACK since these in bot one-2-one and
	 * one-2-N would be in the main pool anyway.
	 */
	if ((ch->chunk_type != SCTP_INITIATION) &&
	    (ch->chunk_type != SCTP_INITIATION_ACK) &&
	    (ch->chunk_type != SCTP_COOKIE_ACK) &&
	    (ch->chunk_type != SCTP_COOKIE_ECHO)) {
		/* Other chunk types go to the tcp pool. */
		find_tcp_pool = 1;
	}
	if (inp_p) {
		retval = sctp_findassociation_addr_sa(to, from, inp_p, netp,
		    find_tcp_pool);
		inp = *inp_p;
	} else {
		retval = sctp_findassociation_addr_sa(to, from, &inp, netp,
		    find_tcp_pool);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("retval:%p inp:%p\n", retval, inp);
	}
#endif
	if (retval == NULL && inp) {
		/* Found a EP but not this address */
		if ((ch->chunk_type == SCTP_INITIATION) ||
		    (ch->chunk_type == SCTP_INITIATION_ACK)) {
			/*
			 * special hook, we do NOT return linp or an
			 * association that is linked to an existing
			 * association that is under the TCP pool (i.e. no
			 * listener exists). The endpoint finding routine
			 * will always find a listner before examining the
			 * TCP pool.
			 */
			if (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) {
				if (inp_p) {
					*inp_p = NULL;
				}
				return (NULL);
			}
			retval = sctp_findassociation_special_addr(m, iphlen,
			    offset, sh, &inp, netp, to);
			if (inp_p != NULL) {
				*inp_p = inp;
			}
		}
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("retval is %p\n", retval);
	}
#endif
	return (retval);
}

/*
 * lookup an association by an ASCONF lookup address.
 * if the lookup address is 0.0.0.0 or ::0, use the vtag to do the lookup
 */
struct sctp_tcb *
sctp_findassociation_ep_asconf(struct mbuf *m, int iphlen, int offset,
    struct sctphdr *sh, struct sctp_inpcb **inp_p, struct sctp_nets **netp)
{
	struct sctp_tcb *stcb;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	struct sockaddr_storage local_store, remote_store;
	struct ip *iph;
	struct sctp_paramhdr parm_buf, *phdr;
	int ptype;
	int zero_address = 0;


	memset(&local_store, 0, sizeof(local_store));
	memset(&remote_store, 0, sizeof(remote_store));

	/* First get the destination address setup too. */
	iph = mtod(m, struct ip *);
	if (iph->ip_v == IPVERSION) {
		/* its IPv4 */
		sin = (struct sockaddr_in *)&local_store;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = sh->dest_port;
		sin->sin_addr.s_addr = iph->ip_dst.s_addr;
	} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
		/* its IPv6 */
		struct ip6_hdr *ip6;

		ip6 = mtod(m, struct ip6_hdr *);
		sin6 = (struct sockaddr_in6 *)&local_store;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = sh->dest_port;
		sin6->sin6_addr = ip6->ip6_dst;
	} else {
		return NULL;
	}

	phdr = sctp_get_next_param(m, offset + sizeof(struct sctp_asconf_chunk),
	    &parm_buf, sizeof(struct sctp_paramhdr));
	if (phdr == NULL) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
			printf("findassociation_ep_asconf: failed to get asconf lookup addr\n");
		}
#endif				/* SCTP_DEBUG */
		return NULL;
	}
	ptype = (int)((uint32_t) ntohs(phdr->param_type));
	/* get the correlation address */
	if (ptype == SCTP_IPV6_ADDRESS) {
		/* ipv6 address param */
		struct sctp_ipv6addr_param *p6, p6_buf;

		if (ntohs(phdr->param_length) != sizeof(struct sctp_ipv6addr_param)) {
			return NULL;
		}
		p6 = (struct sctp_ipv6addr_param *)sctp_get_next_param(m,
		    offset + sizeof(struct sctp_asconf_chunk),
		    &p6_buf.ph, sizeof(*p6));
		if (p6 == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("findassociation_ep_asconf: failed to get asconf v6 lookup addr\n");
			}
#endif				/* SCTP_DEBUG */
			return (NULL);
		}
		sin6 = (struct sockaddr_in6 *)&remote_store;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_port = sh->src_port;
		memcpy(&sin6->sin6_addr, &p6->addr, sizeof(struct in6_addr));
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
			zero_address = 1;
	} else if (ptype == SCTP_IPV4_ADDRESS) {
		/* ipv4 address param */
		struct sctp_ipv4addr_param *p4, p4_buf;

		if (ntohs(phdr->param_length) != sizeof(struct sctp_ipv4addr_param)) {
			return NULL;
		}
		p4 = (struct sctp_ipv4addr_param *)sctp_get_next_param(m,
		    offset + sizeof(struct sctp_asconf_chunk),
		    &p4_buf.ph, sizeof(*p4));
		if (p4 == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_INPUT3) {
				printf("findassociation_ep_asconf: failed to get asconf v4 lookup addr\n");
			}
#endif				/* SCTP_DEBUG */
			return (NULL);
		}
		sin = (struct sockaddr_in *)&remote_store;
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_port = sh->src_port;
		memcpy(&sin->sin_addr, &p4->addr, sizeof(struct in_addr));
		if (sin->sin_addr.s_addr == INADDR_ANY)
			zero_address = 1;
	} else {
		/* invalid address param type */
		return NULL;
	}

	if (zero_address) {
		stcb = sctp_findassoc_by_vtag(NULL, ntohl(sh->v_tag), inp_p,
		    netp, sh->src_port, sh->dest_port, 1);
		/*
		 * printf("findassociation_ep_asconf: zero lookup address
		 * finds stcb 0x%x\n", (uint32_t)stcb);
		 */
	} else {
		stcb = sctp_findassociation_ep_addr(inp_p,
		    (struct sockaddr *)&remote_store, netp,
		    (struct sockaddr *)&local_store, NULL);
	}
	return (stcb);
}


extern int sctp_max_burst_default;

extern unsigned int sctp_delayed_sack_time_default;
extern unsigned int sctp_heartbeat_interval_default;
extern unsigned int sctp_pmtu_raise_time_default;
extern unsigned int sctp_shutdown_guard_time_default;
extern unsigned int sctp_secret_lifetime_default;

extern unsigned int sctp_rto_max_default;
extern unsigned int sctp_rto_min_default;
extern unsigned int sctp_rto_initial_default;
extern unsigned int sctp_init_rto_max_default;
extern unsigned int sctp_valid_cookie_life_default;
extern unsigned int sctp_init_rtx_max_default;
extern unsigned int sctp_assoc_rtx_max_default;
extern unsigned int sctp_path_rtx_max_default;
extern unsigned int sctp_nr_outgoing_streams_default;

/*
 * allocate a sctp_inpcb and setup a temporary binding to a port/all
 * addresses. This way if we don't get a bind we by default pick a ephemeral
 * port with all addresses bound.
 */
int
sctp_inpcb_alloc(struct socket *so)
{
	/*
	 * we get called when a new endpoint starts up. We need to allocate
	 * the sctp_inpcb structure from the zone and init it. Mark it as
	 * unbound and find a port that we can use as an ephemeral with
	 * INADDR_ANY. If the user binds later no problem we can then add in
	 * the specific addresses. And setup the default parameters for the
	 * EP.
	 */
	int i, error;
	struct sctp_inpcb *inp;

	struct sctp_pcb *m;
	struct timeval time;
	sctp_sharedkey_t *null_key;

	error = 0;

	SCTP_INP_INFO_WLOCK();
	inp = (struct sctp_inpcb *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_ep);
	if (inp == NULL) {
		printf("Out of SCTP-INPCB structures - no resources\n");
		SCTP_INP_INFO_WUNLOCK();
		return (ENOBUFS);
	}
	/* zap it */
	bzero(inp, sizeof(*inp));

	/* bump generations */
	/* setup socket pointers */
	inp->sctp_socket = so;
	inp->ip_inp.inp.inp_socket = so;

	inp->partial_delivery_point = so->so_rcv.sb_hiwat >> SCTP_PARTIAL_DELIVERY_SHIFT;
	inp->sctp_frag_point = SCTP_DEFAULT_MAXSEGMENT;

#ifdef IPSEC
	{
		struct inpcbpolicy *pcb_sp = NULL;

		error = ipsec_init_pcbpolicy(so, &pcb_sp);
		/* Arrange to share the policy */
		inp->ip_inp.inp.inp_sp = pcb_sp;
		((struct in6pcb *)(&inp->ip_inp.inp))->in6p_sp = pcb_sp;
	}
	if (error != 0) {
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		SCTP_INP_INFO_WUNLOCK();
		return error;
	}
#endif				/* IPSEC */
	SCTP_INCR_EP_COUNT();
	inp->ip_inp.inp.inp_ip_ttl = ip_defttl;
	SCTP_INP_INFO_WUNLOCK();

	so->so_pcb = (caddr_t)inp;

	if ((so->so_type == SOCK_DGRAM) ||
	    (so->so_type == SOCK_SEQPACKET)) {
		/* UDP style socket */
		inp->sctp_flags = (SCTP_PCB_FLAGS_UDPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);
		sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
		/* Be sure it is NON-BLOCKING IO for UDP */
		/* so->so_state |= SS_NBIO; */
	} else if (so->so_type == SOCK_STREAM) {
		/* TCP style socket */
		inp->sctp_flags = (SCTP_PCB_FLAGS_TCPTYPE |
		    SCTP_PCB_FLAGS_UNBOUND);
		sctp_feature_on(inp, SCTP_PCB_FLAGS_RECVDATAIOEVNT);
		/* Be sure we have blocking IO by default */
		so->so_state &= ~SS_NBIO;
	} else {
		/*
		 * unsupported socket type (RAW, etc)- in case we missed it
		 * in protosw
		 */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		return (EOPNOTSUPP);
	}
	inp->sctp_tcbhash = SCTP_HASH_INIT(sctp_pcbtblsize,
	    &inp->sctp_hashmark);
	if (inp->sctp_tcbhash == NULL) {
		printf("Out of SCTP-INPCB->hashinit - no resources\n");
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
		return (ENOBUFS);
	}
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_LOCK_INIT(inp);
	SCTP_INP_READ_INIT(inp);
	SCTP_ASOC_CREATE_LOCK_INIT(inp);
	/* lock the new ep */
	SCTP_INP_WLOCK(inp);

	/* add it to the info area */
	LIST_INSERT_HEAD(&sctppcbinfo.listhead, inp, sctp_list);
	SCTP_INP_INFO_WUNLOCK();

	TAILQ_INIT(&inp->read_queue);
	LIST_INIT(&inp->sctp_addr_list);
	LIST_INIT(&inp->sctp_asoc_list);

#ifdef SCTP_TRACK_FREED_ASOCS
	/* TEMP CODE */
	LIST_INIT(&inp->sctp_asoc_free_list);
#endif
	/* Init the timer structure for signature change */
	SCTP_OS_TIMER_INIT(&inp->sctp_ep.signature_change.timer);
	inp->sctp_ep.signature_change.type = SCTP_TIMER_TYPE_NEWCOOKIE;

	/* now init the actual endpoint default data */
	m = &inp->sctp_ep;

	/* setup the base timeout information */
	m->sctp_timeoutticks[SCTP_TIMER_SEND] = SEC_TO_TICKS(SCTP_SEND_SEC);	/* needed ? */
	m->sctp_timeoutticks[SCTP_TIMER_INIT] = SEC_TO_TICKS(SCTP_INIT_SEC);	/* needed ? */
	m->sctp_timeoutticks[SCTP_TIMER_RECV] = MSEC_TO_TICKS(sctp_delayed_sack_time_default);
	m->sctp_timeoutticks[SCTP_TIMER_HEARTBEAT] = MSEC_TO_TICKS(sctp_heartbeat_interval_default);
	m->sctp_timeoutticks[SCTP_TIMER_PMTU] = SEC_TO_TICKS(sctp_pmtu_raise_time_default);
	m->sctp_timeoutticks[SCTP_TIMER_MAXSHUTDOWN] = SEC_TO_TICKS(sctp_shutdown_guard_time_default);
	m->sctp_timeoutticks[SCTP_TIMER_SIGNATURE] = SEC_TO_TICKS(sctp_secret_lifetime_default);
	/* all max/min max are in ms */
	m->sctp_maxrto = sctp_rto_max_default;
	m->sctp_minrto = sctp_rto_min_default;
	m->initial_rto = sctp_rto_initial_default;
	m->initial_init_rto_max = sctp_init_rto_max_default;

	m->max_open_streams_intome = MAX_SCTP_STREAMS;

	m->max_init_times = sctp_init_rtx_max_default;
	m->max_send_times = sctp_assoc_rtx_max_default;
	m->def_net_failure = sctp_path_rtx_max_default;
	m->sctp_sws_sender = SCTP_SWS_SENDER_DEF;
	m->sctp_sws_receiver = SCTP_SWS_RECEIVER_DEF;
	m->max_burst = sctp_max_burst_default;
	/* number of streams to pre-open on a association */
	m->pre_open_stream_count = sctp_nr_outgoing_streams_default;

	/* Add adaptation cookie */
	m->adaptation_layer_indicator = 0x504C5253;

	/* seed random number generator */
	m->random_counter = 1;
	m->store_at = SCTP_SIGNATURE_SIZE;
	SCTP_READ_RANDOM(m->random_numbers, sizeof(m->random_numbers));
	sctp_fill_random_store(m);

	/* Minimum cookie size */
	m->size_of_a_cookie = (sizeof(struct sctp_init_msg) * 2) +
	    sizeof(struct sctp_state_cookie);
	m->size_of_a_cookie += SCTP_SIGNATURE_SIZE;

	/* Setup the initial secret */
	SCTP_GETTIME_TIMEVAL(&time);
	m->time_of_secret_change = time.tv_sec;

	for (i = 0; i < SCTP_NUMBER_OF_SECRETS; i++) {
		m->secret_key[0][i] = sctp_select_initial_TSN(m);
	}
	sctp_timer_start(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL);

	/* How long is a cookie good for ? */
	m->def_cookie_life = sctp_valid_cookie_life_default;

	/*
	 * Initialize authentication parameters
	 */
	m->local_hmacs = sctp_default_supported_hmaclist();
	m->local_auth_chunks = sctp_alloc_chunklist();
	sctp_auth_set_default_chunks(m->local_auth_chunks);
	LIST_INIT(&m->shared_keys);
	/* add default NULL key as key id 0 */
	null_key = sctp_alloc_sharedkey();
	sctp_insert_sharedkey(&m->shared_keys, null_key);
	SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 12);
#endif
	return (error);
}


void
sctp_move_pcb_and_assoc(struct sctp_inpcb *old_inp, struct sctp_inpcb *new_inp,
    struct sctp_tcb *stcb)
{
	struct sctp_nets *net;
	uint16_t lport, rport;
	struct sctppcbhead *head;
	struct sctp_laddr *laddr, *oladdr;

	SCTP_TCB_UNLOCK(stcb);
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(old_inp);
	SCTP_INP_WLOCK(new_inp);
	SCTP_TCB_LOCK(stcb);

	new_inp->sctp_ep.time_of_secret_change =
	    old_inp->sctp_ep.time_of_secret_change;
	memcpy(new_inp->sctp_ep.secret_key, old_inp->sctp_ep.secret_key,
	    sizeof(old_inp->sctp_ep.secret_key));
	new_inp->sctp_ep.current_secret_number =
	    old_inp->sctp_ep.current_secret_number;
	new_inp->sctp_ep.last_secret_number =
	    old_inp->sctp_ep.last_secret_number;
	new_inp->sctp_ep.size_of_a_cookie = old_inp->sctp_ep.size_of_a_cookie;

	/* make it so new data pours into the new socket */
	stcb->sctp_socket = new_inp->sctp_socket;
	stcb->sctp_ep = new_inp;

	/* Copy the port across */
	lport = new_inp->sctp_lport = old_inp->sctp_lport;
	rport = stcb->rport;
	/* Pull the tcb from the old association */
	LIST_REMOVE(stcb, sctp_tcbhash);
	LIST_REMOVE(stcb, sctp_tcblist);

	/* Now insert the new_inp into the TCP connected hash */
	head = &sctppcbinfo.sctp_tcpephash[SCTP_PCBHASH_ALLADDR((lport + rport),
	    sctppcbinfo.hashtcpmark)];

	LIST_INSERT_HEAD(head, new_inp, sctp_hash);

	/* Now move the tcb into the endpoint list */
	LIST_INSERT_HEAD(&new_inp->sctp_asoc_list, stcb, sctp_tcblist);
	/*
	 * Question, do we even need to worry about the ep-hash since we
	 * only have one connection? Probably not :> so lets get rid of it
	 * and not suck up any kernel memory in that.
	 */

	/* Ok. Let's restart timer. */
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, new_inp,
		    stcb, net);
	}

	SCTP_INP_INFO_WUNLOCK();
	if (new_inp->sctp_tcbhash != NULL) {
		SCTP_HASH_FREE(new_inp->sctp_tcbhash, new_inp->sctp_hashmark);
		new_inp->sctp_tcbhash = NULL;
	}
	if ((new_inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
		/* Subset bound, so copy in the laddr list from the old_inp */
		LIST_FOREACH(oladdr, &old_inp->sctp_addr_list, sctp_nxt_addr) {
			laddr = (struct sctp_laddr *)SCTP_ZONE_GET(
			    sctppcbinfo.ipi_zone_laddr);
			if (laddr == NULL) {
				/*
				 * Gak, what can we do? This assoc is really
				 * HOSED. We probably should send an abort
				 * here.
				 */
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_PCB1) {
					printf("Association hosed in TCP model, out of laddr memory\n");
				}
#endif				/* SCTP_DEBUG */
				continue;
			}
			SCTP_INCR_LADDR_COUNT();
			bzero(laddr, sizeof(*laddr));
			laddr->ifa = oladdr->ifa;
			LIST_INSERT_HEAD(&new_inp->sctp_addr_list, laddr,
			    sctp_nxt_addr);
			new_inp->laddr_count++;
		}
	}
	/*
	 * Now any running timers need to be adjusted since we really don't
	 * care if they are running or not just blast in the new_inp into
	 * all of them.
	 */

	stcb->asoc.hb_timer.ep = (void *)new_inp;
	stcb->asoc.dack_timer.ep = (void *)new_inp;
	stcb->asoc.asconf_timer.ep = (void *)new_inp;
	stcb->asoc.strreset_timer.ep = (void *)new_inp;
	stcb->asoc.shut_guard_timer.ep = (void *)new_inp;
	stcb->asoc.autoclose_timer.ep = (void *)new_inp;
	stcb->asoc.delayed_event_timer.ep = (void *)new_inp;
	/* now what about the nets? */
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		net->pmtu_timer.ep = (void *)new_inp;
		net->rxt_timer.ep = (void *)new_inp;
		net->fr_timer.ep = (void *)new_inp;
	}
	SCTP_INP_WUNLOCK(new_inp);
	SCTP_INP_WUNLOCK(old_inp);
}

static int
sctp_isport_inuse(struct sctp_inpcb *inp, uint16_t lport)
{
	struct sctppcbhead *head;
	struct sctp_inpcb *t_inp;

	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];

	LIST_FOREACH(t_inp, head, sctp_hash) {
		if (t_inp->sctp_lport != lport) {
			continue;
		}
		/* This one is in use. */
		/* check the v6/v4 binding issue */
		if ((t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
		    SCTP_IPV6_V6ONLY(t_inp)) {
			if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
				/* collision in V6 space */
				return (1);
			} else {
				/* inp is BOUND_V4 no conflict */
				continue;
			}
		} else if (t_inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) {
			/* t_inp is bound v4 and v6, conflict always */
			return (1);
		} else {
			/* t_inp is bound only V4 */
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_BOUND_V6) &&
			    SCTP_IPV6_V6ONLY(t_inp)) {
				/* no conflict */
				continue;
			}
			/* else fall through to conflict */
		}
		return (1);
	}
	return (0);
}



int
sctp_inpcb_bind(struct socket *so, struct sockaddr *addr, struct thread *p)
{
	/* bind a ep to a socket address */
	struct sctppcbhead *head;
	struct sctp_inpcb *inp, *inp_tmp;
	struct inpcb *ip_inp;
	int bindall;
	uint16_t lport;
	int error;

	lport = 0;
	error = 0;
	bindall = 1;
	inp = (struct sctp_inpcb *)so->so_pcb;
	ip_inp = (struct inpcb *)so->so_pcb;
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		if (addr) {
			printf("Bind called port:%d\n",
			    ntohs(((struct sockaddr_in *)addr)->sin_port));
			printf("Addr :");
			sctp_print_address(addr);
		}
	}
#endif				/* SCTP_DEBUG */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) == 0) {
		/* already did a bind, subsequent binds NOT allowed ! */
		return (EINVAL);
	}
	if (addr != NULL) {
		if (addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			/* IPV6_V6ONLY socket? */
			if (SCTP_IPV6_V6ONLY(ip_inp)) {
				return (EINVAL);
			}
			if (addr->sa_len != sizeof(*sin))
				return (EINVAL);

			sin = (struct sockaddr_in *)addr;
			lport = sin->sin_port;

			if (sin->sin_addr.s_addr != INADDR_ANY) {
				bindall = 0;
			}
		} else if (addr->sa_family == AF_INET6) {
			/* Only for pure IPv6 Address. (No IPv4 Mapped!) */
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)addr;

			if (addr->sa_len != sizeof(*sin6))
				return (EINVAL);

			lport = sin6->sin6_port;
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				bindall = 0;
				/* KAME hack: embed scopeid */
				if (sa6_embedscope(sin6, ip6_use_defzone) != 0)
					return (EINVAL);
			}
			/* this must be cleared for ifa_ifwithaddr() */
			sin6->sin6_scope_id = 0;
		} else {
			return (EAFNOSUPPORT);
		}
	}
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
	/* increase our count due to the unlock we do */
	SCTP_INP_INCR_REF(inp);
	if (lport) {
		/*
		 * Did the caller specify a port? if so we must see if a ep
		 * already has this one bound.
		 */
		/* got to be root to get at low ports */
		if (ntohs(lport) < IPPORT_RESERVED) {
			if (p && (error =
			    priv_check(p,
			    PRIV_NETINET_RESERVEDPORT)
			    )) {
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
				SCTP_INP_INFO_WUNLOCK();
				return (error);
			}
		}
		if (p == NULL) {
			SCTP_INP_DECR_REF(inp);
			SCTP_INP_WUNLOCK(inp);
			SCTP_INP_INFO_WUNLOCK();
			return (error);
		}
		SCTP_INP_WUNLOCK(inp);
		inp_tmp = sctp_pcb_findep(addr, 0, 1);
		if (inp_tmp != NULL) {
			/*
			 * lock guy returned and lower count note that we
			 * are not bound so inp_tmp should NEVER be inp. And
			 * it is this inp (inp_tmp) that gets the reference
			 * bump, so we must lower it.
			 */
			SCTP_INP_DECR_REF(inp_tmp);
			SCTP_INP_DECR_REF(inp);
			/* unlock info */
			SCTP_INP_INFO_WUNLOCK();
			return (EADDRNOTAVAIL);
		}
		SCTP_INP_WLOCK(inp);
		if (bindall) {
			/* verify that no lport is not used by a singleton */
			if (sctp_isport_inuse(inp, lport)) {
				/* Sorry someone already has this one bound */
				SCTP_INP_DECR_REF(inp);
				SCTP_INP_WUNLOCK(inp);
				SCTP_INP_INFO_WUNLOCK();
				return (EADDRNOTAVAIL);
			}
		}
	} else {
		/*
		 * get any port but lets make sure no one has any address
		 * with this port bound
		 */

		/*
		 * setup the inp to the top (I could use the union but this
		 * is just as easy
		 */
		uint32_t port_guess;
		uint16_t port_attempt;
		int not_done = 1;

		while (not_done) {
			port_guess = sctp_select_initial_TSN(&inp->sctp_ep);
			port_attempt = (port_guess & 0x0000ffff);
			if (port_attempt == 0) {
				goto next_half;
			}
			if (port_attempt < IPPORT_RESERVED) {
				port_attempt += IPPORT_RESERVED;
			}
			if (sctp_isport_inuse(inp, htons(port_attempt)) == 0) {
				/* got a port we can use */
				not_done = 0;
				continue;
			}
			/* try upper half */
	next_half:
			port_attempt = ((port_guess >> 16) & 0x0000ffff);
			if (port_attempt == 0) {
				goto last_try;
			}
			if (port_attempt < IPPORT_RESERVED) {
				port_attempt += IPPORT_RESERVED;
			}
			if (sctp_isport_inuse(inp, htons(port_attempt)) == 0) {
				/* got a port we can use */
				not_done = 0;
				continue;
			}
			/* try two half's added together */
	last_try:
			port_attempt = (((port_guess >> 16) & 0x0000ffff) +
			    (port_guess & 0x0000ffff));
			if (port_attempt == 0) {
				/* get a new random number */
				continue;
			}
			if (port_attempt < IPPORT_RESERVED) {
				port_attempt += IPPORT_RESERVED;
			}
			if (sctp_isport_inuse(inp, htons(port_attempt)) == 0) {
				/* got a port we can use */
				not_done = 0;
				continue;
			}
		}
		/* we don't get out of the loop until we have a port */
		lport = htons(port_attempt);
	}
	SCTP_INP_DECR_REF(inp);
	if (inp->sctp_flags & (SCTP_PCB_FLAGS_SOCKET_GONE |
	    SCTP_PCB_FLAGS_SOCKET_ALLGONE)) {
		/*
		 * this really should not happen. The guy did a non-blocking
		 * bind and then did a close at the same time.
		 */
		SCTP_INP_WUNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		return (EINVAL);
	}
	/* ok we look clear to give out this port, so lets setup the binding */
	if (bindall) {
		/* binding to all addresses, so just set in the proper flags */
		inp->sctp_flags |= SCTP_PCB_FLAGS_BOUNDALL;
		sctp_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF);
		/* set the automatic addr changes from kernel flag */
		if (sctp_auto_asconf == 0) {
			sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		} else {
			sctp_feature_on(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		}
	} else {
		/*
		 * bind specific, make sure flags is off and add a new
		 * address structure to the sctp_addr_list inside the ep
		 * structure.
		 * 
		 * We will need to allocate one and insert it at the head. The
		 * socketopt call can just insert new addresses in there as
		 * well. It will also have to do the embed scope kame hack
		 * too (before adding).
		 */
		struct ifaddr *ifa;
		struct sockaddr_storage store_sa;

		memset(&store_sa, 0, sizeof(store_sa));
		if (addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)&store_sa;
			memcpy(sin, addr, sizeof(struct sockaddr_in));
			sin->sin_port = 0;
		} else if (addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)&store_sa;
			memcpy(sin6, addr, sizeof(struct sockaddr_in6));
			sin6->sin6_port = 0;
		}
		/*
		 * first find the interface with the bound address need to
		 * zero out the port to find the address! yuck! can't do
		 * this earlier since need port for sctp_pcb_findep()
		 */
		ifa = sctp_find_ifa_by_addr((struct sockaddr *)&store_sa);
		if (ifa == NULL) {
			/* Can't find an interface with that address */
			SCTP_INP_WUNLOCK(inp);
			SCTP_INP_INFO_WUNLOCK();
			return (EADDRNOTAVAIL);
		}
		if (addr->sa_family == AF_INET6) {
			struct in6_ifaddr *ifa6;

			ifa6 = (struct in6_ifaddr *)ifa;
			/*
			 * allow binding of deprecated addresses as per RFC
			 * 2462 and ipng discussion
			 */
			if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
			    IN6_IFF_ANYCAST |
			    IN6_IFF_NOTREADY)) {
				/* Can't bind a non-existent addr. */
				SCTP_INP_WUNLOCK(inp);
				SCTP_INP_INFO_WUNLOCK();
				return (EINVAL);
			}
		}
		/* we're not bound all */
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_BOUNDALL;
		/* set the automatic addr changes from kernel flag */
		if (sctp_auto_asconf == 0) {
			sctp_feature_off(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		} else {
			sctp_feature_on(inp, SCTP_PCB_FLAGS_AUTO_ASCONF);
		}
		/* allow bindx() to send ASCONF's for binding changes */
		sctp_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF);
		/* add this address to the endpoint list */
		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa);
		if (error != 0) {
			SCTP_INP_WUNLOCK(inp);
			SCTP_INP_INFO_WUNLOCK();
			return (error);
		}
		inp->laddr_count++;
	}
	/* find the bucket */
	head = &sctppcbinfo.sctp_ephash[SCTP_PCBHASH_ALLADDR(lport,
	    sctppcbinfo.hashmark)];
	/* put it in the bucket */
	LIST_INSERT_HEAD(head, inp, sctp_hash);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Main hash to bind at head:%p, bound port:%d\n", head, ntohs(lport));
	}
#endif
	/* set in the port */
	inp->sctp_lport = lport;

	/* turn off just the unbound flag */
	inp->sctp_flags &= ~SCTP_PCB_FLAGS_UNBOUND;
	SCTP_INP_WUNLOCK(inp);
	SCTP_INP_INFO_WUNLOCK();
	return (0);
}


static void
sctp_iterator_inp_being_freed(struct sctp_inpcb *inp, struct sctp_inpcb *inp_next)
{
	struct sctp_iterator *it;

	/*
	 * We enter with the only the ITERATOR_LOCK in place and a write
	 * lock on the inp_info stuff.
	 */

	/*
	 * Go through all iterators, we must do this since it is possible
	 * that some iterator does NOT have the lock, but is waiting for it.
	 * And the one that had the lock has either moved in the last
	 * iteration or we just cleared it above. We need to find all of
	 * those guys. The list of iterators should never be very big
	 * though.
	 */
	LIST_FOREACH(it, &sctppcbinfo.iteratorhead, sctp_nxt_itr) {
		if (it == inp->inp_starting_point_for_iterator)
			/* skip this guy, he's special */
			continue;
		if (it->inp == inp) {
			/*
			 * This is tricky and we DON'T lock the iterator.
			 * Reason is he's running but waiting for me since
			 * inp->inp_starting_point_for_iterator has the lock
			 * on me (the guy above we skipped). This tells us
			 * its is not running but waiting for
			 * inp->inp_starting_point_for_iterator to be
			 * released by the guy that does have our INP in a
			 * lock.
			 */
			if (it->iterator_flags & SCTP_ITERATOR_DO_SINGLE_INP) {
				it->inp = NULL;
				it->stcb = NULL;
			} else {
				/* set him up to do the next guy not me */
				it->inp = inp_next;
				it->stcb = NULL;
			}
		}
	}
	it = inp->inp_starting_point_for_iterator;
	if (it) {
		if (it->iterator_flags & SCTP_ITERATOR_DO_SINGLE_INP) {
			it->inp = NULL;
		} else {
			it->inp = inp_next;
		}
		it->stcb = NULL;
	}
}

/* release sctp_inpcb unbind the port */
void
sctp_inpcb_free(struct sctp_inpcb *inp, int immediate, int from)
{
	/*
	 * Here we free a endpoint. We must find it (if it is in the Hash
	 * table) and remove it from there. Then we must also find it in the
	 * overall list and remove it from there. After all removals are
	 * complete then any timer has to be stopped. Then start the actual
	 * freeing. a) Any local lists. b) Any associations. c) The hash of
	 * all associations. d) finally the ep itself.
	 */
	struct sctp_pcb *m;
	struct sctp_inpcb *inp_save;
	struct sctp_tcb *asoc, *nasoc;
	struct sctp_laddr *laddr, *nladdr;
	struct inpcb *ip_pcb;
	struct socket *so;

	struct sctp_queued_to_read *sq;

	int s, cnt;
	sctp_sharedkey_t *shared_key;

	s = splnet();

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 0);
#endif

	SCTP_ITERATOR_LOCK();
	so = inp->sctp_socket;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) {
		/* been here before.. eeks.. get out of here */
		splx(s);
		printf("This conflict in free SHOULD not be happening!\n");
		SCTP_ITERATOR_UNLOCK();
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 1);
#endif
		return;
	}
	SCTP_ASOC_CREATE_LOCK(inp);
	SCTP_INP_INFO_WLOCK();

	SCTP_INP_WLOCK(inp);
	/*
	 * First time through we have the socket lock, after that no more.
	 */
	if (from == 1) {
		/*
		 * Once we are in we can remove the flag from = 1 is only
		 * passed from the actual closing routines that are called
		 * via the sockets layer.
		 */
		inp->sctp_flags &= ~SCTP_PCB_FLAGS_CLOSE_IP;
	}
	sctp_timer_stop(SCTP_TIMER_TYPE_NEWCOOKIE, inp, NULL, NULL,
	    SCTP_FROM_SCTP_PCB + SCTP_LOC_1);

	if (inp->control) {
		sctp_m_freem(inp->control);
		inp->control = NULL;
	}
	if (inp->pkt) {
		sctp_m_freem(inp->pkt);
		inp->pkt = NULL;
	}
	m = &inp->sctp_ep;
	ip_pcb = &inp->ip_inp.inp;	/* we could just cast the main pointer
					 * here but I will be nice :> (i.e.
					 * ip_pcb = ep;) */
	if (immediate == 0) {
		int cnt_in_sd;

		cnt_in_sd = 0;
		for ((asoc = LIST_FIRST(&inp->sctp_asoc_list)); asoc != NULL;
		    asoc = nasoc) {
			nasoc = LIST_NEXT(asoc, sctp_tcblist);
			if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
				/* Skip guys being freed */
				asoc->sctp_socket = NULL;
				cnt_in_sd++;
				continue;
			}
			if ((SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_COOKIE_WAIT) ||
			    (SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_COOKIE_ECHOED)) {
				/* Just abandon things in the front states */
				if (asoc->asoc.total_output_queue_size == 0) {
					sctp_free_assoc(inp, asoc, SCTP_PCBFREE_NOFORCE, SCTP_FROM_SCTP_PCB + SCTP_LOC_2);
					continue;
				}
			}
			SCTP_TCB_LOCK(asoc);
			/* Disconnect the socket please */
			asoc->sctp_socket = NULL;
			asoc->asoc.state |= SCTP_STATE_CLOSED_SOCKET;
			if ((asoc->asoc.size_on_reasm_queue > 0) ||
			    (asoc->asoc.control_pdapi) ||
			    (asoc->asoc.size_on_all_streams > 0) ||
			    (so && (so->so_rcv.sb_cc > 0))
			    ) {
				/* Left with Data unread */
				struct mbuf *op_err;

				op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
				    0, M_DONTWAIT, 1, MT_DATA);
				if (op_err) {
					/* Fill in the user initiated abort */
					struct sctp_paramhdr *ph;
					uint32_t *ippp;

					SCTP_BUF_LEN(op_err) =
					    sizeof(struct sctp_paramhdr) + sizeof(uint32_t);
					ph = mtod(op_err,
					    struct sctp_paramhdr *);
					ph->param_type = htons(
					    SCTP_CAUSE_USER_INITIATED_ABT);
					ph->param_length = htons(SCTP_BUF_LEN(op_err));
					ippp = (uint32_t *) (ph + 1);
					*ippp = htonl(SCTP_FROM_SCTP_PCB + SCTP_LOC_3);
				}
				asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_3;
				sctp_send_abort_tcb(asoc, op_err);
				SCTP_STAT_INCR_COUNTER32(sctps_aborted);
				if ((SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_OPEN) ||
				    (SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
				}
				sctp_free_assoc(inp, asoc, SCTP_PCBFREE_NOFORCE, SCTP_FROM_SCTP_PCB + SCTP_LOC_4);
				continue;
			} else if (TAILQ_EMPTY(&asoc->asoc.send_queue) &&
				    TAILQ_EMPTY(&asoc->asoc.sent_queue) &&
				    (asoc->asoc.stream_queue_cnt == 0)
			    ) {
				if (asoc->asoc.locked_on_sending) {
					goto abort_anyway;
				}
				if ((SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_SHUTDOWN_SENT) &&
				    (SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_SHUTDOWN_ACK_SENT)) {
					/*
					 * there is nothing queued to send,
					 * so I send shutdown
					 */
					sctp_send_shutdown(asoc, asoc->asoc.primary_destination);
					asoc->asoc.state = SCTP_STATE_SHUTDOWN_SENT;
					SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWN, asoc->sctp_ep, asoc,
					    asoc->asoc.primary_destination);
					sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, asoc->sctp_ep, asoc,
					    asoc->asoc.primary_destination);
					sctp_chunk_output(inp, asoc, SCTP_OUTPUT_FROM_SHUT_TMR);
				}
			} else {
				/* mark into shutdown pending */
				struct sctp_stream_queue_pending *sp;

				asoc->asoc.state |= SCTP_STATE_SHUTDOWN_PENDING;
				sctp_timer_start(SCTP_TIMER_TYPE_SHUTDOWNGUARD, asoc->sctp_ep, asoc,
				    asoc->asoc.primary_destination);
				if (asoc->asoc.locked_on_sending) {
					sp = TAILQ_LAST(&((asoc->asoc.locked_on_sending)->outqueue),
					    sctp_streamhead);
					if (sp == NULL) {
						printf("Error, sp is NULL, locked on sending is %p strm:%d\n",
						    asoc->asoc.locked_on_sending,
						    asoc->asoc.locked_on_sending->stream_no);
					} else {
						if ((sp->length == 0) && (sp->msg_is_complete == 0))
							asoc->asoc.state |= SCTP_STATE_PARTIAL_MSG_LEFT;
					}
				}
				if (TAILQ_EMPTY(&asoc->asoc.send_queue) &&
				    TAILQ_EMPTY(&asoc->asoc.sent_queue) &&
				    (asoc->asoc.state & SCTP_STATE_PARTIAL_MSG_LEFT)) {
					struct mbuf *op_err;

			abort_anyway:
					op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
					    0, M_DONTWAIT, 1, MT_DATA);
					if (op_err) {
						/*
						 * Fill in the user
						 * initiated abort
						 */
						struct sctp_paramhdr *ph;
						uint32_t *ippp;

						SCTP_BUF_LEN(op_err) =
						    (sizeof(struct sctp_paramhdr) +
						    sizeof(uint32_t));
						ph = mtod(op_err,
						    struct sctp_paramhdr *);
						ph->param_type = htons(
						    SCTP_CAUSE_USER_INITIATED_ABT);
						ph->param_length = htons(SCTP_BUF_LEN(op_err));
						ippp = (uint32_t *) (ph + 1);
						*ippp = htonl(SCTP_FROM_SCTP_PCB + SCTP_LOC_5);
					}
					asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_5;
					sctp_send_abort_tcb(asoc, op_err);
					SCTP_STAT_INCR_COUNTER32(sctps_aborted);
					if ((SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_OPEN) ||
					    (SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
						SCTP_STAT_DECR_GAUGE32(sctps_currestab);
					}
					sctp_free_assoc(inp, asoc, SCTP_PCBFREE_NOFORCE, SCTP_FROM_SCTP_PCB + SCTP_LOC_6);
					continue;
				}
			}
			cnt_in_sd++;
			SCTP_TCB_UNLOCK(asoc);
		}
		/* now is there some left in our SHUTDOWN state? */
		if (cnt_in_sd) {
			splx(s);

			SCTP_INP_WUNLOCK(inp);
			SCTP_ASOC_CREATE_UNLOCK(inp);
			SCTP_INP_INFO_WUNLOCK();
			SCTP_ITERATOR_UNLOCK();
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, NULL, 2);
#endif
			return;
		}
	}
	inp->sctp_socket = NULL;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) !=
	    SCTP_PCB_FLAGS_UNBOUND) {
		/*
		 * ok, this guy has been bound. It's port is somewhere in
		 * the sctppcbinfo hash table. Remove it!
		 */
		LIST_REMOVE(inp, sctp_hash);
		inp->sctp_flags |= SCTP_PCB_FLAGS_UNBOUND;
	}
	/*
	 * If there is a timer running to kill us, forget it, since it may
	 * have a contest on the INP lock.. which would cause us to die ...
	 */
	cnt = 0;
	for ((asoc = LIST_FIRST(&inp->sctp_asoc_list)); asoc != NULL;
	    asoc = nasoc) {
		nasoc = LIST_NEXT(asoc, sctp_tcblist);
		if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
			cnt++;
			continue;
		}
		/* Free associations that are NOT killing us */
		SCTP_TCB_LOCK(asoc);
		if ((SCTP_GET_STATE(&asoc->asoc) != SCTP_STATE_COOKIE_WAIT) &&
		    ((asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) == 0)) {
			struct mbuf *op_err;
			uint32_t *ippp;

			op_err = sctp_get_mbuf_for_msg((sizeof(struct sctp_paramhdr) + sizeof(uint32_t)),
			    0, M_DONTWAIT, 1, MT_DATA);
			if (op_err) {
				/* Fill in the user initiated abort */
				struct sctp_paramhdr *ph;

				SCTP_BUF_LEN(op_err) = (sizeof(struct sctp_paramhdr) +
				    sizeof(uint32_t));
				ph = mtod(op_err, struct sctp_paramhdr *);
				ph->param_type = htons(
				    SCTP_CAUSE_USER_INITIATED_ABT);
				ph->param_length = htons(SCTP_BUF_LEN(op_err));
				ippp = (uint32_t *) (ph + 1);
				*ippp = htonl(SCTP_FROM_SCTP_PCB + SCTP_LOC_7);

			}
			asoc->sctp_ep->last_abort_code = SCTP_FROM_SCTP_PCB + SCTP_LOC_7;
			sctp_send_abort_tcb(asoc, op_err);
			SCTP_STAT_INCR_COUNTER32(sctps_aborted);
		} else if (asoc->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) {
			cnt++;
			SCTP_TCB_UNLOCK(asoc);
			continue;
		}
		if ((SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_OPEN) ||
		    (SCTP_GET_STATE(&asoc->asoc) == SCTP_STATE_SHUTDOWN_RECEIVED)) {
			SCTP_STAT_DECR_GAUGE32(sctps_currestab);
		}
		sctp_free_assoc(inp, asoc, SCTP_PCBFREE_FORCE, SCTP_FROM_SCTP_PCB + SCTP_LOC_8);
	}
	if (cnt) {
		/* Ok we have someone out there that will kill us */
		SCTP_OS_TIMER_STOP(&inp->sctp_ep.signature_change.timer);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		SCTP_ITERATOR_UNLOCK();
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 3);
#endif
		return;
	}
	if ((inp->refcount) || (inp->sctp_flags & SCTP_PCB_FLAGS_CLOSE_IP)) {
		SCTP_OS_TIMER_STOP(&inp->sctp_ep.signature_change.timer);
		sctp_timer_start(SCTP_TIMER_TYPE_INPKILL, inp, NULL, NULL);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ASOC_CREATE_UNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		SCTP_ITERATOR_UNLOCK();
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 4);
#endif
		return;
	}
	SCTP_OS_TIMER_STOP(&inp->sctp_ep.signature_change.timer);
	inp->sctp_ep.signature_change.type = 0;
	inp->sctp_flags |= SCTP_PCB_FLAGS_SOCKET_ALLGONE;

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 5);
#endif

	SCTP_OS_TIMER_STOP(&inp->sctp_ep.signature_change.timer);
	inp->sctp_ep.signature_change.type = SCTP_TIMER_TYPE_NONE;
	/* Clear the read queue */
	while ((sq = TAILQ_FIRST(&inp->read_queue)) != NULL) {
		TAILQ_REMOVE(&inp->read_queue, sq, next);
		sctp_free_remote_addr(sq->whoFrom);
		if (so)
			so->so_rcv.sb_cc -= sq->length;
		if (sq->data) {
			sctp_m_freem(sq->data);
			sq->data = NULL;
		}
		/*
		 * no need to free the net count, since at this point all
		 * assoc's are gone.
		 */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_readq, sq);
		SCTP_DECR_READQ_COUNT();
	}
	/* Now the sctp_pcb things */
	/*
	 * free each asoc if it is not already closed/free. we can't use the
	 * macro here since le_next will get freed as part of the
	 * sctp_free_assoc() call.
	 */
	cnt = 0;
	if (so) {
#ifdef IPSEC
		ipsec4_delete_pcbpolicy(ip_pcb);
#endif				/* IPSEC */

		/* Unlocks not needed since the socket is gone now */
	}
	if (ip_pcb->inp_options) {
		(void)sctp_m_free(ip_pcb->inp_options);
		ip_pcb->inp_options = 0;
	}
	if (ip_pcb->inp_moptions) {
		ip_freemoptions(ip_pcb->inp_moptions);
		ip_pcb->inp_moptions = 0;
	}
#ifdef INET6
	if (ip_pcb->inp_vflag & INP_IPV6) {
		struct in6pcb *in6p;

		in6p = (struct in6pcb *)inp;
		ip6_freepcbopts(in6p->in6p_outputopts);
	}
#endif				/* INET6 */
	ip_pcb->inp_vflag = 0;
	/* free up authentication fields */
	if (inp->sctp_ep.local_auth_chunks != NULL)
		sctp_free_chunklist(inp->sctp_ep.local_auth_chunks);
	if (inp->sctp_ep.local_hmacs != NULL)
		sctp_free_hmaclist(inp->sctp_ep.local_hmacs);

	shared_key = LIST_FIRST(&inp->sctp_ep.shared_keys);
	while (shared_key) {
		LIST_REMOVE(shared_key, next);
		sctp_free_sharedkey(shared_key);
		shared_key = LIST_FIRST(&inp->sctp_ep.shared_keys);
	}

	inp_save = LIST_NEXT(inp, sctp_list);
	LIST_REMOVE(inp, sctp_list);

	/* fix any iterators only after out of the list */
	sctp_iterator_inp_being_freed(inp, inp_save);
	/*
	 * if we have an address list the following will free the list of
	 * ifaddr's that are set into this ep. Again macro limitations here,
	 * since the LIST_FOREACH could be a bad idea.
	 */
	for ((laddr = LIST_FIRST(&inp->sctp_addr_list)); laddr != NULL;
	    laddr = nladdr) {
		nladdr = LIST_NEXT(laddr, sctp_nxt_addr);
		LIST_REMOVE(laddr, sctp_nxt_addr);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
		SCTP_DECR_LADDR_COUNT();
	}

#ifdef SCTP_TRACK_FREED_ASOCS
	/* TEMP CODE */
	for ((asoc = LIST_FIRST(&inp->sctp_asoc_free_list)); asoc != NULL;
	    asoc = nasoc) {
		nasoc = LIST_NEXT(asoc, sctp_tcblist);
		LIST_REMOVE(asoc, sctp_tcblist);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, asoc);
		SCTP_DECR_ASOC_COUNT();
	}
	/* *** END TEMP CODE *** */
#endif
	/* Now lets see about freeing the EP hash table. */
	if (inp->sctp_tcbhash != NULL) {
		SCTP_HASH_FREE(inp->sctp_tcbhash, inp->sctp_hashmark);
		inp->sctp_tcbhash = NULL;
	}
	/* Now we must put the ep memory back into the zone pool */
	SCTP_INP_LOCK_DESTROY(inp);
	SCTP_INP_READ_DESTROY(inp);
	SCTP_ASOC_CREATE_LOCK_DESTROY(inp);
	SCTP_INP_INFO_WUNLOCK();

	SCTP_ITERATOR_UNLOCK();

	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_ep, inp);
	SCTP_DECR_EP_COUNT();

	splx(s);
}


struct sctp_nets *
sctp_findnet(struct sctp_tcb *stcb, struct sockaddr *addr)
{
	struct sctp_nets *net;

	/* locate the address */
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		if (sctp_cmpaddr(addr, (struct sockaddr *)&net->ro._l_addr))
			return (net);
	}
	return (NULL);
}


/*
 * add's a remote endpoint address, done with the INIT/INIT-ACK as well as
 * when a ASCONF arrives that adds it. It will also initialize all the cwnd
 * stats of stuff.
 */
int
sctp_is_address_on_local_host(struct sockaddr *addr)
{
	struct ifnet *ifn;
	struct ifaddr *ifa;

	TAILQ_FOREACH(ifn, &ifnet, if_list) {
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			if (addr->sa_family == ifa->ifa_addr->sa_family) {
				/* same family */
				if (addr->sa_family == AF_INET) {
					struct sockaddr_in *sin, *sin_c;

					sin = (struct sockaddr_in *)addr;
					sin_c = (struct sockaddr_in *)
					    ifa->ifa_addr;
					if (sin->sin_addr.s_addr ==
					    sin_c->sin_addr.s_addr) {
						/*
						 * we are on the same
						 * machine
						 */
						return (1);
					}
				} else if (addr->sa_family == AF_INET6) {
					struct sockaddr_in6 *sin6, *sin_c6;

					sin6 = (struct sockaddr_in6 *)addr;
					sin_c6 = (struct sockaddr_in6 *)
					    ifa->ifa_addr;
					if (SCTP6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					    &sin_c6->sin6_addr)) {
						/*
						 * we are on the same
						 * machine
						 */
						return (1);
					}
				}
			}
		}
	}
	return (0);
}

int
sctp_add_remote_addr(struct sctp_tcb *stcb, struct sockaddr *newaddr,
    int set_scope, int from)
{
	/*
	 * The following is redundant to the same lines in the
	 * sctp_aloc_assoc() but is needed since other's call the add
	 * address function
	 */
	struct sctp_nets *net, *netfirst;
	int addr_inscope;

#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Adding an address (from:%d) to the peer: ", from);
		sctp_print_address(newaddr);
	}
#endif

	netfirst = sctp_findnet(stcb, newaddr);
	if (netfirst) {
		/*
		 * Lie and return ok, we don't want to make the association
		 * go away for this behavior. It will happen in the TCP
		 * model in a connected socket. It does not reach the hash
		 * table until after the association is built so it can't be
		 * found. Mark as reachable, since the initial creation will
		 * have been cleared and the NOT_IN_ASSOC flag will have
		 * been added... and we don't want to end up removing it
		 * back out.
		 */
		if (netfirst->dest_state & SCTP_ADDR_UNCONFIRMED) {
			netfirst->dest_state = (SCTP_ADDR_REACHABLE |
			    SCTP_ADDR_UNCONFIRMED);
		} else {
			netfirst->dest_state = SCTP_ADDR_REACHABLE;
		}

		return (0);
	}
	addr_inscope = 1;
	if (newaddr->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)newaddr;
		if (sin->sin_addr.s_addr == 0) {
			/* Invalid address */
			return (-1);
		}
		/* zero out the bzero area */
		memset(&sin->sin_zero, 0, sizeof(sin->sin_zero));

		/* assure len is set */
		sin->sin_len = sizeof(struct sockaddr_in);
		if (set_scope) {
#ifdef SCTP_DONT_DO_PRIVADDR_SCOPE
			stcb->ipv4_local_scope = 1;
#else
			if (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) {
				stcb->asoc.ipv4_local_scope = 1;
			}
#endif				/* SCTP_DONT_DO_PRIVADDR_SCOPE */

			if (sctp_is_address_on_local_host(newaddr)) {
				stcb->asoc.loopback_scope = 1;
				stcb->asoc.ipv4_local_scope = 1;
				stcb->asoc.local_scope = 1;
				stcb->asoc.site_scope = 1;
			}
		} else {
			if (from == SCTP_ADDR_IS_CONFIRMED) {
				/* From connectx */
				if (sctp_is_address_on_local_host(newaddr)) {
					stcb->asoc.loopback_scope = 1;
					stcb->asoc.ipv4_local_scope = 1;
					stcb->asoc.local_scope = 1;
					stcb->asoc.site_scope = 1;
				}
			}
			/* Validate the address is in scope */
			if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr)) &&
			    (stcb->asoc.ipv4_local_scope == 0)) {
				addr_inscope = 0;
			}
		}
	} else if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)newaddr;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/* Invalid address */
			return (-1);
		}
		/* assure len is set */
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		if (set_scope) {
			if (sctp_is_address_on_local_host(newaddr)) {
				stcb->asoc.loopback_scope = 1;
				stcb->asoc.local_scope = 1;
				stcb->asoc.ipv4_local_scope = 1;
				stcb->asoc.site_scope = 1;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				/*
				 * If the new destination is a LINK_LOCAL we
				 * must have common site scope. Don't set
				 * the local scope since we may not share
				 * all links, only loopback can do this.
				 * Links on the local network would also be
				 * on our private network for v4 too.
				 */
				stcb->asoc.ipv4_local_scope = 1;
				stcb->asoc.site_scope = 1;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {
				/*
				 * If the new destination is SITE_LOCAL then
				 * we must have site scope in common.
				 */
				stcb->asoc.site_scope = 1;
			}
		} else {
			if (from == SCTP_ADDR_IS_CONFIRMED) {
				/* From connectx so we check for localhost. */
				if (sctp_is_address_on_local_host(newaddr)) {
					stcb->asoc.loopback_scope = 1;
					stcb->asoc.ipv4_local_scope = 1;
					stcb->asoc.local_scope = 1;
					stcb->asoc.site_scope = 1;
				}
			}
			/* Validate the address is in scope */
			if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr) &&
			    (stcb->asoc.loopback_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
			    (stcb->asoc.local_scope == 0)) {
				addr_inscope = 0;
			} else if (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr) &&
			    (stcb->asoc.site_scope == 0)) {
				addr_inscope = 0;
			}
		}
	} else {
		/* not supported family type */
		return (-1);
	}
	net = (struct sctp_nets *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_net);
	if (net == NULL) {
		return (-1);
	}
	SCTP_INCR_RADDR_COUNT();
	bzero(net, sizeof(*net));
	memcpy(&net->ro._l_addr, newaddr, newaddr->sa_len);
	if (newaddr->sa_family == AF_INET) {
		((struct sockaddr_in *)&net->ro._l_addr)->sin_port = stcb->rport;
	} else if (newaddr->sa_family == AF_INET6) {
		((struct sockaddr_in6 *)&net->ro._l_addr)->sin6_port = stcb->rport;
	}
	net->addr_is_local = sctp_is_address_on_local_host(newaddr);
	net->failure_threshold = stcb->asoc.def_net_failure;
	if (addr_inscope == 0) {
		net->dest_state = (SCTP_ADDR_REACHABLE |
		    SCTP_ADDR_OUT_OF_SCOPE);
	} else {
		if (from == SCTP_ADDR_IS_CONFIRMED)
			/* SCTP_ADDR_IS_CONFIRMED is passed by connect_x */
			net->dest_state = SCTP_ADDR_REACHABLE;
		else
			net->dest_state = SCTP_ADDR_REACHABLE |
			    SCTP_ADDR_UNCONFIRMED;
	}
	net->RTO = stcb->asoc.initial_rto;
	stcb->asoc.numnets++;
	*(&net->ref_count) = 1;
	net->tos_flowlabel = 0;
#ifdef AF_INET
	if (newaddr->sa_family == AF_INET)
		net->tos_flowlabel = stcb->asoc.default_tos;
#endif
#ifdef AF_INET6
	if (newaddr->sa_family == AF_INET6)
		net->tos_flowlabel = stcb->asoc.default_flowlabel;
#endif
	/* Init the timer structure */
	SCTP_OS_TIMER_INIT(&net->rxt_timer.timer);
	SCTP_OS_TIMER_INIT(&net->fr_timer.timer);
	SCTP_OS_TIMER_INIT(&net->pmtu_timer.timer);

	/* Now generate a route for this guy */
	/* KAME hack: embed scopeid */
	if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
		(void)sa6_embedscope(sin6, ip6_use_defzone);
		sin6->sin6_scope_id = 0;
	}
	rtalloc_ign((struct route *)&net->ro, 0UL);
	if (newaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&net->ro._l_addr;
		(void)sa6_recoverscope(sin6);
	}
	if ((net->ro.ro_rt) &&
	    (net->ro.ro_rt->rt_ifp)) {
		net->mtu = net->ro.ro_rt->rt_ifp->if_mtu;
		if (from == SCTP_ALLOC_ASOC) {
			stcb->asoc.smallest_mtu = net->mtu;
		}
		/* start things off to match mtu of interface please. */
		net->ro.ro_rt->rt_rmx.rmx_mtu = net->ro.ro_rt->rt_ifp->if_mtu;
	} else {
		net->mtu = stcb->asoc.smallest_mtu;
	}

	if (stcb->asoc.smallest_mtu > net->mtu) {
		stcb->asoc.smallest_mtu = net->mtu;
	}
	/*
	 * We take the max of the burst limit times a MTU or the
	 * INITIAL_CWND. We then limit this to 4 MTU's of sending.
	 */
	net->cwnd = min((net->mtu * 4), max((2 * net->mtu), SCTP_INITIAL_CWND));

	/* we always get at LEAST 2 MTU's */
	if (net->cwnd < (2 * net->mtu)) {
		net->cwnd = 2 * net->mtu;
	}
	net->ssthresh = stcb->asoc.peers_rwnd;

#if defined(SCTP_CWND_MONITOR) || defined(SCTP_CWND_LOGGING)
	sctp_log_cwnd(stcb, net, 0, SCTP_CWND_INITIALIZATION);
#endif

	/*
	 * CMT: CUC algo - set find_pseudo_cumack to TRUE (1) at beginning
	 * of assoc (2005/06/27, iyengar@cis.udel.edu)
	 */
	net->find_pseudo_cumack = 1;
	net->find_rtx_pseudo_cumack = 1;
	net->src_addr_selected = 0;
	netfirst = TAILQ_FIRST(&stcb->asoc.nets);
	if (net->ro.ro_rt == NULL) {
		/* Since we have no route put it at the back */
		TAILQ_INSERT_TAIL(&stcb->asoc.nets, net, sctp_next);
	} else if (netfirst == NULL) {
		/* We are the first one in the pool. */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else if (netfirst->ro.ro_rt == NULL) {
		/*
		 * First one has NO route. Place this one ahead of the first
		 * one.
		 */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else if (net->ro.ro_rt->rt_ifp != netfirst->ro.ro_rt->rt_ifp) {
		/*
		 * This one has a different interface than the one at the
		 * top of the list. Place it ahead.
		 */
		TAILQ_INSERT_HEAD(&stcb->asoc.nets, net, sctp_next);
	} else {
		/*
		 * Ok we have the same interface as the first one. Move
		 * forward until we find either a) one with a NULL route...
		 * insert ahead of that b) one with a different ifp.. insert
		 * after that. c) end of the list.. insert at the tail.
		 */
		struct sctp_nets *netlook;

		do {
			netlook = TAILQ_NEXT(netfirst, sctp_next);
			if (netlook == NULL) {
				/* End of the list */
				TAILQ_INSERT_TAIL(&stcb->asoc.nets, net,
				    sctp_next);
				break;
			} else if (netlook->ro.ro_rt == NULL) {
				/* next one has NO route */
				TAILQ_INSERT_BEFORE(netfirst, net, sctp_next);
				break;
			} else if (netlook->ro.ro_rt->rt_ifp !=
			    net->ro.ro_rt->rt_ifp) {
				TAILQ_INSERT_AFTER(&stcb->asoc.nets, netlook,
				    net, sctp_next);
				break;
			}
			/* Shift forward */
			netfirst = netlook;
		} while (netlook != NULL);
	}

	/* got to have a primary set */
	if (stcb->asoc.primary_destination == 0) {
		stcb->asoc.primary_destination = net;
	} else if ((stcb->asoc.primary_destination->ro.ro_rt == NULL) &&
		    (net->ro.ro_rt) &&
	    ((net->dest_state & SCTP_ADDR_UNCONFIRMED) == 0)) {
		/* No route to current primary adopt new primary */
		stcb->asoc.primary_destination = net;
	}
	sctp_timer_start(SCTP_TIMER_TYPE_PATHMTURAISE, stcb->sctp_ep, stcb,
	    net);
	/* Validate primary is first */
	net = TAILQ_FIRST(&stcb->asoc.nets);
	if ((net != stcb->asoc.primary_destination) &&
	    (stcb->asoc.primary_destination)) {
		/*
		 * first one on the list is NOT the primary sctp_cmpaddr()
		 * is much more efficent if the primary is the first on the
		 * list, make it so.
		 */
		TAILQ_REMOVE(&stcb->asoc.nets,
		    stcb->asoc.primary_destination, sctp_next);
		TAILQ_INSERT_HEAD(&stcb->asoc.nets,
		    stcb->asoc.primary_destination, sctp_next);
	}
	return (0);
}


/*
 * allocate an association and add it to the endpoint. The caller must be
 * careful to add all additional addresses once they are know right away or
 * else the assoc will be may experience a blackout scenario.
 */
struct sctp_tcb *
sctp_aloc_assoc(struct sctp_inpcb *inp, struct sockaddr *firstaddr,
    int for_a_init, int *error, uint32_t override_tag)
{
	struct sctp_tcb *stcb;
	struct sctp_association *asoc;
	struct sctpasochead *head;
	uint16_t rport;
	int err;

	/*
	 * Assumption made here: Caller has done a
	 * sctp_findassociation_ep_addr(ep, addr's); to make sure the
	 * address does not exist already.
	 */
	if (sctppcbinfo.ipi_count_asoc >= SCTP_MAX_NUM_OF_ASOC) {
		/* Hit max assoc, sorry no more */
		*error = ENOBUFS;
		return (NULL);
	}
	SCTP_INP_RLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL) {
		/*
		 * If its in the TCP pool, its NOT allowed to create an
		 * association. The parent listener needs to call
		 * sctp_aloc_assoc.. or the one-2-many socket. If a peeled
		 * off, or connected one does this.. its an error.
		 */
		SCTP_INP_RUNLOCK(inp);
		*error = EINVAL;
		return (NULL);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB3) {
		printf("Allocate an association for peer:");
		if (firstaddr)
			sctp_print_address(firstaddr);
		else
			printf("None\n");
		printf("Port:%d\n",
		    ntohs(((struct sockaddr_in *)firstaddr)->sin_port));
	}
#endif				/* SCTP_DEBUG */
	if (firstaddr->sa_family == AF_INET) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)firstaddr;
		if ((sin->sin_port == 0) || (sin->sin_addr.s_addr == 0)) {
			/* Invalid address */
			SCTP_INP_RUNLOCK(inp);
			*error = EINVAL;
			return (NULL);
		}
		rport = sin->sin_port;
	} else if (firstaddr->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)firstaddr;
		if ((sin6->sin6_port == 0) ||
		    (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))) {
			/* Invalid address */
			SCTP_INP_RUNLOCK(inp);
			*error = EINVAL;
			return (NULL);
		}
		rport = sin6->sin6_port;
	} else {
		/* not supported family type */
		SCTP_INP_RUNLOCK(inp);
		*error = EINVAL;
		return (NULL);
	}
	SCTP_INP_RUNLOCK(inp);
	if (inp->sctp_flags & SCTP_PCB_FLAGS_UNBOUND) {
		/*
		 * If you have not performed a bind, then we need to do the
		 * ephemerial bind for you.
		 */
		if ((err = sctp_inpcb_bind(inp->sctp_socket,
		    (struct sockaddr *)NULL,
		    (struct thread *)NULL
		    ))) {
			/* bind error, probably perm */
			*error = err;
			return (NULL);
		}
	}
	stcb = (struct sctp_tcb *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_asoc);
	if (stcb == NULL) {
		/* out of memory? */
		*error = ENOMEM;
		return (NULL);
	}
	SCTP_INCR_ASOC_COUNT();

	bzero(stcb, sizeof(*stcb));
	asoc = &stcb->asoc;
	SCTP_TCB_LOCK_INIT(stcb);
	SCTP_TCB_SEND_LOCK_INIT(stcb);
	/* setup back pointer's */
	stcb->sctp_ep = inp;
	stcb->sctp_socket = inp->sctp_socket;
	if ((err = sctp_init_asoc(inp, asoc, for_a_init, override_tag))) {
		/* failed */
		SCTP_TCB_LOCK_DESTROY(stcb);
		SCTP_TCB_SEND_LOCK_DESTROY(stcb);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		SCTP_DECR_ASOC_COUNT();
		*error = err;
		return (NULL);
	}
	/* and the port */
	stcb->rport = rport;
	SCTP_INP_INFO_WLOCK();
	SCTP_INP_WLOCK(inp);
	if (inp->sctp_flags & (SCTP_PCB_FLAGS_SOCKET_GONE | SCTP_PCB_FLAGS_SOCKET_ALLGONE)) {
		/* inpcb freed while alloc going on */
		SCTP_TCB_LOCK_DESTROY(stcb);
		SCTP_TCB_SEND_LOCK_DESTROY(stcb);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		SCTP_INP_WUNLOCK(inp);
		SCTP_INP_INFO_WUNLOCK();
		SCTP_DECR_ASOC_COUNT();
		*error = EINVAL;
		return (NULL);
	}
	SCTP_TCB_LOCK(stcb);

	/* now that my_vtag is set, add it to the hash */
	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(stcb->asoc.my_vtag,
	    sctppcbinfo.hashasocmark)];
	/* put it in the bucket in the vtag hash of assoc's for the system */
	LIST_INSERT_HEAD(head, stcb, sctp_asocs);
	SCTP_INP_INFO_WUNLOCK();

	if ((err = sctp_add_remote_addr(stcb, firstaddr, SCTP_DO_SETSCOPE, SCTP_ALLOC_ASOC))) {
		/* failure.. memory error? */
		if (asoc->strmout)
			SCTP_FREE(asoc->strmout);
		if (asoc->mapping_array)
			SCTP_FREE(asoc->mapping_array);

		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		SCTP_DECR_ASOC_COUNT();
		SCTP_TCB_LOCK_DESTROY(stcb);
		SCTP_TCB_SEND_LOCK_DESTROY(stcb);
		*error = ENOBUFS;
		return (NULL);
	}
	/* Init all the timers */
	SCTP_OS_TIMER_INIT(&asoc->hb_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->dack_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->strreset_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->asconf_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->shut_guard_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->autoclose_timer.timer);
	SCTP_OS_TIMER_INIT(&asoc->delayed_event_timer.timer);

	LIST_INSERT_HEAD(&inp->sctp_asoc_list, stcb, sctp_tcblist);
	/* now file the port under the hash as well */
	if (inp->sctp_tcbhash != NULL) {
		head = &inp->sctp_tcbhash[SCTP_PCBHASH_ALLADDR(stcb->rport,
		    inp->sctp_hashmark)];
		LIST_INSERT_HEAD(head, stcb, sctp_tcbhash);
	}
	SCTP_INP_WUNLOCK(inp);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		printf("Association %p now allocated\n", stcb);
	}
#endif
	return (stcb);
}


void
sctp_remove_net(struct sctp_tcb *stcb, struct sctp_nets *net)
{
	struct sctp_association *asoc;

	asoc = &stcb->asoc;
	asoc->numnets--;
	TAILQ_REMOVE(&asoc->nets, net, sctp_next);
	sctp_free_remote_addr(net);
	if (net == asoc->primary_destination) {
		/* Reset primary */
		struct sctp_nets *lnet;

		lnet = TAILQ_FIRST(&asoc->nets);
		/* Try to find a confirmed primary */
		asoc->primary_destination = sctp_find_alternate_net(stcb, lnet,
		    0);
	}
	if (net == asoc->last_data_chunk_from) {
		/* Reset primary */
		asoc->last_data_chunk_from = TAILQ_FIRST(&asoc->nets);
	}
	if (net == asoc->last_control_chunk_from) {
		/* Clear net */
		asoc->last_control_chunk_from = NULL;
	}
/*	if (net == asoc->asconf_last_sent_to) {*/
	/* Reset primary */
/*		asoc->asconf_last_sent_to = TAILQ_FIRST(&asoc->nets);*/
/*	}*/
}

/*
 * remove a remote endpoint address from an association, it will fail if the
 * address does not exist.
 */
int
sctp_del_remote_addr(struct sctp_tcb *stcb, struct sockaddr *remaddr)
{
	/*
	 * Here we need to remove a remote address. This is quite simple, we
	 * first find it in the list of address for the association
	 * (tasoc->asoc.nets) and then if it is there, we do a LIST_REMOVE
	 * on that item. Note we do not allow it to be removed if there are
	 * no other addresses.
	 */
	struct sctp_association *asoc;
	struct sctp_nets *net, *net_tmp;

	asoc = &stcb->asoc;

	/* locate the address */
	for (net = TAILQ_FIRST(&asoc->nets); net != NULL; net = net_tmp) {
		net_tmp = TAILQ_NEXT(net, sctp_next);
		if (net->ro._l_addr.sa.sa_family != remaddr->sa_family) {
			continue;
		}
		if (sctp_cmpaddr((struct sockaddr *)&net->ro._l_addr,
		    remaddr)) {
			/* we found the guy */
			if (asoc->numnets < 2) {
				/* Must have at LEAST two remote addresses */
				return (-1);
			} else {
				sctp_remove_net(stcb, net);
				return (0);
			}
		}
	}
	/* not found. */
	return (-2);
}


void
sctp_add_vtag_to_timewait(struct sctp_inpcb *inp, uint32_t tag, uint32_t time)
{
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	struct timeval now;
	int set, i;

	SCTP_GETTIME_TIMEVAL(&now);
	chain = &sctppcbinfo.vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
	set = 0;
	if (!LIST_EMPTY(chain)) {
		/* Block(s) present, lets find space, and expire on the fly */
		LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
			for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
				if ((twait_block->vtag_block[i].v_tag == 0) &&
				    !set) {
					twait_block->vtag_block[i].tv_sec_at_expire =
					    now.tv_sec + time;
					twait_block->vtag_block[i].v_tag = tag;
					set = 1;
				} else if ((twait_block->vtag_block[i].v_tag) &&
					    ((long)twait_block->vtag_block[i].tv_sec_at_expire >
				    now.tv_sec)) {
					/* Audit expires this guy */
					twait_block->vtag_block[i].tv_sec_at_expire = 0;
					twait_block->vtag_block[i].v_tag = 0;
					if (set == 0) {
						/* Reuse it for my new tag */
						twait_block->vtag_block[0].tv_sec_at_expire = now.tv_sec + SCTP_TIME_WAIT;
						twait_block->vtag_block[0].v_tag = tag;
						set = 1;
					}
				}
			}
			if (set) {
				/*
				 * We only do up to the block where we can
				 * place our tag for audits
				 */
				break;
			}
		}
	}
	/* Need to add a new block to chain */
	if (!set) {
		SCTP_MALLOC(twait_block, struct sctp_tagblock *,
		    sizeof(struct sctp_tagblock), "TimeWait");
		if (twait_block == NULL) {
			return;
		}
		memset(twait_block, 0, sizeof(struct sctp_tagblock));
		LIST_INSERT_HEAD(chain, twait_block, sctp_nxt_tagblock);
		twait_block->vtag_block[0].tv_sec_at_expire = now.tv_sec +
		    SCTP_TIME_WAIT;
		twait_block->vtag_block[0].v_tag = tag;
	}
}


static void
sctp_iterator_asoc_being_freed(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	struct sctp_iterator *it;

	/*
	 * Unlock the tcb lock we do this so we avoid a dead lock scenario
	 * where the iterator is waiting on the TCB lock and the TCB lock is
	 * waiting on the iterator lock.
	 */
	it = stcb->asoc.stcb_starting_point_for_iterator;
	if (it == NULL) {
		return;
	}
	if (it->inp != stcb->sctp_ep) {
		/* hmm, focused on the wrong one? */
		return;
	}
	if (it->stcb != stcb) {
		return;
	}
	it->stcb = LIST_NEXT(stcb, sctp_tcblist);
	if (it->stcb == NULL) {
		/* done with all asoc's in this assoc */
		if (it->iterator_flags & SCTP_ITERATOR_DO_SINGLE_INP) {
			it->inp = NULL;
		} else {
			it->inp = LIST_NEXT(inp, sctp_list);
		}
	}
}

/*
 * Free the association after un-hashing the remote port.
 */
int
sctp_free_assoc(struct sctp_inpcb *inp, struct sctp_tcb *stcb, int from_inpcbfree, int from_location)
{
	int i;
	struct sctp_association *asoc;
	struct sctp_nets *net, *prev;
	struct sctp_laddr *laddr;
	struct sctp_tmit_chunk *chk;
	struct sctp_asconf_addr *aparam;
	struct sctp_stream_reset_list *liste;
	struct sctp_queued_to_read *sq;
	struct sctp_stream_queue_pending *sp;
	sctp_sharedkey_t *shared_key;
	struct socket *so;
	int ccnt = 0;
	int s, cnt = 0;

	/* first, lets purge the entry from the hash table. */
	s = splnet();

#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, stcb, 6);
#endif
	if (stcb->asoc.state == 0) {
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, NULL, 7);
#endif
		splx(s);
		/* there is no asoc, really TSNH :-0 */
		return (1);
	}
	/* TEMP CODE */
	if (stcb->freed_from_where == 0) {
		/* Only record the first place free happened from */
		stcb->freed_from_where = from_location;
	}
	/* TEMP CODE */

	asoc = &stcb->asoc;
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))
		/* nothing around */
		so = NULL;
	else
		so = inp->sctp_socket;

	/*
	 * We used timer based freeing if a reader or writer is in the way.
	 * So we first check if we are actually being called from a timer,
	 * if so we abort early if a reader or writer is still in the way.
	 */
	if ((stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) &&
	    (from_inpcbfree == SCTP_NORMAL_PROC)) {
		/*
		 * is it the timer driving us? if so are the reader/writers
		 * gone?
		 */
		if (stcb->asoc.refcnt) {
			/* nope, reader or writer in the way */
			sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, stcb, NULL);
			/* no asoc destroyed */
			SCTP_TCB_UNLOCK(stcb);
			splx(s);
#ifdef SCTP_LOG_CLOSING
			sctp_log_closing(inp, stcb, 8);
#endif
			return (0);
		}
	}
	/* now clean up any other timers */
	SCTP_OS_TIMER_STOP(&asoc->hb_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->dack_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->strreset_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->asconf_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->autoclose_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->shut_guard_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->delayed_event_timer.timer);

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		SCTP_OS_TIMER_STOP(&net->fr_timer.timer);
		SCTP_OS_TIMER_STOP(&net->rxt_timer.timer);
		SCTP_OS_TIMER_STOP(&net->pmtu_timer.timer);
	}
	/* Now the read queue needs to be cleaned up (only once) */
	cnt = 0;
	if ((stcb->asoc.state & SCTP_STATE_ABOUT_TO_BE_FREED) == 0) {
		SCTP_INP_READ_LOCK(inp);
		TAILQ_FOREACH(sq, &inp->read_queue, next) {
			if (sq->stcb == stcb) {
				sq->do_not_ref_stcb = 1;
				sq->sinfo_cumtsn = stcb->asoc.cumulative_tsn;
				/*
				 * If there is no end, there never will be
				 * now.
				 */
				if (sq->end_added == 0) {
					/* Held for PD-API clear that. */
					sq->pdapi_aborted = 1;
					sq->held_length = 0;
					if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_PDAPIEVNT)) {
						/*
						 * Need to add a PD-API
						 * aborted indication.
						 * Setting the control_pdapi
						 * assures that it will be
						 * added right after this
						 * msg.
						 */
						stcb->asoc.control_pdapi = sq;
						sctp_notify_partial_delivery_indication(stcb,
						    SCTP_PARTIAL_DELIVERY_ABORTED, 1);
						stcb->asoc.control_pdapi = NULL;
					}
				}
				/* Add an end to wake them */
				sq->end_added = 1;
				cnt++;
			}
		}
		SCTP_INP_READ_UNLOCK(inp);
		if (stcb->block_entry) {
			cnt++;
			stcb->block_entry->error = ECONNRESET;
			stcb->block_entry = NULL;
		}
	}
	stcb->asoc.state |= SCTP_STATE_ABOUT_TO_BE_FREED;
	if ((from_inpcbfree != SCTP_PCBFREE_FORCE) && (stcb->asoc.refcnt)) {
		/*
		 * reader or writer in the way, we have hopefully given him
		 * something to chew on above.
		 */
		sctp_timer_start(SCTP_TIMER_TYPE_ASOCKILL, inp, stcb, NULL);
		SCTP_TCB_UNLOCK(stcb);
		if (so) {
			SCTP_INP_RLOCK(inp);
			if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
			    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))
				/* nothing around */
				so = NULL;
			if (so) {
				/* Wake any reader/writers */
				sctp_sorwakeup(inp, so);
				sctp_sowwakeup(inp, so);
			}
			SCTP_INP_RUNLOCK(inp);

		}
		splx(s);
#ifdef SCTP_LOG_CLOSING
		sctp_log_closing(inp, stcb, 9);
#endif
		/* no asoc destroyed */
		return (0);
	}
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, stcb, 10);
#endif
	/*
	 * When I reach here, no others want to kill the assoc yet.. and I
	 * own the lock. Now its possible an abort comes in when I do the
	 * lock exchange below to grab all the locks to do the final take
	 * out. to prevent this we increment the count, which will start a
	 * timer and blow out above thus assuring us that we hold exclusive
	 * killing of the asoc. Note that after getting back the TCB lock we
	 * will go ahead and increment the counter back up and stop any
	 * timer a passing stranger may have started :-S
	 */
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		atomic_add_int(&stcb->asoc.refcnt, 1);

		SCTP_TCB_UNLOCK(stcb);

		SCTP_ITERATOR_LOCK();
		SCTP_INP_INFO_WLOCK();
		SCTP_INP_WLOCK(inp);
		SCTP_TCB_LOCK(stcb);
	}
	/* Double check the GONE flag */
	if ((inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_ALLGONE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE))
		/* nothing around */
		so = NULL;

	if ((inp->sctp_flags & SCTP_PCB_FLAGS_TCPTYPE) ||
	    (inp->sctp_flags & SCTP_PCB_FLAGS_IN_TCPPOOL)) {
		/*
		 * For TCP type we need special handling when we are
		 * connected. We also include the peel'ed off ones to.
		 */
		if (inp->sctp_flags & SCTP_PCB_FLAGS_CONNECTED) {
			inp->sctp_flags &= ~SCTP_PCB_FLAGS_CONNECTED;
			inp->sctp_flags |= SCTP_PCB_FLAGS_WAS_CONNECTED;
			if (so) {
				SOCK_LOCK(so);
				if (so->so_rcv.sb_cc == 0) {
					so->so_state &= ~(SS_ISCONNECTING |
					    SS_ISDISCONNECTING |
					    SS_ISCONFIRMING |
					    SS_ISCONNECTED);
				}
				SOCK_UNLOCK(so);
				sctp_sowwakeup(inp, so);
				sctp_sorwakeup(inp, so);
				wakeup(&so->so_timeo);
			}
		}
	}
	/*
	 * Make it invalid too, that way if its about to run it will abort
	 * and return.
	 */
	sctp_iterator_asoc_being_freed(inp, stcb);
	/* re-increment the lock */
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		atomic_add_int(&stcb->asoc.refcnt, -1);
	}
	asoc->state = 0;
	if (inp->sctp_tcbhash) {
		LIST_REMOVE(stcb, sctp_tcbhash);
	}
	if (stcb->asoc.in_restart_hash) {
		LIST_REMOVE(stcb, sctp_tcbrestarhash);
	}
	/* Now lets remove it from the list of ALL associations in the EP */
	LIST_REMOVE(stcb, sctp_tcblist);
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		SCTP_INP_INCR_REF(inp);
		SCTP_INP_WUNLOCK(inp);
		SCTP_ITERATOR_UNLOCK();
	}
	/* pull from vtag hash */
	LIST_REMOVE(stcb, sctp_asocs);
	sctp_add_vtag_to_timewait(inp, asoc->my_vtag, SCTP_TIME_WAIT);


	/*
	 * Now restop the timers to be sure - this is paranoia at is finest!
	 */
	SCTP_OS_TIMER_STOP(&asoc->strreset_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->hb_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->dack_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->strreset_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->asconf_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->shut_guard_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->autoclose_timer.timer);
	SCTP_OS_TIMER_STOP(&asoc->delayed_event_timer.timer);

	TAILQ_FOREACH(net, &asoc->nets, sctp_next) {
		SCTP_OS_TIMER_STOP(&net->fr_timer.timer);
		SCTP_OS_TIMER_STOP(&net->rxt_timer.timer);
		SCTP_OS_TIMER_STOP(&net->pmtu_timer.timer);
	}

	asoc->strreset_timer.type = SCTP_TIMER_TYPE_NONE;
	prev = NULL;
	/*
	 * The chunk lists and such SHOULD be empty but we check them just
	 * in case.
	 */
	/* anything on the wheel needs to be removed */
	for (i = 0; i < asoc->streamoutcnt; i++) {
		struct sctp_stream_out *outs;

		outs = &asoc->strmout[i];
		/* now clean up any chunks here */
		sp = TAILQ_FIRST(&outs->outqueue);
		while (sp) {
			TAILQ_REMOVE(&outs->outqueue, sp, next);
			if (sp->data) {
				sctp_m_freem(sp->data);
				sp->data = NULL;
				sp->tail_mbuf = NULL;
			}
			sctp_free_remote_addr(sp->net);
			sctp_free_spbufspace(stcb, asoc, sp);
			/* Free the zone stuff  */
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_strmoq, sp);
			SCTP_DECR_STRMOQ_COUNT();
			sp = TAILQ_FIRST(&outs->outqueue);
		}
	}

	while ((sp = TAILQ_FIRST(&asoc->free_strmoq)) != NULL) {
		TAILQ_REMOVE(&asoc->free_strmoq, sp, next);
		if (sp->data) {
			sctp_m_freem(sp->data);
			sp->data = NULL;
			sp->tail_mbuf = NULL;
		}
		/* Free the zone stuff  */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_strmoq, sp);
		SCTP_DECR_STRMOQ_COUNT();
		atomic_add_int(&sctppcbinfo.ipi_free_strmoq, -1);
	}

	while ((liste = TAILQ_FIRST(&asoc->resetHead)) != NULL) {
		TAILQ_REMOVE(&asoc->resetHead, liste, next_resp);
		SCTP_FREE(liste);
	}

	sq = TAILQ_FIRST(&asoc->pending_reply_queue);
	while (sq) {
		TAILQ_REMOVE(&asoc->pending_reply_queue, sq, next);
		if (sq->data) {
			sctp_m_freem(sq->data);
			sq->data = NULL;
		}
		sctp_free_remote_addr(sq->whoFrom);
		sq->whoFrom = NULL;
		sq->stcb = NULL;
		/* Free the ctl entry */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_readq, sq);
		SCTP_DECR_READQ_COUNT();
		sq = TAILQ_FIRST(&asoc->pending_reply_queue);
	}

	chk = TAILQ_FIRST(&asoc->free_chunks);
	while (chk) {
		TAILQ_REMOVE(&asoc->free_chunks, chk, sctp_next);
		if (chk->data) {
			sctp_m_freem(chk->data);
			chk->data = NULL;
		}
		ccnt++;
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
		SCTP_DECR_CHK_COUNT();
		atomic_subtract_int(&sctppcbinfo.ipi_free_chunks, 1);
		asoc->free_chunk_cnt--;
		chk = TAILQ_FIRST(&asoc->free_chunks);
	}
	/* pending send queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->send_queue)) {
		chk = TAILQ_FIRST(&asoc->send_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->send_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			ccnt++;
			sctp_free_remote_addr(chk->whoTo);
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->send_queue);
		}
	}
/*
  if(ccnt) {
  printf("Freed %d from send_queue\n", ccnt);
  ccnt = 0;
  }
*/
	/* sent queue SHOULD be empty */
	if (!TAILQ_EMPTY(&asoc->sent_queue)) {
		chk = TAILQ_FIRST(&asoc->sent_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->sent_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			ccnt++;
			sctp_free_remote_addr(chk->whoTo);
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->sent_queue);
		}
	}
/*
  if(ccnt) {
  printf("Freed %d from sent_queue\n", ccnt);
  ccnt = 0;
  }
*/
	/* control queue MAY not be empty */
	if (!TAILQ_EMPTY(&asoc->control_send_queue)) {
		chk = TAILQ_FIRST(&asoc->control_send_queue);
		while (chk) {
			TAILQ_REMOVE(&asoc->control_send_queue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			ccnt++;
			sctp_free_remote_addr(chk->whoTo);
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->control_send_queue);
		}
	}
/*
  if(ccnt) {
  printf("Freed %d from ctrl_queue\n", ccnt);
  ccnt = 0;
  }
*/
	if (!TAILQ_EMPTY(&asoc->reasmqueue)) {
		chk = TAILQ_FIRST(&asoc->reasmqueue);
		while (chk) {
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			sctp_free_remote_addr(chk->whoTo);
			ccnt++;
			SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_chunk, chk);
			SCTP_DECR_CHK_COUNT();
			chk = TAILQ_FIRST(&asoc->reasmqueue);
		}
	}
/*
  if(ccnt) {
  printf("Freed %d from reasm_queue\n", ccnt);
  ccnt = 0;
  }
*/
	if (asoc->mapping_array) {
		SCTP_FREE(asoc->mapping_array);
		asoc->mapping_array = NULL;
	}
	/* the stream outs */
	if (asoc->strmout) {
		SCTP_FREE(asoc->strmout);
		asoc->strmout = NULL;
	}
	asoc->streamoutcnt = 0;
	if (asoc->strmin) {
		struct sctp_queued_to_read *ctl;
		int i;

		for (i = 0; i < asoc->streamincnt; i++) {
			if (!TAILQ_EMPTY(&asoc->strmin[i].inqueue)) {
				/* We have somethings on the streamin queue */
				ctl = TAILQ_FIRST(&asoc->strmin[i].inqueue);
				while (ctl) {
					TAILQ_REMOVE(&asoc->strmin[i].inqueue,
					    ctl, next);
					sctp_free_remote_addr(ctl->whoFrom);
					if (ctl->data) {
						sctp_m_freem(ctl->data);
						ctl->data = NULL;
					}
					/*
					 * We don't free the address here
					 * since all the net's were freed
					 * above.
					 */
					SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_readq, ctl);
					SCTP_DECR_READQ_COUNT();
					ctl = TAILQ_FIRST(&asoc->strmin[i].inqueue);
				}
			}
		}
		SCTP_FREE(asoc->strmin);
		asoc->strmin = NULL;
	}
	asoc->streamincnt = 0;
	while (!TAILQ_EMPTY(&asoc->nets)) {
		net = TAILQ_FIRST(&asoc->nets);
		/* pull from list */
		if ((sctppcbinfo.ipi_count_raddr == 0) || (prev == net)) {
#ifdef INVARIANTS
			panic("no net's left alloc'ed, or list points to itself");
#endif
			break;
		}
		prev = net;
		TAILQ_REMOVE(&asoc->nets, net, sctp_next);
		sctp_free_remote_addr(net);
	}

	/* local addresses, if any */
	while (!LIST_EMPTY(&asoc->sctp_local_addr_list)) {
		laddr = LIST_FIRST(&asoc->sctp_local_addr_list);
		LIST_REMOVE(laddr, sctp_nxt_addr);
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
		SCTP_DECR_LADDR_COUNT();
	}
	/* pending asconf (address) parameters */
	while (!TAILQ_EMPTY(&asoc->asconf_queue)) {
		aparam = TAILQ_FIRST(&asoc->asconf_queue);
		TAILQ_REMOVE(&asoc->asconf_queue, aparam, next);
		SCTP_FREE(aparam);
	}
	if (asoc->last_asconf_ack_sent != NULL) {
		sctp_m_freem(asoc->last_asconf_ack_sent);
		asoc->last_asconf_ack_sent = NULL;
	}
	/* clean up auth stuff */
	if (asoc->local_hmacs)
		sctp_free_hmaclist(asoc->local_hmacs);
	if (asoc->peer_hmacs)
		sctp_free_hmaclist(asoc->peer_hmacs);

	if (asoc->local_auth_chunks)
		sctp_free_chunklist(asoc->local_auth_chunks);
	if (asoc->peer_auth_chunks)
		sctp_free_chunklist(asoc->peer_auth_chunks);

	sctp_free_authinfo(&asoc->authinfo);

	shared_key = LIST_FIRST(&asoc->shared_keys);
	while (shared_key) {
		LIST_REMOVE(shared_key, next);
		sctp_free_sharedkey(shared_key);
		shared_key = LIST_FIRST(&asoc->shared_keys);
	}

	/* Insert new items here :> */

	/* Get rid of LOCK */
	SCTP_TCB_LOCK_DESTROY(stcb);
	SCTP_TCB_SEND_LOCK_DESTROY(stcb);
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		SCTP_INP_INFO_WUNLOCK();
		SCTP_INP_RLOCK(inp);
	}
#ifdef SCTP_TRACK_FREED_ASOCS
	if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
		/* now clean up the tasoc itself */
		SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
		SCTP_DECR_ASOC_COUNT();
	} else {
		LIST_INSERT_HEAD(&inp->sctp_asoc_free_list, stcb, sctp_tcblist);
	}
#else
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_asoc, stcb);
	SCTP_DECR_ASOC_COUNT();
#endif
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		if (inp->sctp_flags & SCTP_PCB_FLAGS_SOCKET_GONE) {
			/*
			 * If its NOT the inp_free calling us AND sctp_close
			 * as been called, we call back...
			 */
			SCTP_INP_RUNLOCK(inp);
			/*
			 * This will start the kill timer (if we are the
			 * lastone) since we hold an increment yet. But this
			 * is the only safe way to do this since otherwise
			 * if the socket closes at the same time we are here
			 * we might collide in the cleanup.
			 */
			sctp_inpcb_free(inp, 0, 0);
			SCTP_INP_DECR_REF(inp);
			goto out_of;
		} else {
			/* The socket is still open. */
			SCTP_INP_DECR_REF(inp);
		}
	}
	if (from_inpcbfree == SCTP_NORMAL_PROC) {
		SCTP_INP_RUNLOCK(inp);
	}
out_of:
	splx(s);
	/* destroyed the asoc */
#ifdef SCTP_LOG_CLOSING
	sctp_log_closing(inp, NULL, 11);
#endif
	return (1);
}



/*
 * determine if a destination is "reachable" based upon the addresses bound
 * to the current endpoint (e.g. only v4 or v6 currently bound)
 */
/*
 * FIX: if we allow assoc-level bindx(), then this needs to be fixed to use
 * assoc level v4/v6 flags, as the assoc *may* not have the same address
 * types bound as its endpoint
 */
int
sctp_destination_is_reachable(struct sctp_tcb *stcb, struct sockaddr *destaddr)
{
	struct sctp_inpcb *inp;
	int answer;

	/*
	 * No locks here, the TCB, in all cases is already locked and an
	 * assoc is up. There is either a INP lock by the caller applied (in
	 * asconf case when deleting an address) or NOT in the HB case,
	 * however if HB then the INP increment is up and the INP will not
	 * be removed (on top of the fact that we have a TCB lock). So we
	 * only want to read the sctp_flags, which is either bound-all or
	 * not.. no protection needed since once an assoc is up you can't be
	 * changing your binding.
	 */
	inp = stcb->sctp_ep;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* if bound all, destination is not restricted */
		/*
		 * RRS: Question during lock work: Is this correct? If you
		 * are bound-all you still might need to obey the V4--V6
		 * flags??? IMO this bound-all stuff needs to be removed!
		 */
		return (1);
	}
	/* NOTE: all "scope" checks are done when local addresses are added */
	if (destaddr->sa_family == AF_INET6) {
		answer = inp->ip_inp.inp.inp_vflag & INP_IPV6;
	} else if (destaddr->sa_family == AF_INET) {
		answer = inp->ip_inp.inp.inp_vflag & INP_IPV4;
	} else {
		/* invalid family, so it's unreachable */
		answer = 0;
	}
	return (answer);
}

/*
 * update the inp_vflags on an endpoint
 */
static void
sctp_update_ep_vflag(struct sctp_inpcb *inp)
{
	struct sctp_laddr *laddr;

	/* first clear the flag */
	inp->ip_inp.inp.inp_vflag = 0;
	/* set the flag based on addresses on the ep list */
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_PCB1) {
				printf("An ounce of prevention is worth a pound of cure\n");
			}
#endif				/* SCTP_DEBUG */
			continue;
		}
		if (laddr->ifa->ifa_addr) {
			continue;
		}
		if (laddr->ifa->ifa_addr->sa_family == AF_INET6) {
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
		} else if (laddr->ifa->ifa_addr->sa_family == AF_INET) {
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
		}
	}
}

/*
 * Add the address to the endpoint local address list There is nothing to be
 * done if we are bound to all addresses
 */
int
sctp_add_local_addr_ep(struct sctp_inpcb *inp, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;
	int fnd, error;

	fnd = 0;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (0);
	}
	if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct in6_ifaddr *ifa6;

		ifa6 = (struct in6_ifaddr *)ifa;
		if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
		    IN6_IFF_DEPRECATED | IN6_IFF_ANYCAST | IN6_IFF_NOTREADY))
			/* Can't bind a non-existent addr. */
			return (-1);
	}
	/* first, is it already present? */
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}

	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) && (fnd == 0)) {
		/* Not bound to all */
		error = sctp_insert_laddr(&inp->sctp_addr_list, ifa);
		if (error != 0)
			return (error);
		inp->laddr_count++;
		/* update inp_vflag flags */
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			inp->ip_inp.inp.inp_vflag |= INP_IPV6;
		} else if (ifa->ifa_addr->sa_family == AF_INET) {
			inp->ip_inp.inp.inp_vflag |= INP_IPV4;
		}
	}
	return (0);
}


/*
 * select a new (hopefully reachable) destination net (should only be used
 * when we deleted an ep addr that is the only usable source address to reach
 * the destination net)
 */
static void
sctp_select_primary_destination(struct sctp_tcb *stcb)
{
	struct sctp_nets *net;

	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		/* for now, we'll just pick the first reachable one we find */
		if (net->dest_state & SCTP_ADDR_UNCONFIRMED)
			continue;
		if (sctp_destination_is_reachable(stcb,
		    (struct sockaddr *)&net->ro._l_addr)) {
			/* found a reachable destination */
			stcb->asoc.primary_destination = net;
		}
	}
	/* I can't there from here! ...we're gonna die shortly... */
}


/*
 * Delete the address from the endpoint local address list There is nothing
 * to be done if we are bound to all addresses
 */
int
sctp_del_local_addr_ep(struct sctp_inpcb *inp, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;
	int fnd;

	fnd = 0;
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (EINVAL);
	}
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			fnd = 1;
			break;
		}
	}
	if (fnd && (inp->laddr_count < 2)) {
		/* can't delete unless there are at LEAST 2 addresses */
		return (-1);
	}
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) && (fnd)) {
		/*
		 * clean up any use of this address go through our
		 * associations and clear any last_used_address that match
		 * this one for each assoc, see if a new primary_destination
		 * is needed
		 */
		struct sctp_tcb *stcb;

		/* clean up "next_addr_touse" */
		if (inp->next_addr_touse == laddr)
			/* delete this address */
			inp->next_addr_touse = NULL;

		/* clean up "last_used_address" */
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			if (stcb->asoc.last_used_address == laddr)
				/* delete this address */
				stcb->asoc.last_used_address = NULL;
		}		/* for each tcb */

		/* remove it from the ep list */
		sctp_remove_laddr(laddr);
		inp->laddr_count--;
		/* update inp_vflag flags */
		sctp_update_ep_vflag(inp);
		/* select a new primary destination if needed */
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			/*
			 * presume caller (sctp_asconf.c) already owns INP
			 * lock
			 */
			SCTP_TCB_LOCK(stcb);
			if (sctp_destination_is_reachable(stcb,
			    (struct sockaddr *)&stcb->asoc.primary_destination->ro._l_addr) == 0) {
				sctp_select_primary_destination(stcb);
			}
			SCTP_TCB_UNLOCK(stcb);
		}		/* for each tcb */
	}
	return (0);
}

/*
 * Add the addr to the TCB local address list For the BOUNDALL or dynamic
 * case, this is a "pending" address list (eg. addresses waiting for an
 * ASCONF-ACK response) For the subset binding, static case, this is a
 * "valid" address list
 */
int
sctp_add_local_addr_assoc(struct sctp_tcb *stcb, struct ifaddr *ifa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	int error;

	/*
	 * Assumes TCP is locked.. and possiblye the INP. May need to
	 * confirm/fix that if we need it and is not the case.
	 */
	inp = stcb->sctp_ep;
	if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct in6_ifaddr *ifa6;

		ifa6 = (struct in6_ifaddr *)ifa;
		if (ifa6->ia6_flags & (IN6_IFF_DETACHED |
		/* IN6_IFF_DEPRECATED | */
		    IN6_IFF_ANYCAST |
		    IN6_IFF_NOTREADY))
			/* Can't bind a non-existent addr. */
			return (-1);
	}
	/* does the address already exist? */
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == ifa) {
			return (-1);
		}
	}

	/* add to the list */
	error = sctp_insert_laddr(&stcb->asoc.sctp_local_addr_list, ifa);
	if (error != 0)
		return (error);
	return (0);
}

/*
 * insert an laddr entry with the given ifa for the desired list
 */
int
sctp_insert_laddr(struct sctpladdr *list, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;
	int s;

	s = splnet();

	laddr = (struct sctp_laddr *)SCTP_ZONE_GET(sctppcbinfo.ipi_zone_laddr);
	if (laddr == NULL) {
		/* out of memory? */
		splx(s);
		return (EINVAL);
	}
	SCTP_INCR_LADDR_COUNT();
	bzero(laddr, sizeof(*laddr));
	laddr->ifa = ifa;
	/* insert it */
	LIST_INSERT_HEAD(list, laddr, sctp_nxt_addr);

	splx(s);
	return (0);
}

/*
 * Remove an laddr entry from the local address list (on an assoc)
 */
void
sctp_remove_laddr(struct sctp_laddr *laddr)
{
	int s;

	s = splnet();
	/* remove from the list */
	LIST_REMOVE(laddr, sctp_nxt_addr);
	SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_laddr, laddr);
	SCTP_DECR_LADDR_COUNT();
	splx(s);
}

/*
 * Remove an address from the TCB local address list
 */
int
sctp_del_local_addr_assoc(struct sctp_tcb *stcb, struct ifaddr *ifa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;

	/*
	 * This is called by asconf work. It is assumed that a) The TCB is
	 * locked and b) The INP is locked. This is true in as much as I can
	 * trace through the entry asconf code where I did these locks.
	 * Again, the ASCONF code is a bit different in that it does lock
	 * the INP during its work often times. This must be since we don't
	 * want other proc's looking up things while what they are looking
	 * up is changing :-D
	 */

	inp = stcb->sctp_ep;
	/* if subset bound and don't allow ASCONF's, can't delete last */
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) &&
	    (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_DO_ASCONF) == 0)) {
		if (stcb->asoc.numnets < 2) {
			/* can't delete last address */
			return (-1);
		}
	}
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		/* remove the address if it exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa == ifa) {
			sctp_remove_laddr(laddr);
			return (0);
		}
	}

	/* address not found! */
	return (-1);
}

/*
 * Remove an address from the TCB local address list lookup using a sockaddr
 * addr
 */
int
sctp_del_local_addr_assoc_sa(struct sctp_tcb *stcb, struct sockaddr *sa)
{
	struct sctp_inpcb *inp;
	struct sctp_laddr *laddr;
	struct sockaddr *l_sa;

	/*
	 * This function I find does not seem to have a caller. As such we
	 * NEED TO DELETE this code. If we do find a caller, the caller MUST
	 * have locked the TCB at the least and probably the INP as well.
	 */
	inp = stcb->sctp_ep;
	/* if subset bound and don't allow ASCONF's, can't delete last */
	if (((inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) &&
	    (sctp_is_feature_off(inp, SCTP_PCB_FLAGS_DO_ASCONF) == 0)) {
		if (stcb->asoc.numnets < 2) {
			/* can't delete last address */
			return (-1);
		}
	}
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		/* make sure the address exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa->ifa_addr == NULL)
			continue;

		l_sa = laddr->ifa->ifa_addr;
		if (l_sa->sa_family == AF_INET6) {
			/* IPv6 address */
			struct sockaddr_in6 *sin1, *sin2;

			sin1 = (struct sockaddr_in6 *)l_sa;
			sin2 = (struct sockaddr_in6 *)sa;
			if (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
			    sizeof(struct in6_addr)) == 0) {
				/* matched */
				sctp_remove_laddr(laddr);
				return (0);
			}
		} else if (l_sa->sa_family == AF_INET) {
			/* IPv4 address */
			struct sockaddr_in *sin1, *sin2;

			sin1 = (struct sockaddr_in *)l_sa;
			sin2 = (struct sockaddr_in *)sa;
			if (sin1->sin_addr.s_addr == sin2->sin_addr.s_addr) {
				/* matched */
				sctp_remove_laddr(laddr);
				return (0);
			}
		} else {
			/* invalid family */
			return (-1);
		}
	}			/* end foreach */
	/* address not found! */
	return (-1);
}

static char sctp_pcb_initialized = 0;

/*
 * Temporarily remove for __APPLE__ until we use the Tiger equivalents
 */
/* sysctl */
static int sctp_max_number_of_assoc = SCTP_MAX_NUM_OF_ASOC;
static int sctp_scale_up_for_address = SCTP_SCALE_FOR_ADDR;

void
sctp_pcb_init()
{
	/*
	 * SCTP initialization for the PCB structures should be called by
	 * the sctp_init() funciton.
	 */
	int i;

	if (sctp_pcb_initialized != 0) {
		/* error I was called twice */
		return;
	}
	sctp_pcb_initialized = 1;

	bzero(&sctpstat, sizeof(struct sctpstat));

	/* init the empty list of (All) Endpoints */
	LIST_INIT(&sctppcbinfo.listhead);

	/* init the iterator head */
	LIST_INIT(&sctppcbinfo.iteratorhead);

	/* init the hash table of endpoints */
	TUNABLE_INT_FETCH("net.inet.sctp.tcbhashsize", &sctp_hashtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.pcbhashsize", &sctp_pcbtblsize);
	TUNABLE_INT_FETCH("net.inet.sctp.chunkscale", &sctp_chunkscale);
	sctppcbinfo.sctp_asochash = SCTP_HASH_INIT((sctp_hashtblsize * 31),
	    &sctppcbinfo.hashasocmark);
	sctppcbinfo.sctp_ephash = SCTP_HASH_INIT(sctp_hashtblsize,
	    &sctppcbinfo.hashmark);
	sctppcbinfo.sctp_tcpephash = SCTP_HASH_INIT(sctp_hashtblsize,
	    &sctppcbinfo.hashtcpmark);
	sctppcbinfo.hashtblsize = sctp_hashtblsize;

	/* init the small hash table we use to track restarted asoc's */
	sctppcbinfo.sctp_restarthash = SCTP_HASH_INIT(SCTP_STACK_VTAG_HASH_SIZE,
	    &sctppcbinfo.hashrestartmark);

	/* init the zones */
	/*
	 * FIX ME: Should check for NULL returns, but if it does fail we are
	 * doomed to panic anyways... add later maybe.
	 */
	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_ep, "sctp_ep",
	    sizeof(struct sctp_inpcb), maxsockets);

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_asoc, "sctp_asoc",
	    sizeof(struct sctp_tcb), sctp_max_number_of_assoc);

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_laddr, "sctp_laddr",
	    sizeof(struct sctp_laddr),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_net, "sctp_raddr",
	    sizeof(struct sctp_nets),
	    (sctp_max_number_of_assoc * sctp_scale_up_for_address));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_chunk, "sctp_chunk",
	    sizeof(struct sctp_tmit_chunk),
	    (sctp_max_number_of_assoc * sctp_chunkscale));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_readq, "sctp_readq",
	    sizeof(struct sctp_queued_to_read),
	    (sctp_max_number_of_assoc * sctp_chunkscale));

	SCTP_ZONE_INIT(sctppcbinfo.ipi_zone_strmoq, "sctp_stream_msg_out",
	    sizeof(struct sctp_stream_queue_pending),
	    (sctp_max_number_of_assoc * sctp_chunkscale));

	/* Master Lock INIT for info structure */
	SCTP_INP_INFO_LOCK_INIT();
	SCTP_STATLOG_INIT_LOCK();
	SCTP_ITERATOR_LOCK_INIT();

	SCTP_IPI_COUNT_INIT();
	SCTP_IPI_ADDR_INIT();
	LIST_INIT(&sctppcbinfo.addr_wq);

	/* not sure if we need all the counts */
	sctppcbinfo.ipi_count_ep = 0;
	/* assoc/tcb zone info */
	sctppcbinfo.ipi_count_asoc = 0;
	/* local addrlist zone info */
	sctppcbinfo.ipi_count_laddr = 0;
	/* remote addrlist zone info */
	sctppcbinfo.ipi_count_raddr = 0;
	/* chunk info */
	sctppcbinfo.ipi_count_chunk = 0;

	/* socket queue zone info */
	sctppcbinfo.ipi_count_readq = 0;

	/* stream out queue cont */
	sctppcbinfo.ipi_count_strmoq = 0;

	sctppcbinfo.ipi_free_strmoq = 0;
	sctppcbinfo.ipi_free_chunks = 0;

	SCTP_OS_TIMER_INIT(&sctppcbinfo.addr_wq_timer.timer);

	/* Init the TIMEWAIT list */
	for (i = 0; i < SCTP_STACK_VTAG_HASH_SIZE; i++) {
		LIST_INIT(&sctppcbinfo.vtag_timewait[i]);
	}

}


int
sctp_load_addresses_from_init(struct sctp_tcb *stcb, struct mbuf *m,
    int iphlen, int offset, int limit, struct sctphdr *sh,
    struct sockaddr *altsa)
{
	/*
	 * grub through the INIT pulling addresses and loading them to the
	 * nets structure in the asoc. The from address in the mbuf should
	 * also be loaded (if it is not already). This routine can be called
	 * with either INIT or INIT-ACK's as long as the m points to the IP
	 * packet and the offset points to the beginning of the parameters.
	 */
	struct sctp_inpcb *inp, *l_inp;
	struct sctp_nets *net, *net_tmp;
	struct ip *iph;
	struct sctp_paramhdr *phdr, parm_buf;
	struct sctp_tcb *stcb_tmp;
	uint16_t ptype, plen;
	struct sockaddr *sa;
	struct sockaddr_storage dest_store;
	struct sockaddr *local_sa = (struct sockaddr *)&dest_store;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	uint8_t store[384];
	struct sctp_auth_random *random = NULL;
	uint16_t random_len = 0;
	struct sctp_auth_hmac_algo *hmacs = NULL;
	uint16_t hmacs_len = 0;
	struct sctp_auth_chunk_list *chunks = NULL;
	uint16_t num_chunks = 0;
	sctp_key_t *new_key;
	uint32_t keylen;
	int got_random = 0, got_hmacs = 0, got_chklist = 0;

	/* First get the destination address setup too. */
	memset(&sin, 0, sizeof(sin));
	memset(&sin6, 0, sizeof(sin6));

	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = stcb->rport;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_port = stcb->rport;
	if (altsa == NULL) {
		iph = mtod(m, struct ip *);
		if (iph->ip_v == IPVERSION) {
			/* its IPv4 */
			struct sockaddr_in *sin_2;

			sin_2 = (struct sockaddr_in *)(local_sa);
			memset(sin_2, 0, sizeof(sin));
			sin_2->sin_family = AF_INET;
			sin_2->sin_len = sizeof(sin);
			sin_2->sin_port = sh->dest_port;
			sin_2->sin_addr.s_addr = iph->ip_dst.s_addr;
			sin.sin_addr = iph->ip_src;
			sa = (struct sockaddr *)&sin;
		} else if (iph->ip_v == (IPV6_VERSION >> 4)) {
			/* its IPv6 */
			struct ip6_hdr *ip6;
			struct sockaddr_in6 *sin6_2;

			ip6 = mtod(m, struct ip6_hdr *);
			sin6_2 = (struct sockaddr_in6 *)(local_sa);
			memset(sin6_2, 0, sizeof(sin6));
			sin6_2->sin6_family = AF_INET6;
			sin6_2->sin6_len = sizeof(struct sockaddr_in6);
			sin6_2->sin6_port = sh->dest_port;
			sin6.sin6_addr = ip6->ip6_src;
			sa = (struct sockaddr *)&sin6;
		} else {
			sa = NULL;
		}
	} else {
		/*
		 * For cookies we use the src address NOT from the packet
		 * but from the original INIT
		 */
		sa = altsa;
	}
	/* Turn off ECN until we get through all params */
	stcb->asoc.ecn_allowed = 0;
	TAILQ_FOREACH(net, &stcb->asoc.nets, sctp_next) {
		/* mark all addresses that we have currently on the list */
		net->dest_state |= SCTP_ADDR_NOT_IN_ASSOC;
	}
	/* does the source address already exist? if so skip it */
	l_inp = inp = stcb->sctp_ep;

	atomic_add_int(&stcb->asoc.refcnt, 1);
	stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net_tmp, local_sa, stcb);
	atomic_add_int(&stcb->asoc.refcnt, -1);

	if ((stcb_tmp == NULL && inp == stcb->sctp_ep) || inp == NULL) {
		/* we must add the source address */
		/* no scope set here since we have a tcb already. */
		if ((sa->sa_family == AF_INET) &&
		    (stcb->asoc.ipv4_addr_legal)) {
			if (sctp_add_remote_addr(stcb, sa, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_2)) {
				return (-1);
			}
		} else if ((sa->sa_family == AF_INET6) &&
		    (stcb->asoc.ipv6_addr_legal)) {
			if (sctp_add_remote_addr(stcb, sa, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_3)) {
				return (-2);
			}
		}
	} else {
		if (net_tmp != NULL && stcb_tmp == stcb) {
			net_tmp->dest_state &= ~SCTP_ADDR_NOT_IN_ASSOC;
		} else if (stcb_tmp != stcb) {
			/* It belongs to another association? */
			SCTP_TCB_UNLOCK(stcb_tmp);
			return (-3);
		}
	}
	if (stcb->asoc.state == 0) {
		/* the assoc was freed? */
		return (-4);
	}
	/* now we must go through each of the params. */
	phdr = sctp_get_next_param(m, offset, &parm_buf, sizeof(parm_buf));
	while (phdr) {
		ptype = ntohs(phdr->param_type);
		plen = ntohs(phdr->param_length);
		/*
		 * printf("ptype => %0x, plen => %d\n", (uint32_t)ptype,
		 * (int)plen);
		 */
		if (offset + plen > limit) {
			break;
		}
		if (plen == 0) {
			break;
		}
		if (ptype == SCTP_IPV4_ADDRESS) {
			if (stcb->asoc.ipv4_addr_legal) {
				struct sctp_ipv4addr_param *p4, p4_buf;

				/* ok get the v4 address and check/add */
				phdr = sctp_get_next_param(m, offset,
				    (struct sctp_paramhdr *)&p4_buf, sizeof(p4_buf));
				if (plen != sizeof(struct sctp_ipv4addr_param) ||
				    phdr == NULL) {
					return (-5);
				}
				p4 = (struct sctp_ipv4addr_param *)phdr;
				sin.sin_addr.s_addr = p4->addr;
				sa = (struct sockaddr *)&sin;
				inp = stcb->sctp_ep;
				atomic_add_int(&stcb->asoc.refcnt, 1);
				stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net,
				    local_sa, stcb);
				atomic_add_int(&stcb->asoc.refcnt, -1);

				if ((stcb_tmp == NULL && inp == stcb->sctp_ep) ||
				    inp == NULL) {
					/* we must add the source address */
					/*
					 * no scope set since we have a tcb
					 * already
					 */

					/*
					 * we must validate the state again
					 * here
					 */
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-7);
					}
					if (sctp_add_remote_addr(stcb, sa, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_4)) {
						return (-8);
					}
				} else if (stcb_tmp == stcb) {
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-10);
					}
					if (net != NULL) {
						/* clear flag */
						net->dest_state &=
						    ~SCTP_ADDR_NOT_IN_ASSOC;
					}
				} else {
					/*
					 * strange, address is in another
					 * assoc? straighten out locks.
					 */
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-12);
					}
					return (-13);
				}
			}
		} else if (ptype == SCTP_IPV6_ADDRESS) {
			if (stcb->asoc.ipv6_addr_legal) {
				/* ok get the v6 address and check/add */
				struct sctp_ipv6addr_param *p6, p6_buf;

				phdr = sctp_get_next_param(m, offset,
				    (struct sctp_paramhdr *)&p6_buf, sizeof(p6_buf));
				if (plen != sizeof(struct sctp_ipv6addr_param) ||
				    phdr == NULL) {
					return (-14);
				}
				p6 = (struct sctp_ipv6addr_param *)phdr;
				memcpy((caddr_t)&sin6.sin6_addr, p6->addr,
				    sizeof(p6->addr));
				sa = (struct sockaddr *)&sin6;
				inp = stcb->sctp_ep;
				atomic_add_int(&stcb->asoc.refcnt, 1);
				stcb_tmp = sctp_findassociation_ep_addr(&inp, sa, &net,
				    local_sa, stcb);
				atomic_add_int(&stcb->asoc.refcnt, -1);
				if (stcb_tmp == NULL && (inp == stcb->sctp_ep ||
				    inp == NULL)) {
					/*
					 * we must validate the state again
					 * here
					 */
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-16);
					}
					/*
					 * we must add the address, no scope
					 * set
					 */
					if (sctp_add_remote_addr(stcb, sa, SCTP_DONOT_SETSCOPE, SCTP_LOAD_ADDR_5)) {
						return (-17);
					}
				} else if (stcb_tmp == stcb) {
					/*
					 * we must validate the state again
					 * here
					 */
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-19);
					}
					if (net != NULL) {
						/* clear flag */
						net->dest_state &=
						    ~SCTP_ADDR_NOT_IN_ASSOC;
					}
				} else {
					/*
					 * strange, address is in another
					 * assoc? straighten out locks.
					 */
					if (stcb->asoc.state == 0) {
						/* the assoc was freed? */
						return (-21);
					}
					return (-22);
				}
			}
		} else if (ptype == SCTP_ECN_CAPABLE) {
			stcb->asoc.ecn_allowed = 1;
		} else if (ptype == SCTP_ULP_ADAPTATION) {
			if (stcb->asoc.state != SCTP_STATE_OPEN) {
				struct sctp_adaptation_layer_indication ai,
				                                *aip;

				phdr = sctp_get_next_param(m, offset,
				    (struct sctp_paramhdr *)&ai, sizeof(ai));
				aip = (struct sctp_adaptation_layer_indication *)phdr;
				sctp_ulp_notify(SCTP_NOTIFY_ADAPTATION_INDICATION,
				    stcb, ntohl(aip->indication), NULL);
			}
		} else if (ptype == SCTP_SET_PRIM_ADDR) {
			struct sctp_asconf_addr_param lstore, *fee;
			struct sctp_asconf_addrv4_param *fii;
			int lptype;
			struct sockaddr *lsa = NULL;

			stcb->asoc.peer_supports_asconf = 1;
			if (plen > sizeof(lstore)) {
				return (-23);
			}
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&lstore, plen);
			if (phdr == NULL) {
				return (-24);
			}
			fee = (struct sctp_asconf_addr_param *)phdr;
			lptype = ntohs(fee->addrp.ph.param_type);
			if (lptype == SCTP_IPV4_ADDRESS) {
				if (plen !=
				    sizeof(struct sctp_asconf_addrv4_param)) {
					printf("Sizeof setprim in init/init ack not %d but %d - ignored\n",
					    (int)sizeof(struct sctp_asconf_addrv4_param),
					    plen);
				} else {
					fii = (struct sctp_asconf_addrv4_param *)fee;
					sin.sin_addr.s_addr = fii->addrp.addr;
					lsa = (struct sockaddr *)&sin;
				}
			} else if (lptype == SCTP_IPV6_ADDRESS) {
				if (plen !=
				    sizeof(struct sctp_asconf_addr_param)) {
					printf("Sizeof setprim (v6) in init/init ack not %d but %d - ignored\n",
					    (int)sizeof(struct sctp_asconf_addr_param),
					    plen);
				} else {
					memcpy(sin6.sin6_addr.s6_addr,
					    fee->addrp.addr,
					    sizeof(fee->addrp.addr));
					lsa = (struct sockaddr *)&sin6;
				}
			}
			if (lsa) {
				sctp_set_primary_addr(stcb, sa, NULL);
			}
		} else if (ptype == SCTP_PRSCTP_SUPPORTED) {
			/* Peer supports pr-sctp */
			stcb->asoc.peer_supports_prsctp = 1;
		} else if (ptype == SCTP_SUPPORTED_CHUNK_EXT) {
			/* A supported extension chunk */
			struct sctp_supported_chunk_types_param *pr_supported;
			uint8_t local_store[128];
			int num_ent, i;

			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)&local_store, plen);
			if (phdr == NULL) {
				return (-25);
			}
			stcb->asoc.peer_supports_asconf = 0;
			stcb->asoc.peer_supports_prsctp = 0;
			stcb->asoc.peer_supports_pktdrop = 0;
			stcb->asoc.peer_supports_strreset = 0;
			stcb->asoc.peer_supports_auth = 0;
			pr_supported = (struct sctp_supported_chunk_types_param *)phdr;
			num_ent = plen - sizeof(struct sctp_paramhdr);
			for (i = 0; i < num_ent; i++) {
				switch (pr_supported->chunk_types[i]) {
				case SCTP_ASCONF:
				case SCTP_ASCONF_ACK:
					stcb->asoc.peer_supports_asconf = 1;
					break;
				case SCTP_FORWARD_CUM_TSN:
					stcb->asoc.peer_supports_prsctp = 1;
					break;
				case SCTP_PACKET_DROPPED:
					stcb->asoc.peer_supports_pktdrop = 1;
					break;
				case SCTP_STREAM_RESET:
					stcb->asoc.peer_supports_strreset = 1;
					break;
				case SCTP_AUTHENTICATION:
					stcb->asoc.peer_supports_auth = 1;
					break;
				default:
					/* one I have not learned yet */
					break;

				}
			}
		} else if (ptype == SCTP_ECN_NONCE_SUPPORTED) {
			/* Peer supports ECN-nonce */
			stcb->asoc.peer_supports_ecn_nonce = 1;
			stcb->asoc.ecn_nonce_allowed = 1;
		} else if (ptype == SCTP_RANDOM) {
			if (plen > sizeof(store))
				break;
			if (got_random) {
				/* already processed a RANDOM */
				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)store,
			    plen);
			if (phdr == NULL)
				return (-26);
			random = (struct sctp_auth_random *)phdr;
			random_len = plen - sizeof(*random);
			/* enforce the random length */
			if (random_len != SCTP_AUTH_RANDOM_SIZE_REQUIRED) {
#ifdef SCTP_DEBUG
				if (sctp_debug_on & SCTP_DEBUG_AUTH1)
					printf("SCTP: invalid RANDOM len\n");
#endif
				return (-27);
			}
			got_random = 1;
		} else if (ptype == SCTP_HMAC_LIST) {
			int num_hmacs;
			int i;

			if (plen > sizeof(store))
				break;
			if (got_hmacs) {
				/* already processed a HMAC list */
				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)store,
			    plen);
			if (phdr == NULL)
				return (-28);
			hmacs = (struct sctp_auth_hmac_algo *)phdr;
			hmacs_len = plen - sizeof(*hmacs);
			num_hmacs = hmacs_len / sizeof(hmacs->hmac_ids[0]);
			/* validate the hmac list */
			if (sctp_verify_hmac_param(hmacs, num_hmacs)) {
				return (-29);
			}
			if (stcb->asoc.peer_hmacs != NULL)
				sctp_free_hmaclist(stcb->asoc.peer_hmacs);
			stcb->asoc.peer_hmacs = sctp_alloc_hmaclist(num_hmacs);
			if (stcb->asoc.peer_hmacs != NULL) {
				for (i = 0; i < num_hmacs; i++) {
					sctp_auth_add_hmacid(stcb->asoc.peer_hmacs,
					    ntohs(hmacs->hmac_ids[i]));
				}
			}
			got_hmacs = 1;
		} else if (ptype == SCTP_CHUNK_LIST) {
			int i;

			if (plen > sizeof(store))
				break;
			if (got_chklist) {
				/* already processed a Chunks list */
				goto next_param;
			}
			phdr = sctp_get_next_param(m, offset,
			    (struct sctp_paramhdr *)store,
			    plen);
			if (phdr == NULL)
				return (-30);
			chunks = (struct sctp_auth_chunk_list *)phdr;
			num_chunks = plen - sizeof(*chunks);
			if (stcb->asoc.peer_auth_chunks != NULL)
				sctp_clear_chunklist(stcb->asoc.peer_auth_chunks);
			else
				stcb->asoc.peer_auth_chunks = sctp_alloc_chunklist();
			for (i = 0; i < num_chunks; i++) {
				sctp_auth_add_chunk(chunks->chunk_types[i],
				    stcb->asoc.peer_auth_chunks);
			}
			got_chklist = 1;
		} else if ((ptype == SCTP_HEARTBEAT_INFO) ||
			    (ptype == SCTP_STATE_COOKIE) ||
			    (ptype == SCTP_UNRECOG_PARAM) ||
			    (ptype == SCTP_COOKIE_PRESERVE) ||
			    (ptype == SCTP_SUPPORTED_ADDRTYPE) ||
			    (ptype == SCTP_ADD_IP_ADDRESS) ||
			    (ptype == SCTP_DEL_IP_ADDRESS) ||
			    (ptype == SCTP_ERROR_CAUSE_IND) ||
		    (ptype == SCTP_SUCCESS_REPORT)) {
			 /* don't care */ ;
		} else {
			if ((ptype & 0x8000) == 0x0000) {
				/*
				 * must stop processing the rest of the
				 * param's. Any report bits were handled
				 * with the call to
				 * sctp_arethere_unrecognized_parameters()
				 * when the INIT or INIT-ACK was first seen.
				 */
				break;
			}
		}
next_param:
		offset += SCTP_SIZE32(plen);
		if (offset >= limit) {
			break;
		}
		phdr = sctp_get_next_param(m, offset, &parm_buf,
		    sizeof(parm_buf));
	}
	/* Now check to see if we need to purge any addresses */
	for (net = TAILQ_FIRST(&stcb->asoc.nets); net != NULL; net = net_tmp) {
		net_tmp = TAILQ_NEXT(net, sctp_next);
		if ((net->dest_state & SCTP_ADDR_NOT_IN_ASSOC) ==
		    SCTP_ADDR_NOT_IN_ASSOC) {
			/* This address has been removed from the asoc */
			/* remove and free it */
			stcb->asoc.numnets--;
			TAILQ_REMOVE(&stcb->asoc.nets, net, sctp_next);
			sctp_free_remote_addr(net);
			if (net == stcb->asoc.primary_destination) {
				stcb->asoc.primary_destination = NULL;
				sctp_select_primary_destination(stcb);
			}
		}
	}
	/* validate authentication required parameters */
	if (got_random && got_hmacs) {
		stcb->asoc.peer_supports_auth = 1;
	} else {
		stcb->asoc.peer_supports_auth = 0;
	}
	if (!sctp_asconf_auth_nochk && stcb->asoc.peer_supports_asconf &&
	    !stcb->asoc.peer_supports_auth) {
		return (-31);
	}
	/* concatenate the full random key */
	keylen = random_len + num_chunks + hmacs_len;
	new_key = sctp_alloc_key(keylen);
	if (new_key != NULL) {
		/* copy in the RANDOM */
		if (random != NULL)
			bcopy(random->random_data, new_key->key, random_len);
		/* append in the AUTH chunks */
		if (chunks != NULL)
			bcopy(chunks->chunk_types, new_key->key + random_len,
			    num_chunks);
		/* append in the HMACs */
		if (hmacs != NULL)
			bcopy(hmacs->hmac_ids, new_key->key + random_len + num_chunks,
			    hmacs_len);
	} else {
		return (-32);
	}
	if (stcb->asoc.authinfo.peer_random != NULL)
		sctp_free_key(stcb->asoc.authinfo.peer_random);
	stcb->asoc.authinfo.peer_random = new_key;
#ifdef SCTP_AUTH_DRAFT_04
	/* don't include the chunks and hmacs for draft -04 */
	stcb->asoc.authinfo.peer_random->keylen = random_len;
#endif
	sctp_clear_cachedkeys(stcb, stcb->asoc.authinfo.assoc_keyid);
	sctp_clear_cachedkeys(stcb, stcb->asoc.authinfo.recv_keyid);

	return (0);
}

int
sctp_set_primary_addr(struct sctp_tcb *stcb, struct sockaddr *sa,
    struct sctp_nets *net)
{
	/* make sure the requested primary address exists in the assoc */
	if (net == NULL && sa)
		net = sctp_findnet(stcb, sa);

	if (net == NULL) {
		/* didn't find the requested primary address! */
		return (-1);
	} else {
		/* set the primary address */
		if (net->dest_state & SCTP_ADDR_UNCONFIRMED) {
			/* Must be confirmed */
			return (-1);
		}
		stcb->asoc.primary_destination = net;
		net->dest_state &= ~SCTP_ADDR_WAS_PRIMARY;
		net = TAILQ_FIRST(&stcb->asoc.nets);
		if (net != stcb->asoc.primary_destination) {
			/*
			 * first one on the list is NOT the primary
			 * sctp_cmpaddr() is much more efficent if the
			 * primary is the first on the list, make it so.
			 */
			TAILQ_REMOVE(&stcb->asoc.nets, stcb->asoc.primary_destination, sctp_next);
			TAILQ_INSERT_HEAD(&stcb->asoc.nets, stcb->asoc.primary_destination, sctp_next);
		}
		return (0);
	}
}


int
sctp_is_vtag_good(struct sctp_inpcb *inp, uint32_t tag, struct timeval *now)
{
	/*
	 * This function serves two purposes. It will see if a TAG can be
	 * re-used and return 1 for yes it is ok and 0 for don't use that
	 * tag. A secondary function it will do is purge out old tags that
	 * can be removed.
	 */
	struct sctpasochead *head;
	struct sctpvtaghead *chain;
	struct sctp_tagblock *twait_block;
	struct sctp_tcb *stcb;
	int i;

	SCTP_INP_INFO_WLOCK();
	chain = &sctppcbinfo.vtag_timewait[(tag % SCTP_STACK_VTAG_HASH_SIZE)];
	/* First is the vtag in use ? */

	head = &sctppcbinfo.sctp_asochash[SCTP_PCBHASH_ASOC(tag,
	    sctppcbinfo.hashasocmark)];
	if (head == NULL) {
		goto check_restart;
	}
	LIST_FOREACH(stcb, head, sctp_asocs) {

		if (stcb->asoc.my_vtag == tag) {
			/*
			 * We should remove this if and return 0 always if
			 * we want vtags unique across all endpoints. For
			 * now within a endpoint is ok.
			 */
			if (inp == stcb->sctp_ep) {
				/* bad tag, in use */
				SCTP_INP_INFO_WUNLOCK();
				return (0);
			}
		}
	}
check_restart:
	/* Now lets check the restart hash */
	head = &sctppcbinfo.sctp_restarthash[SCTP_PCBHASH_ASOC(tag,
	    sctppcbinfo.hashrestartmark)];
	if (head == NULL) {
		goto check_time_wait;
	}
	LIST_FOREACH(stcb, head, sctp_tcbrestarhash) {
		if (stcb->asoc.assoc_id == tag) {
			/* candidate */
			if (inp == stcb->sctp_ep) {
				/* bad tag, in use */
				SCTP_INP_INFO_WUNLOCK();
				return (0);
			}
		}
	}
check_time_wait:
	/* Now what about timed wait ? */
	if (!LIST_EMPTY(chain)) {
		/*
		 * Block(s) are present, lets see if we have this tag in the
		 * list
		 */
		LIST_FOREACH(twait_block, chain, sctp_nxt_tagblock) {
			for (i = 0; i < SCTP_NUMBER_IN_VTAG_BLOCK; i++) {
				if (twait_block->vtag_block[i].v_tag == 0) {
					/* not used */
					continue;
				} else if ((long)twait_block->vtag_block[i].tv_sec_at_expire >
				    now->tv_sec) {
					/* Audit expires this guy */
					twait_block->vtag_block[i].tv_sec_at_expire = 0;
					twait_block->vtag_block[i].v_tag = 0;
				} else if (twait_block->vtag_block[i].v_tag ==
				    tag) {
					/* Bad tag, sorry :< */
					SCTP_INP_INFO_WUNLOCK();
					return (0);
				}
			}
		}
	}
	/* Not found, ok to use the tag */
	SCTP_INP_INFO_WUNLOCK();
	return (1);
}


/*
 * Delete the address from the endpoint local address list Lookup using a
 * sockaddr address (ie. not an ifaddr)
 */
int
sctp_del_local_addr_ep_sa(struct sctp_inpcb *inp, struct sockaddr *sa)
{
	struct sctp_laddr *laddr;
	struct sockaddr *l_sa;
	int found = 0;

	/*
	 * Here is another function I cannot find a caller for. As such we
	 * SHOULD delete it if we have no users. If we find a user that user
	 * MUST have the INP locked.
	 * 
	 */

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/* You are already bound to all. You have it already */
		return (EINVAL);
	}
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		/* make sure the address exists */
		if (laddr->ifa == NULL)
			continue;
		if (laddr->ifa->ifa_addr == NULL)
			continue;

		l_sa = laddr->ifa->ifa_addr;
		if (l_sa->sa_family == AF_INET6) {
			/* IPv6 address */
			struct sockaddr_in6 *sin1, *sin2;

			sin1 = (struct sockaddr_in6 *)l_sa;
			sin2 = (struct sockaddr_in6 *)sa;
			if (memcmp(&sin1->sin6_addr, &sin2->sin6_addr,
			    sizeof(struct in6_addr)) == 0) {
				/* matched */
				found = 1;
				break;
			}
		} else if (l_sa->sa_family == AF_INET) {
			/* IPv4 address */
			struct sockaddr_in *sin1, *sin2;

			sin1 = (struct sockaddr_in *)l_sa;
			sin2 = (struct sockaddr_in *)sa;
			if (sin1->sin_addr.s_addr == sin2->sin_addr.s_addr) {
				/* matched */
				found = 1;
				break;
			}
		} else {
			/* invalid family */
			return (-1);
		}
	}

	if (found && inp->laddr_count < 2) {
		/* can't delete unless there are at LEAST 2 addresses */
		return (-1);
	}
	if (found && (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) == 0) {
		/*
		 * remove it from the ep list, this should NOT be done until
		 * its really gone from the interface list and we won't be
		 * receiving more of these. Probably right away. If we do
		 * allow a removal of an address from an association
		 * (sub-set bind) than this should NOT be called until the
		 * all ASCONF come back from this association.
		 */
		sctp_remove_laddr(laddr);
		return (0);
	} else {
		return (-1);
	}
}

static sctp_assoc_t reneged_asoc_ids[256];
static uint8_t reneged_at = 0;

extern int sctp_do_drain;

static void
sctp_drain_mbufs(struct sctp_inpcb *inp, struct sctp_tcb *stcb)
{
	/*
	 * We must hunt this association for MBUF's past the cumack (i.e.
	 * out of order data that we can renege on).
	 */
	struct sctp_association *asoc;
	struct sctp_tmit_chunk *chk, *nchk;
	uint32_t cumulative_tsn_p1, tsn;
	struct sctp_queued_to_read *ctl, *nctl;
	int cnt, strmat, gap;

	/* We look for anything larger than the cum-ack + 1 */

	SCTP_STAT_INCR(sctps_protocol_drain_calls);
	if (sctp_do_drain == 0) {
		return;
	}
	asoc = &stcb->asoc;
	if (asoc->cumulative_tsn == asoc->highest_tsn_inside_map) {
		/* none we can reneg on. */
		return;
	}
	SCTP_STAT_INCR(sctps_protocol_drains_done);
	cumulative_tsn_p1 = asoc->cumulative_tsn + 1;
	cnt = 0;
	/* First look in the re-assembly queue */
	chk = TAILQ_FIRST(&asoc->reasmqueue);
	while (chk) {
		/* Get the next one */
		nchk = TAILQ_NEXT(chk, sctp_next);
		if (compare_with_wrap(chk->rec.data.TSN_seq,
		    cumulative_tsn_p1, MAX_TSN)) {
			/* Yep it is above cum-ack */
			cnt++;
			tsn = chk->rec.data.TSN_seq;
			if (tsn >= asoc->mapping_array_base_tsn) {
				gap = tsn - asoc->mapping_array_base_tsn;
			} else {
				gap = (MAX_TSN - asoc->mapping_array_base_tsn) +
				    tsn + 1;
			}
			asoc->size_on_reasm_queue = sctp_sbspace_sub(asoc->size_on_reasm_queue, chk->send_size);
			sctp_ucount_decr(asoc->cnt_on_reasm_queue);
			SCTP_UNSET_TSN_PRESENT(asoc->mapping_array, gap);
			TAILQ_REMOVE(&asoc->reasmqueue, chk, sctp_next);
			if (chk->data) {
				sctp_m_freem(chk->data);
				chk->data = NULL;
			}
			sctp_free_remote_addr(chk->whoTo);
			sctp_free_a_chunk(stcb, chk);
		}
		chk = nchk;
	}
	/* Ok that was fun, now we will drain all the inbound streams? */
	for (strmat = 0; strmat < asoc->streamincnt; strmat++) {
		ctl = TAILQ_FIRST(&asoc->strmin[strmat].inqueue);
		while (ctl) {
			nctl = TAILQ_NEXT(ctl, next);
			if (compare_with_wrap(ctl->sinfo_tsn,
			    cumulative_tsn_p1, MAX_TSN)) {
				/* Yep it is above cum-ack */
				cnt++;
				tsn = ctl->sinfo_tsn;
				if (tsn >= asoc->mapping_array_base_tsn) {
					gap = tsn -
					    asoc->mapping_array_base_tsn;
				} else {
					gap = (MAX_TSN -
					    asoc->mapping_array_base_tsn) +
					    tsn + 1;
				}
				asoc->size_on_all_streams = sctp_sbspace_sub(asoc->size_on_all_streams, ctl->length);
				sctp_ucount_decr(asoc->cnt_on_all_streams);

				SCTP_UNSET_TSN_PRESENT(asoc->mapping_array,
				    gap);
				TAILQ_REMOVE(&asoc->strmin[strmat].inqueue,
				    ctl, next);
				if (ctl->data) {
					sctp_m_freem(ctl->data);
					ctl->data = NULL;
				}
				sctp_free_remote_addr(ctl->whoFrom);
				SCTP_ZONE_FREE(sctppcbinfo.ipi_zone_readq, ctl);
				SCTP_DECR_READQ_COUNT();
			}
			ctl = nctl;
		}
	}
	/*
	 * Question, should we go through the delivery queue? The only
	 * reason things are on here is the app not reading OR a p-d-api up.
	 * An attacker COULD send enough in to initiate the PD-API and then
	 * send a bunch of stuff to other streams... these would wind up on
	 * the delivery queue.. and then we would not get to them. But in
	 * order to do this I then have to back-track and un-deliver
	 * sequence numbers in streams.. el-yucko. I think for now we will
	 * NOT look at the delivery queue and leave it to be something to
	 * consider later. An alternative would be to abort the P-D-API with
	 * a notification and then deliver the data.... Or another method
	 * might be to keep track of how many times the situation occurs and
	 * if we see a possible attack underway just abort the association.
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_PCB1) {
		if (cnt) {
			printf("Freed %d chunks from reneg harvest\n", cnt);
		}
	}
#endif				/* SCTP_DEBUG */
	if (cnt) {
		/*
		 * Now do we need to find a new
		 * asoc->highest_tsn_inside_map?
		 */
		if (asoc->highest_tsn_inside_map >= asoc->mapping_array_base_tsn) {
			gap = asoc->highest_tsn_inside_map - asoc->mapping_array_base_tsn;
		} else {
			gap = (MAX_TSN - asoc->mapping_array_base_tsn) +
			    asoc->highest_tsn_inside_map + 1;
		}
		if (gap >= (asoc->mapping_array_size << 3)) {
			/*
			 * Something bad happened or cum-ack and high were
			 * behind the base, but if so earlier checks should
			 * have found NO data... wierd... we will start at
			 * end of mapping array.
			 */
			printf("Gap was larger than array?? %d set to max:%d maparraymax:%x\n",
			    (int)gap,
			    (int)(asoc->mapping_array_size << 3),
			    (int)asoc->highest_tsn_inside_map);
			gap = asoc->mapping_array_size << 3;
		}
		while (gap > 0) {
			if (SCTP_IS_TSN_PRESENT(asoc->mapping_array, gap)) {
				/* found the new highest */
				asoc->highest_tsn_inside_map = asoc->mapping_array_base_tsn + gap;
				break;
			}
			gap--;
		}
		if (gap == 0) {
			/* Nothing left in map */
			memset(asoc->mapping_array, 0, asoc->mapping_array_size);
			asoc->mapping_array_base_tsn = asoc->cumulative_tsn + 1;
			asoc->highest_tsn_inside_map = asoc->cumulative_tsn;
		}
		asoc->last_revoke_count = cnt;
		SCTP_OS_TIMER_STOP(&stcb->asoc.dack_timer.timer);
		sctp_send_sack(stcb);
		reneged_asoc_ids[reneged_at] = sctp_get_associd(stcb);
		reneged_at++;
	}
	/*
	 * Another issue, in un-setting the TSN's in the mapping array we
	 * DID NOT adjust the higest_tsn marker.  This will cause one of two
	 * things to occur. It may cause us to do extra work in checking for
	 * our mapping array movement. More importantly it may cause us to
	 * SACK every datagram. This may not be a bad thing though since we
	 * will recover once we get our cum-ack above and all this stuff we
	 * dumped recovered.
	 */
}

void
sctp_drain()
{
	/*
	 * We must walk the PCB lists for ALL associations here. The system
	 * is LOW on MBUF's and needs help. This is where reneging will
	 * occur. We really hope this does NOT happen!
	 */
	struct sctp_inpcb *inp;
	struct sctp_tcb *stcb;

	SCTP_INP_INFO_RLOCK();
	LIST_FOREACH(inp, &sctppcbinfo.listhead, sctp_list) {
		/* For each endpoint */
		SCTP_INP_RLOCK(inp);
		LIST_FOREACH(stcb, &inp->sctp_asoc_list, sctp_tcblist) {
			/* For each association */
			SCTP_TCB_LOCK(stcb);
			sctp_drain_mbufs(inp, stcb);
			SCTP_TCB_UNLOCK(stcb);
		}
		SCTP_INP_RUNLOCK(inp);
	}
	SCTP_INP_INFO_RUNLOCK();
}

/*
 * start a new iterator
 * iterates through all endpoints and associations based on the pcb_state
 * flags and asoc_state.  "af" (mandatory) is executed for all matching
 * assocs and "ef" (optional) is executed when the iterator completes.
 * "inpf" (optional) is executed for each new endpoint as it is being
 * iterated through.
 */
int
sctp_initiate_iterator(inp_func inpf, asoc_func af, uint32_t pcb_state,
    uint32_t pcb_features, uint32_t asoc_state, void *argp, uint32_t argi,
    end_func ef, struct sctp_inpcb *s_inp, uint8_t chunk_output_off)
{
	struct sctp_iterator *it = NULL;
	int s;

	if (af == NULL) {
		return (-1);
	}
	SCTP_MALLOC(it, struct sctp_iterator *, sizeof(struct sctp_iterator),
	    "Iterator");
	if (it == NULL) {
		return (ENOMEM);
	}
	memset(it, 0, sizeof(*it));
	it->function_assoc = af;
	it->function_inp = inpf;
	it->function_atend = ef;
	it->pointer = argp;
	it->val = argi;
	it->pcb_flags = pcb_state;
	it->pcb_features = pcb_features;
	it->asoc_state = asoc_state;
	it->no_chunk_output = chunk_output_off;
	if (s_inp) {
		it->inp = s_inp;
		it->iterator_flags = SCTP_ITERATOR_DO_SINGLE_INP;
	} else {
		SCTP_INP_INFO_RLOCK();
		it->inp = LIST_FIRST(&sctppcbinfo.listhead);
		SCTP_INP_INFO_RUNLOCK();
		it->iterator_flags = SCTP_ITERATOR_DO_ALL_INP;

	}
	/* Init the timer */
	SCTP_OS_TIMER_INIT(&it->tmr.timer);
	/* add to the list of all iterators */
	SCTP_INP_INFO_WLOCK();
	LIST_INSERT_HEAD(&sctppcbinfo.iteratorhead, it, sctp_nxt_itr);
	SCTP_INP_INFO_WUNLOCK();
	s = splnet();
	sctp_timer_start(SCTP_TIMER_TYPE_ITERATOR, (struct sctp_inpcb *)it,
	    NULL, NULL);
	splx(s);
	return (0);
}
