/*
 *	       Input/Output VJ Compressed packets
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
 * $Id: vjcomp.c,v 1.11 1997/10/26 01:04:01 brian Exp $
 *
 *  TODO:
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <string.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcpproto.h"
#include "slcompress.h"
#include "hdlc.h"
#include "ipcp.h"
#include "vjcomp.h"

#define MAX_VJHEADER 16		/* Maximum size of compressed header */

struct slcompress cslc;

void
VjInit(int max_state)
{
  sl_compress_init(&cslc, max_state);
}

void
SendPppFrame(struct mbuf * bp)
{
  int type;
  int proto;
  int cproto = IpcpInfo.his_compproto >> 16;

  LogPrintf(LogDEBUG, "SendPppFrame: proto = %x\n", IpcpInfo.his_compproto);
  if (((struct ip *) MBUF_CTOP(bp))->ip_p == IPPROTO_TCP
      && cproto == PROTO_VJCOMP) {
    type = sl_compress_tcp(bp, (struct ip *) MBUF_CTOP(bp), &cslc, IpcpInfo.his_compproto & 0xff);

    LogPrintf(LogDEBUG, "SendPppFrame: type = %x\n", type);
    switch (type) {
    case TYPE_IP:
      proto = PROTO_IP;
      break;
    case TYPE_UNCOMPRESSED_TCP:
      proto = PROTO_VJUNCOMP;
      break;
    case TYPE_COMPRESSED_TCP:
      proto = PROTO_VJCOMP;
      break;
    default:
      LogPrintf(LogERROR, "Unknown frame type %x\n", type);
      pfree(bp);
      return;
    }
  } else
    proto = PROTO_IP;
  HdlcOutput(PRI_NORMAL, proto, bp);
}

static struct mbuf *
VjUncompressTcp(struct mbuf * bp, u_char type)
{
  u_char *bufp;
  int len, olen, rlen;
  struct mbuf *nbp;
  u_char work[MAX_HDR + MAX_VJHEADER];	/* enough to hold TCP/IP header */

  olen = len = plength(bp);
  if (type == TYPE_UNCOMPRESSED_TCP) {

    /*
     * Uncompressed packet does NOT change its size, so that we can use mbuf
     * space for uncompression job.
     */
    bufp = MBUF_CTOP(bp);
    len = sl_uncompress_tcp(&bufp, len, type, &cslc);
    if (len <= 0) {
      pfree(bp);
      bp = NULLBUFF;
    }
    return (bp);
  }

  /*
   * Handle compressed packet. 1) Read upto MAX_VJHEADER bytes into work
   * space. 2) Try to uncompress it. 3) Compute amount of necesary space. 4)
   * Copy unread data info there.
   */
  if (len > MAX_VJHEADER)
    len = MAX_VJHEADER;
  rlen = len;
  bufp = work + MAX_HDR;
  bp = mbread(bp, bufp, rlen);
  len = sl_uncompress_tcp(&bufp, olen, type, &cslc);
  if (len <= 0) {
    pfree(bp);
    return NULLBUFF;
  }
  len -= olen;
  len += rlen;
  nbp = mballoc(len, MB_VJCOMP);
  memcpy(MBUF_CTOP(nbp), bufp, len);
  nbp->next = bp;
  return (nbp);
}

struct mbuf *
VjCompInput(struct mbuf * bp, int proto)
{
  u_char type;

  LogPrintf(LogDEBUG, "VjCompInput: proto %02x\n", proto);
  LogDumpBp(LogDEBUG, "Raw packet info:", bp);

  switch (proto) {
  case PROTO_VJCOMP:
    type = TYPE_COMPRESSED_TCP;
    break;
  case PROTO_VJUNCOMP:
    type = TYPE_UNCOMPRESSED_TCP;
    break;
  default:
    LogPrintf(LogERROR, "VjCompInput...???\n");
    return (bp);
  }
  bp = VjUncompressTcp(bp, type);
  return (bp);
}
