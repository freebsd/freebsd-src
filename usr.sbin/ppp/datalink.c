/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: datalink.c,v 1.1.2.41 1998/04/18 23:17:25 brian Exp $
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "throughput.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "mp.h"
#include "bundle.h"
#include "chat.h"
#include "auth.h"
#include "modem.h"
#include "prompt.h"
#include "lcpproto.h"
#include "pap.h"
#include "chap.h"
#include "command.h"
#include "datalink.h"

static const char *datalink_State(struct datalink *);
static void datalink_LoginDone(struct datalink *);

static void
datalink_OpenTimeout(void *v)
{
  struct datalink *dl = (struct datalink *)v;

  StopTimer(&dl->dial_timer);
  if (dl->state == DATALINK_OPENING)
    LogPrintf(LogPHASE, "%s: Redial timer expired.\n", dl->name);
}

static void
datalink_StartDialTimer(struct datalink *dl, int Timeout)
{
  StopTimer(&dl->dial_timer);
 
  if (Timeout) { 
    if (Timeout > 0)
      dl->dial_timer.load = Timeout * SECTICKS;
    else
      dl->dial_timer.load = (random() % DIAL_TIMEOUT) * SECTICKS;
    dl->dial_timer.func = datalink_OpenTimeout;
    dl->dial_timer.name = "dial";
    dl->dial_timer.arg = dl;
    StartTimer(&dl->dial_timer);
    if (dl->state == DATALINK_OPENING)
      LogPrintf(LogPHASE, "%s: Enter pause (%d) for redialing.\n",
                dl->name, Timeout);
  }
}

static void
datalink_HangupDone(struct datalink *dl)
{
  if (dl->physical->type == PHYS_DEDICATED && !dl->bundle->CleaningUp &&
      Physical_GetFD(dl->physical) != -1) {
    /* Don't close our modem if the link is dedicated */
    datalink_LoginDone(dl);
    return;
  }

  modem_Close(dl->physical);
  dl->phone.chosen = "N/A";

  if (dl->bundle->CleaningUp ||
      (dl->physical->type == PHYS_STDIN) ||
      ((!dl->dial_tries || (dl->dial_tries < 0 && !dl->reconnect_tries)) &&
       !(dl->physical->type & (PHYS_PERM|PHYS_DEDICATED)))) {
    LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
    dl->state = DATALINK_CLOSED;
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
    bundle_LinkClosed(dl->bundle, dl);
    if (!dl->bundle->CleaningUp)
      datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
  } else {
    LogPrintf(LogPHASE, "%s: Re-entering OPENING state\n", dl->name);
    dl->state = DATALINK_OPENING;
    if (dl->dial_tries < 0) {
      datalink_StartDialTimer(dl, dl->cfg.reconnect.timeout);
      dl->dial_tries = dl->cfg.dial.max;
      dl->reconnect_tries--;
    } else {
      if (dl->phone.next == NULL)
        datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
      else
        datalink_StartDialTimer(dl, dl->cfg.dial.next_timeout);
    }
  }
}

static const char *
datalink_ChoosePhoneNumber(struct datalink *dl)
{
  char *phone;

  if (dl->phone.alt == NULL) {
    if (dl->phone.next == NULL) {
      strncpy(dl->phone.list, dl->cfg.phone.list, sizeof dl->phone.list - 1);
      dl->phone.list[sizeof dl->phone.list - 1] = '\0';
      dl->phone.next = dl->phone.list;
    }
    dl->phone.alt = strsep(&dl->phone.next, ":");
  }
  phone = strsep(&dl->phone.alt, "|");
  dl->phone.chosen = *phone ? phone : "[NONE]";
  if (*phone)
    LogPrintf(LogPHASE, "Phone: %s\n", phone);
  return phone;
}

static void
datalink_LoginDone(struct datalink *dl)
{
  if (!dl->script.packetmode) { 
    dl->dial_tries = -1;
    LogPrintf(LogPHASE, "%s: Entering READY state\n", dl->name);
    dl->state = DATALINK_READY;
  } else if (modem_Raw(dl->physical, dl->bundle) < 0) {
    dl->dial_tries = 0;
    LogPrintf(LogWARN, "datalink_LoginDone: Not connected.\n");
    if (dl->script.run) { 
      LogPrintf(LogPHASE, "%s: Entering HANGUP state\n", dl->name);
      dl->state = DATALINK_HANGUP;
      modem_Offline(dl->physical);
      chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
    } else {
      if (dl->physical->type == PHYS_DEDICATED)
        /* force a redial timeout */
        modem_Close(dl->physical);
      datalink_HangupDone(dl);
    }
  } else {
    dl->dial_tries = -1;

    hdlc_Init(&dl->physical->hdlc);
    async_Init(&dl->physical->async);

    lcp_Setup(&dl->physical->link.lcp, dl->state == DATALINK_READY ?
              0 : dl->physical->link.lcp.cfg.openmode);
    ccp_Setup(&dl->physical->link.ccp);

    LogPrintf(LogPHASE, "%s: Entering LCP state\n", dl->name);
    dl->state = DATALINK_LCP;
    FsmUp(&dl->physical->link.lcp.fsm);
    FsmOpen(&dl->physical->link.lcp.fsm);
  }
}

static int
datalink_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                   int *n)
{
  struct datalink *dl = descriptor2datalink(d);
  int result;

  result = 0;
  switch (dl->state) {
    case DATALINK_CLOSED:
      if ((dl->physical->type & (PHYS_STDIN|PHYS_DEDICATED|PHYS_1OFF)) &&
          !bundle_IsDead(dl->bundle))
        /*
         * Our first time in - DEDICATED never comes down, and STDIN & 1OFF
         * get deleted when they enter DATALINK_CLOSED.  Go to
         * DATALINK_OPENING via datalink_Up() and fall through.
         */
        datalink_Up(dl, 1, 1);
      else
        break;
      /* fall through */

    case DATALINK_OPENING:
      if (dl->dial_timer.state != TIMER_RUNNING) {
        if (--dl->dial_tries < 0)
          dl->dial_tries = 0;
        if (modem_Open(dl->physical, dl->bundle) >= 0) {
          if (dl->script.run) {
            LogPrintf(LogPHASE, "%s: Entering DIAL state\n", dl->name);
            dl->state = DATALINK_DIAL;
            chat_Init(&dl->chat, dl->physical, dl->cfg.script.dial, 1,
                      datalink_ChoosePhoneNumber(dl));
            if (!(dl->physical->type & (PHYS_PERM|PHYS_DEDICATED)) &&
                dl->cfg.dial.max)
              LogPrintf(LogCHAT, "%s: Dial attempt %u of %d\n",
                        dl->name, dl->cfg.dial.max - dl->dial_tries,
                        dl->cfg.dial.max);
            return datalink_UpdateSet(d, r, w, e, n);
          } else
            datalink_LoginDone(dl);
        } else {
          if (!(dl->physical->type & (PHYS_PERM|PHYS_DEDICATED)) &&
              dl->cfg.dial.max)
            LogPrintf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
                      dl->cfg.dial.max - dl->dial_tries, dl->cfg.dial.max);
          else
            LogPrintf(LogCHAT, "Failed to open modem\n");

          if (dl->bundle->CleaningUp ||
              (!(dl->physical->type & (PHYS_PERM|PHYS_DEDICATED)) &&
               dl->cfg.dial.max && dl->dial_tries == 0)) {
            LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
            dl->state = DATALINK_CLOSED;
            dl->reconnect_tries = 0;
            dl->dial_tries = -1;
            bundle_LinkClosed(dl->bundle, dl);
          }
          if (!dl->bundle->CleaningUp)
            datalink_StartDialTimer(dl, dl->cfg.dial.timeout);
        }
      }
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      result = descriptor_UpdateSet(&dl->chat.desc, r, w, e, n);
      switch (dl->chat.state) {
        case CHAT_DONE:
          /* script succeeded */
          chat_Destroy(&dl->chat);
          switch(dl->state) {
            case DATALINK_HANGUP:
              datalink_HangupDone(dl);
              break;
            case DATALINK_DIAL:
              LogPrintf(LogPHASE, "%s: Entering LOGIN state\n", dl->name);
              dl->state = DATALINK_LOGIN;
              chat_Init(&dl->chat, dl->physical, dl->cfg.script.login, 0, NULL);
              return datalink_UpdateSet(d, r, w, e, n);
            case DATALINK_LOGIN:
              datalink_LoginDone(dl);
              break;
          }
          break;
        case CHAT_FAILED:
          /* Going down - script failed */
          LogPrintf(LogWARN, "Chat script failed\n");
          chat_Destroy(&dl->chat);
          switch(dl->state) {
            case DATALINK_HANGUP:
              datalink_HangupDone(dl);
              break;
            case DATALINK_DIAL:
            case DATALINK_LOGIN:
              LogPrintf(LogPHASE, "%s: Entering HANGUP state\n", dl->name);
              dl->state = DATALINK_HANGUP;
              modem_Offline(dl->physical);
              chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
              return datalink_UpdateSet(d, r, w, e, n);
          }
          break;
      }
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_OPEN:
      result = descriptor_UpdateSet(&dl->physical->desc, r, w, e, n);
      break;
  }
  return result;
}

static int
datalink_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      return descriptor_IsSet(&dl->chat.desc, fdset);

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_OPEN:
      return descriptor_IsSet(&dl->physical->desc, fdset);
  }
  return 0;
}

static void
datalink_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      descriptor_Read(&dl->chat.desc, bundle, fdset);
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_OPEN:
      descriptor_Read(&dl->physical->desc, bundle, fdset);
      break;
  }
}

static void
datalink_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
    case DATALINK_OPENING:
      break;

    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      descriptor_Write(&dl->chat.desc, bundle, fdset);
      break;

    case DATALINK_READY:
    case DATALINK_LCP:
    case DATALINK_AUTH:
    case DATALINK_OPEN:
      descriptor_Write(&dl->physical->desc, bundle, fdset);
      break;
  }
}

static void
datalink_ComeDown(struct datalink *dl, int stay)
{
  if (stay) {
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
  }

  if (dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP) {
    modem_Offline(dl->physical);
    if (dl->script.run && dl->state != DATALINK_OPENING) {
      LogPrintf(LogPHASE, "%s: Entering HANGUP state\n", dl->name);
      dl->state = DATALINK_HANGUP;
      chat_Init(&dl->chat, dl->physical, dl->cfg.script.hangup, 1, NULL);
    } else
      datalink_HangupDone(dl);
  }
}

static void
datalink_LayerStart(void *v, struct fsm *fp)
{
  /* The given FSM is about to start up ! */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP)
    (*dl->parent->LayerStart)(dl->parent->object, fp);
}

static void
datalink_LayerUp(void *v, struct fsm *fp)
{
  /* The given fsm is now up */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    dl->physical->link.lcp.auth_ineed = dl->physical->link.lcp.want_auth;
    dl->physical->link.lcp.auth_iwait = dl->physical->link.lcp.his_auth;
    if (dl->physical->link.lcp.his_auth || dl->physical->link.lcp.want_auth) {
      if (bundle_Phase(dl->bundle) == PHASE_ESTABLISH)
        bundle_NewPhase(dl->bundle, PHASE_AUTHENTICATE);
      LogPrintf(LogPHASE, "%s: his = %s, mine = %s\n", dl->name,
                Auth2Nam(dl->physical->link.lcp.his_auth),
                Auth2Nam(dl->physical->link.lcp.want_auth));
      if (dl->physical->link.lcp.his_auth == PROTO_PAP)
        StartAuthChallenge(&dl->pap, dl->physical, SendPapChallenge);
      if (dl->physical->link.lcp.want_auth == PROTO_CHAP)
        StartAuthChallenge(&dl->chap.auth, dl->physical, SendChapChallenge);
    } else
      datalink_AuthOk(dl);
  }
}

void
datalink_AuthOk(struct datalink *dl)
{
  /* XXX: Connect to another ppp instance HERE */

  FsmUp(&dl->physical->link.ccp.fsm);
  FsmOpen(&dl->physical->link.ccp.fsm);
  dl->state = DATALINK_OPEN;
  bundle_NewPhase(dl->bundle, PHASE_NETWORK);
  (*dl->parent->LayerUp)(dl->parent->object, &dl->physical->link.lcp.fsm);
}

void
datalink_AuthNotOk(struct datalink *dl)
{
  dl->state = DATALINK_LCP;
  FsmClose(&dl->physical->link.lcp.fsm);
}

static void
datalink_LayerDown(void *v, struct fsm *fp)
{
  /* The given FSM has been told to come down */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    switch (dl->state) {
      case DATALINK_OPEN:
        FsmDown(&dl->physical->link.ccp.fsm);
        FsmClose(&dl->physical->link.ccp.fsm);
        (*dl->parent->LayerDown)(dl->parent->object, fp);
        /* fall through */

      case DATALINK_AUTH:
        StopTimer(&dl->pap.authtimer);
        StopTimer(&dl->chap.auth.authtimer);
    }
    dl->state = DATALINK_LCP;
  }
}

static void
datalink_LayerFinish(void *v, struct fsm *fp)
{
  /* The given fsm is now down */
  struct datalink *dl = (struct datalink *)v;

  if (fp->proto == PROTO_LCP) {
    FsmDown(fp);	/* Bring us to INITIAL or STARTING */
    (*dl->parent->LayerFinish)(dl->parent->object, fp);
    datalink_ComeDown(dl, 0);
  }
}

struct datalink *
datalink_Create(const char *name, struct bundle *bundle,
                const struct fsm_parent *parent, int type)
{
  struct datalink *dl;

  dl = (struct datalink *)malloc(sizeof(struct datalink));
  if (dl == NULL)
    return dl;

  dl->desc.type = DATALINK_DESCRIPTOR;
  dl->desc.next = NULL;
  dl->desc.UpdateSet = datalink_UpdateSet;
  dl->desc.IsSet = datalink_IsSet;
  dl->desc.Read = datalink_Read;
  dl->desc.Write = datalink_Write;

  dl->state = DATALINK_CLOSED;

  *dl->cfg.script.dial = '\0';
  *dl->cfg.script.login = '\0';
  *dl->cfg.script.hangup = '\0';
  *dl->cfg.phone.list = '\0';
  *dl->phone.list = '\0';
  dl->phone.next = NULL;
  dl->phone.alt = NULL;
  dl->phone.chosen = "N/A";
  dl->script.run = 1;
  dl->script.packetmode = 1;
  mp_linkInit(&dl->mp);

  dl->bundle = bundle;
  dl->next = NULL;

  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);

  dl->dial_tries = 0;
  dl->cfg.dial.max = 1;
  dl->cfg.dial.next_timeout = DIAL_NEXT_TIMEOUT;
  dl->cfg.dial.timeout = DIAL_TIMEOUT;

  dl->reconnect_tries = 0;
  dl->cfg.reconnect.max = 0;
  dl->cfg.reconnect.timeout = RECONNECT_TIMEOUT;

  dl->name = strdup(name);
  dl->parent = parent;
  dl->fsmp.LayerStart = datalink_LayerStart;
  dl->fsmp.LayerUp = datalink_LayerUp;
  dl->fsmp.LayerDown = datalink_LayerDown;
  dl->fsmp.LayerFinish = datalink_LayerFinish;
  dl->fsmp.object = dl;

  authinfo_Init(&dl->pap);
  authinfo_Init(&dl->chap.auth);

  if ((dl->physical = modem_Create(dl, type)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }
  chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

  LogPrintf(LogPHASE, "%s: Created in CLOSED state\n", dl->name);

  return dl;
}

struct datalink *
datalink_Clone(struct datalink *odl, const char *name)
{
  struct datalink *dl;

  dl = (struct datalink *)malloc(sizeof(struct datalink));
  if (dl == NULL)
    return dl;

  dl->desc.type = DATALINK_DESCRIPTOR;
  dl->desc.next = NULL;
  dl->desc.UpdateSet = datalink_UpdateSet;
  dl->desc.IsSet = datalink_IsSet;
  dl->desc.Read = datalink_Read;
  dl->desc.Write = datalink_Write;

  dl->state = DATALINK_CLOSED;

  memcpy(&dl->cfg, &odl->cfg, sizeof dl->cfg);
  mp_linkInit(&dl->mp);
  *dl->phone.list = '\0';
  dl->bundle = odl->bundle;
  dl->next = NULL;
  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);
  dl->dial_tries = 0;
  dl->reconnect_tries = 0;
  dl->name = strdup(name);
  dl->parent = odl->parent;
  memcpy(&dl->fsmp, &odl->fsmp, sizeof dl->fsmp);
  authinfo_Init(&dl->pap);
  dl->pap.cfg.fsmretry = odl->pap.cfg.fsmretry;

  authinfo_Init(&dl->chap.auth);
  dl->chap.auth.cfg.fsmretry = odl->chap.auth.cfg.fsmretry;

  if ((dl->physical = modem_Create(dl, PHYS_MANUAL)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }
  memcpy(&dl->physical->cfg, &odl->physical->cfg, sizeof dl->physical->cfg);
  memcpy(&dl->physical->link.lcp.cfg, &odl->physical->link.lcp.cfg,
         sizeof dl->physical->link.lcp.cfg);
  memcpy(&dl->physical->link.ccp.cfg, &odl->physical->link.ccp.cfg,
         sizeof dl->physical->link.ccp.cfg);
  memcpy(&dl->physical->async.cfg, &odl->physical->async.cfg,
         sizeof dl->physical->async.cfg);

  chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

  LogPrintf(LogPHASE, "%s: Created in CLOSED state\n", dl->name);

  return dl;
}

struct datalink *
datalink_Destroy(struct datalink *dl)
{
  struct datalink *result;

  if (dl->state != DATALINK_CLOSED) {
    LogPrintf(LogERROR, "Oops, destroying a datalink in state %s\n",
              datalink_State(dl));
    switch (dl->state) {
      case DATALINK_HANGUP:
      case DATALINK_DIAL:
      case DATALINK_LOGIN:
        chat_Destroy(&dl->chat);	/* Gotta blat the timers ! */
        break;
    }
  }

  result = dl->next;
  modem_Destroy(dl->physical);
  free(dl->name);
  free(dl);

  return result;
}

void
datalink_Up(struct datalink *dl, int runscripts, int packetmode)
{
  if (dl->physical->type & (PHYS_STDIN|PHYS_DEDICATED))
    /* Ignore scripts */
    runscripts = 0;

  switch (dl->state) {
    case DATALINK_CLOSED:
      LogPrintf(LogPHASE, "%s: Entering OPENING state\n", dl->name);
      if (bundle_Phase(dl->bundle) == PHASE_DEAD ||
          bundle_Phase(dl->bundle) == PHASE_TERMINATE)
        bundle_NewPhase(dl->bundle, PHASE_ESTABLISH);
      dl->state = DATALINK_OPENING;
      dl->reconnect_tries =
        dl->physical->type == PHYS_STDIN ? 0 : dl->cfg.reconnect.max;
      dl->dial_tries = dl->cfg.dial.max;
      dl->script.run = runscripts;
      dl->script.packetmode = packetmode;
      break;

    case DATALINK_OPENING:
      if (!dl->script.run && runscripts)
        dl->script.run = 1;
      /* fall through */

    case DATALINK_DIAL:
    case DATALINK_LOGIN:
    case DATALINK_READY:
      if (!dl->script.packetmode && packetmode) {
        dl->script.packetmode = 1;
        if (dl->state == DATALINK_READY)
          datalink_LoginDone(dl);
      }
      break;
  }
}

void
datalink_Close(struct datalink *dl, int stay)
{
  /* Please close */
  switch (dl->state) {
    case DATALINK_OPEN:
      FsmDown(&dl->physical->link.ccp.fsm);
      FsmClose(&dl->physical->link.ccp.fsm);
      /* fall through */

    case DATALINK_AUTH:
    case DATALINK_LCP:
      FsmClose(&dl->physical->link.lcp.fsm);
      if (stay) {
        dl->dial_tries = -1;
        dl->reconnect_tries = 0;
      }
      break;

    default:
      datalink_ComeDown(dl, stay);
  }
}

void
datalink_Down(struct datalink *dl, int stay)
{
  /* Carrier is lost */
  switch (dl->state) {
    case DATALINK_OPEN:
      FsmDown(&dl->physical->link.ccp.fsm);
      FsmClose(&dl->physical->link.ccp.fsm);
      /* fall through */

    case DATALINK_AUTH:
    case DATALINK_LCP:
      FsmDown(&dl->physical->link.lcp.fsm);
      if (stay)
        FsmClose(&dl->physical->link.lcp.fsm);
      else
        FsmOpen(&dl->physical->link.ccp.fsm);
      /* fall through */

    default:
      datalink_ComeDown(dl, stay);
  }
}

void
datalink_StayDown(struct datalink *dl)
{
  dl->reconnect_tries = 0;
}

void
datalink_Show(struct datalink *dl, struct prompt *prompt)
{
  prompt_Printf(prompt, "Name: %s\n", dl->name);
  prompt_Printf(prompt, " State:            %s\n", datalink_State(dl));
  prompt_Printf(prompt, " CHAP Encryption:  %s\n",
                dl->chap.using_MSChap ? "MSChap" : "MD5" );
  prompt_Printf(prompt, "\nDefaults:\n");
  prompt_Printf(prompt, " Phone List:       %s\n", dl->cfg.phone.list);
  if (dl->cfg.dial.max)
    prompt_Printf(prompt, " Dial tries:       %d, delay ", dl->cfg.dial.max);
  else
    prompt_Printf(prompt, " Dial tries:       infinite, delay ");
  if (dl->cfg.dial.next_timeout > 0)
    prompt_Printf(prompt, "%ds/", dl->cfg.dial.next_timeout);
  else
    prompt_Printf(prompt, "random/");
  if (dl->cfg.dial.timeout > 0)
    prompt_Printf(prompt, "%ds\n", dl->cfg.dial.timeout);
  else
    prompt_Printf(prompt, "random\n");
  prompt_Printf(prompt, " Reconnect tries:  %d, delay ", dl->cfg.reconnect.max);
  if (dl->cfg.reconnect.timeout > 0)
    prompt_Printf(prompt, "%ds\n", dl->cfg.reconnect.timeout);
  else
    prompt_Printf(prompt, "random\n");
  prompt_Printf(prompt, " Dial Script:      %s\n", dl->cfg.script.dial);
  prompt_Printf(prompt, " Login Script:     %s\n", dl->cfg.script.login);
  prompt_Printf(prompt, " Hangup Script:    %s\n", dl->cfg.script.hangup);
}

int
datalink_SetReconnect(struct cmdargs const *arg)
{
  if (arg->argc == arg->argn+2) {
    arg->cx->cfg.reconnect.timeout = atoi(arg->argv[arg->argn]);
    arg->cx->cfg.reconnect.max = atoi(arg->argv[arg->argn+1]);
    return 0;
  }
  return -1;
}

int
datalink_SetRedial(struct cmdargs const *arg)
{
  int timeout;
  int tries;
  char *dot;

  if (arg->argc == arg->argn+1 || arg->argc == arg->argn+2) {
    if (strncasecmp(arg->argv[arg->argn], "random", 6) == 0 &&
	(arg->argv[arg->argn][6] == '\0' || arg->argv[arg->argn][6] == '.')) {
      arg->cx->cfg.dial.timeout = -1;
      randinit();
    } else {
      timeout = atoi(arg->argv[arg->argn]);

      if (timeout >= 0)
	arg->cx->cfg.dial.timeout = timeout;
      else {
	LogPrintf(LogWARN, "Invalid redial timeout\n");
	return -1;
      }
    }

    dot = strchr(arg->argv[arg->argn], '.');
    if (dot) {
      if (strcasecmp(++dot, "random") == 0) {
	arg->cx->cfg.dial.next_timeout = -1;
	randinit();
      } else {
	timeout = atoi(dot);
	if (timeout >= 0)
	  arg->cx->cfg.dial.next_timeout = timeout;
	else {
	  LogPrintf(LogWARN, "Invalid next redial timeout\n");
	  return -1;
	}
      }
    } else
      /* Default next timeout */
      arg->cx->cfg.dial.next_timeout = DIAL_NEXT_TIMEOUT;

    if (arg->argc == arg->argn+2) {
      tries = atoi(arg->argv[arg->argn+1]);

      if (tries >= 0) {
	arg->cx->cfg.dial.max = tries;
      } else {
	LogPrintf(LogWARN, "Invalid retry value\n");
	return 1;
      }
    }
    return 0;
  }
  return -1;
}

static const char *states[] = {
  "closed",
  "opening",
  "hangup",
  "dial",
  "login",
  "ready",
  "lcp",
  "auth",
  "open"
};

static const char *
datalink_State(struct datalink *dl)
{
  if (dl->state < 0 || dl->state >= sizeof states / sizeof states[0])
    return "unknown";
  return states[dl->state];
}
