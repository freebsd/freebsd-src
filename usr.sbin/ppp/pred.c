#include "fsm.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "ccp.h"

/*
 *
 * $Id: pred.c,v 1.13 1997/06/09 23:38:37 brian Exp $
 *
 * pred.c -- Test program for Dave Rand's rendition of the
 * predictor algorithm
 * Updated by: iand@labtam.labtam.oz.au (Ian Donaldson)
 * Updated by: Carsten Bormann <cabo@cs.tu-berlin.de>
 * Original  : Dave Rand <dlr@bungi.com>/<dave_rand@novell.com>
 */

/* The following hash code is the heart of the algorithm:
 * It builds a sliding hash sum of the previous 3-and-a-bit characters
 * which will be used to index the guess table.
 * A better hash function would result in additional compression,
 * at the expense of time.
 */
#define IHASH(x) do {iHash = (iHash << 4) ^ (x);} while(0)
#define OHASH(x) do {oHash = (oHash << 4) ^ (x);} while(0)

static unsigned short int iHash, oHash;
static unsigned char InputGuessTable[65536];
static unsigned char OutputGuessTable[65536];

static int
compress(u_char * source, u_char * dest, int len)
{
  int i, bitmask;
  unsigned char *flagdest, flags, *orgdest;

  orgdest = dest;
  while (len) {
    flagdest = dest++;
    flags = 0;			/* All guess wrong initially */
    for (bitmask = 1, i = 0; i < 8 && len; i++, bitmask <<= 1) {
      if (OutputGuessTable[oHash] == *source) {
	flags |= bitmask;	/* Guess was right - don't output */
      } else {
	OutputGuessTable[oHash] = *source;
	*dest++ = *source;	/* Guess wrong, output char */
      }
      OHASH(*source++);
      len--;
    }
    *flagdest = flags;
  }
  return (dest - orgdest);
}

static void
SyncTable(u_char * source, u_char * dest, int len)
{

  while (len--) {
    if (InputGuessTable[iHash] != *source) {
      InputGuessTable[iHash] = *source;
    }
    IHASH(*dest++ = *source++);
  }
}

static int
decompress(u_char * source, u_char * dest, int len)
{
  int i, bitmask;
  unsigned char flags, *orgdest;

  orgdest = dest;
  while (len) {
    flags = *source++;
    len--;
    for (i = 0, bitmask = 1; i < 8; i++, bitmask <<= 1) {
      if (flags & bitmask) {
	*dest = InputGuessTable[iHash];	/* Guess correct */
      } else {
	if (!len)
	  break;		/* we seem to be really done -- cabo */
	InputGuessTable[iHash] = *source;	/* Guess wrong */
	*dest = *source++;	/* Read from source */
	len--;
      }
      IHASH(*dest++);
    }
  }
  return (dest - orgdest);
}

void
Pred1Init(int direction)
{
  if (direction & 1) {		/* Input part */
    iHash = 0;
    bzero(InputGuessTable, sizeof(InputGuessTable));
  }
  if (direction & 2) {		/* Output part */
    oHash = 0;
    bzero(OutputGuessTable, sizeof(OutputGuessTable));
  }
}

void
Pred1Output(int pri, u_short proto, struct mbuf * bp)
{
  struct mbuf *mwp;
  u_char *cp, *wp, *hp;
  int orglen, len;
  u_char bufp[MAX_MTU + 2];
  u_short fcs;

  orglen = plength(bp) + 2;	/* add count of proto */
  mwp = mballoc((orglen + 2) / 8 * 9 + 12, MB_HDLCOUT);
  hp = wp = MBUF_CTOP(mwp);
  cp = bufp;
  *wp++ = *cp++ = orglen >> 8;
  *wp++ = *cp++ = orglen & 0377;
  *cp++ = proto >> 8;
  *cp++ = proto & 0377;
  mbread(bp, cp, orglen - 2);
  fcs = HdlcFcs(INITFCS, bufp, 2 + orglen);
  fcs = ~fcs;

  len = compress(bufp + 2, wp, orglen);
  LogPrintf(LogDEBUG, "Pred1Output: orglen (%d) --> len (%d)\n", orglen, len);
  CcpInfo.orgout += orglen;
  if (len < orglen) {
    *hp |= 0x80;
    wp += len;
    CcpInfo.compout += len;
  } else {
    bcopy(bufp + 2, wp, orglen);
    wp += orglen;
    CcpInfo.compout += orglen;
  }

  *wp++ = fcs & 0377;
  *wp++ = fcs >> 8;
  mwp->cnt = wp - MBUF_CTOP(mwp);
  HdlcOutput(PRI_NORMAL, PROTO_COMPD, mwp);
}

void
Pred1Input(struct mbuf * bp)
{
  u_char *cp, *pp;
  int len, olen, len1;
  struct mbuf *wp;
  u_char *bufp;
  u_short fcs, proto;

  wp = mballoc(MAX_MTU + 2, MB_IPIN);
  cp = MBUF_CTOP(bp);
  olen = plength(bp);
  pp = bufp = MBUF_CTOP(wp);
  *pp++ = *cp & 0177;
  len = *cp++ << 8;
  *pp++ = *cp;
  len += *cp++;
  CcpInfo.orgin += len & 0x7fff;
  if (len & 0x8000) {
    len1 = decompress(cp, pp, olen - 4);
    CcpInfo.compin += olen;
    len &= 0x7fff;
    if (len != len1) {		/* Error is detected. Send reset request */
      LogPrintf(LogLCP, "%s: Length Error\n", CcpFsm.name);
      CcpSendResetReq(&CcpFsm);
      pfree(bp);
      pfree(wp);
      return;
    }
    cp += olen - 4;
    pp += len1;
  } else {
    CcpInfo.compin += len;
    SyncTable(cp, pp, len);
    cp += len;
    pp += len;
  }
  *pp++ = *cp++;		/* CRC */
  *pp++ = *cp++;
  fcs = HdlcFcs(INITFCS, bufp, wp->cnt = pp - bufp);
  if (fcs != GOODFCS)
    LogPrintf(LogDEBUG, "Pred1Input: fcs = 0x%04x (%s), len = 0x%x,"
	      " olen = 0x%x\n", fcs, (fcs == GOODFCS) ? "good" : "bad",
	      len, olen);
  if (fcs == GOODFCS) {
    wp->offset += 2;		/* skip length */
    wp->cnt -= 4;		/* skip length & CRC */
    pp = MBUF_CTOP(wp);
    proto = *pp++;
    if (proto & 1) {
      wp->offset++;
      wp->cnt--;
    } else {
      wp->offset += 2;
      wp->cnt -= 2;
      proto = (proto << 8) | *pp++;
    }
    DecodePacket(proto, wp);
  } else {
    LogDumpBp(LogHDLC, "Bad FCS", wp);
    CcpSendResetReq(&CcpFsm);
    pfree(wp);
  }
  pfree(bp);
}
