/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: phase.c,v 1.6.4.5 1998/01/31 02:48:28 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "lcpproto.h"
#include "auth.h"
#include "pap.h"
#include "chap.h"
#include "defs.h"
#include "iplist.h"
#include "throughput.h"
#include "hdlc.h"
#include "link.h"
#include "ipcp.h"
#include "ccp.h"
#include "main.h"
#include "loadalias.h"
#include "vars.h"
#include "modem.h"
#include "tun.h"
#include "phase.h"

int phase = 0;			/* Curent phase */

static const char *PhaseNames[] = {
  "Dead", "Establish", "Authenticate", "Network", "Terminate"
};

static const char *
Auth2Nam(u_short auth)
{
  switch (auth) {
  case PROTO_PAP:
    return "PAP";
  case PROTO_CHAP:
    return "CHAP";
  case 0:
    return "none";
  }
  return "unknown";
}

void
NewPhase(struct bundle *bundle, struct physical *physical, int new)
{
  phase = new;
  LogPrintf(LogPHASE, "NewPhase: %s\n", PhaseNames[phase]);
  switch (phase) {
  case PHASE_AUTHENTICATE:
    LcpInfo.auth_ineed = LcpInfo.want_auth;
    LcpInfo.auth_iwait = LcpInfo.his_auth;
    if (LcpInfo.his_auth || LcpInfo.want_auth) {
      LogPrintf(LogPHASE, " his = %s, mine = %s\n",
                Auth2Nam(LcpInfo.his_auth), Auth2Nam(LcpInfo.want_auth));
       /* XXX-ML AuthPapInfo and AuthChapInfo must be allocated! */
      if (LcpInfo.his_auth == PROTO_PAP)
	StartAuthChallenge(&AuthPapInfo, physical);
      if (LcpInfo.want_auth == PROTO_CHAP)
	StartAuthChallenge(&AuthChapInfo, physical);
    } else
      NewPhase(bundle, physical, PHASE_NETWORK);
    break;

  case PHASE_NETWORK:
    tun_configure(bundle, LcpInfo.his_mru, ModemSpeed(physical));
    IpcpUp();
    IpcpOpen();
    CcpUp();
    CcpOpen();
    break;

  case PHASE_DEAD:
    if (mode & MODE_DIRECT)
      Cleanup(EX_DEAD);
    if (mode & MODE_BACKGROUND && reconnectState != RECON_TRUE)
      Cleanup(EX_DEAD);
    break;
  }
}
