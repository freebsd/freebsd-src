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
 *	$Id: datalink.c,v 1.1.2.1 1998/02/15 23:59:55 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <alias.h>
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
#include "hdlc.h"
#include "async.h"
#include "throughput.h"
#include "link.h"
#include "physical.h"
#include "bundle.h"
#include "chat.h"
#include "datalink.h"
#include "ccp.h"
#include "main.h"
#include "modem.h"
#include "iplist.h"
#include "ipcp.h"

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
      dl->dial_timer.load = (random() % REDIAL_PERIOD) * SECTICKS;
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

  if (!dl->dial_tries || (dl->dial_tries < 0 && !dl->reconnect_tries)) {
    LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
    dl->state = DATALINK_CLOSED;
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
    bundle_LinkClosed(dl->bundle, dl);
  } else {
    LogPrintf(LogPHASE, "%s: Entering OPENING state\n", dl->name);
    dl->state = DATALINK_OPENING;
  }

  if (VarNextPhone == NULL)
    datalink_StartDialTimer(dl, VarRedialTimeout);
  else
    datalink_StartDialTimer(dl, VarRedialNextTimeout);
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
          LogPrintf(LogPHASE, "%s: Entering DIAL state\n", dl->name);
          dl->state = DATALINK_DIAL;
          chat_Init(&dl->chat, dl->physical, VarDialScript, 1);
          if (!(mode & MODE_DDIAL) && VarDialTries)
            LogPrintf(LogCHAT, "%s: Dial attempt %u of %d\n",
                      dl->name, VarDialTries - dl->dial_tries, VarDialTries);
        } else {
          if (!(mode & MODE_DDIAL) && VarDialTries)
            LogPrintf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
                      VarDialTries - dl->dial_tries, VarDialTries);
          else
            LogPrintf(LogCHAT, "Failed to open modem\n");

          if (!(mode & MODE_DDIAL) && VarDialTries && dl->dial_tries == 0) {
            LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);
            dl->state = DATALINK_CLOSED;
            dl->reconnect_tries = 0;
            dl->dial_tries = -1;
            if (mode & MODE_BACKGROUND)
              CleaningUp = 1;
          } else
            datalink_StartDialTimer(dl, VarRedialTimeout);
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
              chat_Init(&dl->chat, dl->physical, VarLoginScript, 0);
              break;
            case DATALINK_LOGIN:
              dl->dial_tries = 0;
              if (modem_Raw(dl->physical, dl->bundle) < 0) {
                LogPrintf(LogWARN, "PacketMode: Not connected.\n");
                LogPrintf(LogPHASE, "%s: Entering HANGUP state\n", dl->name);
                dl->state = DATALINK_HANGUP;
                modem_Offline(dl->physical);
                chat_Init(&dl->chat, dl->physical, VarHangupScript, 1);
              } else {
                LogPrintf(LogPHASE, "%s: Entering OPEN state\n", dl->name);
                dl->state = DATALINK_OPEN;
                dl->dial_tries = -1;
                PacketMode(dl->bundle, VarOpenMode);
              }
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
              chat_Init(&dl->chat, dl->physical, VarHangupScript, 1);
              break;
          }
          break;
      }
      break;
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
      break;
    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      return descriptor_IsSet(&dl->chat.desc, fdset);
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
      break;
    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      descriptor_Read(&dl->chat.desc, bundle, fdset);
      break;
    case DATALINK_OPEN:
      descriptor_Read(&dl->physical->desc, bundle, fdset);
      break;
  }
}

static void
datalink_Write(struct descriptor *d, const fd_set *fdset)
{
  struct datalink *dl = descriptor2datalink(d);

  switch (dl->state) {
    case DATALINK_CLOSED:
      break;
    case DATALINK_HANGUP:
    case DATALINK_DIAL:
    case DATALINK_LOGIN:
      descriptor_Write(&dl->chat.desc, fdset);
      break;
    case DATALINK_OPEN:
      descriptor_Write(&dl->physical->desc, fdset);
      break;
  }
}

struct datalink *
datalink_Create(const char *name, struct bundle *bundle)
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
  dl->bundle = bundle;
  dl->next = NULL;
  memset(&dl->dial_timer, '\0', sizeof dl->dial_timer);
  dl->dial_tries = 0;
  dl->name = strdup(name);
  if ((dl->physical = modem_Create(dl->name)) == NULL) {
    free(dl->name);
    free(dl);
    return NULL;
  }

  IpcpDefAddress();
  LcpInit(dl->bundle, dl->physical);
  CcpInit(dl->bundle, &dl->physical->link);

  LogPrintf(LogPHASE, "%s: Entering CLOSED state\n", dl->name);

  return dl;
}

struct datalink *
datalink_Destroy(struct datalink *dl)
{
  struct datalink *result;

  if (dl->state != DATALINK_CLOSED)
    LogPrintf(LogERROR, "Oops, destroying a datalink in state %d\n", dl->state);

  result = dl->next;
  chat_Destroy(&dl->chat);
  link_Destroy(&dl->physical->link);
  free(dl->name);
  free(dl);

  return result;
}

void
datalink_Up(struct datalink *dl)
{
  if (dl->state == DATALINK_CLOSED) {
    LogPrintf(LogPHASE, "%s: Entering OPENING state\n", dl->name);
    dl->state = DATALINK_OPENING;
    dl->reconnect_tries = VarReconnectTries;
    dl->dial_tries = VarDialTries;
  }
}

void
datalink_Close(struct datalink *dl, int stay)
{
  /* Please close */
  FsmClose(&CcpInfo.fsm);
  FsmClose(&LcpInfo.fsm);
  if (stay) {
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
  }
}

void
datalink_Down(struct datalink *dl, int stay)
{
  /* Carrier is lost */
  LogPrintf(LogPHASE, "datalink_Down: %sstay down\n", stay ? "" : "don't ");
  FsmDown(&CcpInfo.fsm);
  FsmClose(&CcpInfo.fsm);
  FsmDown(&LcpInfo.fsm);
  if (CleaningUp || stay)
    FsmClose(&LcpInfo.fsm);
  else
    FsmOpen(&CcpInfo.fsm);

  if (dl->state != DATALINK_CLOSED && dl->state != DATALINK_HANGUP) {
    LogPrintf(LogPHASE, "%s: Entering HANGUP state\n", dl->name);
    dl->state = DATALINK_HANGUP;
    modem_Offline(dl->physical);
    chat_Init(&dl->chat, dl->physical, VarHangupScript, 1);
  }

  if (stay) {
    dl->dial_tries = -1;
    dl->reconnect_tries = 0;
  }
}

void
datalink_StayDown(struct datalink *dl)
{
  dl->reconnect_tries = 0;
}
