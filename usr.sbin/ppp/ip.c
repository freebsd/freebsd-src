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
 * $Id: ip.c,v 1.21 1997/06/09 03:27:23 brian Exp $
 *
 *	TODO:
 *		o Return ICMP message for filterd packet
 *		  and optionaly record it into log.
 */
#include "fsm.h"
#include "lcpproto.h"
#include "hdlc.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <alias.h>
#include "loadalias.h"
#include "vars.h"
#include "filter.h"
#include "mbuf.h"
#include "log.h"

extern void SendPppFrame();
extern void LcpClose();

static struct pppTimer IdleTimer;

static void IdleTimeout()
{
  LogPrintf(LogPHASE, "Idle timer expired.\n");
  reconnect(RECON_FALSE);
  LcpClose();
}

/*
 *  Start Idle timer. If timeout is reached, we call LcpClose() to
 *  close LCP and link.
 */
void
StartIdleTimer()
{
  if (!(mode & (MODE_DEDICATED|MODE_DDIAL))) {
    StopTimer(&IdleTimer);
    IdleTimer.func = IdleTimeout;
    IdleTimer.load = VarIdleTimeout * SECTICKS;
    IdleTimer.state = TIMER_STOPPED;
    StartTimer(&IdleTimer);
  }
}

void
UpdateIdleTimer()
{
  if (IdleTimer.state == TIMER_RUNNING)
    StartIdleTimer();
}

void
StopIdleTimer()
{
  StopTimer(&IdleTimer);
}

/*
 *  If any IP layer traffic is detected, refresh IdleTimer.
 */
static void
RestartIdleTimer()
{
  if (!(mode & (MODE_DEDICATED|MODE_DDIAL)) && ipKeepAlive ) {
    StartTimer(&IdleTimer);
    ipIdleSecs = 0;
  }
}

static u_short interactive_ports[32] = {
  544, 513, 514,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,  21,  22,  23,   0,   0,   0,   0,   0,   0,   0, 543,
};

#define	INTERACTIVE(p)	(interactive_ports[(p) & 0x1F] == (p))

static char *TcpFlags[] = {
  "FIN", "SYN", "RST", "PSH", "ACK", "URG",
};

static char *Direction[] = { "INP", "OUT", "OUT", "IN/OUT" };
static struct filterent *Filters[] = { ifilters, ofilters, dfilters, afilters };

static int
PortMatch(op, pport, rport)
int op;
u_short pport, rport;
{
  switch (op) {
  case OP_EQ:
    return(pport == rport);
  case OP_GT:
    return(pport > rport);
  case OP_LT:
    return(pport < rport);
  default:
    return(0);
  }
}

/*
 *  Check a packet against with defined filters
 */
static int
FilterCheck(pip, direction)
struct ip *pip;
int direction;
{
  struct filterent *fp = Filters[direction];
  int gotinfo, cproto, estab, n;
  struct tcphdr *th;
  struct udphdr *uh;
  struct icmp *ih;
  char *ptop;
  u_short sport, dport;

  if (fp->action) {
    cproto = gotinfo = estab = 0;
    sport = dport = 0;
    for (n = 0; n < MAXFILTERS; n++) {
      if (fp->action) {
         /* permit fragments on in and out filter */
         if ((direction == FL_IN || direction == FL_OUT) &&
             (ntohs(pip->ip_off) & IP_OFFMASK) != 0) {
              return(A_PERMIT);
         }
        LogPrintf(LogDEBUG, "rule = %d\n", n);
	if ((pip->ip_src.s_addr & fp->smask.s_addr) == fp->saddr.s_addr
	    && (pip->ip_dst.s_addr & fp->dmask.s_addr) == fp->daddr.s_addr) {
	  if (fp->proto) {
	    if (!gotinfo) {
	      ptop = (char *)pip + (pip->ip_hl << 2);

	      switch (pip->ip_p) {
	      case IPPROTO_ICMP:
		cproto = P_ICMP; ih = (struct icmp *)ptop;
		sport = ih->icmp_type; estab = 1;
		break;
	      case IPPROTO_UDP:
		cproto = P_UDP; uh = (struct udphdr *)ptop;
		sport = ntohs(uh->uh_sport); dport = ntohs(uh->uh_dport);
		estab = 1;
		break;
	      case IPPROTO_TCP:
		cproto = P_TCP; th = (struct tcphdr *)ptop;
		sport = ntohs(th->th_sport); dport = ntohs(th->th_dport);
		estab = (th->th_flags & TH_ACK);
                if (estab == 0)
                  LogPrintf(LogDEBUG, "flag = %02x, sport = %d, dport = %d\n",
                            th->th_flags, sport, dport);
		break;
	      default:
		return(A_DENY);	/* We'll block unknown type of packet */
	      }
	      gotinfo = 1;
              LogPrintf(LogDEBUG, "dir = %d, proto = %d, srcop = %d,"
                       " dstop = %d, estab = %d\n", direction, cproto,
                       fp->opt.srcop, fp->opt.dstop, estab);
	    }

	    LogPrintf(LogDEBUG, "check0: rule = %d, proto = %d, sport = %d,"
                      " dport = %d\n", n, cproto, sport, dport);
	    LogPrintf(LogDEBUG, "check0: action = %d\n", fp->action);

	    if (cproto == fp->proto) {
	      if ((fp->opt.srcop == OP_NONE ||
		  PortMatch(fp->opt.srcop, sport, fp->opt.srcport))
	       &&
		  (fp->opt.dstop == OP_NONE ||
		  PortMatch(fp->opt.dstop, dport, fp->opt.dstport))
	       &&
		  (fp->opt.estab == 0 || estab)) {
		return(fp->action);
	      }
	    }
	  } else {
	    /* Address is mached. Make a decision. */
	    LogPrintf(LogDEBUG, "check1: action = %d\n", fp->action);
	    return(fp->action);
	  }
	}
      }
      fp++;
    }
    return(A_DENY);	/* No rule is mached. Deny this packet */
  }
  return(A_PERMIT);	/* No rule is given. Permit this packet */
}

static void
IcmpError(pip, code)
struct ip *pip;
int code;
{
#ifdef notdef
  struct mbuf *bp;

  if (pip->ip_p != IPPROTO_ICMP) {
    bp = mballoc(cnt, MB_IPIN);
    bcopy(ptr, MBUF_CTOP(bp), cnt);
    SendPppFrame(bp);
    RestartIdleTimer();
    ipOutOctets += cnt;
  }
#endif
}

/*
 *  For debugging aid.
 */
int
PacketCheck(cp, nb, direction)
char *cp;
int nb;
int direction;
{
  struct ip *pip;
  struct tcphdr *th;
  struct udphdr *uh;
  struct icmp *icmph;
  char *ptop;
  int mask, len, n;
  int pri = PRI_NORMAL;
  int logit, loglen;
  static char logbuf[200];

  logit = LogIsKept(LogTCPIP);
  loglen = 0;

  pip = (struct ip *)cp;

  if (logit && loglen < sizeof logbuf) {
    snprintf(logbuf+loglen, sizeof logbuf - loglen, "%s ",
             Direction[direction]);
    loglen += strlen(logbuf+loglen);
  }

  ptop = (cp + (pip->ip_hl << 2));

  switch (pip->ip_p) {
  case IPPROTO_ICMP:
    if (logit && loglen < sizeof logbuf) {
      icmph = (struct icmp *)ptop;
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "ICMP: %s:%d ---> ", inet_ntoa(pip->ip_src), icmph->icmp_type);
      loglen += strlen(logbuf+loglen);
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "%s:%d", inet_ntoa(pip->ip_dst), icmph->icmp_type);
      loglen += strlen(logbuf+loglen);
    }
    break;
  case IPPROTO_UDP:
    if (logit && loglen < sizeof logbuf) {
      uh = (struct udphdr *)ptop;
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "UDP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(uh->uh_sport));
      loglen += strlen(logbuf+loglen);
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "%s:%d", inet_ntoa(pip->ip_dst), ntohs(uh->uh_dport));
      loglen += strlen(logbuf+loglen);
    }
    break;
  case IPPROTO_TCP:
    th = (struct tcphdr *)ptop;
    if (pip->ip_tos == IPTOS_LOWDELAY)
      pri = PRI_FAST;
    else if ((ntohs(pip->ip_off) & IP_OFFMASK) == 0) {
      if (INTERACTIVE(ntohs(th->th_sport)) || INTERACTIVE(ntohs(th->th_dport)))
	 pri = PRI_FAST;
    }

    if (logit && loglen < sizeof logbuf) {
      len = ntohs(pip->ip_len) - (pip->ip_hl << 2) - (th->th_off << 2);
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "TCP: %s:%d ---> ", inet_ntoa(pip->ip_src), ntohs(th->th_sport));
      loglen += strlen(logbuf+loglen);
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "%s:%d", inet_ntoa(pip->ip_dst), ntohs(th->th_dport));
      loglen += strlen(logbuf+loglen);
      n = 0;
      for (mask = TH_FIN; mask != 0x40; mask <<= 1) {
	if (th->th_flags & mask) {
          snprintf(logbuf+loglen, sizeof logbuf - loglen, " %s", TcpFlags[n]);
          loglen += strlen(logbuf+loglen);
        }
	n++;
      }
      snprintf(logbuf+loglen, sizeof logbuf - loglen,
               "  seq:%x  ack:%x (%d/%d)",
               ntohl(th->th_seq), ntohl(th->th_ack), len, nb);
      loglen += strlen(logbuf+loglen);
      if ((th->th_flags & TH_SYN) && nb > 40) {
        u_short *sp;

	ptop += 20;
	sp = (u_short *)ptop;
	if (ntohs(sp[0]) == 0x0204) {
          snprintf(logbuf+loglen, sizeof logbuf - loglen,
	           " MSS = %d", ntohs(sp[1]));
          loglen += strlen(logbuf+loglen);
        }
      }
    }
    break;
  }

  if (logit)
    LogPrintf(LogTCPIP, "%s\n", logbuf);
  
  if ((FilterCheck(pip, direction) & A_DENY)) {
    LogPrintf(LogDEBUG, "blocked.\n");
    if (direction == 0) IcmpError(pip, pri);
    return(-1);
  } else {
    if ( FilterCheck(pip, FL_KEEP ) & A_DENY ) {  /* Check Keep Alive filter */
	ipKeepAlive = FALSE;
    } else {
	ipKeepAlive = TRUE;
    }
    return(pri);
  }
}

void
IpInput(bp)
struct mbuf *bp;		/* IN: Pointer to IP pakcet */
{
  u_char *cp;
  struct mbuf *wp;
  int nb, nw;
  u_char tunbuff[MAX_MRU];

  cp = tunbuff;
  nb = 0;
  for (wp = bp; wp; wp = wp->next) {		/* Copy to contiguous region */
    bcopy(MBUF_CTOP(wp), cp, wp->cnt);
    cp += wp->cnt;
    nb += wp->cnt;
  }

  if (mode & MODE_ALIAS) {
    int iresult;
    char *fptr;

    iresult = VarPacketAliasIn(tunbuff, sizeof tunbuff);
    nb = ntohs(((struct ip *) tunbuff)->ip_len);

    if (nb > MAX_MRU) {
      LogPrintf(LogERROR, "IpInput: Problem with IP header length\n");
      pfree(bp);
      return;
    }

    if (iresult == PKT_ALIAS_OK
     || iresult == PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
      if ( PacketCheck(tunbuff, nb, FL_IN ) < 0) {
          pfree(bp);
          return;
      }

      ipInOctets += nb;

      nb = ntohs(((struct ip *) tunbuff)->ip_len);
      nw = write(tun_out, tunbuff, nb);
      if (nw != nb)
        LogPrintf(LogERROR, "IpInput: wrote %d, got %d\n", nb, nw);

      if (iresult == PKT_ALIAS_FOUND_HEADER_FRAGMENT) {
        while ((fptr = VarGetNextFragmentPtr(tunbuff)) != NULL) {
          VarFragmentAliasIn(tunbuff, fptr);
          nb = ntohs(((struct ip *) fptr)->ip_len);
          nw = write(tun_out, fptr, nb);
          if (nw != nb)
            LogPrintf(LogERROR, "IpInput: wrote %d, got %d\n", nb, nw);
          free(fptr);
        }
      }
    }
    else if (iresult == PKT_ALIAS_UNRESOLVED_FRAGMENT) {
      nb = ntohs(((struct ip *) tunbuff)->ip_len);
      fptr = malloc(nb);
      if (fptr == NULL)
        LogPrintf(LogALERT, "IpInput: Cannot allocate memory for fragment\n");
      else {
        memcpy(fptr, tunbuff, nb);
        VarSaveFragmentPtr(fptr);
      }
    }
  }
  else
  { /* no aliasing */
    if ( PacketCheck(tunbuff, nb, FL_IN ) < 0)
    {
      pfree(bp);
      return;
    }

    ipInOctets += nb;
    nw = write(tun_out, tunbuff, nb);
    if (nw != nb)
      LogPrintf(LogERROR, "IpInput: wrote %d, got %d\n", nb, nw);
  }
  pfree(bp);

  RestartIdleTimer();
}

static struct mqueue IpOutputQueues[PRI_FAST+1];

void
IpEnqueue(pri, ptr, count)
int pri;
char *ptr;
int count;
{
  struct mbuf *bp;

  bp = mballoc(count, MB_IPQ);
  bcopy(ptr, MBUF_CTOP(bp), count);
  Enqueue(&IpOutputQueues[pri], bp);
}

int
IsIpEnqueued()
{
  struct mqueue *queue;
  int    exist = FALSE;
  for (queue = &IpOutputQueues[PRI_FAST]; queue >= IpOutputQueues; queue--) {
     if ( queue->qlen > 0 ) {
       exist = TRUE;
       break;
     }
  }
  return( exist );
}

void
IpStartOutput()
{
  struct mqueue *queue;
  struct mbuf *bp;
  int cnt;

  if (IpcpFsm.state != ST_OPENED)
    return;
  for (queue = &IpOutputQueues[PRI_FAST]; queue >= IpOutputQueues; queue--) {
    if (queue->top) {
      bp = Dequeue(queue);
      if (bp) {
	cnt = plength(bp);
	SendPppFrame(bp);
	RestartIdleTimer();
	ipOutOctets += cnt;
	break;
       }
    }
  }
}
