/*
 * $Id: phase.c,v 1.1 1997/10/26 01:03:31 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdio.h>

#include "mbuf.h"
#include "log.h"
#include "lcp.h"
#include "lcpproto.h"
#include "timer.h"
#include "auth.h"
#include "pap.h"
#include "chap.h"
#include "ipcp.h"
#include "ccp.h"
#include "defs.h"
#include "main.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "phase.h"

int phase = 0;			/* Curent phase */

static char *PhaseNames[] = {
  "Dead", "Establish", "Authenticate", "Network", "Terminate"
};

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
      LogPrintf(LogPHASE, " his = %x, mine = %x\n", lcp->his_auth, lcp->want_auth);
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
