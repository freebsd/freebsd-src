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
 *  $Id: physical.h,v 1.1.2.3 1998/02/02 19:33:39 brian Exp $
 *
 */

struct physical {
  struct link link;
  struct async async;          /* Our async state */
  int fd;                      /* File descriptor for this device */
  int mbits;                   /* Current DCD status */
  unsigned dev_is_modem : 1;   /* Is the device an actual modem?
                                  Faked for sync devices, though...
                                  (Possibly this should be
                                  dev_is_not_tcp?) XXX-ML */

  unsigned is_dedicated : 1;   /* Dedicated mode?  XXX-ML - not yet initialized */
  unsigned is_direct : 1;      /* Direct mode?  XXX-ML - not yet initialized */
  struct mbuf *out;
  int connect_count;

  /* XXX-ML Most of the below is device specific, and probably do not
      belong in the generic physical struct. It comes from modem.c. */
  unsigned rts_cts : 1;        /* Is rts/cts enabled? */
  unsigned int parity;         /* What parity is enabled? (TTY flags) */
  unsigned int speed;          /* Modem speed */
  struct termios ios;          /* To be able to reset from raw mode */
};

#define physical2link(p) ((struct link *)p)
#define link2physical(l) (l->type == PHYSICAL_LINK ? (struct physical *)l : 0)

int Physical_GetFD(struct physical *);
int Physical_IsATTY(struct physical *);
int Physical_IsSync(struct physical *);
int Physical_IsDedicated(struct physical *phys);
int Physical_IsDirect(struct physical *phys);
const char *Physical_GetDevice(struct physical *);


void Physical_SetDevice(struct physical *phys, const char *new_device_list);
int /* Was this speed OK? */
Physical_SetSpeed(struct physical *phys, int speed);

/* XXX-ML I'm not certain this is the right way to handle this, but we
   can solve that later. */
void Physical_SetSync(struct physical *phys);

int /* Can this be set?  (Might not be a relevant attribute for this
       device, for instance) */
Physical_SetRtsCts(struct physical *phys, int enable);

void Physical_SetDedicated(struct physical *phys, int enable);
void Physical_SetDirect(struct physical *phys, int enable);

void Physical_FD_SET(struct physical *, fd_set *);
int Physical_FD_ISSET(struct physical *, fd_set *);

void Physical_DupAndClose(struct physical *);
ssize_t Physical_Read(struct physical *phys, void *buf, size_t nbytes);
ssize_t Physical_Write(struct physical *phys, const void *buf, size_t nbytes);
int Physical_ReportProtocolStatus(struct cmdargs const *);
