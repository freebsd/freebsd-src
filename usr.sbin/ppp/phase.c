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
 *	$Id$
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "lcp.h"
#include "lcpproto.h"
#include "timer.h"
#include "auth.h"
#include "pap.h"
#include "chap.h"
#include "defs.h"
#include "ipcp.h"
#include "ccp.h"
#include "main.h"
#include "loadalias.h"
#include "vars.h"
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
NewPhase(int new)
{
  struct lcpstate *lcp = &LcpInfo;

  phase = new;
  LogPrintf(LogPHASE, "NewPhase: %s\n", PhaseNames[phase]);
  switch (phase) {
  case PHASE_AUTHENTICATE:
    lcp->auth_ineed = lcp->want_auth;
    lcp->auth_iwait = lcp->his_auth;
    if (lcp->his_auth || lcp->want_auth) {
      LogPrintf(LogPHASE, " his = %s, mine = %s\n",
                Auth2Nam(lcp->his_auth), Auth2Nam(lcp->want_auth));
      if (lcp->his_auth == PROTO_PAP)
	StartAuthChallenge(&AuthPapInfo);
      if (lcp->want_auth == PROTO_CHAP)
	StartAuthChallenge(&AuthChapInfo);
    } else
      NewPhase(PHASE_NETWORK);
    break;
  case PHASE_NETWORK:
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
