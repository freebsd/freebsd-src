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
 *  $Id$
 *
 */

#include <sys/param.h>
#include <sys/tty.h>
#include <sys/uio.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


/* XXX Name space pollution from vars.h */
#include <netinet/in.h>
#include <alias.h>
#include "defs.h"
#include "command.h"
#include "loadalias.h"

/* XXX Name space pollution from hdlc.h */
#include "mbuf.h"

/* Name space pollution for physical.h */
#include "hdlc.h"
#include "timer.h"
#include "throughput.h"

#define PHYSICAL_DEVICE
#include "physical.h"


#include "vars.h"

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
Physical_IsActive(struct physical *phys) {
   return phys->fd >= 0;
}

int
Physical_IsSync(struct physical *phys) {
   return phys->speed == 0;
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


/* XXX-ML - must be moved into the physical struct  */
const char *Physical_GetDevice(struct physical *phys) {
   return VarDevice;
}

/* XXX-ML - must be moved into the physical struct  */
void
Physical_SetDevice(struct physical *phys, const char *new_device_list) {
   strncpy(VarDeviceList, new_device_list, sizeof VarDeviceList - 1);
   VarDeviceList[sizeof VarDeviceList - 1] = '\0';
}


int
Physical_SetSpeed(struct physical *phys, int speed) {
   if (IntToSpeed(speed) != B0) {
      phys->speed = speed;
      return 1;
   } else {
	  return 0;
   }
}

void
Physical_SetSync(struct physical *phys) {
   phys->speed = 0;
}


int
Physical_SetRtsCts(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->rts_cts = enable;
   return 1;
}

void
Physical_SetDedicated(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->is_dedicated = enable;
}

void
Physical_SetDirect(struct physical *phys, int enable) {
   assert(enable == 0 || enable == 1);

   phys->is_direct = enable;
}

int
Physical_IsDirect(struct physical *phys) {
   return phys->is_direct;
}

int
Physical_IsDedicated(struct physical *phys) {
   return phys->is_dedicated;
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
