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
 *	$Id: datalink.c,v 1.1.2.22 1998/03/13 21:07:30 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <alias.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "loadalias.h"
#include "vars.h"
#include "timer.h"
#include "fsm.h"
#include "lcp.h"
#include "descriptor.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "throughput.h"
#include "link.h"
#include "physical.h"
#include "iplist.h"
#include "ipcp.h"
#include "filter.h"
#include "bundle.h"
#include "chat.h"
#include "ccp.h"
#include "auth.h"
#include "main.h"
#include "modem.h"
#include "prompt.h"
#include "lcpproto.h"
#include "pap.h"
#include "chap.h"
#include "datalink.h"

static const char *datalink_State(struct datalink *);

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
    dl->dial_timer.state = TIMER_STOPPED;
    if (Timeout > 0)
      dl->dial_timer.load = Timeout * SECTICKS;
    else
      dl->dial_timer.load = (random() % DIAL_TIMEOUT) * SECTICKS;
    dl->dial_timer.func = datalink_OpenTimeout;
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
  modem_Close(dl->physical);
  dl->phone.chosen = "[N/A]";

  if (!dl->dial_tries || (dl->dial_tries < 0 && !dl->reconnect_tries)) {
    LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
    dl->state = DATALINK_CLOSED;
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
    bundle_LinkClosed(dl->bundle, dl);
    datalink_StartDialTimer(dl, dl->cfg.dial_timeout);
  } else {
    LogPrintf(LogPHASE, "%s: Re-entering OPENING state\n", dl->name);
    dl->state = DATALINK_OPENING;
    if (dl->dial_tries < 0) {
      datalink_StartDialTimer(dl, dl->cfg.reconnect_timeout);
      dl->dial_tries = dl->cfg.max_dial;
      dl->reconnect_tries--;
    } else {
      dl->dial_tries--;
      if (dl->phone.next == NULL)
        datalink_StartDialTimer(dl, dl->cfg.dial_timeout);
      else
        datalink_StartDialTimer(dl, dl->cfg.dial_next_timeout);
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
    } else
      datalink_HangupDone(dl);
  } else {
    dl->dial_tries = -1;

    lcp_Setup(&dl->lcp, dl->state == DATALINK_READY ? 0 : VarOpenMode);
    ccp_Setup(&dl->ccp);

    LogPrintf(LogPHASE, "%s: Entering LCP state\n", dl->name);
    dl->state = DATALINK_LCP;
    FsmUp(&dl->lcp.fsm);
    FsmOpen(&dl->lcp.fsm);
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
      break;

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
            if (!(mode & MODE_DDIAL) && dl->cfg.max_dial)
              LogPrintf(LogCHAT, "%s: Dial attempt %u of %d\n",
                        dl->name, dl->cfg.max_dial - dl->dial_tries,
                        dl->cfg.max_dial);
          } else
            datalink_LoginDone(dl);
        } else {
          if (!(mode & MODE_DDIAL) && dl->cfg.max_dial)
            LogPrintf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
                      dl->cfg.max_dial - dl->dial_tries, dl->cfg.max_dial);
          else
            LogPrintf(LogCHAT, "Failed to open modem\n");

          if (!(mode & MODE_DDIAL) && dl->cfg.max_dial && dl->dial_tries == 0) {
            LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
            dl->state = DATALINK_CLOSED;
            dl->reconnect_tries = 0;
            dl->dial_tries = -1;
            bundle_LinkClosed(dl->bundle, dl);
          }
          datalink_StartDialTimer(dl, dl->cfg.dial_timeout);
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
          switch(dl->state) {
            case DATALINK_HANGUP:
              datalink_HangupDone(dl);
              break;
            case DATALINK_DIAL:
              LogPrintf(LogPHASE, "%s: Entering LOGIN state\n", dl->name);
              dl->state = DATALINK_LOGIN;
              chat_Init(&dl->chat, dl->physical, dl->cfg.script.login, 0, NULL);
              break;
            case DATALINK_LOGIN:
              datalink_LoginDone(dl);
              break;
          }
          break;
        case CHAT_FAILED:
          /* Going down - script failed */
          LogPrintf(LogWARN, "Chat script failed\n");
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
              break;
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
datalink_IsSet(struct descriptor *d, fd_set *fdset)
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

  if (fp == &dl->lcp.fsm)
    (*dl->parent->LayerStart)(dl->parent->object, fp);
}

static void
datalink_LayerUp(void *v, struct fsm *fp)
{
  /* The given fsm is now up */
  struct datalink *dl = (struct datalink *)v;

  if (fp == &dl->lcp.fsm) {
    dl->lcp.auth_ineed = dl->lcp.want_auth;
    dl->lcp.auth_iwait = dl->lcp.his_auth;
    if (dl->lcp.his_auth || dl->lcp.want_auth) {
      if (dl->bundle->phase == PHASE_DEAD ||
          dl->bundle->phase == PHASE_ESTABLISH)
        bundle_NewPhase(dl->bundle, PHASE_AUTHENTICATE);
      LogPrintf(LogPHASE, "%s: his = %s, mine = %s\n", dl->name,
                Auth2Nam(dl->lcp.his_auth), Auth2Nam(dl->lcp.want_auth));
      if (dl->lcp.his_auth == PROTO_PAP)
        StartAuthChallenge(&dl->pap, dl->physical, SendPapChallenge);
      if (dl->lcp.want_auth == PROTO_CHAP)
        StartAuthChallenge(&dl->chap.auth, dl->physical, SendChapChallenge);
    } else
      datalink_AuthOk(dl);
  }
}

void
datalink_AuthOk(struct datalink *dl)
{
  FsmUp(&dl->ccp.fsm);
  FsmOpen(&dl->ccp.fsm);
  dl->state = DATALINK_OPEN;
  (*dl->parent->LayerUp)(dl->parent->object, &dl->lcp.fsm);
}

void
datalink_AuthNotOk(struct datalink *dl)
{
  dl->state = DATALINK_LCP;
  FsmClose(&dl->lcp.fsm);
}

static void
datalink_LayerDown(void *v, struct fsm *fp)
{
  /* The given FSM has been told to come down */
  struct datalink *dl = (struct datalink *)v;

  if (fp == &dl->lcp.fsm) {
    switch (dl->state) {
      case DATALINK_OPEN:
        FsmDown(&dl->ccp.fsm);
        FsmClose(&dl->ccp.fsm);
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

  if (fp == &dl->lcp.fsm) {
    (*dl->parent->LayerFinish)(dl->parent->object, fp);
    datalink_ComeDown(dl, 0);
  }
}

struct datalink *
datalink_Create(const char *name, struct bundle *bundle,
                const struct fsm_parent *parent)
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

  dl->bundle = bundle;
  dl->next = NULL;

  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);

  dl->dial_tries = 0;
  dl->cfg.max_dial = 1;
  dl->cfg.dial_timeout = DIAL_TIMEOUT;
  dl->cfg.dial_next_timeout = DIAL_NEXT_TIMEOUT;

  dl->reconnect_tries = 0;
  dl->cfg.max_reconnect = 0;
  dl->cfg.reconnect_timeout = RECONNECT_TIMEOUT;

  dl->name = strdup(name);
  if ((dl->physical = modem_Create(dl->name, dl)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }
  chat_Init(&dl->chat, dl->physical, NULL, 1, NULL);

  dl->parent = parent;
  dl->fsm.LayerStart = datalink_LayerStart;
  dl->fsm.LayerUp = datalink_LayerUp;
  dl->fsm.LayerDown = datalink_LayerDown;
  dl->fsm.LayerFinish = datalink_LayerFinish;
  dl->fsm.object = dl;

  lcp_Init(&dl->lcp, dl->bundle, dl->physical, &dl->fsm);
  ccp_Init(&dl->ccp, dl->bundle, &dl->physical->link, &dl->fsm);

  authinfo_Init(&dl->pap);
  authinfo_Init(&dl->chap.auth);

  LogPrintf(LogPHASE, "%s: Created in CLOSED state\n", dl->name);

  return dl;
}

struct datalink *
datalink_Destroy(struct datalink *dl)
{
  struct datalink *result;

  if (dl->state != DATALINK_CLOSED)
    LogPrintf(LogERROR, "Oops, destroying a datalink in state %s\n",
              datalink_State(dl));

  result = dl->next;
  chat_Destroy(&dl->chat);
  link_Destroy(&dl->physical->link);
  free(dl->name);
  free(dl);

  return result;
}

void
datalink_Up(struct datalink *dl, int runscripts, int packetmode)
{
  switch (dl->state) {
    case DATALINK_CLOSED:
      LogPrintf(LogPHASE, "%s: Entering OPENING state\n", dl->name);
      dl->state = DATALINK_OPENING;
      dl->reconnect_tries = dl->cfg.max_reconnect;
      dl->dial_tries = dl->cfg.max_dial;
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
      FsmDown(&dl->ccp.fsm);
      FsmClose(&dl->ccp.fsm);
      /* fall through */

    case DATALINK_AUTH:
    case DATALINK_LCP:
      FsmClose(&dl->lcp.fsm);
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
      FsmDown(&dl->ccp.fsm);
      FsmClose(&dl->ccp.fsm);
      /* fall through */

    case DATALINK_AUTH:
    case DATALINK_LCP:
      FsmDown(&dl->lcp.fsm);
      if (stay)
        FsmClose(&dl->lcp.fsm);
      else
        FsmOpen(&dl->ccp.fsm);
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
datalink_Show(struct datalink *dl)
{
  prompt_Printf(&prompt, "Link %s: State %s\n", dl->name, datalink_State(dl));
}

static char *states[] = {
  "CLOSED",
  "OPENING",
  "HANGUP",
  "DIAL",
  "LOGIN",
  "READY",
  "LCP"
  "AUTH"
  "OPEN"
};

static const char *
datalink_State(struct datalink *dl)
{
  if (dl->state < 0 || dl->state >= sizeof states / sizeof states[0])
    return "unknown";
  return states[dl->state];
}
