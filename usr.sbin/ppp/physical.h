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
 *  $Id: physical.h,v 1.1.2.10 1998/02/18 19:35:58 brian Exp $
 *
 */

struct physical {
  struct link link;
  struct descriptor desc;
  struct async async;          /* Our async state */
  struct hdlc hdlc;            /* Our hdlc state */
  int fd;                      /* File descriptor for this device */
  int mbits;                   /* Current DCD status */
  unsigned abort : 1;          /* Something's gone horribly wrong */
  unsigned dev_is_modem : 1;   /* Is the device an actual modem?
                                  Faked for sync devices, though...
                                  (Possibly this should be
                                  dev_is_not_tcp?) XXX-ML */

  struct mbuf *out;
  int connect_count;

  /* XXX-ML Most of the below is device specific, and probably do not
      belong in the generic physical struct. It comes from modem.c. */

  struct {
    unsigned is_dedicated : 1; /* Dedicated mode?  XXX-ML - not yet used */
    unsigned is_direct : 1;    /* Direct mode?  XXX-ML - not yet used */
    unsigned rts_cts : 1;      /* Is rts/cts enabled? */
    unsigned int parity;       /* What parity is enabled? (TTY flags) */
    unsigned int speed;        /* Modem speed */
  } cfg;

  struct termios ios;          /* To be able to reset from raw mode */
};

#define field2phys(fp, name) \
  ((struct physical *)((char *)fp - (int)(&((struct physical *)0)->name)))

#define physical2link(p) (&(p)->link)
#define link2physical(l) \
  ((l)->type == PHYSICAL_LINK ? field2phys(l, link) : NULL)

#define physical2descriptor(p) (&(p)->desc)
#define descriptor2physical(d) \
  ((d)->type == PHYSICAL_DESCRIPTOR ? field2phys(d, desc) : NULL)

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
int Physical_UpdateSet(struct descriptor *, fd_set *, fd_set *, fd_set *,
                       int *, int);
int Physical_IsSet(struct descriptor *, fd_set *);
void Physical_DescriptorWrite(struct descriptor *, const fd_set *);
