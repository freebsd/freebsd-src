/*
 *	     PPP High Level Link Control (HDLC) Module
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
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: hdlc.c,v 1.20 1997/10/26 01:02:43 brian Exp $
 *
 *	TODO:
 */
#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "hdlc.h"
#include "lcpproto.h"
#include "ipcp.h"
#include "ip.h"
#include "vjcomp.h"
#include "pap.h"
#include "chap.h"
#include "lcp.h"
#include "lqr.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "pred.h"
#include "modem.h"
#include "ccp.h"

struct hdlcstat {
  int badfcs;
  int badaddr;
  int badcommand;
  int unknownproto;
}        HdlcStat;

static int ifOutPackets;
static int ifOutOctets;
static int ifOutLQRs;
static int ifInPackets;
static int ifInOctets;

struct protostat {
  u_short number;
  char *name;
  u_long in_count;
  u_long out_count;
}         ProtocolStat[] = {

  {
    PROTO_IP, "IP"
  },
  {
    PROTO_VJUNCOMP, "VJ_UNCOMP"
  },
  {
    PROTO_VJCOMP, "VJ_COMP"
  },
  {
    PROTO_COMPD, "COMPD"
  },
  {
    PROTO_LCP, "LCP"
  },
  {
    PROTO_IPCP, "IPCP"
  },
  {
    PROTO_CCP, "CCP"
  },
  {
    PROTO_PAP, "PAP"
  },
  {
    PROTO_LQR, "LQR"
  },
  {
    PROTO_CHAP, "CHAP"
  },
  {
    0, "Others"
  },
};

static u_short const fcstab[256] = {
   /* 00 */ 0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
   /* 08 */ 0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
   /* 10 */ 0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
   /* 18 */ 0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
   /* 20 */ 0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
   /* 28 */ 0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
   /* 30 */ 0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
   /* 38 */ 0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
   /* 40 */ 0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
   /* 48 */ 0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
   /* 50 */ 0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
   /* 58 */ 0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
   /* 60 */ 0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
   /* 68 */ 0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
   /* 70 */ 0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
   /* 78 */ 0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
   /* 80 */ 0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
   /* 88 */ 0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
   /* 90 */ 0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
   /* 98 */ 0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
   /* a0 */ 0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
   /* a8 */ 0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
   /* b0 */ 0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
   /* b8 */ 0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
   /* c0 */ 0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
   /* c8 */ 0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
   /* d0 */ 0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
   /* d8 */ 0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
   /* e0 */ 0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
   /* e8 */ 0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
   /* f0 */ 0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
   /* f8 */ 0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

u_char EscMap[33];

void
HdlcInit()
{
  ifInOctets = ifOutOctets = 0;
  ifInPackets = ifOutPackets = 0;
  ifOutLQRs = 0;
}

/*
 *  HDLC FCS computation. Read RFC 1171 Appendix B and CCITT X.25 section
 *  2.27 for further details.
 */
inline u_short
HdlcFcs(u_short fcs, u_char * cp, int len)
{
  while (len--)
    fcs = (fcs >> 8) ^ fcstab[(fcs ^ *cp++) & 0xff];
  return (fcs);
}

void
HdlcOutput(int pri, u_short proto, struct mbuf * bp)
{
  struct mbuf *mhp, *mfcs;
  struct protostat *statp;
  struct lqrdata *lqr;
  u_char *cp;
  u_short fcs;

  if ((proto & 0xfff1) == 0x21) {	/* Network Layer protocol */
    if (CcpFsm.state == ST_OPENED) {
      if (CcpInfo.want_proto == TY_PRED1) {
        Pred1Output(pri, proto, bp);
        return;
      }
    }
  }
  if (DEV_IS_SYNC)
    mfcs = NULLBUFF;
  else
    mfcs = mballoc(2, MB_HDLCOUT);
  mhp = mballoc(4, MB_HDLCOUT);
  mhp->cnt = 0;
  cp = MBUF_CTOP(mhp);
  if (proto == PROTO_LCP || LcpInfo.his_acfcomp == 0) {
    *cp++ = HDLC_ADDR;
    *cp++ = HDLC_UI;
    mhp->cnt += 2;
  }

  /*
   * If possible, compress protocol field.
   */
  if (LcpInfo.his_protocomp && (proto & 0xff00) == 0) {
    *cp++ = proto;
    mhp->cnt++;
  } else {
    *cp++ = proto >> 8;
    *cp = proto & 0377;
    mhp->cnt += 2;
  }
  mhp->next = bp;
  bp->next = mfcs;

  lqr = &MyLqrData;
  lqr->PeerOutPackets = ifOutPackets++;
  ifOutOctets += plength(mhp) + 1;
  lqr->PeerOutOctets = ifOutOctets;

  if (proto == PROTO_LQR) {
    lqr->MagicNumber = LcpInfo.want_magic;
    lqr->LastOutLQRs = HisLqrData.PeerOutLQRs;
    lqr->LastOutPackets = HisLqrData.PeerOutPackets;
    lqr->LastOutOctets = HisLqrData.PeerOutOctets;
    lqr->PeerInLQRs = HisLqrSave.SaveInLQRs;
    lqr->PeerInPackets = HisLqrSave.SaveInPackets;
    lqr->PeerInDiscards = HisLqrSave.SaveInDiscards;
    lqr->PeerInErrors = HisLqrSave.SaveInErrors;
    lqr->PeerInOctets = HisLqrSave.SaveInOctets;
    lqr->PeerOutLQRs = ++ifOutLQRs;
    LqrDump("LqrOutput", lqr);
    LqrChangeOrder(lqr, (struct lqrdata *) (MBUF_CTOP(bp)));
  }
  if (!DEV_IS_SYNC) {
    fcs = HdlcFcs(INITFCS, MBUF_CTOP(mhp), mhp->cnt);
    fcs = HdlcFcs(fcs, MBUF_CTOP(bp), bp->cnt);
    fcs = ~fcs;
    cp = MBUF_CTOP(mfcs);
    *cp++ = fcs & 0377;		/* Low byte first!! */
    *cp++ = fcs >> 8;
  }
  LogDumpBp(LogHDLC, "HdlcOutput", mhp);
  for (statp = ProtocolStat; statp->number; statp++)
    if (statp->number == proto)
      break;
  statp->out_count++;
  if (DEV_IS_SYNC)
    ModemOutput(pri, mhp);
  else
    AsyncOutput(pri, mhp, proto);
}

void
DecodePacket(u_short proto, struct mbuf * bp)
{
  u_char *cp;

  LogPrintf(LogDEBUG, "DecodePacket: proto = %04x\n", proto);

  switch (proto) {
  case PROTO_LCP:
    LcpInput(bp);
    break;
  case PROTO_PAP:
    PapInput(bp);
    break;
  case PROTO_LQR:
    HisLqrSave.SaveInLQRs++;
    LqrInput(bp);
    break;
  case PROTO_CHAP:
    ChapInput(bp);
    break;
  case PROTO_VJUNCOMP:
  case PROTO_VJCOMP:
    bp = VjCompInput(bp, proto);
    if (bp == NULLBUFF) {
      break;
    }
    /* fall down */
  case PROTO_IP:
    IpInput(bp);
    break;
  case PROTO_IPCP:
    IpcpInput(bp);
    break;
  case PROTO_CCP:
    CcpInput(bp);
    break;
  case PROTO_COMPD:
    Pred1Input(bp);
    break;
  default:
    LogPrintf(LogPHASE, "Unknown protocol 0x%04x\n", proto);
    bp->offset -= 2;
    bp->cnt += 2;
    cp = MBUF_CTOP(bp);
    LcpSendProtoRej(cp, bp->cnt);
    HisLqrSave.SaveInDiscards++;
    HdlcStat.unknownproto++;
    pfree(bp);
    break;
  }
}

int
ReportProtStatus()
{
  struct protostat *statp;
  int cnt;

  statp = ProtocolStat;
  statp--;
  cnt = 0;
  fprintf(VarTerm, "    Protocol     in        out      Protocol      in       out\n");
  do {
    statp++;
    fprintf(VarTerm, "   %-9s: %8lu, %8lu",
	    statp->name, statp->in_count, statp->out_count);
    if (++cnt == 2) {
      fprintf(VarTerm, "\n");
      cnt = 0;
    }
  } while (statp->number);
  if (cnt)
    fprintf(VarTerm, "\n");
  return (1);
}

int
ReportHdlcStatus()
{
  struct hdlcstat *hp = &HdlcStat;

  if (VarTerm) {
    fprintf(VarTerm, "HDLC level errors\n\n");
    fprintf(VarTerm, "FCS: %u  ADDR: %u  COMMAND: %u  PROTO: %u\n",
	    hp->badfcs, hp->badaddr, hp->badcommand, hp->unknownproto);
  }
  return 0;
}

static struct hdlcstat laststat;

void
HdlcErrorCheck()
{
  struct hdlcstat *hp = &HdlcStat;
  struct hdlcstat *op = &laststat;

  if (memcmp(hp, op, sizeof(laststat))) {
    LogPrintf(LogPHASE, "HDLC errors -> FCS: %u ADDR: %u COMD: %u PROTO: %u\n",
	      hp->badfcs - op->badfcs, hp->badaddr - op->badaddr,
      hp->badcommand - op->badcommand, hp->unknownproto - op->unknownproto);
  }
  laststat = HdlcStat;
}

void
HdlcInput(struct mbuf * bp)
{
  u_short fcs, proto;
  u_char *cp, addr, ctrl;
  struct protostat *statp;

  LogDumpBp(LogHDLC, "HdlcInput:", bp);
  if (DEV_IS_SYNC)
    fcs = GOODFCS;
  else
    fcs = HdlcFcs(INITFCS, MBUF_CTOP(bp), bp->cnt);
  HisLqrSave.SaveInOctets += bp->cnt + 1;

  LogPrintf(LogDEBUG, "HdlcInput: fcs = %04x (%s)\n",
	    fcs, (fcs == GOODFCS) ? "good" : "bad");
  if (fcs != GOODFCS) {
    HisLqrSave.SaveInErrors++;
    LogPrintf(LogDEBUG, "HdlcInput: Bad FCS\n");
    HdlcStat.badfcs++;
    pfree(bp);
    return;
  }
  if (!DEV_IS_SYNC)
    bp->cnt -= 2;		/* discard FCS part */

  if (bp->cnt < 2) {		/* XXX: raise this bar ? */
    pfree(bp);
    return;
  }
  cp = MBUF_CTOP(bp);

  ifInPackets++;
  ifInOctets += bp->cnt;

  if (!LcpInfo.want_acfcomp) {

    /*
     * We expect that packet is not compressed.
     */
    addr = *cp++;
    if (addr != HDLC_ADDR) {
      HisLqrSave.SaveInErrors++;
      HdlcStat.badaddr++;
      LogPrintf(LogDEBUG, "HdlcInput: addr %02x\n", *cp);
      pfree(bp);
      return;
    }
    ctrl = *cp++;
    if (ctrl != HDLC_UI) {
      HisLqrSave.SaveInErrors++;
      HdlcStat.badcommand++;
      LogPrintf(LogDEBUG, "HdlcInput: %02x\n", *cp);
      pfree(bp);
      return;
    }
    bp->offset += 2;
    bp->cnt -= 2;
  } else if (cp[0] == HDLC_ADDR && cp[1] == HDLC_UI) {

    /*
     * We can receive compressed packet, but peer still send uncompressed
     * packet to me.
     */
    cp += 2;
    bp->offset += 2;
    bp->cnt -= 2;
  }
  if (LcpInfo.want_protocomp) {
    proto = 0;
    cp--;
    do {
      cp++;
      bp->offset++;
      bp->cnt--;
      proto = proto << 8;
      proto += *cp;
    } while (!(proto & 1));
  } else {
    proto = *cp++ << 8;
    proto |= *cp++;
    bp->offset += 2;
    bp->cnt -= 2;
  }

  for (statp = ProtocolStat; statp->number; statp++)
    if (statp->number == proto)
      break;
  statp->in_count++;
  HisLqrSave.SaveInPackets++;

  DecodePacket(proto, bp);
}
