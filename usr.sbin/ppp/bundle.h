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
 *	$Id: bundle.h,v 1.1.2.1 1998/02/02 19:32:02 brian Exp $
 */

struct bundle {
  int unit;
  int ifIndex;
  int tun_fd;
  char dev[20];
  char *ifname;
  int routing_seq;

  /* These belong at the NCP level */
  int linkup;
  struct in_addr if_mine, if_peer;
};

extern struct bundle *bundle_Create(const char *dev);
extern int  bundle_InterfaceDown(struct bundle *);
extern int  bundle_SetIPaddress(struct bundle *, struct in_addr,
                                struct in_addr);
extern int  bundle_TrySetIPaddress(struct bundle *, struct in_addr,
                                   struct in_addr);
extern void bundle_Linkup(struct bundle *);
extern int  bundle_LinkIsUp(const struct bundle *);
extern void bundle_Linkdown(struct bundle *);
extern void bundle_SetRoute(struct bundle *, int, struct in_addr,
                            struct in_addr, struct in_addr, int);
