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
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#ifndef NOINET6
#include <netinet6/nd6.h>
#endif
#include <sys/un.h>

#include <errno.h>
#include <string.h>
#include <stdarg.h>
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
#include "ncpaddr.h"
#include "ipcp.h"
#include "filter.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "ipv6cp.h"
#include "ncp.h"
#include "bundle.h"
#include "prompt.h"
#include "iface.h"


struct iface *
iface_Create(const char *name)
{
  int mib[6], maxtries, err;
  size_t needed, namelen;
  char *buf, *ptr, *end;
  struct if_msghdr *ifm;
  struct ifa_msghdr *ifam;
  struct sockaddr_dl *dl;
  struct sockaddr *sa[RTAX_MAX];
  struct iface *iface;
  struct iface_addr *addr;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_IFLIST;
  mib[5] = 0;

  maxtries = 20;
  err = 0;
  do {
    if (maxtries-- == 0 || (err && err != ENOMEM)) {
      fprintf(stderr, "iface_Create: sysctl: %s\n", strerror(err));
      return NULL;
    }

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
      fprintf(stderr, "iface_Create: sysctl: estimate: %s\n",
                strerror(errno));
      return NULL;
    }

    if ((buf = (char *)malloc(needed)) == NULL) {
      fprintf(stderr, "iface_Create: malloc failed: %s\n", strerror(errno));
      return NULL;
    }

    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
      err = errno;
      free(buf);
      buf = NULL;
    }
  } while (buf == NULL);

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
      iface->index = ifm->ifm_index;
      iface->flags = ifm->ifm_flags;
      iface->mtu = 0;
      iface->addrs = 0;
      iface->addr = NULL;
    }
    ptr += ifm->ifm_msglen;				/* First ifa_msghdr */
    for (; ptr < end; ptr += ifam->ifam_msglen) {
      ifam = (struct ifa_msghdr *)ptr;			/* Next if address */

      if (ifam->ifam_type != RTM_NEWADDR)		/* finished this if */
        break;

      if (iface != NULL && ifam->ifam_addrs & RTA_IFA) {
        /* Found a configured interface ! */
        iface_ParseHdr(ifam, sa);

        if (sa[RTAX_IFA] && (sa[RTAX_IFA]->sa_family == AF_INET
#ifndef NOINET6
                             || sa[RTAX_IFA]->sa_family == AF_INET6
#endif
                             )) {
          /* Record the address */

          addr = (struct iface_addr *)
            realloc(iface->addr, (iface->addrs + 1) * sizeof iface->addr[0]);
          if (addr == NULL)
            break;
          iface->addr = addr;

          addr += iface->addrs;
          iface->addrs++;

          ncprange_setsa(&addr->ifa, sa[RTAX_IFA], sa[RTAX_NETMASK]);
          if (sa[RTAX_BRD])
            ncpaddr_setsa(&addr->peer, sa[RTAX_BRD]);
          else
            ncpaddr_init(&addr->peer);
        }
      }
    }
  }

  free(buf);

  return iface;
}

static int
iface_addr_Zap(const char *name, struct iface_addr *addr, int s)
{
  struct ifaliasreq ifra;
#ifndef NOINET6
  struct in6_aliasreq ifra6;
#endif
  struct sockaddr_in *me4, *msk4, *peer4;
  struct sockaddr_storage ssme, sspeer, ssmsk;
  int res;

  ncprange_getsa(&addr->ifa, &ssme, &ssmsk);
  ncpaddr_getsa(&addr->peer, &sspeer);
  res = 0;

  switch (ncprange_family(&addr->ifa)) {
  case AF_INET:
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, name, sizeof ifra.ifra_name - 1);

    me4 = (struct sockaddr_in *)&ifra.ifra_addr;
    memcpy(me4, &ssme, sizeof *me4);

    msk4 = (struct sockaddr_in *)&ifra.ifra_mask;
    memcpy(msk4, &ssmsk, sizeof *msk4);

    peer4 = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    if (ncpaddr_family(&addr->peer) == AF_UNSPEC) {
      peer4->sin_family = AF_INET;
      peer4->sin_len = sizeof(*peer4);
      peer4->sin_addr.s_addr = INADDR_NONE;
    } else
      memcpy(peer4, &sspeer, sizeof *peer4);

    res = ID0ioctl(s, SIOCDIFADDR, &ifra);
    if (log_IsKept(LogDEBUG)) {
      char buf[100];

      snprintf(buf, sizeof buf, "%s", ncprange_ntoa(&addr->ifa));
      log_Printf(LogWARN, "%s: DIFADDR %s -> %s returns %d\n",
                 ifra.ifra_name, buf, ncpaddr_ntoa(&addr->peer), res);
    }
    break;

#ifndef NOINET6
  case AF_INET6:
    memset(&ifra6, '\0', sizeof ifra6);
    strncpy(ifra6.ifra_name, name, sizeof ifra6.ifra_name - 1);

    memcpy(&ifra6.ifra_addr, &ssme, sizeof ifra6.ifra_addr);
    memcpy(&ifra6.ifra_prefixmask, &ssmsk, sizeof ifra6.ifra_prefixmask);
    ifra6.ifra_prefixmask.sin6_family = AF_UNSPEC;
    if (ncpaddr_family(&addr->peer) == AF_UNSPEC)
      ifra6.ifra_dstaddr.sin6_family = AF_UNSPEC;
    else
      memcpy(&ifra6.ifra_dstaddr, &sspeer, sizeof ifra6.ifra_dstaddr);
    ifra6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
    ifra6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

    res = ID0ioctl(s, SIOCDIFADDR_IN6, &ifra6);
    break;
#endif
  }

  if (res == -1) {
    char dst[40];
    const char *end =
#ifndef NOINET6
      ncprange_family(&addr->ifa) == AF_INET6 ? "_IN6" :
#endif
      "";

    if (ncpaddr_family(&addr->peer) == AF_UNSPEC)
      log_Printf(LogWARN, "iface rm: ioctl(SIOCDIFADDR%s, %s): %s\n",
                 end, ncprange_ntoa(&addr->ifa), strerror(errno));
    else {
      snprintf(dst, sizeof dst, "%s", ncpaddr_ntoa(&addr->peer));
      log_Printf(LogWARN, "iface rm: ioctl(SIOCDIFADDR%s, %s -> %s): %s\n",
                 end, ncprange_ntoa(&addr->ifa), dst, strerror(errno));
    }
  }

  return res != -1;
}

static int
iface_addr_Add(const char *name, struct iface_addr *addr, int s)
{
  struct ifaliasreq ifra;
#ifndef NOINET6
  struct in6_aliasreq ifra6;
#endif
  struct sockaddr_in *me4, *msk4, *peer4;
  struct sockaddr_storage ssme, sspeer, ssmsk;
  int res;

  ncprange_getsa(&addr->ifa, &ssme, &ssmsk);
  ncpaddr_getsa(&addr->peer, &sspeer);
  res = 0;

  switch (ncprange_family(&addr->ifa)) {
  case AF_INET:
    memset(&ifra, '\0', sizeof ifra);
    strncpy(ifra.ifra_name, name, sizeof ifra.ifra_name - 1);

    me4 = (struct sockaddr_in *)&ifra.ifra_addr;
    memcpy(me4, &ssme, sizeof *me4);

    msk4 = (struct sockaddr_in *)&ifra.ifra_mask;
    memcpy(msk4, &ssmsk, sizeof *msk4);

    peer4 = (struct sockaddr_in *)&ifra.ifra_broadaddr;
    if (ncpaddr_family(&addr->peer) == AF_UNSPEC) {
      peer4->sin_family = AF_INET;
      peer4->sin_len = sizeof(*peer4);
      peer4->sin_addr.s_addr = INADDR_NONE;
    } else
      memcpy(peer4, &sspeer, sizeof *peer4);

    res = ID0ioctl(s, SIOCAIFADDR, &ifra);
    if (log_IsKept(LogDEBUG)) {
      char buf[100];

      snprintf(buf, sizeof buf, "%s", ncprange_ntoa(&addr->ifa));
      log_Printf(LogWARN, "%s: AIFADDR %s -> %s returns %d\n",
                 ifra.ifra_name, buf, ncpaddr_ntoa(&addr->peer), res);
    }
    break;

#ifndef NOINET6
  case AF_INET6:
    memset(&ifra6, '\0', sizeof ifra6);
    strncpy(ifra6.ifra_name, name, sizeof ifra6.ifra_name - 1);

    memcpy(&ifra6.ifra_addr, &ssme, sizeof ifra6.ifra_addr);
    memcpy(&ifra6.ifra_prefixmask, &ssmsk, sizeof ifra6.ifra_prefixmask);
    if (ncpaddr_family(&addr->peer) == AF_UNSPEC)
      ifra6.ifra_dstaddr.sin6_family = AF_UNSPEC;
    else
      memcpy(&ifra6.ifra_dstaddr, &sspeer, sizeof ifra6.ifra_dstaddr);
    ifra6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
    ifra6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;

    res = ID0ioctl(s, SIOCAIFADDR_IN6, &ifra6);
    break;
#endif
  }

  if (res == -1) {
    char dst[40];
    const char *end =
#ifndef NOINET6
      ncprange_family(&addr->ifa) == AF_INET6 ? "_IN6" :
#endif
      "";

    if (ncpaddr_family(&addr->peer) == AF_UNSPEC)
      log_Printf(LogWARN, "iface add: ioctl(SIOCAIFADDR%s, %s): %s\n",
                 end, ncprange_ntoa(&addr->ifa), strerror(errno));
    else {
      snprintf(dst, sizeof dst, "%s", ncpaddr_ntoa(&addr->peer));
      log_Printf(LogWARN, "iface add: ioctl(SIOCAIFADDR%s, %s -> %s): %s\n",
                 end, ncprange_ntoa(&addr->ifa), dst, strerror(errno));
    }
  }

  return res != -1;
}


void
iface_Clear(struct iface *iface, struct ncp *ncp, int family, int how)
{
  int addrs, af, inskip, in6skip, n, s4 = -1, s6 = -1, *s;

  if (iface->addrs) {
    inskip = in6skip = how == IFACE_CLEAR_ALL ? 0 : 1;
    addrs = 0;

    for (n = 0; n < iface->addrs; n++) {
      af = ncprange_family(&iface->addr[n].ifa);
      if (family == 0 || family == af) {
        if (!iface->addr[n].system && (how & IFACE_SYSTEM))
          continue;
        switch (af) {
        case AF_INET:
          if (inskip) {
            inskip = 0;
            continue;
          }
          s = &s4;
          break;

#ifndef NOINET6
        case AF_INET6:
          if (in6skip) {
            in6skip = 0;
            continue;
          }
          s = &s6;
          break;
#endif
        default:
          continue;
        }

        if (*s == -1 && (*s = ID0socket(af, SOCK_DGRAM, 0)) == -1)
          log_Printf(LogERROR, "iface_Clear: socket(): %s\n", strerror(errno));
        else if (iface_addr_Zap(iface->name, iface->addr + n, *s)) {
          ncp_IfaceAddrDeleted(ncp, iface->addr + n);
          bcopy(iface->addr + n + 1, iface->addr + n,
                (iface->addrs - n - 1) * sizeof *iface->addr);
          iface->addrs--;
          n--;
        }
      }
    }

    /* Don't bother realloc()ing - we have little to gain */

    if (s4)
      close(s4);
    if (s6)
      close(s6);
  }
}

int
iface_Add(struct iface *iface, struct ncp *ncp, const struct ncprange *ifa,
          const struct ncpaddr *peer, int how)
{
  int af, n, removed, s, width;
  struct ncpaddr ncplocal;
  struct iface_addr *addr, newaddr;

  af = ncprange_family(ifa);
  if ((s = ID0socket(af, SOCK_DGRAM, 0)) == -1) {
    log_Printf(LogERROR, "iface_Add: socket(): %s\n", strerror(errno));
    return 0;
  }
  ncprange_getaddr(ifa, &ncplocal);

  for (n = 0; n < iface->addrs; n++) {
    if (ncprange_contains(&iface->addr[n].ifa, &ncplocal) ||
        ncpaddr_equal(&iface->addr[n].peer, peer)) {
      /* Replace this sockaddr */
      if (!(how & IFACE_FORCE_ADD)) {
        close(s);
        return 0;	/* errno = EEXIST; */
      }

      if (ncprange_equal(&iface->addr[n].ifa, ifa) &&
          ncpaddr_equal(&iface->addr[n].peer, peer)) {
        close(s);
        return 1;	/* Already there */
      }

      width =
#ifndef NOINET6
        (af == AF_INET6) ? 128 :
#endif
      32;
      removed = iface_addr_Zap(iface->name, iface->addr + n, s);
      if (removed)
        ncp_IfaceAddrDeleted(ncp, iface->addr + n);
      ncprange_copy(&iface->addr[n].ifa, ifa);
      ncpaddr_copy(&iface->addr[n].peer, peer);
      if (!iface_addr_Add(iface->name, iface->addr + n, s)) {
        if (removed) {
          bcopy(iface->addr + n + 1, iface->addr + n,
                (iface->addrs - n - 1) * sizeof *iface->addr);
          iface->addrs--;
          n--;
        }
        close(s);
        return 0;
      }
      close(s);
      ncp_IfaceAddrAdded(ncp, iface->addr + n);
      return 1;
    }
  }

  addr = (struct iface_addr *)realloc
    (iface->addr, (iface->addrs + 1) * sizeof iface->addr[0]);
  if (addr == NULL) {
    log_Printf(LogERROR, "iface_inAdd: realloc: %s\n", strerror(errno));
    close(s);
    return 0;
  }
  iface->addr = addr;

  ncprange_copy(&newaddr.ifa, ifa);
  ncpaddr_copy(&newaddr.peer, peer);
  newaddr.system = !!(how & IFACE_SYSTEM);
  if (!iface_addr_Add(iface->name, &newaddr, s)) {
    close(s);
    return 0;
  }

  if (how & IFACE_ADD_FIRST) {
    /* Stuff it at the start of our list */
    n = 0;
    bcopy(iface->addr, iface->addr + 1, iface->addrs * sizeof *iface->addr);
  } else
    n = iface->addrs;

  iface->addrs++;
  memcpy(iface->addr + n, &newaddr, sizeof(*iface->addr));

  close(s);
  ncp_IfaceAddrAdded(ncp, iface->addr + n);

  return 1;
}

int
iface_Delete(struct iface *iface, struct ncp *ncp, const struct ncpaddr *del)
{
  struct ncpaddr found;
  int n, res, s;

  if ((s = ID0socket(ncpaddr_family(del), SOCK_DGRAM, 0)) == -1) {
    log_Printf(LogERROR, "iface_Delete: socket(): %s\n", strerror(errno));
    return 0;
  }

  for (n = res = 0; n < iface->addrs; n++) {
    ncprange_getaddr(&iface->addr[n].ifa, &found);
    if (ncpaddr_equal(&found, del)) {
      if (iface_addr_Zap(iface->name, iface->addr + n, s)) {
        ncp_IfaceAddrDeleted(ncp, iface->addr + n);
        bcopy(iface->addr + n + 1, iface->addr + n,
              (iface->addrs - n - 1) * sizeof *iface->addr);
        iface->addrs--;
        res = 1;
      }
      break;
    }
  }

  close(s);

  return res;
}

#define IFACE_ADDFLAGS 1
#define IFACE_DELFLAGS 2

static int
iface_ChangeFlags(const char *ifname, int flags, int how)
{
  struct ifreq ifrq;
  int s, new_flags;

  s = ID0socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    log_Printf(LogERROR, "iface_ChangeFlags: socket: %s\n", strerror(errno));
    return 0;
  }

  memset(&ifrq, '\0', sizeof ifrq);
  strncpy(ifrq.ifr_name, ifname, sizeof ifrq.ifr_name - 1);
  ifrq.ifr_name[sizeof ifrq.ifr_name - 1] = '\0';
  if (ID0ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    log_Printf(LogERROR, "iface_ChangeFlags: ioctl(SIOCGIFFLAGS): %s\n",
       strerror(errno));
    close(s);
    return 0;
  }
  new_flags = (ifrq.ifr_flags & 0xffff) | (ifrq.ifr_flagshigh << 16);

  if (how == IFACE_ADDFLAGS)
    new_flags |= flags;
  else
    new_flags &= ~flags;
  ifrq.ifr_flags = new_flags & 0xffff;
  ifrq.ifr_flagshigh = new_flags >> 16;

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
iface_SetFlags(const char *ifname, int flags)
{
  return iface_ChangeFlags(ifname, flags, IFACE_ADDFLAGS);
}

int
iface_ClearFlags(const char *ifname, int flags)
{
  return iface_ChangeFlags(ifname, flags, IFACE_DELFLAGS);
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
    free(iface->addr);
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
  struct ncpaddr ncpaddr;
  struct iface *iface = arg->bundle->iface, *current;
  int f, flags;
#ifndef NOINET6
  int scopeid, width;
#endif
  struct in_addr mask;

  current = iface_Create(iface->name);
  flags = iface->flags = current->flags;
  iface_Destroy(current);

  prompt_Printf(arg->prompt, "%s (idx %d) <", iface->name, iface->index);
  for (f = 0; f < sizeof if_flags / sizeof if_flags[0]; f++)
    if ((if_flags[f].flag & flags)) {
      prompt_Printf(arg->prompt, "%s%s", flags == iface->flags ? "" : ",",
                    if_flags[f].value);
      flags &= ~if_flags[f].flag;
    }

#if 0
  if (flags)
    prompt_Printf(arg->prompt, "%s0x%x", flags == iface->flags ? "" : ",",
                  flags);
#endif

  prompt_Printf(arg->prompt, "> mtu %d has %d address%s:\n", iface->mtu,
                iface->addrs, iface->addrs == 1 ? "" : "es");

  for (f = 0; f < iface->addrs; f++) {
    ncprange_getaddr(&iface->addr[f].ifa, &ncpaddr);
    switch (ncprange_family(&iface->addr[f].ifa)) {
    case AF_INET:
      prompt_Printf(arg->prompt, "  inet %s --> ", ncpaddr_ntoa(&ncpaddr));
      if (ncpaddr_family(&iface->addr[f].peer) == AF_UNSPEC)
        prompt_Printf(arg->prompt, "255.255.255.255");
      else
        prompt_Printf(arg->prompt, "%s", ncpaddr_ntoa(&iface->addr[f].peer));
      ncprange_getip4mask(&iface->addr[f].ifa, &mask);
      prompt_Printf(arg->prompt, " netmask 0x%08lx", (long)ntohl(mask.s_addr));
      break;

#ifndef NOINET6
    case AF_INET6:
      prompt_Printf(arg->prompt, "  inet6 %s", ncpaddr_ntoa(&ncpaddr));
      if (ncpaddr_family(&iface->addr[f].peer) != AF_UNSPEC)
        prompt_Printf(arg->prompt, " --> %s",
                      ncpaddr_ntoa(&iface->addr[f].peer));
      ncprange_getwidth(&iface->addr[f].ifa, &width);
      if (ncpaddr_family(&iface->addr[f].peer) == AF_UNSPEC)
        prompt_Printf(arg->prompt, " prefixlen %d", width);
      if ((scopeid = ncprange_scopeid(&iface->addr[f].ifa)) != -1)
        prompt_Printf(arg->prompt, " scopeid 0x%x", (unsigned)scopeid);
      break;
#endif
    }
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
