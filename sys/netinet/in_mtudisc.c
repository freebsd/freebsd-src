/*-
 * Copyright (c) 1993, University of Vermont and State
 *  Agricultural College.
 * Copyright (c) 1993, Garrett A. Wollman.
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR AUTHORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: in_mtudisc.c,v 1.2 1993/12/19 00:52:36 wollman Exp $
 */

#ifdef MTUDISC

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "in_systm.h"
#include "net/if.h"
#include "net/route.h"
#include "in.h"
#include "in_var.h"
#include "ip.h"
#include "protosw.h"
#include "in_pcb.h"

#ifdef INET

/*
 * checkpcbs[] lists all the PCB heads that might call on the services
 * of MTU discovery.
 * This is really bogus 'cuz a ULP needs to both get its entry added here
 * /and/ set INP_DISCOVERMTU in each PCB.
 */
extern struct inpcb tcb;	/* XXX move to header file */

struct inpcb *checkpcbs[] = {
  &tcb,
  0
};


/*
 * Table of likely MTU values, courtesy of RFC 1191.
 * This MUST remain in sorted order.
 */
static const u_short in_mtus[] = {
  65535,			/* maximum */
  32767,			/* convenient power of 2 - 1 */
  17914,			/* 16Mb Token Ring */
  16383,			/* convenient power of 2 - 1 */
  8166,				/* IEEE 802.4 */
  6288,				/* convenient stopping point */
  4352,				/* FDDI */
  3144,				/* convenient stopping point */
  2002,				/* IEEE 802.5 */
  1492,				/* IEEE 802.3 */
  1006,				/* BBN 1822 */
  508,				/* ARCNET */
  296,				/* SLIP, PPP */
  128				/* minimum we'll accept */
};

#define NMTUS ((sizeof in_mtus)/(sizeof in_mtus[0]))

/*
 * Find the next MTU in the sequence from CURRENT.
 * If HIGHER, increase size; else decrease.
 * Return of zero means we're stuck.
 * NB: We might be called with a CURRENT MTU that's not in the
 * table (as, for example, when an ICMP tells us there's a problem
 * and reports a max path MTU value).
 */
unsigned
in_nextmtu(unsigned current, int higher) {
  int i;

  for(i = 0; i < NMTUS; i++) {
    if(in_mtus[i] <= (u_short)current)
      break;
  }

  if(i == NMTUS) {
    if(higher) 	return in_mtus[NMTUS - 1];
    else	return 0;	/* error return */
  }

  /*
   * Now we know that CURRENT lies somewhere in the interval
   * (in_mtus[i - 1], in_mtus[i]].  If we want to go higher,
   * take in_mtus[i - 1] always.  If we want to go lower, we
   * must check the lower bound to see if it's equal, and if so,
   * take in_mtus[i + 1], unless i == NMTUS - 1, in which case
   * we return failure.
   * Got that?
   */
  if(higher)
    return in_mtus[(i >= 1) ? (i - 1) : 0];

  /* now we know it's lower */
  if(current == in_mtus[i]) {
    if(i == NMTUS - 1)
      return 0;
    else
      return in_mtus[i + 1];
  }

  return in_mtus[i];
}

/*
 * Set up the route to do MTU discovery.  This only works for host routes,
 * not net routes; in any case, ALL systems should have all IP routes
 * marked with RTF_CLONING (and a genmask of zero), which will do the right
 * thing, and also arrange for the pre-ARPing code to get called on
 * on appropriate interfaces.
 *
 * We also go to some pains to keep listeners on the routing socket aware
 * of what's going on when we fiddle the flags or metrics.  I don't know
 * if this is really necessary or not (or even if we're doing it in the
 * right way).
 */
int in_routemtu(struct route *ro) {
  if(!ro->ro_rt)
    return 0;

  if((ro->ro_rt->rt_flags & (RTF_HOST | RTF_UP)) != (RTF_HOST | RTF_UP))
    return 0;

  if(ro->ro_rt->rt_rmx.rmx_mtu) {
    /*
     * Let the user know that we've turned on MTU discovery for this
     * route entry.  This doesn't do anything at present, but may
     * be useful later on.
     */
    if(!(ro->ro_rt->rt_flags & RTF_PROTO1)) {
      ro->ro_rt->rt_flags |= RTF_PROTO1;
    }
    return 1;
  }

  if(ro->ro_rt->rt_ifp && !(ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU)) {
    ro->ro_rt->rt_flags |= RTF_PROTO1;
    /*
     * Subtraction is necessary because the interface's MTU includes
     * the interface's own headers.  We subtract the header length
     * provided and hope for the best.
     */
    ro->ro_rt->rt_rmx.rmx_mtu = 
      ro->ro_rt->rt_ifp->if_mtu - ro->ro_rt->rt_ifp->if_hdrlen;
    return 1;
  }
  return 0;
}

/*
 * Perform the PCB fiddling necessary when the route changes.
 * Protect against recursion, since we might get called as a
 * result of notifying someone else that the MTU is changing.
 */
void
in_pcbmtu(struct inpcb *inp) {
  static int notifying = 0;
  static int timerstarted = 0;
  unsigned oldmtu = inp->inp_pmtu;
  int oldflags = inp->inp_flags;

  if (!timerstarted) {
    timeout(in_mtutimer, 0, 60 * hz);
    timerstarted = 1;
  }

  if (inp->inp_flags & INP_DISCOVERMTU) {
    /*
     * If no route present, get one.
     * If there is one present, but it's marked as being `down',
     * try to get another one.
     */
    if(!inp->inp_route.ro_rt)
      rtalloc(&inp->inp_route);
    else if((inp->inp_route.ro_rt->rt_flags & RTF_UP) == 0) {
      RTFREE(inp->inp_route.ro_rt);
      inp->inp_route.ro_rt = 0;
      rtalloc(&inp->inp_route);
    }

    if(in_routemtu(&inp->inp_route)) {
      inp->inp_flags |= INP_MTUDISCOVERED;
      inp->inp_pmtu = inp->inp_route.ro_rt->rt_rmx.rmx_mtu;
      inp->inp_ip.ip_off |= IP_DF;
    } else {
      inp->inp_flags &= ~INP_MTUDISCOVERED;
      inp->inp_ip.ip_off &= ~IP_DF;
    }
    /*
     * If nothing has changed since the last value we had,
     * don't waste any time notifying everybody that nothing
     * has changed.
     */
    if(inp->inp_pmtu != oldmtu
       || (inp->inp_flags ^ oldflags)) {
      notifying = 1;
      /*
       * If the MTU has decreased, use timer 2.
       */
      inp->inp_mtutimer = 
	(inp->inp_pmtu < oldmtu) ? in_mtutimer2 : in_mtutimer1;
      in_mtunotify(inp);
      notifying = 0;
    }
  }
}

/*
 * Tell the clients that have the same destination as INP that they
 * need to take a new look at the MTU value and flags.
 */
void
in_mtunotify(struct inpcb *inp) {
  in_pcbnotify(inp->inp_head, &inp->inp_route.ro_dst, 0, zeroin_addr,
	       0, PRC_MTUCHANGED, inp->inp_mtunotify);
}

/*
 * Adjust the MTU listed in the route on the basis of an ICMP
 * Unreachable: Need Fragmentation message.
 * Note that the PRC_MSGSIZE error is still delivered; this just
 * makes the adjustment in the route, and depends on the ULPs which
 * are required to translate PRC_MSGSIZE into an in_pcbmtu() which will
 * pick up the new size.
 */
void
in_mtureduce(struct in_addr dst, unsigned newsize) {
  struct route ro;

  ro.ro_dst.sa_family = AF_INET;
  ro.ro_dst.sa_len = sizeof ro.ro_dst;
  ((struct sockaddr_in *)&ro.ro_dst)->sin_addr = dst;
  ro.ro_rt = 0;
  rtalloc(&ro);

  /*
   * If there was no route, just forget about it, can't do anything.
   */
  if(!ro.ro_rt)
    return;

  /*
   * If there was a route, but it's the wrong kind, forget it.
   */
  if((ro.ro_rt->rt_flags & (RTF_UP | RTF_HOST)) != (RTF_UP | RTF_HOST)) {
    RTFREE(ro.ro_rt);
    return;
  }

  /*
   * If the MTU is locked by some outside agency, forget it.
   */
  if(ro.ro_rt->rt_rmx.rmx_locks & RTV_MTU) {
    RTFREE(ro.ro_rt);
    return;
  }

  /*
   * If newsize == 0, then we got an ICMP from a router
   * which doesn't support the MTU extension, so just go down one.
   */
  newsize = in_nextmtu(ro.ro_rt->rt_rmx.rmx_mtu, 0);

  if(!newsize) {
    ro.ro_rt->rt_rmx.rmx_mtu = 0; /* we can't go any lower */
    RTFREE(ro.ro_rt);
    return;
  }
  /*
   * If the new MTU is greater than the old MTU, forget it.  (Prevent
   * denial-of-service attack.)  Don't bother if the new MTU is the
   * same as the old one.
   */
  if(ro.ro_rt->rt_rmx.rmx_mtu <= newsize) {
    RTFREE(ro.ro_rt);
    return;
  }

  /*
   * OK, do it.
   */
  ro.ro_rt->rt_rmx.rmx_mtu = newsize;
  RTFREE(ro.ro_rt);
}

/*
 * Walk through all the PCB lists in checkpcbs[] and decrement the
 * timers on the ones still participating in MTU discovery.
 * If the timers reach zero, bump the MTU (clamped to the interface
 * MTU), assuming the route is still good.
 */
void
in_mtutimer(caddr_t dummy1, int dummy2) {
  int i;
  struct inpcb *inp;
  struct rtentry *rt;
  int s = splnet();

  for(i = 0; checkpcbs[i]; i++) {
    inp = checkpcbs[i];

    while(inp = inp->inp_next) {
      if(inp->inp_flags & INP_MTUDISCOVERED) {
	if(!inp->inp_route.ro_rt
	   || !(inp->inp_route.ro_rt->rt_flags & RTF_UP)) {
	  inp->inp_flags &= ~INP_MTUDISCOVERED;
	  continue;		/* we'll notice it later */
	}

	if(--inp->inp_mtutimer == 0) {
	  in_bumpmtu(inp);
	  inp->inp_mtutimer = in_mtutimer1;
	  if(inp->inp_route.ro_rt->rt_rmx.rmx_rtt
	     && ((in_mtutimer1 * 60) 
		 > (inp->inp_route.ro_rt->rt_rmx.rmx_rtt / RTM_RTTUNIT))) {
	    inp->inp_mtutimer = 
	      inp->inp_route.ro_rt->rt_rmx.rmx_rtt / RTM_RTTUNIT;
	  }
	}
      }
    }
  }
  splx(s);
  timeout(in_mtutimer, (caddr_t)0, 60 * hz);
}

/*
 * Try to increase the MTU and let everyone know that it has changed.
 * Must be called with a valid route in inp->inp_route.  Probably
 * must be at splnet(), too.
 */
void
in_bumpmtu(struct inpcb *inp) {
  struct route *ro;
  unsigned newmtu;

  ro = &inp->inp_route;
  newmtu = in_nextmtu(inp->inp_pmtu, 1);
  if(!newmtu) return;		/* doing the best we can */
  if(newmtu <= ro->ro_rt->rt_ifp->if_mtu) {
    if(!(ro->ro_rt->rt_rmx.rmx_locks & RTV_MTU)) {
      ro->ro_rt->rt_rmx.rmx_mtu = newmtu;
      in_pcbmtu(inp);
    }
  }
}

#endif /* INET */
#endif /* MTUDISC */
