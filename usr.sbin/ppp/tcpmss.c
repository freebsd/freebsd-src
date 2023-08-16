/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2000 Ruslan Ermilov and Brian Somers <brian@Awfulhak.org>
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
 */

#include <sys/param.h>

#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#ifndef NOINET6
#include <netinet/ip6.h>
#endif
#include <netinet/tcp.h>
#include <sys/un.h>

#include <termios.h>

#include "layer.h"
#include "defs.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "mbuf.h"
#include "throughput.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "iplist.h"
#include "slcompress.h"
#include "ncpaddr.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "mp.h"
#include "iface.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "ipv6cp.h"
#include "ncp.h"
#include "bundle.h"


/*-
 * Compute the MSS as described in RFC 6691.
 */
#define MAXMSS4(mtu) ((mtu) - sizeof(struct ip) - sizeof(struct tcphdr))
#ifndef NOINET6
#define MAXMSS6(mtu) ((mtu) - sizeof(struct ip6_hdr) - sizeof(struct tcphdr))
#endif


/*-
 * The following macro is used to update an
 * internet checksum.  "acc" is a 32-bit
 * accumulation of all the changes to the
 * checksum (adding in old 16-bit words and
 * subtracting out new words), and "cksum"
 * is the checksum value to be updated.
 */
#define ADJUST_CHECKSUM(acc, cksum) { \
  acc += cksum; \
  if (acc < 0) { \
    acc = -acc; \
    acc = (acc >> 16) + (acc & 0xffff); \
    acc += acc >> 16; \
    cksum = (u_short) ~acc; \
  } else { \
    acc = (acc >> 16) + (acc & 0xffff); \
    acc += acc >> 16; \
    cksum = (u_short) acc; \
  } \
}

static void
MSSFixup(struct tcphdr *tc, size_t pktlen, u_int16_t maxmss)
{
  size_t hlen, olen, optlen;
  u_char *opt;
  u_int16_t *mss;
  int accumulate;

  hlen = tc->th_off << 2;

  /* Invalid header length or header without options. */
  if (hlen <= sizeof(struct tcphdr) || hlen > pktlen)
    return;

  /* MSS option only allowed within SYN packets. */
  if (!(tc->th_flags & TH_SYN))
    return;

  for (olen = hlen - sizeof(struct tcphdr), opt = (u_char *)(tc + 1);
       olen > 0; olen -= optlen, opt += optlen) {
    if (*opt == TCPOPT_EOL)
      break;
    else if (*opt == TCPOPT_NOP)
      optlen = 1;
    else {
      optlen = *(opt + 1);
      if (optlen <= 0 || optlen > olen)
        break;
      if (*opt == TCPOPT_MAXSEG) {
        if (optlen != TCPOLEN_MAXSEG)
          continue;
        mss = (u_int16_t *)(opt + 2);
        if (ntohs(*mss) > maxmss) {
          log_Printf(LogDEBUG, "MSS: %u -> %u\n",
               ntohs(*mss), maxmss);
          accumulate = *mss;
          *mss = htons(maxmss);
          accumulate -= *mss;
          ADJUST_CHECKSUM(accumulate, tc->th_sum);
        }
      }
    }
  }
}

static struct mbuf *
tcpmss_Check(struct bundle *bundle, struct mbuf *bp)
{
  struct ip *pip;
#ifndef NOINET6
  struct ip6_hdr *pip6;
  struct ip6_frag *pfrag;
#endif
  size_t hlen, plen;

  if (!Enabled(bundle, OPT_TCPMSSFIXUP))
    return bp;

  bp = m_pullup(bp);
  plen = m_length(bp);
  if (plen < sizeof(struct ip))
    return bp;
  pip = (struct ip *)MBUF_CTOP(bp);

  switch (pip->ip_v) {
  case IPVERSION:
    /*
     * Check for MSS option only for TCP packets with zero fragment offsets
     * and correct total and header lengths.
     */
    hlen = pip->ip_hl << 2;
    if (pip->ip_p == IPPROTO_TCP && (ntohs(pip->ip_off) & IP_OFFMASK) == 0 &&
        ntohs(pip->ip_len) == plen && hlen <= plen &&
        plen >= sizeof(struct tcphdr) + hlen)
      MSSFixup((struct tcphdr *)(MBUF_CTOP(bp) + hlen), plen - hlen,
               MAXMSS4(bundle->iface->mtu));
    break;
#ifndef NOINET6
  case IPV6_VERSION >> 4:
    /*
     * Check for MSS option only for TCP packets with no extension headers
     * or a single extension header which is a fragmentation header with
     * offset 0. Furthermore require that the length field is correct.
     */
    if (plen < sizeof(struct ip6_hdr))
      break;
    pip6 = (struct ip6_hdr *)MBUF_CTOP(bp);
    if (ntohs(pip6->ip6_plen) + sizeof(struct ip6_hdr) != plen)
      break;
    hlen = 0;
    switch (pip6->ip6_nxt) {
    case IPPROTO_TCP:
      hlen = sizeof(struct ip6_hdr);
      break;
    case IPPROTO_FRAGMENT:
      if (plen >= sizeof(struct ip6_frag) + sizeof(struct ip6_hdr)) {
        pfrag = (struct ip6_frag *)(MBUF_CTOP(bp) + sizeof(struct ip6_hdr));
        if (pfrag->ip6f_nxt == IPPROTO_TCP &&
            ntohs(pfrag->ip6f_offlg & IP6F_OFF_MASK) == 0)
          hlen = sizeof(struct ip6_hdr)+ sizeof(struct ip6_frag);
      }
      break;
    }
    if (hlen > 0 && plen >= sizeof(struct tcphdr) + hlen)
      MSSFixup((struct tcphdr *)(MBUF_CTOP(bp) + hlen), plen - hlen,
               MAXMSS6(bundle->iface->mtu));
    break;
#endif
  default:
    log_Printf(LogDEBUG, "tcpmss_Check: Unknown IP family %u\n", pip->ip_v);
    break;
  }
  return bp;
}

static struct mbuf *
tcpmss_LayerPush(struct bundle *bundle, struct link *l __unused,
		 struct mbuf *bp, int pri __unused, u_short *proto __unused)
{
	return tcpmss_Check(bundle, bp);
}

static struct mbuf *
tcpmss_LayerPull(struct bundle *bundle, struct link *l __unused,
		 struct mbuf *bp, u_short *proto __unused)
{
	return tcpmss_Check(bundle, bp);
}

struct layer tcpmsslayer =
  { LAYER_PROTO, "tcpmss", tcpmss_LayerPush, tcpmss_LayerPull };
