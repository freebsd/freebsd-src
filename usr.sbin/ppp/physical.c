/*
 * Written by Eivind Eklund <eivind@yes.no>
 *    for Yes Interactive
 *
 * Copyright (C) 1998, Yes Interactive.  All rights reserved.
 *
 * Redistribution and use in any form is permitted.  Redistribution in
 * source form should include the above copyright and this set of
 * conditions, because large sections american law seems to have been
 * created by a bunch of jerks on drugs that are now illegal, forcing
 * me to include this copyright-stuff instead of placing this in the
 * public domain.  The name of of 'Yes Interactive' or 'Eivind Eklund'
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  $Id: physical.c,v 1.1.2.19 1998/03/20 19:48:16 brian Exp $
 *
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <sys/tty.h>
#include <sys/uio.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>


/* XXX Name space pollution from vars.h */
#include <netinet/in.h>
#include <alias.h>

#include "defs.h"
#include "command.h"
#include "loadalias.h"

/* XXX Name space pollution from hdlc.h */
#include "mbuf.h"

/* Name space pollution for physical.h */
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "async.h"
#include "ccp.h"
#include "link.h"

#include "descriptor.h"
#include "physical.h"

#include "vars.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "mp.h"
#include "auth.h"
#include "chap.h"
#include "pap.h"
#include "chat.h"
#include "datalink.h"
#include "bundle.h"
#include "log.h"
#include "id.h"

/* External calls - should possibly be moved inline */
extern int IntToSpeed(int);


int
Physical_GetFD(struct physical *phys) {
   return phys->fd;
}

int
Physical_IsATTY(struct physical *phys) {
   return isatty(phys->fd);
}

int
Physical_IsSync(struct physical *phys) {
   return phys->cfg.speed == 0;
}

int
Physical_FD_ISSET(struct physical *phys, fd_set *set) {
   return phys->fd >= 0 && FD_ISSET(phys->fd, set);
}

void
Physical_FD_SET(struct physical *phys, fd_set *set) {
   assert(phys->fd >= 0);
   FD_SET(phys->fd, set);
}


const char *Physical_GetDevice(struct physical *phys)
{
   return phys->name.full;
}

/* XXX-ML - must be moved into the physical struct  */
void
Physical_SetDeviceList(struct physical *phys, const char *new_device_list) {
   strncpy(phys->cfg.devlist, new_device_list, sizeof phys->cfg.devlist - 1);
   phys->cfg.devlist[sizeof phys->cfg.devlist - 1] = '\0';
}


int
Physical_SetSpeed(struct physical *phys, int speed) {
   if (IntToSpeed(speed) != B0) {
      phys->cfg.speed = speed;
      return 1;
   } else {
      return 0;
   }
}

void
Physical_SetSync(struct physical *phys) {
   phys->cfg.speed = 0;
}


int
Physical_SetRtsCts(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->cfg.rts_cts = enable;
   return 1;
}

void
Physical_SetDedicated(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->cfg.is_dedicated = enable;
}

void
Physical_SetDirect(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->cfg.is_direct = enable;
}

int
Physical_IsDirect(struct physical *phys) {
   return phys->cfg.is_direct;
}

int
Physical_IsDedicated(struct physical *phys) {
   return phys->cfg.is_dedicated;
}


void
Physical_DupAndClose(struct physical *phys) {
   int nmodem;

   nmodem = dup(phys->fd);
   close(phys->fd);
   phys->fd = nmodem;
}

/* Encapsulation for a read on the FD.  Avoids some exposure, and
   concentrates control. */
ssize_t
Physical_Read(struct physical *phys, void *buf, size_t nbytes) {
   return read(phys->fd, buf, nbytes);
}

ssize_t
Physical_Write(struct physical *phys, const void *buf, size_t nbytes) {
   return write(phys->fd, buf, nbytes);
}

int
Physical_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e,
                   int *n, int force)
{
  struct physical *p = descriptor2physical(d);
  int sets;

  sets = 0;
  if (p->fd >= 0) {
    if (r) {
      FD_SET(p->fd, r);
      sets++;
    }
    if (e) {
      FD_SET(p->fd, e);
      sets++;
    }
    if (w && (force || link_QueueLen(&p->link))) {
      FD_SET(p->fd, w);
      sets++;
    }
    if (sets && *n < p->fd + 1)
      *n = p->fd + 1;
  }

  return sets;
}

int
Physical_IsSet(struct descriptor *d, const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  return p->fd >= 0 && FD_ISSET(p->fd, fdset);
}

void
Physical_Login(struct physical *phys, const char *name)
{
  if ((mode & MODE_DIRECT) && Physical_IsATTY(phys) && Enabled(ConfUtmp))
    if (phys->Utmp)
      LogPrintf(LogERROR, "Oops, already logged in on %s\n", phys->name.base);
    else {
      struct utmp ut;

      memset(&ut, 0, sizeof ut);
      time(&ut.ut_time);
      strncpy(ut.ut_name, name, sizeof ut.ut_name);
      strncpy(ut.ut_line, phys->name.base, sizeof ut.ut_line - 1);
      ID0login(&ut);
      phys->Utmp = 1;
    }
}

void
Physical_Logout(struct physical *phys)
{
  if (phys->Utmp) {
    ID0logout(phys->name.base);
    phys->Utmp = 0;
  }
}
