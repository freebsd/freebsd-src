/*-
 * Copyright (c) 1996 - 2001 Brian Somers <brian@Awfulhak.org>
 *          based on work by Toshiharu OHNO <tony-o@iij.ad.jp>
 *                           Internet Initiative Japan, Inc (IIJ)
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "proto.h"
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
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "tun.h"
#include "ip.h"


#define OPCODE_QUERY	0
#define OPCODE_IQUERY	1
#define OPCODE_STATUS	2

struct dns_header {
  u_short id;
  unsigned qr : 1;
  unsigned opcode : 4;
  unsigned aa : 1;
  unsigned tc : 1;
  unsigned rd : 1;
  unsigned ra : 1;
  unsigned z : 3;
  unsigned rcode : 4;
  u_short qdcount;
  u_short ancount;
  u_short nscount;
  u_short arcount;
};

static const char *
dns_Qclass2Txt(u_short qclass)
{
  static char failure[6];
  struct {
    u_short id;
    const char *txt;
  } qtxt[] = {
    /* rfc1035 */
    { 1, "IN" }, { 2, "CS" }, { 3, "CH" }, { 4, "HS" }, { 255, "*" }
  };
  int f;

  for (f = 0; f < sizeof qtxt / sizeof *qtxt; f++)
    if (qtxt[f].id == qclass)
      return qtxt[f].txt;

  return HexStr(qclass, failure, sizeof failure);
}

static const char *
dns_Qtype2Txt(u_short qtype)
{
  static char failure[6];
  struct {
    u_short id;
    const char *txt;
  } qtxt[] = {
    /* rfc1035/rfc1700 */
    { 1, "A" }, { 2, "NS" }, { 3, "MD" }, { 4, "MF" }, { 5, "CNAME" },
    { 6, "SOA" }, { 7, "MB" }, { 8, "MG" }, { 9, "MR" }, { 10, "NULL" },
    { 11, "WKS" }, { 12, "PTR" }, { 13, "HINFO" }, { 14, "MINFO" },
    { 15, "MX" }, { 16, "TXT" }, { 17, "RP" }, { 18, "AFSDB" },
    { 19, "X25" }, { 20, "ISDN" }, { 21, "RT" }, { 22, "NSAP" },
    { 23, "NSAP-PTR" }, { 24, "SIG" }, { 25, "KEY" }, { 26, "PX" },
    { 27, "GPOS" }, { 28, "AAAA" }, { 252, "AXFR" }, { 253, "MAILB" },
    { 254, "MAILA" }, { 255, "*" }
  };
  int f;

  for (f = 0; f < sizeof qtxt / sizeof *qtxt; f++)
    if (qtxt[f].id == qtype)
      return qtxt[f].txt;

  return HexStr(qtype, failure, sizeof failure);
}

static __inline int
PortMatch(int op, u_short pport, u_short rport)
{
  switch (op) {
  case OP_EQ:
    return pport == rport;
  case OP_GT:
    return pport > rport;
  case OP_LT:
    return pport < rport;
  default:
    return 0;
  }
}

/*
 *  Check a packet against a defined filter
 *  Returns 0 to accept the packet, non-zero to drop the packet
 *
 *  If filtering is enabled, the initial fragment of a datagram must
 *  contain the complete protocol header, and subsequent fragments
 *  must not attempt to over-write it.
 */
static int
FilterCheck(const struct ip *pip, const struct filter *filter, unsigned *psecs)
{
  int gotinfo;			/* true if IP payload decoded */
  int cproto;			/* P_* protocol type if (gotinfo) */
  int estab, syn, finrst;	/* TCP state flags if (gotinfo) */
  u_short sport, dport;		/* src, dest port from packet if (gotinfo) */
  int n;			/* filter rule to process */
  int len;			/* bytes used in dbuff */
  int didname;			/* true if filter header printed */
  int match;			/* true if condition matched */
  const struct filterent *fp = filter->rule;
  char dbuff[100], dstip[16];

  if (fp->f_action == A_NONE)
    return 0;		/* No rule is given. Permit this packet */

  /*
   * Deny any packet fragment that tries to over-write the header.
   * Since we no longer have the real header available, punt on the
   * largest normal header - 20 bytes for TCP without options, rounded
   * up to the next possible fragment boundary.  Since the smallest
   * `legal' MTU is 576, and the smallest recommended MTU is 296, any
   * fragmentation within this range is dubious at best
   */
  len = ntohs(pip->ip_off) & IP_OFFMASK;	/* fragment offset */ 
  if (len > 0) {		/* Not first fragment within datagram */
    if (len < (24 >> 3)) {	/* don't allow fragment to over-write header */
      log_Printf(LogFILTER, " error: illegal header\n");
      return 1;
    }
    /* permit fragments on in and out filter */
    if (!filter->fragok) {
      log_Printf(LogFILTER, " error: illegal fragmentation\n");
      return 1;
    } else
      return 0;
  }
  
  cproto = gotinfo = estab = syn = finrst = didname = 0;
  sport = dport = 0;
  for (n = 0; n < MAXFILTERS; ) {
    if (fp->f_action == A_NONE) {
      n++;
      fp++;
      continue;
    }

    if (!didname) {
      log_Printf(LogDEBUG, "%s filter:\n", filter->name);
      didname = 1;
    }

    match = 0;
    if (!((pip->ip_src.s_addr ^ fp->f_src.ipaddr.s_addr) &
          fp->f_src.mask.s_addr) &&
        !((pip->ip_dst.s_addr ^ fp->f_dst.ipaddr.s_addr) &
          fp->f_dst.mask.s_addr)) {
      if (fp->f_proto != P_NONE) {
        if (!gotinfo) {
          const char *ptop = (const char *) pip + (pip->ip_hl << 2);
          const struct tcphdr *th;
          const struct udphdr *uh;
          const struct icmp *ih;
          int datalen;	/* IP datagram length */

          datalen = ntohs(pip->ip_len) - (pip->ip_hl << 2);
          switch (pip->ip_p) {
          case IPPROTO_ICMP:
            cproto = P_ICMP;
            if (datalen < 8) {	/* ICMP must be at least 8 octets */
              log_Printf(LogFILTER, " error: ICMP must be at least 8 octets\n");
              return 1;
            }

            ih = (const struct icmp *) ptop;
            sport = ih->icmp_type;
            estab = syn = finrst = -1;
            if (log_IsKept(LogDEBUG))
              snprintf(dbuff, sizeof dbuff, "sport = %d", sport);
            break;
          case IPPROTO_IGMP:
            cproto = P_IGMP;
            if (datalen < 8) {	/* IGMP uses 8-octet messages */
              log_Printf(LogFILTER, " error: IGMP must be at least 8 octets\n");
              return 1;
            }
            estab = syn = finrst = -1;
            sport = ntohs(0);
            break;
#ifdef IPPROTO_GRE
          case IPPROTO_GRE:
            cproto = P_GRE;
            if (datalen < 2) {    /* GRE uses 2-octet+ messages */
              log_Printf(LogFILTER, " error: GRE must be at least 2 octets\n");
              return 1;
            }
            estab = syn = finrst = -1;
            sport = ntohs(0);
            break;
#endif
#ifdef IPPROTO_OSPFIGP
          case IPPROTO_OSPFIGP:
            cproto = P_OSPF;
            if (datalen < 8) {	/* IGMP uses 8-octet messages */
              log_Printf(LogFILTER, " error: IGMP must be at least 8 octets\n");
              return 1;
            }
            estab = syn = finrst = -1;
            sport = ntohs(0);
            break;
#endif
          case IPPROTO_ESP:
            cproto = P_ESP;
            estab = syn = finrst = -1;
            sport = ntohs(0);
            break;
          case IPPROTO_AH:
            cproto = P_AH;
            estab = syn = finrst = -1;
            sport = ntohs(0);
            break;
          case IPPROTO_IPIP:
            cproto = P_IPIP;
            sport = dport = 0;
            estab = syn = finrst = -1;
            break;
          case IPPROTO_UDP:
            cproto = P_UDP;
            if (datalen < 8) {	/* UDP header is 8 octets */
              log_Printf(LogFILTER, " error: UDP/IPIP"
                         " must be at least 8 octets\n");
              return 1;
            }

            uh = (const struct udphdr *) ptop;
            sport = ntohs(uh->uh_sport);
            dport = ntohs(uh->uh_dport);
            estab = syn = finrst = -1;
            if (log_IsKept(LogDEBUG))
              snprintf(dbuff, sizeof dbuff, "sport = %d, dport = %d",
                       sport, dport);
            break;
          case IPPROTO_TCP:
            cproto = P_TCP;
            th = (const struct tcphdr *) ptop;
            /* TCP headers are variable length.  The following code
             * ensures that the TCP header length isn't de-referenced if
             * the datagram is too short
             */
            if (datalen < 20 || datalen < (th->th_off << 2)) {
              log_Printf(LogFILTER, " error: TCP header incorrect\n");
              return 1;
            }
            sport = ntohs(th->th_sport);
            dport = ntohs(th->th_dport);
            estab = (th->th_flags & TH_ACK);
            syn = (th->th_flags & TH_SYN);
            finrst = (th->th_flags & (TH_FIN|TH_RST));
            if (log_IsKept(LogDEBUG)) {
              if (!estab)
                snprintf(dbuff, sizeof dbuff,
                         "flags = %02x, sport = %d, dport = %d",
                         th->th_flags, sport, dport);
              else
                *dbuff = '\0';
            }
            break;
          default:
            log_Printf(LogFILTER, " error: unknown protocol\n");
            return 1;		/* We'll block unknown type of packet */
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
          if (fp->f_srcop != OP_NONE) {
            snprintf(dbuff, sizeof dbuff, ", src %s %d",
                     filter_Op2Nam(fp->f_srcop), fp->f_srcport);
            len = strlen(dbuff);
          } else
            len = 0;
          if (fp->f_dstop != OP_NONE) {
            snprintf(dbuff + len, sizeof dbuff - len,
                     ", dst %s %d", filter_Op2Nam(fp->f_dstop),
                     fp->f_dstport);
          } else if (!len)
            *dbuff = '\0';

          log_Printf(LogDEBUG, "  rule = %d: Address match, "
                     "check against proto %s%s, action = %s\n",
                     n, filter_Proto2Nam(fp->f_proto),
                     dbuff, filter_Action2Nam(fp->f_action));
        }

        if (cproto == fp->f_proto) {
          if ((fp->f_srcop == OP_NONE ||
               PortMatch(fp->f_srcop, sport, fp->f_srcport)) &&
              (fp->f_dstop == OP_NONE ||
               PortMatch(fp->f_dstop, dport, fp->f_dstport)) &&
              (fp->f_estab == 0 || estab) &&
              (fp->f_syn == 0 || syn) &&
              (fp->f_finrst == 0 || finrst)) {
            match = 1;
          }
        }
      } else {
        /* Address is matched and no protocol specified. Make a decision. */
        log_Printf(LogDEBUG, "  rule = %d: Address match, action = %s\n", n,
                   filter_Action2Nam(fp->f_action));
        match = 1;
      }
    } else
      log_Printf(LogDEBUG, "  rule = %d: Address mismatch\n", n);

    if (match != fp->f_invert) {
      /* Take specified action */
      if (fp->f_action < A_NONE)
        fp = &filter->rule[n = fp->f_action];
      else {
        if (fp->f_action == A_PERMIT) {
          if (psecs != NULL)
            *psecs = fp->timeout;
          if (strcmp(filter->name, "DIAL") == 0) {
            /* If dial filter then even print out accept packets */
            if (log_IsKept(LogFILTER)) {
              snprintf(dstip, sizeof dstip, "%s", inet_ntoa(pip->ip_dst));
              log_Printf(LogFILTER, "%sbound rule = %d accept %s "
                         "src = %s/%d dst = %s/%d\n", 
                         filter->name, n, filter_Proto2Nam(cproto),
                         inet_ntoa(pip->ip_src), sport, dstip, dport);
            }
          }
          return 0;
        } else {
          if (log_IsKept(LogFILTER)) {
            snprintf(dstip, sizeof dstip, "%s", inet_ntoa(pip->ip_dst));
            log_Printf(LogFILTER, 
                       "%sbound rule = %d deny %s src = %s/%d dst = %s/%d\n", 
                       filter->name, n, filter_Proto2Nam(cproto),
                       inet_ntoa(pip->ip_src), sport, dstip, dport);
          }
          return 1;	
        }		/* Explict math.  Deny this packet */
      }
    } else {
      n++;
      fp++;
    }
  }

  if (log_IsKept(LogFILTER)) {
    snprintf(dstip, sizeof dstip, "%s", inet_ntoa(pip->ip_dst));
    log_Printf(LogFILTER, 
               "%sbound rule = implicit deny %s src = %s/%d dst = %s/%d\n", 
               filter->name, filter_Proto2Nam(cproto), 
               inet_ntoa(pip->ip_src), sport, dstip, dport);
  }

  return 1;		/* No rule is mached. Deny this packet */
}

#ifdef notdef
static void
IcmpError(struct ip *pip, int code)
{
  struct mbuf *bp;

  if (pip->ip_p != IPPROTO_ICMP) {
    bp = m_get(m_len, MB_IPIN);
    memcpy(MBUF_CTOP(bp), ptr, m_len);
    vj_SendFrame(bp);
    ipcp_AddOutOctets(m_len);
  }
}
#endif

static void
ip_LogDNS(const struct udphdr *uh, const char *direction)
{
  struct dns_header header;
  const u_short *pktptr;
  const u_char *ptr;
  u_short *hptr, tmp;
  int len;

  ptr = (const char *)uh + sizeof *uh;
  len = ntohs(uh->uh_ulen) - sizeof *uh;
  if (len < sizeof header + 5)		/* rfc1024 */
    return;

  pktptr = (const u_short *)ptr;
  hptr = (u_short *)&header;
  ptr += sizeof header;
  len -= sizeof header;

  while (pktptr < (const u_short *)ptr) {
    *hptr++ = ntohs(*pktptr);		/* Careful of macro side-effects ! */
    pktptr++;
  }

  if (header.opcode == OPCODE_QUERY && header.qr == 0) {
    /* rfc1035 */
    char namewithdot[MAXHOSTNAMELEN + 1], *n;
    const char *qtype, *qclass;
    const u_char *end;

    n = namewithdot;
    end = ptr + len - 4;
    if (end - ptr >= sizeof namewithdot)
      end = ptr + sizeof namewithdot - 1;
    while (ptr < end) {
      len = *ptr++;
      if (len > end - ptr)
        len = end - ptr;
      if (n != namewithdot)
        *n++ = '.';
      memcpy(n, ptr, len);
      ptr += len;
      n += len;
    }
    *n = '\0';

    if (log_IsKept(LogDNS)) {
      memcpy(&tmp, end, sizeof tmp);
      qtype = dns_Qtype2Txt(ntohs(tmp));
      memcpy(&tmp, end + 2, sizeof tmp);
      qclass = dns_Qclass2Txt(ntohs(tmp));

      log_Printf(LogDNS, "%sbound query %s %s %s\n",
                 direction, qclass, qtype, namewithdot);
    }
  }
}

/*
 *  For debugging aid.
 */
int
PacketCheck(struct bundle *bundle, unsigned char *cp, int nb,
            struct filter *filter, const char *prefix, unsigned *psecs)
{
  static const char *const TcpFlags[] = {
    "FIN", "SYN", "RST", "PSH", "ACK", "URG"
  };
  struct ip *pip;
  struct tcphdr *th;
  struct udphdr *uh;
  struct icmp *icmph;
  unsigned char *ptop;
  int mask, len, n, pri, logit, loglen, result;
  char logbuf[200];

  logit = (log_IsKept(LogTCPIP) || log_IsKept(LogDNS)) &&
          (!filter || filter->logok);
  loglen = 0;
  pri = 0;

  pip = (struct ip *)cp;
  uh = NULL;

  if (logit && loglen < sizeof logbuf) {
    if (prefix)
      snprintf(logbuf + loglen, sizeof logbuf - loglen, "%s", prefix);
    else if (filter)
      snprintf(logbuf + loglen, sizeof logbuf - loglen, "%s ", filter->name);
    else
      snprintf(logbuf + loglen, sizeof logbuf - loglen, "  ");
    loglen += strlen(logbuf + loglen);
  }
  ptop = (cp + (pip->ip_hl << 2));

  switch (pip->ip_p) {
  case IPPROTO_ICMP:
    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2) - sizeof *icmph;
      icmph = (struct icmp *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "ICMP: %s:%d ---> ", inet_ntoa(pip->ip_src), icmph->icmp_type);
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "%s:%d (%d/%d)", inet_ntoa(pip->ip_dst), icmph->icmp_type,
               len, nb);
      loglen += strlen(logbuf + loglen);
    }
    break;

  case IPPROTO_UDP:
    uh = (struct udphdr *) ptop;
    if (pip->ip_tos == IPTOS_LOWDELAY && bundle->ncp.ipcp.cfg.urgent.tos)
      pri++;

    if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0 &&
        ipcp_IsUrgentUdpPort(&bundle->ncp.ipcp, ntohs(uh->uh_sport),
                          ntohs(uh->uh_dport)))
      pri++;

    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2) - sizeof *uh;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "UDP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(uh->uh_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "%s:%d (%d/%d)", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport),
               len, nb);
      loglen += strlen(logbuf + loglen);
    }

    if (Enabled(bundle, OPT_FILTERDECAP) &&
        ptop[sizeof *uh] == HDLC_ADDR && ptop[sizeof *uh + 1] == HDLC_UI) {
      u_short proto;
      const char *type;

      memcpy(&proto, ptop + sizeof *uh + 2, sizeof proto);
      type = NULL;

      switch (ntohs(proto)) {
        case PROTO_IP:
          snprintf(logbuf + loglen, sizeof logbuf - loglen, " contains ");
          result = PacketCheck(bundle, ptop + sizeof *uh + 4,
                               nb - (ptop - cp) - sizeof *uh - 4, filter,
                               logbuf, psecs);
          if (result != -2)
              return result;
          type = "IP";
          break;

        case PROTO_VJUNCOMP: type = "compressed VJ";   break;
        case PROTO_VJCOMP:   type = "uncompressed VJ"; break;
        case PROTO_MP:       type = "Multi-link"; break;
        case PROTO_ICOMPD:   type = "Individual link CCP"; break;
        case PROTO_COMPD:    type = "CCP"; break;
        case PROTO_IPCP:     type = "IPCP"; break;
        case PROTO_LCP:      type = "LCP"; break;
        case PROTO_PAP:      type = "PAP"; break;
        case PROTO_CBCP:     type = "CBCP"; break;
        case PROTO_LQR:      type = "LQR"; break;
        case PROTO_CHAP:     type = "CHAP"; break;
      }
      if (type) {
        snprintf(logbuf + loglen, sizeof logbuf - loglen,
                 " - %s data", type);
        loglen += strlen(logbuf + loglen);
      }
    }

    break;

#ifdef IPPROTO_GRE
  case IPPROTO_GRE:
    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
          "GRE: %s ---> ", inet_ntoa(pip->ip_src));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
              "%s (%d/%d)", inet_ntoa(pip->ip_dst), len, nb);
      loglen += strlen(logbuf + loglen);
    }
    break;
#endif

#ifdef IPPROTO_OSPFIGP
  case IPPROTO_OSPFIGP:
    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "OSPF: %s ---> ", inet_ntoa(pip->ip_src));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "%s (%d/%d)", inet_ntoa(pip->ip_dst), len, nb);
      loglen += strlen(logbuf + loglen);
    }
    break;
#endif

  case IPPROTO_IPIP:
    if (logit && loglen < sizeof logbuf) {
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "IPIP: %s ---> ", inet_ntoa(pip->ip_src));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "%s", inet_ntoa(pip->ip_dst));
      loglen += strlen(logbuf + loglen);

      if (((struct ip *)ptop)->ip_v == 4) {
        snprintf(logbuf + loglen, sizeof logbuf - loglen, " contains ");
        result = PacketCheck(bundle, ptop, nb - (ptop - cp), filter,
                             logbuf, psecs);
        if (result != -2)
          return result;
      }
    }
    break;

  case IPPROTO_ESP:
    if (logit && loglen < sizeof logbuf) {
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "ESP: %s ---> ", inet_ntoa(pip->ip_src));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen, "%s, spi %p",
               inet_ntoa(pip->ip_dst), ptop);
      loglen += strlen(logbuf + loglen);
    }
    break;

  case IPPROTO_AH:
    if (logit && loglen < sizeof logbuf) {
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "AH: %s ---> ", inet_ntoa(pip->ip_src));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen, "%s, spi %p",
               inet_ntoa(pip->ip_dst), ptop + sizeof(u_int32_t));
      loglen += strlen(logbuf + loglen);
    }
    break;

  case IPPROTO_IGMP:
    if (logit && loglen < sizeof logbuf) {
      uh = (struct udphdr *) ptop;
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "IGMP: %s:%d ---> ", inet_ntoa(pip->ip_src),
               ntohs(uh->uh_sport));
      loglen += strlen(logbuf + loglen);
      snprintf(logbuf + loglen, sizeof logbuf - loglen,
               "%s:%d", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport));
      loglen += strlen(logbuf + loglen);
    }
    break;

  case IPPROTO_TCP:
    th = (struct tcphdr *) ptop;
    if (pip->ip_tos == IPTOS_LOWDELAY && bundle->ncp.ipcp.cfg.urgent.tos)
      pri++;

    if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0 &&
        ipcp_IsUrgentTcpPort(&bundle->ncp.ipcp, ntohs(th->th_sport),
                          ntohs(th->th_dport)))
      pri++;

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
               "  seq:%lx  ack:%lx (%d/%d)",
               (u_long)ntohl(th->th_seq), (u_long)ntohl(th->th_ack), len, nb);
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

  default:
    if (prefix)
      return -2;
  }

  if (filter && FilterCheck(pip, filter, psecs)) {
    if (logit)
      log_Printf(LogTCPIP, "%s - BLOCKED\n", logbuf);
#ifdef notdef
    if (direction == 0)
      IcmpError(pip, pri);
#endif
    result = -1;
  } else {
    /* Check Keep Alive filter */
    if (logit && log_IsKept(LogTCPIP)) {
      unsigned alivesecs;

      alivesecs = 0;
      if (filter && FilterCheck(pip, &bundle->filter.alive, &alivesecs))
        log_Printf(LogTCPIP, "%s - NO KEEPALIVE\n", logbuf);
      else if (psecs != NULL) {
        if(*psecs == 0)
          *psecs = alivesecs;
        if (*psecs) {
          if (*psecs != alivesecs)
            log_Printf(LogTCPIP, "%s - (timeout = %d / ALIVE = %d secs)\n", 
                       logbuf, *psecs, alivesecs);
          else
            log_Printf(LogTCPIP, "%s - (timeout = %d secs)\n", logbuf, *psecs);
        } else
          log_Printf(LogTCPIP, "%s\n", logbuf);
      }
    }
    result = pri;
  }

  if (filter && uh && ntohs(uh->uh_dport) == 53 && log_IsKept(LogDNS))
    ip_LogDNS(uh, filter->name);

  return result;
}

struct mbuf *
ip_Input(struct bundle *bundle, struct link *l, struct mbuf *bp)
{
  int nb, nw;
  struct tun_data tun;
  struct ip *pip;
  char *data;
  unsigned secs, alivesecs;

  if (bundle->ncp.ipcp.fsm.state != ST_OPENED) {
    log_Printf(LogWARN, "ip_Input: IPCP not open - packet dropped\n");
    m_freem(bp);
    return NULL;
  }

  m_settype(bp, MB_IPIN);
  nb = m_length(bp);
  if (nb > sizeof tun.data) {
    log_Printf(LogWARN, "ip_Input: %s: Packet too large (got %d, max %d)\n",
               l->name, nb, (int)(sizeof tun.data));
    m_freem(bp);
    return NULL;
  }
  mbuf_Read(bp, tun.data, nb);

  secs = 0;
  if (PacketCheck(bundle, tun.data, nb, &bundle->filter.in, NULL, &secs) < 0)
    return NULL;

  pip = (struct ip *)tun.data;
  alivesecs = 0;
  if (!FilterCheck(pip, &bundle->filter.alive, &alivesecs)) {
    if (secs == 0)
      secs = alivesecs;
    bundle_StartIdleTimer(bundle, secs);
  }

  ipcp_AddInOctets(&bundle->ncp.ipcp, nb);

  if (bundle->dev.header) {
    tun.header.family = htonl(AF_INET);
    nb += sizeof tun - sizeof tun.data;
    data = (char *)&tun;
  } else
    data = tun.data;

  nw = write(bundle->dev.fd, data, nb);
  if (nw != nb) {
    if (nw == -1)
      log_Printf(LogERROR, "ip_Input: %s: wrote %d, got %s\n",
                 l->name, nb, strerror(errno));
    else
      log_Printf(LogERROR, "ip_Input: %s: wrote %d, got %d\n", l->name, nb, nw);
  }

  return NULL;
}

void
ip_Enqueue(struct ipcp *ipcp, int pri, char *ptr, int count)
{
  struct mbuf *bp;

  if (pri < 0 || pri >= IPCP_QUEUES(ipcp))
    log_Printf(LogERROR, "Can't store in ip queue %d\n", pri);
  else {
    /*
     * We allocate an extra 6 bytes, four at the front and two at the end.
     * This is an optimisation so that we need to do less work in
     * m_prepend() in acf_LayerPush() and proto_LayerPush() and
     * appending in hdlc_LayerPush().
     */
    bp = m_get(count + 6, MB_IPOUT);
    bp->m_offset += 4;
    bp->m_len -= 6;
    memcpy(MBUF_CTOP(bp), ptr, count);
    m_enqueue(ipcp->Queue + pri, bp);
  }
}

void
ip_DeleteQueue(struct ipcp *ipcp)
{
  struct mqueue *queue;

  for (queue = ipcp->Queue; queue < ipcp->Queue + IPCP_QUEUES(ipcp); queue++)
    while (queue->top)
      m_freem(m_dequeue(queue));
}

size_t
ip_QueueLen(struct ipcp *ipcp)
{
  struct mqueue *queue;
  size_t result;

  result = 0;
  for (queue = ipcp->Queue; queue < ipcp->Queue + IPCP_QUEUES(ipcp); queue++)
    result += queue->len;

  return result;
}

int
ip_PushPacket(struct link *l, struct bundle *bundle)
{
  struct ipcp *ipcp = &bundle->ncp.ipcp;
  struct mqueue *queue;
  struct mbuf *bp;
  struct ip *pip;
  int m_len;
  u_int32_t secs = 0;
  unsigned alivesecs = 0;

  if (ipcp->fsm.state != ST_OPENED)
    return 0;

  /*
   * If ccp is not open but is required, do nothing.
   */
  if (l->ccp.fsm.state != ST_OPENED && ccp_Required(&l->ccp)) {
    log_Printf(LogPHASE, "%s: Not transmitting... waiting for CCP\n", l->name);
    return 0;
  }

  queue = ipcp->Queue + IPCP_QUEUES(ipcp) - 1;
  do {
    if (queue->top) {
      bp = m_dequeue(queue);
      bp = mbuf_Read(bp, &secs, sizeof secs);
      bp = m_pullup(bp);
      m_len = m_length(bp);
      pip = (struct ip *)MBUF_CTOP(bp);
      if (!FilterCheck(pip, &bundle->filter.alive, &alivesecs)) {
        if (secs == 0)
          secs = alivesecs;
        bundle_StartIdleTimer(bundle, secs);
      }
      link_PushPacket(l, bp, bundle, 0, PROTO_IP);
      ipcp_AddOutOctets(ipcp, m_len);
      return 1;
    }
  } while (queue-- != ipcp->Queue);

  return 0;
}
