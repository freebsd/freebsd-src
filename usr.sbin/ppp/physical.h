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
 *  $Id: physical.h,v 1.1.2.26 1998/05/15 23:58:26 brian Exp $
 *
 */

struct bundle;

struct physical {
  struct link link;
  struct descriptor desc;
  int type;                    /* What sort of PHYS_* link are we ? */
  struct async async;          /* Our async state */
  struct hdlc hdlc;            /* Our hdlc state */
  int fd;                      /* File descriptor for this device */
  int mbits;                   /* Current DCD status */
  unsigned dev_is_modem : 1;   /* Is the device an actual modem?
                                  Faked for sync devices, though...
                                  (Possibly this should be
                                  dev_is_not_tcp?) XXX-ML */

  struct mbuf *out;            /* mbuf that suffered a short write */
  int connect_count;
  struct datalink *dl;         /* my owner */

  struct {
    char full[40];
    char *base;
  } name;

  unsigned Utmp : 1;           /* Are we in utmp ? */

  /* XXX-ML Most of the below is device specific, and probably do not
      belong in the generic physical struct. It comes from modem.c. */

  struct {
    unsigned rts_cts : 1;      /* Is rts/cts enabled? */
    unsigned parity;           /* What parity is enabled? (TTY flags) */
    unsigned speed;            /* Modem speed */
    char devlist[LINE_LEN];    /* Comma-separated list of devices */
  } cfg;

  struct termios ios;          /* To be able to reset from raw mode */

  struct pppTimer Timer;       /* CD checks */
};

#define field2phys(fp, name) \
  ((struct physical *)((char *)fp - (int)(&((struct physical *)0)->name)))

#define link2physical(l) \
  ((l)->type == PHYSICAL_LINK ? field2phys(l, link) : NULL)

#define descriptor2physical(d) \
  ((d)->type == PHYSICAL_DESCRIPTOR ? field2phys(d, desc) : NULL)

extern int physical_GetFD(struct physical *);
extern int physical_IsATTY(struct physical *);
extern int physical_IsSync(struct physical *);
extern const char *physical_GetDevice(struct physical *);
extern void physical_SetDeviceList(struct physical *, int, const char *const *);
extern int physical_SetSpeed(struct physical *, int);

/*
 * XXX-ML I'm not certain this is the right way to handle this, but we
 * can solve that later.
 */
extern void physical_SetSync(struct physical *);

/*
 * Can this be set?  (Might not be a relevant attribute for this
 * device, for instance)
 */
extern int physical_SetRtsCts(struct physical *, int);

extern ssize_t physical_Read(struct physical *, void *, size_t);
extern ssize_t physical_Write(struct physical *, const void *, size_t);
extern int physical_UpdateSet(struct descriptor *, fd_set *, fd_set *,
                              fd_set *, int *, int);
extern int physical_IsSet(struct descriptor *, const fd_set *);
extern void physical_Login(struct physical *, const char *);
extern void physical_Logout(struct physical *);
extern int physical_RemoveFromSet(struct physical *, fd_set *, fd_set *,
                                  fd_set *);
extern int physical_SetMode(struct physical *, int);
