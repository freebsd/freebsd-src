/*
 * Routines to compress and uncompess tcp packets (for transmission
 * over low speed serial lines.
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: slcompress.c,v 1.14 1997/11/22 03:37:50 brian Exp $
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 */

#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <string.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "slcompress.h"
#include "loadalias.h"
#include "vars.h"

static struct slstat {
  int sls_packets;		/* outbound packets */
  int sls_compressed;		/* outbound compressed packets */
  int sls_searches;		/* searches for connection state */
  int sls_misses;		/* times couldn't find conn. state */
  int sls_uncompressedin;	/* inbound uncompressed packets */
  int sls_compressedin;		/* inbound compressed packets */
  int sls_errorin;		/* inbound unknown type packets */
  int sls_tossed;		/* inbound packets tossed because of error */
} slstat;

#define INCR(counter)	slstat.counter++;

void
sl_compress_init(struct slcompress * comp, int max_state)
{
  register u_int i;
  register struct cstate *tstate = comp->tstate;

  memset(comp, '\0', sizeof *comp);
  for (i = max_state; i > 0; --i) {
    tstate[i].cs_id = i;
    tstate[i].cs_next = &tstate[i - 1];
  }
  tstate[0].cs_next = &tstate[max_state];
  tstate[0].cs_id = 0;
  comp->last_cs = &tstate[0];
  comp->last_recv = 255;
  comp->last_xmit = 255;
  comp->flags = SLF_TOSS;
}


/* ENCODE encodes a number that is known to be non-zero.  ENCODEZ
 * checks for zero (since zero has to be encoded in the long, 3 byte
 * form).
 */
#define ENCODE(n) { \
	if ((u_short)(n) >= 256) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}
#define ENCODEZ(n) { \
	if ((u_short)(n) >= 256 || (u_short)(n) == 0) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}

#define DECODEL(f) { \
	if (*cp == 0) {\
		(f) = htonl(ntohl(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htonl(ntohl(f) + (u_long)*cp++); \
	} \
}

#define DECODES(f) { \
	if (*cp == 0) {\
		(f) = htons(ntohs(f) + ((cp[1] << 8) | cp[2])); \
		cp += 3; \
	} else { \
		(f) = htons(ntohs(f) + (u_long)*cp++); \
	} \
}

#define DECODEU(f) { \
	if (*cp == 0) {\
		(f) = htons((cp[1] << 8) | cp[2]); \
		cp += 3; \
	} else { \
		(f) = htons((u_long)*cp++); \
	} \
}


u_char
sl_compress_tcp(struct mbuf * m,
		struct ip * ip,
		struct slcompress * comp,
		int compress_cid)
{
  register struct cstate *cs = comp->last_cs->cs_next;
  register u_int hlen = ip->ip_hl;
  register struct tcphdr *oth;
  register struct tcphdr *th;
  register u_int deltaS, deltaA;
  register u_int changes = 0;
  u_char new_seq[16];
  register u_char *cp = new_seq;

  /*
   * Bail if this is an IP fragment or if the TCP packet isn't `compressible'
   * (i.e., ACK isn't set or some other control bit is set).  (We assume that
   * the caller has already made sure the packet is IP proto TCP).
   */
  if ((ip->ip_off & htons(0x3fff)) || m->cnt < 40) {
    LogPrintf(LogDEBUG, "??? 1 ip_off = %x, cnt = %d\n",
	      ip->ip_off, m->cnt);
    LogDumpBp(LogDEBUG, "", m);
    return (TYPE_IP);
  }
  th = (struct tcphdr *) & ((int *) ip)[hlen];
  if ((th->th_flags & (TH_SYN | TH_FIN | TH_RST | TH_ACK)) != TH_ACK) {
    LogPrintf(LogDEBUG, "??? 2 th_flags = %x\n", th->th_flags);
    LogDumpBp(LogDEBUG, "", m);
    return (TYPE_IP);
  }

  /*
   * Packet is compressible -- we're going to send either a COMPRESSED_TCP or
   * UNCOMPRESSED_TCP packet.  Either way we need to locate (or create) the
   * connection state.  Special case the most recently used connection since
   * it's most likely to be used again & we don't have to do any reordering
   * if it's used.
   */
  INCR(sls_packets)
    if (ip->ip_src.s_addr != cs->cs_ip.ip_src.s_addr ||
	ip->ip_dst.s_addr != cs->cs_ip.ip_dst.s_addr ||
	*(int *) th != ((int *) &cs->cs_ip)[cs->cs_ip.ip_hl]) {

    /*
     * Wasn't the first -- search for it.
     * 
     * States are kept in a circularly linked list with last_cs pointing to the
     * end of the list.  The list is kept in lru order by moving a state to
     * the head of the list whenever it is referenced.  Since the list is
     * short and, empirically, the connection we want is almost always near
     * the front, we locate states via linear search.  If we don't find a
     * state for the datagram, the oldest state is (re-)used.
     */
    register struct cstate *lcs;
    register struct cstate *lastcs = comp->last_cs;

    do {
      lcs = cs;
      cs = cs->cs_next;
      INCR(sls_searches)
	if (ip->ip_src.s_addr == cs->cs_ip.ip_src.s_addr
	    && ip->ip_dst.s_addr == cs->cs_ip.ip_dst.s_addr
	    && *(int *) th == ((int *) &cs->cs_ip)[cs->cs_ip.ip_hl])
	goto found;
    } while (cs != lastcs);

    /*
     * Didn't find it -- re-use oldest cstate.  Send an uncompressed packet
     * that tells the other side what connection number we're using for this
     * conversation. Note that since the state list is circular, the oldest
     * state points to the newest and we only need to set last_cs to update
     * the lru linkage.
     */
    INCR(sls_misses)
      comp->last_cs = lcs;
#define	THOFFSET(th)	(th->th_off)
    hlen += th->th_off;
    hlen <<= 2;
    if (hlen > m->cnt)
      return (TYPE_IP);
    goto uncompressed;

found:

    /*
     * Found it -- move to the front on the connection list.
     */
    if (cs == lastcs)
      comp->last_cs = lcs;
    else {
      lcs->cs_next = cs->cs_next;
      cs->cs_next = lastcs->cs_next;
      lastcs->cs_next = cs;
    }
  }

  /*
   * Make sure that only what we expect to change changed. The first line of
   * the `if' checks the IP protocol version, header length & type of
   * service.  The 2nd line checks the "Don't fragment" bit. The 3rd line
   * checks the time-to-live and protocol (the protocol check is unnecessary
   * but costless).  The 4th line checks the TCP header length.  The 5th line
   * checks IP options, if any.  The 6th line checks TCP options, if any.  If
   * any of these things are different between the previous & current
   * datagram, we send the current datagram `uncompressed'.
   */
  oth = (struct tcphdr *) & ((int *) &cs->cs_ip)[hlen];
  deltaS = hlen;
  hlen += th->th_off;
  hlen <<= 2;
  if (hlen > m->cnt)
    return (TYPE_IP);

  if (((u_short *) ip)[0] != ((u_short *) & cs->cs_ip)[0] ||
      ((u_short *) ip)[3] != ((u_short *) & cs->cs_ip)[3] ||
      ((u_short *) ip)[4] != ((u_short *) & cs->cs_ip)[4] ||
      THOFFSET(th) != THOFFSET(oth) ||
      (deltaS > 5 &&
       memcmp(ip + 1, &cs->cs_ip + 1, (deltaS - 5) << 2)) ||
      (THOFFSET(th) > 5 &&
       memcmp(th + 1, oth + 1, (THOFFSET(th) - 5) << 2))) {
    goto uncompressed;
  }

  /*
   * Figure out which of the changing fields changed.  The receiver expects
   * changes in the order: urgent, window, ack, seq (the order minimizes the
   * number of temporaries needed in this section of code).
   */
  if (th->th_flags & TH_URG) {
    deltaS = ntohs(th->th_urp);
    ENCODEZ(deltaS);
    changes |= NEW_U;
  } else if (th->th_urp != oth->th_urp) {

    /*
     * argh! URG not set but urp changed -- a sensible implementation should
     * never do this but RFC793 doesn't prohibit the change so we have to
     * deal with it.
     */
    goto uncompressed;
  }
  deltaS = (u_short) (ntohs(th->th_win) - ntohs(oth->th_win));
  if (deltaS) {
    ENCODE(deltaS);
    changes |= NEW_W;
  }
  deltaA = ntohl(th->th_ack) - ntohl(oth->th_ack);
  if (deltaA) {
    if (deltaA > 0xffff) {
      goto uncompressed;
    }
    ENCODE(deltaA);
    changes |= NEW_A;
  }
  deltaS = ntohl(th->th_seq) - ntohl(oth->th_seq);
  if (deltaS) {
    if (deltaS > 0xffff) {
      goto uncompressed;
    }
    ENCODE(deltaS);
    changes |= NEW_S;
  }
  switch (changes) {

  case 0:

    /*
     * Nothing changed. If this packet contains data and the last one didn't,
     * this is probably a data packet following an ack (normal on an
     * interactive connection) and we send it compressed.  Otherwise it's
     * probably a retransmit, retransmitted ack or window probe.  Send it
     * uncompressed in case the other side missed the compressed version.
     */
    if (ip->ip_len != cs->cs_ip.ip_len &&
	ntohs(cs->cs_ip.ip_len) == hlen)
      break;

    /* (fall through) */

  case SPECIAL_I:
  case SPECIAL_D:

    /*
     * actual changes match one of our special case encodings -- send packet
     * uncompressed.
     */
    goto uncompressed;

  case NEW_S | NEW_A:
    if (deltaS == deltaA &&
	deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
      /* special case for echoed terminal traffic */
      changes = SPECIAL_I;
      cp = new_seq;
    }
    break;

  case NEW_S:
    if (deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
      /* special case for data xfer */
      changes = SPECIAL_D;
      cp = new_seq;
    }
    break;
  }

  deltaS = ntohs(ip->ip_id) - ntohs(cs->cs_ip.ip_id);
  if (deltaS != 1) {
    ENCODEZ(deltaS);
    changes |= NEW_I;
  }
  if (th->th_flags & TH_PUSH)
    changes |= TCP_PUSH_BIT;

  /*
   * Grab the cksum before we overwrite it below.  Then update our state with
   * this packet's header.
   */
  deltaA = ntohs(th->th_sum);
  memcpy(&cs->cs_ip, ip, hlen);

  /*
   * We want to use the original packet as our compressed packet. (cp -
   * new_seq) is the number of bytes we need for compressed sequence numbers.
   * In addition we need one byte for the change mask, one for the connection
   * id and two for the tcp checksum. So, (cp - new_seq) + 4 bytes of header
   * are needed.  hlen is how many bytes of the original packet to toss so
   * subtract the two to get the new packet size.
   */
  deltaS = cp - new_seq;
  cp = (u_char *) ip;

  /*
   * Since fastq traffic can jump ahead of the background traffic, we don't
   * know what order packets will go on the line.  In this case, we always
   * send a "new" connection id so the receiver state stays synchronized.
   */
  if (comp->last_xmit == cs->cs_id && compress_cid) {
    hlen -= deltaS + 3;
    cp += hlen;
    *cp++ = changes;
  } else {
    comp->last_xmit = cs->cs_id;
    hlen -= deltaS + 4;
    cp += hlen;
    *cp++ = changes | NEW_C;
    *cp++ = cs->cs_id;
  }
  m->cnt -= hlen;
  m->offset += hlen;
  *cp++ = deltaA >> 8;
  *cp++ = deltaA;
  memcpy(cp, new_seq, deltaS);
  INCR(sls_compressed)
    return (TYPE_COMPRESSED_TCP);

  /*
   * Update connection state cs & send uncompressed packet ('uncompressed'
   * means a regular ip/tcp packet but with the 'conversation id' we hope to
   * use on future compressed packets in the protocol field).
   */
uncompressed:
  memcpy(&cs->cs_ip, ip, hlen);
  ip->ip_p = cs->cs_id;
  comp->last_xmit = cs->cs_id;
  return (TYPE_UNCOMPRESSED_TCP);
}


int
sl_uncompress_tcp(u_char ** bufp,
		  int len,
		  u_int type,
		  struct slcompress * comp)
{
  register u_char *cp;
  register u_int hlen, changes;
  register struct tcphdr *th;
  register struct cstate *cs;
  register struct ip *ip;

  switch (type) {

  case TYPE_UNCOMPRESSED_TCP:
    ip = (struct ip *) * bufp;
    if (ip->ip_p >= MAX_STATES)
      goto bad;
    cs = &comp->rstate[comp->last_recv = ip->ip_p];
    comp->flags &= ~SLF_TOSS;
    ip->ip_p = IPPROTO_TCP;

    /*
     * Calculate the size of the TCP/IP header and make sure that we don't
     * overflow the space we have available for it.
     */
    hlen = ip->ip_hl << 2;
    if (hlen + sizeof(struct tcphdr) > len)
      goto bad;
    th = (struct tcphdr *) & ((char *) ip)[hlen];
    hlen += THOFFSET(th) << 2;
    if (hlen > MAX_HDR)
      goto bad;
    memcpy(&cs->cs_ip, ip, hlen);
    cs->cs_ip.ip_sum = 0;
    cs->cs_hlen = hlen;
    INCR(sls_uncompressedin)
      return (len);

  default:
    goto bad;

  case TYPE_COMPRESSED_TCP:
    break;
  }
  /* We've got a compressed packet. */
  INCR(sls_compressedin)
    cp = *bufp;
  changes = *cp++;
  LogPrintf(LogDEBUG, "compressed: changes = %02x\n", changes);
  if (changes & NEW_C) {

    /*
     * Make sure the state index is in range, then grab the state. If we have
     * a good state index, clear the 'discard' flag.
     */
    if (*cp >= MAX_STATES || comp->last_recv == 255)
      goto bad;

    comp->flags &= ~SLF_TOSS;
    comp->last_recv = *cp++;
  } else {

    /*
     * this packet has an implicit state index.  If we've had a line error
     * since the last time we got an explicit state index, we have to toss
     * the packet.
     */
    if (comp->flags & SLF_TOSS) {
      INCR(sls_tossed)
	return (0);
    }
  }
  cs = &comp->rstate[comp->last_recv];
  hlen = cs->cs_ip.ip_hl << 2;
  th = (struct tcphdr *) & ((u_char *) & cs->cs_ip)[hlen];
  th->th_sum = htons((*cp << 8) | cp[1]);
  cp += 2;
  if (changes & TCP_PUSH_BIT)
    th->th_flags |= TH_PUSH;
  else
    th->th_flags &= ~TH_PUSH;

  switch (changes & SPECIALS_MASK) {
  case SPECIAL_I:
    {
      register u_int i = ntohs(cs->cs_ip.ip_len) - cs->cs_hlen;

      th->th_ack = htonl(ntohl(th->th_ack) + i);
      th->th_seq = htonl(ntohl(th->th_seq) + i);
    }
    break;

  case SPECIAL_D:
    th->th_seq = htonl(ntohl(th->th_seq) + ntohs(cs->cs_ip.ip_len)
		       - cs->cs_hlen);
    break;

  default:
    if (changes & NEW_U) {
      th->th_flags |= TH_URG;
      DECODEU(th->th_urp)
    } else
      th->th_flags &= ~TH_URG;
    if (changes & NEW_W)
      DECODES(th->th_win)
	if (changes & NEW_A)
	DECODEL(th->th_ack)
	  if (changes & NEW_S) {
	  LogPrintf(LogDEBUG, "NEW_S: %02x, %02x, %02x\n",
		    *cp, cp[1], cp[2]);
	  DECODEL(th->th_seq)
	}
    break;
  }
  if (changes & NEW_I) {
    DECODES(cs->cs_ip.ip_id)
  } else
    cs->cs_ip.ip_id = htons(ntohs(cs->cs_ip.ip_id) + 1);

  LogPrintf(LogDEBUG, "Uncompress: id = %04x, seq = %08x\n",
	    cs->cs_ip.ip_id, ntohl(th->th_seq));

  /*
   * At this point, cp points to the first byte of data in the packet.  If
   * we're not aligned on a 4-byte boundary, copy the data down so the ip &
   * tcp headers will be aligned.  Then back up cp by the tcp/ip header
   * length to make room for the reconstructed header (we assume the packet
   * we were handed has enough space to prepend 128 bytes of header).  Adjust
   * the length to account for the new header & fill in the IP total length.
   */
  len -= (cp - *bufp);
  if (len < 0)

    /*
     * we must have dropped some characters (crc should detect this but the
     * old slip framing won't)
     */
    goto bad;

#ifdef notdef
  if ((int) cp & 3) {
    if (len > 0)
      (void) bcopy(cp, (caddr_t) ((int) cp & ~3), len);
    cp = (u_char *) ((int) cp & ~3);
  }
#endif

  cp -= cs->cs_hlen;
  len += cs->cs_hlen;
  cs->cs_ip.ip_len = htons(len);
  memcpy(cp, &cs->cs_ip, cs->cs_hlen);
  *bufp = cp;

  /* recompute the ip header checksum */
  {
    register u_short *bp = (u_short *) cp;

    for (changes = 0; hlen > 0; hlen -= 2)
      changes += *bp++;
    changes = (changes & 0xffff) + (changes >> 16);
    changes = (changes & 0xffff) + (changes >> 16);
    ((struct ip *) cp)->ip_sum = ~changes;
  }
  return (len);
bad:
  comp->flags |= SLF_TOSS;
  INCR(sls_errorin)
    return (0);
}

int
ReportCompress(struct cmdargs const *arg)
{
  if (!VarTerm)
    return 1;

  fprintf(VarTerm, "Out:  %d (compress) / %d (total)",
	  slstat.sls_compressed, slstat.sls_packets);
  fprintf(VarTerm, "  %d (miss) / %d (search)\n",
	  slstat.sls_misses, slstat.sls_searches);
  fprintf(VarTerm, "In:  %d (compress), %d (uncompress)",
	  slstat.sls_compressedin, slstat.sls_uncompressedin);
  fprintf(VarTerm, "  %d (error),  %d (tossed)\n",
	  slstat.sls_errorin, slstat.sls_tossed);
  return 0;
}
