/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: loadalias.h,v 1.4.2.3 1998/05/01 19:25:03 brian Exp $
 */

struct aliasHandlers {
  void *dl;

  char *(*GetFragment)(char *);
  void (*Init)(void);
  int (*In)(char *, int);
  int (*Out)(char *, int);
  struct alias_link *(*RedirectAddr)(struct in_addr, struct in_addr);
  struct alias_link *(*RedirectPort)(struct in_addr, u_short, struct in_addr,
                                     u_short, struct in_addr, u_short, u_char);
  int (*SaveFragment)(char *);
  void (*SetAddress)(struct in_addr);
  unsigned (*SetMode)(unsigned, unsigned);
  void (*FragmentIn)(char *, char *);
};

extern struct aliasHandlers PacketAlias;

#define alias_IsEnabled() (PacketAlias.dl ? 1 : 0)
extern int alias_Load(void);
extern void alias_Unload(void);
