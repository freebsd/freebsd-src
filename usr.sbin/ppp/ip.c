/*
 *		PPP IP Protocol Interface
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: ip.c,v 1.46 1998/06/27 14:17:26 brian Exp $
 *
 *	TODO:
 *		o Return ICMP message for filterd packet
 *		  and optionaly record it into log.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>

#ifndef NOALIAS
#include <alias.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#include "bundle.h"
#include "vjcomp.h"
#include "tun.h"
#include "ip.h"

static const u_short interactive_ports[32] = {
  544, 513, 514, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 21, 22, 23, 0, 0, 0, 0, 0, 0, 0, 543,
};

#define	INTERACTIVE(p)	(interactive_ports[(p) & 0x1F] == (p))

static const char *TcpFlags[] = { "FIN", "SYN", "RST", "PSH", "ACK", "URG" };

static int
PortMatch(int op, u_short pport, u_short rport)
{
  switch (op) {
    case OP_EQ:
    return (pport == rport);
  case OP_GT:
    return (pport > rport);
  case OP_LT:
    return (pport < rport);
  default:
    return (0);
  }
}

/*
 *  Check a packet against with defined filters
 */
static int
FilterCheck(struct ip *pip, struct filter *filter)
{
  int gotinfo, cproto, estab, syn, finrst, n, len, didname;
  struct tcphdr *th;
  struct udphdr *uh;
  struct icmp *ih;
  char *ptop;
  u_short sport, dport;
  struct filterent *fp = filter->rule;
  char dbuff[100];

  if (fp->action) {
    cproto = gotinfo = estab = syn = finrst = didname = 0;
    sport = dport = 0;
    for (n = 0; n < MAXFILTERS; n++) {
      if (fp->action) {
	/* permit fragments on in and out filter */
        if (filter->fragok && (ntohs(pip->ip_off) & IP_OFFMASK) != 0)
	  return (A_PERMIT);

        if (!didname)
          log_Printf(LogDEBUG, "%s filter:\n", filter->name);
        didname = 1;

	if ((pip->ip_src.s_addr & fp->smask.s_addr) ==
	    (fp->saddr.s_addr & fp->smask.s_addr) &&
	    (pip->ip_dst.s_addr & fp->dmask.s_addr) ==
	    (fp->daddr.s_addr & fp->dmask.s_addr)) {
	  if (fp->proto) {
	    if (!gotinfo) {
	      ptop = (char *) pip + (pip->ip_hl << 2);

	      switch (pip->ip_p) {
	      case IPPROTO_ICMP:
		cproto = P_ICMP;
		ih = (struct icmp *) ptop;
		sport = ih->icmp_type;
		estab = syn = finrst = -1;
                if (log_IsKept(LogDEBUG))
		  snprintf(dbuff, sizeof dbuff, "sport = %d", sport);
		break;
	      case IPPROTO_UDP:
	      case IPPROTO_IGMP:
	      case IPPROTO_IPIP:
		cproto = P_UDP;
		uh = (struct udphdr *) ptop;
		sport = ntohs(uh->uh_sport);
		dport = ntohs(uh->uh_dport);
		estab = syn = finrst = -1;
                if (log_IsKept(LogDEBUG))
		  snprintf(dbuff, sizeof dbuff, "sport = %d, dport = %d",
                           sport, dport);
		break;
	      case IPPROTO_TCP:
		cproto = P_TCP;
		th = (struct tcphdr *) ptop;
		sport = ntohs(th->th_sport);
		dport = ntohs(th->th_dport);
		estab = (th->th_flags & TH_ACK);
		syn = (th->th_flags & TH_SYN);
		finrst = (th->th_flags & (TH_FIN|TH_RST));
                if (log_IsKept(LogDEBUG) && !estab)
		  snprintf(dbuff, sizeof dbuff,
                           "flags = %02x, sport = %d, dport = %d",
                           th->th_flags, sport, dport);
		break;
	      default:
		return (A_DENY);       /* We'll block unknown type of packet */
	      }
              if (log_IsKept(LogDEBUG)) {
                if (estab != -1) {
                  len = strlen(dbuff);
                  snprintf(dbuff + len, sizeof dbuff - len,
                           ", estab = %d, syn = %d, finrst = %d",
                           estab, syn, finrst);
                }
	        log_Printf(LogDEBUG, " Filter: proto = %s, %s\n",
                          filter_Proto2Nam(cproto), dbuff);
              }
	      gotinfo = 1;
	    }
            if (log_IsKept(LogDEBUG)) {
	      if (fp->opt.srcop != OP_NONE) {
                snprintf(dbuff, sizeof dbuff, ", src %s %d",
                         filter_Op2Nam(fp->opt.srcop), fp->opt.srcport);
                len = strlen(dbuff);
              } else
                len = 0;
	      if (fp->opt.dstop != OP_NONE) {
                snprintf(dbuff + len, sizeof dbuff - len,
                         ", dst %s %d", filter_Op2Nam(fp->opt.dstop),
                         fp->opt.dstport);
              } else if (!len)
                *dbuff = '\0';

	      log_Printf(LogDEBUG, "  rule = %d: Address match, "
                        "check against proto %s%s, action = %s\n",
                        n, filter_Proto2Nam(fp->proto),
                        dbuff, filter_Action2Nam(fp->action));
            }

	    if (cproto == fp->proto) {
	      if ((fp->opt.srcop == OP_NONE ||
		   PortMatch(fp->opt.srcop, sport, fp->opt.srcport)) &&
		  (fp->opt.dstop == OP_NONE ||
		   PortMatch(fp->opt.dstop, dport, fp->opt.dstport)) &&
		  (fp->opt.estab == 0 || estab) &&
		  (fp->opt.syn == 0 || syn) &&
		  (fp->opt.finrst == 0 || finrst)) {
		return (fp->action);
	      }
	    }
	  } else {
	    /* Address is mached. Make a decision. */
	    log_Printf(LogDEBUG, "  rule = %d: Address match, action = %s\n", n,
                      filter_Action2Nam(fp->action));
	    return (fp->action);
	  }
	} else
	  log_Printf(LogDEBUG, "  rule = %d: Address mismatch\n", n);
      }
      fp++;
    }
    return (A_DENY);		/* No rule is mached. Deny this packet */
  }
  return (A_PERMIT);		/* No rule is given. Permit this packet */
}

#ifdef notdef
static void
IcmpError(struct ip * pip, int code)
{
  struct mbuf *bp;

  if (pip->ip_p != IPPROTO_ICMP) {
    bp = mbuf_Alloc(cnt, MB_IPIN);
    memcpy(MBUF_CTOP(bp), ptr, cnt);
    vj_SendFrame(bp);
    ipcp_AddOutOctets(cnt);
  }
}
#endif

/*
 *  For debugging aid.
 */
int
PacketCheck(struct bundle *bundle, char *cp, int nb, struct filter *filter)
{
  struct ip *pip;
  struct tcphdr *th;
  struct udphdr *uh;
  struct icmp *icmph;
  char *ptop;
  int mask, len, n;
  int pri = PRI_NORMAL;
  int logit, loglen;
  char logbuf[200];

  logit = log_IsKept(LogTCPIP) && filter->logok;
  loglen = 0;

  pip = (struct ip *) cp;

  if (logit && loglen < sizeof logbuf) {
    snprintf(logbuf + loglen, sizeof logbuf - loglen, "%s ", filter->name);
    loglen += strlen(logbuf + loglen);
  }
  ptop = (cp + (pip->ip_hl << 2));

  switch (pip->ip_p) {
  case IPPROTO_ICMP:
    if (logit && loglen < sizeof logbuf) {
      icmph = (struct icmp *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	     "ICMP: %s:%d ---> ", inet_ntoa(pip->ip_src), icmph->icmp_type);
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "%s:%d", inet_ntoa(pip->ip_dst), icmph->icmp_type);
      loglen += strlen(logbuf + loglen);
    }
    break;
  case IPPROTO_UDP:
    if (logit && loglen < sizeof logbuf) {
      uh = (struct udphdr *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	   "UDP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(uh->uh_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "%s:%d", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport));
      loglen += strlen(logbuf + loglen);
    }
    break;
  case IPPROTO_IPIP:
    if (logit && loglen < sizeof logbuf) {
      uh = (struct udphdr *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	   "IPIP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(uh->uh_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "%s:%d", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport));
      loglen += strlen(logbuf + loglen);
    }
    break;
  case IPPROTO_IGMP:
    if (logit && loglen < sizeof logbuf) {
      uh = (struct udphdr *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	   "IGMP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(uh->uh_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "%s:%d", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport));
      loglen += strlen(logbuf + loglen);
    }
    break;
  case IPPROTO_TCP:
    th = (struct tcphdr *) ptop;
    if (pip->ip_tos == IPTOS_LOWDELAY)
      pri = PRI_FAST;
    else if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0) {
      if (INTERACTIVE(ntohs(th->th_sport)) || INTERACTIVE(ntohs(th->th_dport)))
	pri = PRI_FAST;
    }
    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2) - (th->th_off << 2);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	   "TCP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(th->th_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "%s:%d", inet_ntoa(pip->ip_dst), ntohs(th->th_dport));
      loglen += strlen(logbuf + loglen);
      n = 0;
      for (mask = TH_FIN; mask != 0x40; mask <<= 1) {
	if (th->th_flags & mask) {
	  snprintf(logbuf + loglen, sizeof logbuf - loglen, " %s", TcpFlags[n]);
	  loglen += strlen(logbuf + loglen);
	}
	n++;
      }
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
	       "  seq:%x  ack:%x (%d/%d)",
	       ntohl(th->th_seq), ntohl(th->th_ack), len, nb);
      loglen += strlen(logbuf + loglen);
      if ((th->th_flags & TH_SYN) && nb > 40) {
	u_short *sp;

	ptop += 20;
	sp = (u_short *) ptop;
	if (ntohs(sp[0]) == 0x0204) {
	  snprintf(logbuf + loglen, sizeof logbuf - loglen,
		   " MSS = %d", ntohs(sp[1]));
	  loglen += strlen(logbuf + loglen);
	}
      }
    }
    break;
  }

  if ((FilterCheck(pip, filter) & A_DENY)) {
    if (logit)
      log_Printf(LogTCPIP, "%s - BLOCKED\n", logbuf);
#ifdef notdef
    if (direction == 0)
      IcmpError(pip, pri);
#endif
    return (-1);
  } else {
    /* Check Keep Alive filter */
    if (logit) {
      if (FilterCheck(pip, &bundle->filter.alive) & A_DENY)
        log_Printf(LogTCPIP, "%s - NO KEEPALIVE\n", logbuf);
      else
        log_Printf(LogTCPIP, "%s\n", logbuf);
    }
    return (pri);
  }
}

void
ip_Input(struct bundle *bundle, struct mbuf * bp)
{
  u_char *cp;
  struct mbuf *wp;
  int nb, nw;
  struct tun_data tun;
  struct ip *pip = (struct ip *)tun.data;
  struct ip *piip = (struct ip *)((char *)pip + (pip->ip_hl << 2));

  tun_fill_header(tun, AF_INET);
  cp = tun.data;
  nb = 0;
  for (wp = bp; wp; wp = wp->next) {	/* Copy to contiguous region */
    if (sizeof tun.data - (cp - tun.data) < wp->cnt) {
      log_Printf(LogWARN, "ip_Input: Packet too large (%d) - dropped\n",
                mbuf_Length(bp));
      mbuf_Free(bp);
      return;
    }
    memcpy(cp, MBUF_CTOP(wp), wp->cnt);
    cp += wp->cnt;
    nb += wp->cnt;
  }

#ifndef NOALIAS
  if (bundle->AliasEnabled && pip->ip_p != IPPROTO_IGMP &&
      (pip->ip_p != IPPROTO_IPIP || !IN_CLASSD(ntohl(piip->ip_dst.s_addr)))) {
    struct tun_data *frag;
    int iresult;
    char *fptr;

    iresult = PacketAliasIn(tun.data, sizeof tun.data);
    nb = ntohs(((struct ip *) tun.data)->ip_len);

    if (nb > MAX_MRU) {
      log_Printf(LogWARN, "ip_Input: Problem with IP header length\n");
      mbuf_Free(bp);
      return;
    }
    if (iresult == PKT_ALIAS_OK
	|| iresult == PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
      if (PacketCheck(bundle, tun.data, nb, &bundle->filter.in) < 0) {
	mbuf_Free(bp);
	return;
      }

      if (!(FilterCheck(pip, &bundle->filter.alive) & A_DENY))
        bundle_StartIdleTimer(bundle);

      ipcp_AddInOctets(&bundle->ncp.ipcp, nb);

      nb = ntohs(((struct ip *) tun.data)->ip_len);
      nb += sizeof tun - sizeof tun.data;
      nw = write(bundle->dev.fd, &tun, nb);
      if (nw != nb) {
        if (nw == -1)
	  log_Printf(LogERROR, "ip_Input: wrote %d, got %s\n", nb,
                    strerror(errno));
        else
	  log_Printf(LogERROR, "ip_Input: wrote %d, got %d\n", nb, nw);
      }

      if (iresult == PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
	while ((fptr = PacketAliasGetFragment(tun.data)) != NULL) {
	  PacketAliasFragmentIn(tun.data, fptr);
	  nb = ntohs(((struct ip *) fptr)->ip_len);
          frag = (struct tun_data *)
	    ((char *)fptr - sizeof tun + sizeof tun.data);
          nb += sizeof tun - sizeof tun.data;
	  nw = write(bundle->dev.fd, frag, nb);
	  if (nw != nb) {
            if (nw == -1)
	      log_Printf(LogERROR, "ip_Input: wrote %d, got %s\n", nb,
                        strerror(errno));
            else
	      log_Printf(LogERROR, "ip_Input: wrote %d, got %d\n", nb, nw);
          }
	  free(frag);
	}
      }
    } else if (iresult == PKT_ALIAS_UNRESOLVED_FRAGMENT) {
      nb = ntohs(((struct ip *) tun.data)->ip_len);
      nb += sizeof tun - sizeof tun.data;
      frag = (struct tun_data *)malloc(nb);
      if (frag == NULL)
	log_Printf(LogALERT, "ip_Input: Cannot allocate memory for fragment\n");
      else {
        tun_fill_header(*frag, AF_INET);
	memcpy(frag->data, tun.data, nb - sizeof tun + sizeof tun.data);
	PacketAliasSaveFragment(frag->data);
      }
    }
  } else
#endif /* #ifndef NOALIAS */
  {			/* no aliasing */
    if (PacketCheck(bundle, tun.data, nb, &bundle->filter.in) < 0) {
      mbuf_Free(bp);
      return;
    }

    if (!(FilterCheck(pip, &bundle->filter.alive) & A_DENY))
      bundle_StartIdleTimer(bundle);

    ipcp_AddInOctets(&bundle->ncp.ipcp, nb);

    nb += sizeof tun - sizeof tun.data;
    nw = write(bundle->dev.fd, &tun, nb);
    if (nw != nb) {
      if (nw == -1)
	log_Printf(LogERROR, "ip_Input: wrote %d, got %s\n", nb, strerror(errno));
      else
        log_Printf(LogERROR, "ip_Input: wrote %d, got %d\n", nb, nw);
    }
  }
  mbuf_Free(bp);
}

static struct mqueue IpOutputQueues[PRI_FAST + 1];

void
ip_Enqueue(int pri, char *ptr, int count)
{
  struct mbuf *bp;

  bp = mbuf_Alloc(count, MB_IPQ);
  memcpy(MBUF_CTOP(bp), ptr, count);
  mbuf_Enqueue(&IpOutputQueues[pri], bp);
}

int
ip_QueueLen()
{
  struct mqueue *queue;
  int result = 0;

  for (queue = &IpOutputQueues[PRI_MAX]; queue >= IpOutputQueues; queue--)
    result += queue->qlen;

  return result;
}

int
ip_FlushPacket(struct link *l, struct bundle *bundle)
{
  struct mqueue *queue;
  struct mbuf *bp;
  int cnt;

  if (bundle->ncp.ipcp.fsm.state != ST_OPENED)
    return 0;

  for (queue = &IpOutputQueues[PRI_FAST]; queue >= IpOutputQueues; queue--)
    if (queue->top) {
      bp = mbuf_Dequeue(queue);
      if (bp) {
        struct ip *pip = (struct ip *)MBUF_CTOP(bp);

	cnt = mbuf_Length(bp);
	vj_SendFrame(l, bp, bundle);
        if (!(FilterCheck(pip, &bundle->filter.alive) & A_DENY))
          bundle_StartIdleTimer(bundle);
        ipcp_AddOutOctets(&bundle->ncp.ipcp, cnt);
	return 1;
      }
    }

  return 0;
}
