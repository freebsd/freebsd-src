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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <arpa/inet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "iplist.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "slcompress.h"
#include "descriptor.h"
#include "ipcp.h"
#include "filter.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "prompt.h"
#include "iface.h"


static int
bitsinmask(struct in_addr mask)
{
  u_int32_t bitmask, maskaddr;
  int bits;

  bitmask = 0xffffffff;
  maskaddr = ntohl(mask.s_addr);
  for (bits = 32; bits >= 0; bits--) {
    if (maskaddr == bitmask)
      break;
    bitmask &= ~(1 << (32 - bits));
  }

  return bits;
}

struct iface *
iface_Create(const char *name)
{
  int mib[6], s;
  size_t needed, namelen;
  char *buf, *ptr, *end;
  struct if_msghdr *ifm;
  struct ifa_msghdr *ifam;
  struct sockaddr_dl *dl;
  struct sockaddr *sa[RTAX_MAX];
  struct iface *iface;
  struct iface_addr *addr;

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    fprintf(stderr, "iface_Create: socket(): %s\n", strerror(errno));
    return NULL;
  }

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_IFLIST;
  mib[5] = 0;

  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    fprintf(stderr, "iface_Create: sysctl: estimate: %s\n",
              strerror(errno));
    close(s);
    return NULL;
  }

  if ((buf = (char *)malloc(needed)) == NULL) {
    fprintf(stderr, "iface_Create: malloc failed: %s\n", strerror(errno));
    close(s);
    return NULL;
  }

  if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
    fprintf(stderr, "iface_Create: sysctl: %s\n", strerror(errno));
    free(buf);
    close(s);
    return NULL;
  }

  ptr = buf;
  end = buf + needed;
  iface = NULL;
  namelen = strlen(name);

  while (ptr < end && iface == NULL) {
    ifm = (struct if_msghdr *)ptr;			/* On if_msghdr */
    if (ifm->ifm_type != RTM_IFINFO)
      break;
    dl = (struct sockaddr_dl *)(ifm + 1);		/* Single _dl at end */
    if (dl->sdl_nlen == namelen && !strncmp(name, dl->sdl_data, namelen)) {
      iface = (struct iface *)malloc(sizeof *iface);
      if (iface == NULL) {
        fprintf(stderr, "iface_Create: malloc: %s\n", strerror(errno));
        return NULL;
      }
      iface->name = strdup(name);
      iface->flags = ifm->ifm_flags;
      iface->index = ifm->ifm_index;
      iface->in_addrs = 0;
      iface->in_addr = NULL;
    }
    ptr += ifm->ifm_msglen;				/* First ifa_msghdr */
    for (; ptr < end; ptr += ifam->ifam_msglen) {
      ifam = (struct ifa_msghdr *)ptr;			/* Next if address */

      if (ifam->ifam_type != RTM_NEWADDR)		/* finished this if */
        break;

      if (iface != NULL && ifam->ifam_addrs & RTA_IFA) {
        /* Found a configured interface ! */
        iface_ParseHdr(ifam, sa);

        if (sa[RTAX_IFA] && sa[RTAX_IFA]->sa_family == AF_INET) {
          /* Record the address */

          addr = (struct iface_addr *)realloc
            (iface->in_addr, (iface->in_addrs + 1) * sizeof iface->in_addr[0]);
          if (addr == NULL)
            break;
          iface->in_addr = addr;

          addr += iface->in_addrs;
          iface->in_addrs++;

          addr->ifa = ((struct sockaddr_in *)sa[RTAX_IFA])->sin_addr;

          if (sa[RTAX_BRD])
            addr->brd = ((struct sockaddr_in *)sa[RTAX_BRD])->sin_addr;
          else
            addr->brd.s_addr = INADDR_ANY;

          if (sa[RTAX_NETMASK])
            addr->mask = ((struct sockaddr_in *)sa[RTAX_NETMASK])->sin_addr;
          else
            addr->mask.s_addr = INADDR_ANY;

          addr->bits = bitsinmask(addr->mask);
        }
      }
    }
  }

  free(buf);
  close(s);

  return iface;
}

static void
iface_addr_Zap(const char *name, struct iface_addr *addr)
{
  struct ifaliasreq ifra;
  struct sockaddr_in *me, *peer;
  int s;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0)
    log_Printf(LogERROR, "iface_addr_Zap: socket(): %s\n", strerror(errno));
  else {
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, name, sizeof ifra.ifra_name - 1);
    me = (struct sockaddr_in *)&ifra.ifra_addr;
    peer = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    me->sin_family = peer->sin_family = AF_INET;
    me->sin_len = peer->sin_len = sizeof(struct sockaddr_in);
    me->sin_addr = addr->ifa;
    peer->sin_addr = addr->brd;
    log_Printf(LogDEBUG, "Delete %s\n", inet_ntoa(addr->ifa));
    if (ID0ioctl(s, SIOCDIFADDR, &ifra) < 0)
      log_Printf(LogWARN, "iface_addr_Zap: ioctl(SIOCDIFADDR, %s): %s\n",
                 inet_ntoa(addr->ifa), strerror(errno));
    close(s);
  }
}

void
iface_inClear(struct iface *iface, int how)
{
  int n, addrs;

  if (iface->in_addrs) {
    addrs = n = how == IFACE_CLEAR_ALL ? 0 : 1;
    for (; n < iface->in_addrs; n++)
      iface_addr_Zap(iface->name, iface->in_addr + n);

    iface->in_addrs = addrs;
    /* Don't bother realloc()ing - we have little to gain */
  }
}

int
iface_inAdd(struct iface *iface, struct in_addr ifa, struct in_addr mask,
            struct in_addr brd, int how)
{
  int slot, s, chg, nochange;
  struct ifaliasreq ifra;
  struct sockaddr_in *me, *peer, *msk;
  struct iface_addr *addr;

  for (slot = 0; slot < iface->in_addrs; slot++)
    if (iface->in_addr[slot].ifa.s_addr == ifa.s_addr) {
      if (how & IFACE_FORCE_ADD)
        break;
      else
        /* errno = EEXIST; */
        return 0;
    }

  addr = (struct iface_addr *)realloc
    (iface->in_addr, (iface->in_addrs + 1) * sizeof iface->in_addr[0]);
  if (addr == NULL) {
    log_Printf(LogERROR, "iface_inAdd: realloc: %s\n", strerror(errno));
    return 0;
  }
  iface->in_addr = addr;

  /*
   * We've gotta be careful here.  If we try to add an address with the
   * same destination as an existing interface, nothing will work.
   * Instead, we tweak all previous address entries that match the
   * to-be-added destination to 255.255.255.255 (w/ a similar netmask).
   * There *may* be more than one - if the user has ``iface add''ed
   * stuff previously.
   */
  nochange = 0;
  s = -1;
  for (chg = 0; chg < iface->in_addrs; chg++) {
    if ((iface->in_addr[chg].brd.s_addr == brd.s_addr &&
         brd.s_addr != INADDR_BROADCAST) || chg == slot) {
      /*
       * If we've found an entry that exactly matches what we want to add,
       * don't remove it and then add it again.  If we do, it's possible
       * that the kernel will (correctly) ``tidy up'' any routes that use
       * the IP number as a destination.
       */
      if (chg == slot && iface->in_addr[chg].mask.s_addr == mask.s_addr) {
        nochange = 1;
        continue;
      }
      if (s == -1 && (s = ID0socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        log_Printf(LogERROR, "iface_inAdd: socket(): %s\n", strerror(errno));
        return 0;
      }

      memset(&ifra, '\0', sizeof ifra);
      strncpy(ifra.ifra_name, iface->name, sizeof ifra.ifra_name - 1);
      me = (struct sockaddr_in *)&ifra.ifra_addr;
      msk = (struct sockaddr_in *)&ifra.ifra_mask;
      peer = (struct sockaddr_in *)&ifra.ifra_broadaddr;
      me->sin_family = msk->sin_family = peer->sin_family = AF_INET;
      me->sin_len = msk->sin_len = peer->sin_len = sizeof(struct sockaddr_in);
      me->sin_addr = iface->in_addr[chg].ifa;
      msk->sin_addr = iface->in_addr[chg].mask;
      peer->sin_addr = iface->in_addr[chg].brd;
      log_Printf(LogDEBUG, "Delete %s\n", inet_ntoa(me->sin_addr));
      ID0ioctl(s, SIOCDIFADDR, &ifra);	/* Don't care if it fails... */
      if (chg != slot) {
        peer->sin_addr.s_addr = iface->in_addr[chg].brd.s_addr =
          msk->sin_addr.s_addr = iface->in_addr[chg].mask.s_addr =
            INADDR_BROADCAST;
        iface->in_addr[chg].bits = 32;
        log_Printf(LogDEBUG, "Add %s -> 255.255.255.255\n",
                   inet_ntoa(me->sin_addr));
        if (ID0ioctl(s, SIOCAIFADDR, &ifra) < 0 && errno != EEXIST) {
          /* Oops - that's bad(ish) news !  We've lost an alias ! */
          log_Printf(LogERROR, "iface_inAdd: ioctl(SIOCAIFADDR): %s: %s\n",
               inet_ntoa(me->sin_addr), strerror(errno));
          iface->in_addrs--;
          bcopy(iface->in_addr + chg + 1, iface->in_addr + chg,
                (iface->in_addrs - chg) * sizeof iface->in_addr[0]);
          if (slot > chg)
            slot--;
          chg--;
        }
      }
    }
  }

  if (!nochange) {
    if (s == -1 && (s = ID0socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      log_Printf(LogERROR, "iface_inAdd: socket(): %s\n", strerror(errno));
      return 0;
    }
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, iface->name, sizeof ifra.ifra_name - 1);
    me = (struct sockaddr_in *)&ifra.ifra_addr;
    msk = (struct sockaddr_in *)&ifra.ifra_mask;
    peer = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    me->sin_family = msk->sin_family = peer->sin_family = AF_INET;
    me->sin_len = msk->sin_len = peer->sin_len = sizeof(struct sockaddr_in);
    me->sin_addr = ifa;
    msk->sin_addr = mask;
    peer->sin_addr = brd;

    if (log_IsKept(LogDEBUG)) {
      char buf[16];

      strncpy(buf, inet_ntoa(brd), sizeof buf-1);
      buf[sizeof buf - 1] = '\0';
      log_Printf(LogDEBUG, "Add %s -> %s\n", inet_ntoa(ifa), buf);
    }

    /* An EEXIST failure w/ brd == INADDR_BROADCAST is ok (and works!) */
    if (ID0ioctl(s, SIOCAIFADDR, &ifra) < 0 &&
        (brd.s_addr != INADDR_BROADCAST || errno != EEXIST)) {
      log_Printf(LogERROR, "iface_inAdd: ioctl(SIOCAIFADDR): %s: %s\n",
                 inet_ntoa(ifa), strerror(errno));
      ID0ioctl(s, SIOCDIFADDR, &ifra);	/* EEXIST ? */
      close(s);
      return 0;
    }
  }

  if (s != -1)
    close(s);

  if (slot == iface->in_addrs) {
    /* We're adding a new interface address */

    if (how & IFACE_ADD_FIRST) {
      /* Stuff it at the start of our list */
      slot = 0;
      bcopy(iface->in_addr, iface->in_addr + 1,
            iface->in_addrs * sizeof iface->in_addr[0]);
    }

    iface->in_addrs++;
  } else if (how & IFACE_ADD_FIRST) {
    /* Shift it up to the first slot */
    bcopy(iface->in_addr, iface->in_addr + 1, slot * sizeof iface->in_addr[0]);
    slot = 0;
  }

  iface->in_addr[slot].ifa = ifa;
  iface->in_addr[slot].mask = mask;
  iface->in_addr[slot].brd = brd;
  iface->in_addr[slot].bits = bitsinmask(iface->in_addr[slot].mask);

  return 1;
}

int
iface_inDelete(struct iface *iface, struct in_addr ip)
{
  int n;

  for (n = 0; n < iface->in_addrs; n++)
    if (iface->in_addr[n].ifa.s_addr == ip.s_addr) {
      iface_addr_Zap(iface->name, iface->in_addr + n);
      bcopy(iface->in_addr + n + 1, iface->in_addr + n,
            (iface->in_addrs - n - 1) * sizeof iface->in_addr[0]);
      iface->in_addrs--;
      return 1;
    }

  return 0;
}

#define IFACE_ADDFLAGS 1
#define IFACE_DELFLAGS 2

static int
iface_ChangeFlags(struct iface *iface, int flags, int how)
{
  struct ifreq ifrq;
  int s;

  s = ID0socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "iface_ChangeFlags: socket: %s\n", strerror(errno));
    return 0;
  }

  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, iface->name, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "iface_ChangeFlags: ioctl(SIOCGIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return 0;
  }

  if (how == IFACE_ADDFLAGS)
    ifrq.ifr_flags |= flags;
  else
    ifrq.ifr_flags &= ~flags;

  if (ID0ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "iface_ChangeFlags: ioctl(SIOCSIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return 0;
  }
  close(s);

  return 1;	/* Success */
}

int
iface_SetFlags(struct iface *iface, int flags)
{
  return iface_ChangeFlags(iface, flags, IFACE_ADDFLAGS);
}

int
iface_ClearFlags(struct iface *iface, int flags)
{
  return iface_ChangeFlags(iface, flags, IFACE_DELFLAGS);
}

void
iface_Destroy(struct iface *iface)
{
  /*
   * iface_Clear(iface, IFACE_CLEAR_ALL) must be called manually
   * if that's what the user wants.  It's better to leave the interface
   * allocated so that existing connections can continue to work.
   */

  if (iface != NULL) {
    free(iface->name);
    free(iface->in_addr);
    free(iface);
  }
}

#define if_entry(x) { IFF_##x, #x }

struct {
  int flag;
  const char *value;
} if_flags[] = {
  if_entry(UP),
  if_entry(BROADCAST),
  if_entry(DEBUG),
  if_entry(LOOPBACK),
  if_entry(POINTOPOINT),
  if_entry(RUNNING),
  if_entry(NOARP),
  if_entry(PROMISC),
  if_entry(ALLMULTI),
  if_entry(OACTIVE),
  if_entry(SIMPLEX),
  if_entry(LINK0),
  if_entry(LINK1),
  if_entry(LINK2),
  if_entry(MULTICAST),
  { 0, "???" }
};

int
iface_Show(struct cmdargs const *arg)
{
  struct iface *iface = arg->bundle->iface, *current;
  int f, flags;

  current = iface_Create(iface->name);
  flags = iface->flags = current->flags;
  iface_Destroy(current);

  prompt_Printf(arg->prompt, "%s (idx %d) <", iface->name, iface->index);
  for (f = 0; f < sizeof if_flags / sizeof if_flags[0]; f++)
    if ((if_flags[f].flag & flags) || (!if_flags[f].flag && flags)) {
      prompt_Printf(arg->prompt, "%s%s", flags == iface->flags ? "" : ",",
                    if_flags[f].value);
      flags &= ~if_flags[f].flag;
    }
  prompt_Printf(arg->prompt, "> mtu %d has %d address%s:\n", arg->bundle->mtu,
                iface->in_addrs, iface->in_addrs == 1 ? "" : "es");

  for (f = 0; f < iface->in_addrs; f++) {
    prompt_Printf(arg->prompt, "  %s", inet_ntoa(iface->in_addr[f].ifa));
    if (iface->in_addr[f].bits >= 0)
      prompt_Printf(arg->prompt, "/%d", iface->in_addr[f].bits);
    if (iface->flags & IFF_POINTOPOINT)
      prompt_Printf(arg->prompt, " -> %s", inet_ntoa(iface->in_addr[f].brd));
    else if (iface->flags & IFF_BROADCAST)
      prompt_Printf(arg->prompt, " broadcast %s",
                    inet_ntoa(iface->in_addr[f].brd));
    if (iface->in_addr[f].bits < 0)
      prompt_Printf(arg->prompt, " (mask %s)",
                    inet_ntoa(iface->in_addr[f].mask));
    prompt_Printf(arg->prompt, "\n");
  }

  return 0;
}

void
iface_ParseHdr(struct ifa_msghdr *ifam, struct sockaddr *sa[RTAX_MAX])
{
  char *wp;
  int rtax;

  wp = (char *)(ifam + 1);

  for (rtax = 0; rtax < RTAX_MAX; rtax++)
    if (ifam->ifam_addrs & (1 << rtax)) {
      sa[rtax] = (struct sockaddr *)wp;
      wp += ROUNDUP(sa[rtax]->sa_len);
    } else
      sa[rtax] = NULL;
}
