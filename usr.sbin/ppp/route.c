/*
 *	      PPP Routing related Module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1994, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>
#include <netdb.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "iplist.h"
#include "timer.h"
#include "throughput.h"
#include "lqr.h"
#include "hdlc.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "bundle.h"
#include "route.h"
#include "prompt.h"
#include "iface.h"
#include "id.h"


static void
p_sockaddr(struct prompt *prompt, struct sockaddr *phost,
           struct sockaddr *pmask, int width)
{
  char buf[29];
  struct sockaddr_in *ihost4 = (struct sockaddr_in *)phost;
  struct sockaddr_in *mask4 = (struct sockaddr_in *)pmask;
  struct sockaddr_dl *dl = (struct sockaddr_dl *)phost;

  if (log_IsKept(LogDEBUG)) {
    char tmp[50];

    log_Printf(LogDEBUG, "Found the following sockaddr:\n");
    log_Printf(LogDEBUG, "  Family %d, len %d\n",
               (int)phost->sa_family, (int)phost->sa_len);
    inet_ntop(phost->sa_family, phost->sa_data, tmp, sizeof tmp);
    log_Printf(LogDEBUG, "  Addr %s\n", tmp);
    if (pmask) {
      inet_ntop(pmask->sa_family, pmask->sa_data, tmp, sizeof tmp);
      log_Printf(LogDEBUG, "  Mask %s\n", tmp);
    }
  }

  switch (phost->sa_family) {
  case AF_INET:
    if (!phost)
      buf[0] = '\0';
    else if (ihost4->sin_addr.s_addr == INADDR_ANY)
      strcpy(buf, "default");
    else if (!pmask) 
      strcpy(buf, inet_ntoa(ihost4->sin_addr));
    else {
      u_int32_t msk = ntohl(mask4->sin_addr.s_addr);
      u_int32_t tst;
      int bits;
      int len;
      struct sockaddr_in net;

      for (tst = 1, bits = 32; tst; tst <<= 1, bits--)
        if (msk & tst)
          break;

      for (tst <<= 1; tst; tst <<= 1)
        if (!(msk & tst))
          break;

      net.sin_addr.s_addr = ihost4->sin_addr.s_addr & mask4->sin_addr.s_addr;
      strcpy(buf, inet_ntoa(net.sin_addr));
      for (len = strlen(buf); len > 3; buf[len -= 2] = '\0')
        if (strcmp(buf + len - 2, ".0"))
          break;

      if (tst)    /* non-contiguous :-( */
        sprintf(buf + strlen(buf),"&0x%08lx", (u_long)msk);
      else
        sprintf(buf + strlen(buf), "/%d", bits);
    }
    break;

  case AF_LINK:
    if (dl->sdl_nlen)
      snprintf(buf, sizeof buf, "%.*s", dl->sdl_nlen, dl->sdl_data);
    else if (dl->sdl_alen) {
      if (dl->sdl_type == IFT_ETHER) {
        if (dl->sdl_alen < sizeof buf / 3) {
          int f;
          u_char *MAC;

          MAC = (u_char *)dl->sdl_data + dl->sdl_nlen;
          for (f = 0; f < dl->sdl_alen; f++)
            sprintf(buf+f*3, "%02x:", MAC[f]);
          buf[f*3-1] = '\0';
        } else
          strcpy(buf, "??:??:??:??:??:??");
      } else
        sprintf(buf, "<IFT type %d>", dl->sdl_type);
    }  else if (dl->sdl_slen)
      sprintf(buf, "<slen %d?>", dl->sdl_slen);
    else
      sprintf(buf, "link#%d", dl->sdl_index);
    break;

#ifndef NOINET6
  case AF_INET6:
    if (!phost)
      buf[0] = '\0';
    else {
      const u_char masks[] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe };
      struct sockaddr_in6 *ihost6 = (struct sockaddr_in6 *)phost;
      struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *)pmask;
      int masklen, len;
      const u_char *c;

      /* XXX: ?????!?!?!!!!!  This is horrible ! */
      if (IN6_IS_ADDR_LINKLOCAL(&ihost6->sin6_addr) ||
          IN6_IS_ADDR_MC_LINKLOCAL(&ihost6->sin6_addr)) {
        ihost6->sin6_scope_id =
          ntohs(*(u_short *)&ihost6->sin6_addr.s6_addr[2]);
        *(u_short *)&ihost6->sin6_addr.s6_addr[2] = 0;
      }

      if (mask6) {
        const u_char *p, *end;

        p = (const u_char *)&mask6->sin6_addr;
        end = p + 16;
        for (masklen = 0, end = p + 16; p < end && *p == 0xff; p++)
          masklen += 8;

        if (p < end) {
          for (c = masks; c < masks + sizeof masks; c++)
            if (*c == *p) {
              masklen += c - masks;
              break;
            }
        }
      } else
        masklen = 128;

      if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&ihost6->sin6_addr))
        snprintf(buf, sizeof buf, "default");
      else {
        getnameinfo(phost, ihost6->sin6_len, buf, sizeof buf,
                    NULL, 0, NI_WITHSCOPEID | NI_NUMERICHOST);
        if (mask6 && (len = strlen(buf)) < sizeof buf - 1)
          snprintf(buf + len, sizeof buf - len, "/%d", masklen);
      }
    }
    break;
#endif

  default:
    sprintf(buf, "<AF type %d>", phost->sa_family);
    break;
  }

  prompt_Printf(prompt, "%-*s ", width-1, buf);
}

static struct bits {
  u_int32_t b_mask;
  char b_val;
} bits[] = {
  { RTF_UP, 'U' },
  { RTF_GATEWAY, 'G' },
  { RTF_HOST, 'H' },
  { RTF_REJECT, 'R' },
  { RTF_DYNAMIC, 'D' },
  { RTF_MODIFIED, 'M' },
  { RTF_DONE, 'd' },
  { RTF_CLONING, 'C' },
  { RTF_XRESOLVE, 'X' },
  { RTF_LLINFO, 'L' },
  { RTF_STATIC, 'S' },
  { RTF_PROTO1, '1' },
  { RTF_PROTO2, '2' },
  { RTF_BLACKHOLE, 'B' },
#ifdef RTF_WASCLONED
  { RTF_WASCLONED, 'W' },
#endif
#ifdef RTF_PRCLONING
  { RTF_PRCLONING, 'c' },
#endif
#ifdef RTF_PROTO3
  { RTF_PROTO3, '3' },
#endif
#ifdef RTF_BROADCAST
  { RTF_BROADCAST, 'b' },
#endif
  { 0, '\0' }
};

#ifndef RTF_WASCLONED
#define RTF_WASCLONED (0)
#endif

static void
p_flags(struct prompt *prompt, u_int32_t f, int max)
{
  char name[33], *flags;
  register struct bits *p = bits;

  if (max > sizeof name - 1)
    max = sizeof name - 1;

  for (flags = name; p->b_mask && flags - name < max; p++)
    if (p->b_mask & f)
      *flags++ = p->b_val;
  *flags = '\0';
  prompt_Printf(prompt, "%-*.*s", max, max, name);
}

const char *
Index2Nam(int idx)
{
  /*
   * XXX: Maybe we should select() on the routing socket so that we can
   *      notice interfaces that come & go (PCCARD support).
   *      Or we could even support a signal that resets these so that
   *      the PCCARD insert/remove events can signal ppp.
   */
  static char **ifs;		/* Figure these out once */
  static int nifs, debug_done;	/* Figure out how many once, and debug once */

  if (idx > nifs || (idx > 0 && ifs[idx-1] == NULL)) {
    int mib[6], have, had;
    size_t needed;
    char *buf, *ptr, *end;
    struct sockaddr_dl *dl;
    struct if_msghdr *ifm;

    if (ifs) {
      free(ifs);
      ifs = NULL;
      nifs = 0;
    }
    debug_done = 0;

    mib[0] = CTL_NET;
    mib[1] = PF_ROUTE;
    mib[2] = 0;
    mib[3] = 0;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
      log_Printf(LogERROR, "Index2Nam: sysctl: estimate: %s\n",
                 strerror(errno));
      return NumStr(idx, NULL, 0);
    }
    if ((buf = malloc(needed)) == NULL)
      return NumStr(idx, NULL, 0);
    if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
      free(buf);
      return NumStr(idx, NULL, 0);
    }
    end = buf + needed;

    have = 0;
    for (ptr = buf; ptr < end; ptr += ifm->ifm_msglen) {
      ifm = (struct if_msghdr *)ptr;
      if (ifm->ifm_type != RTM_IFINFO)
        continue;
      dl = (struct sockaddr_dl *)(ifm + 1);
      if (ifm->ifm_index > 0) {
        if (ifm->ifm_index > have) {
          char **newifs;

          had = have;
          have = ifm->ifm_index + 5;
          if (had)
            newifs = (char **)realloc(ifs, sizeof(char *) * have);
          else
            newifs = (char **)malloc(sizeof(char *) * have);
          if (!newifs) {
            log_Printf(LogDEBUG, "Index2Nam: %s\n", strerror(errno));
            nifs = 0;
            if (ifs) {
              free(ifs);
              ifs = NULL;
            }
            free(buf);
            return NumStr(idx, NULL, 0);
          }
          ifs = newifs;
          memset(ifs + had, '\0', sizeof(char *) * (have - had));
        }
        if (ifs[ifm->ifm_index-1] == NULL) {
          ifs[ifm->ifm_index-1] = (char *)malloc(dl->sdl_nlen+1);
          memcpy(ifs[ifm->ifm_index-1], dl->sdl_data, dl->sdl_nlen);
          ifs[ifm->ifm_index-1][dl->sdl_nlen] = '\0';
          if (nifs < ifm->ifm_index)
            nifs = ifm->ifm_index;
        }
      } else if (log_IsKept(LogDEBUG))
        log_Printf(LogDEBUG, "Skipping out-of-range interface %d!\n",
                  ifm->ifm_index);
    }
    free(buf);
  }

  if (log_IsKept(LogDEBUG) && !debug_done) {
    int f;

    log_Printf(LogDEBUG, "Found the following interfaces:\n");
    for (f = 0; f < nifs; f++)
      if (ifs[f] != NULL)
        log_Printf(LogDEBUG, " Index %d, name \"%s\"\n", f+1, ifs[f]);
    debug_done = 1;
  }

  if (idx < 1 || idx > nifs || ifs[idx-1] == NULL)
    return NumStr(idx, NULL, 0);

  return ifs[idx-1];
}

void
route_ParseHdr(struct rt_msghdr *rtm, struct sockaddr *sa[RTAX_MAX])
{
  char *wp;
  int rtax;

  wp = (char *)(rtm + 1);

  for (rtax = 0; rtax < RTAX_MAX; rtax++)
    if (rtm->rtm_addrs & (1 << rtax)) {
      sa[rtax] = (struct sockaddr *)wp;
      wp += ROUNDUP(sa[rtax]->sa_len);
    } else
      sa[rtax] = NULL;
}

int
route_Show(struct cmdargs const *arg)
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa[RTAX_MAX];
  char *sp, *ep, *cp;
  size_t needed;
  int mib[6];

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_Show: sysctl: estimate: %s\n", strerror(errno));
    return (1);
  }
  sp = malloc(needed);
  if (sp == NULL)
    return (1);
  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_Show: sysctl: getroute: %s\n", strerror(errno));
    free(sp);
    return (1);
  }
  ep = sp + needed;

  prompt_Printf(arg->prompt, "%-20s%-20sFlags  Netif\n",
                "Destination", "Gateway");
  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)cp;

    route_ParseHdr(rtm, sa);

    if (sa[RTAX_DST] && sa[RTAX_GATEWAY]) {
      p_sockaddr(arg->prompt, sa[RTAX_DST], sa[RTAX_NETMASK], 20);
      p_sockaddr(arg->prompt, sa[RTAX_GATEWAY], NULL, 20);

      p_flags(arg->prompt, rtm->rtm_flags, 6);
      prompt_Printf(arg->prompt, " %s\n", Index2Nam(rtm->rtm_index));
    } else
      prompt_Printf(arg->prompt, "<can't parse routing entry>\n");
  }
  free(sp);
  return 0;
}

/*
 *  Delete routes associated with our interface
 */
void
route_IfDelete(struct bundle *bundle, int all)
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa[RTAX_MAX];
  struct sockaddr_in **in;
  struct in_addr sa_none;
  int pass;
  size_t needed;
  char *sp, *cp, *ep;
  int mib[6];

  log_Printf(LogDEBUG, "route_IfDelete (%d)\n", bundle->iface->index);
  sa_none.s_addr = INADDR_ANY;
  in = (struct sockaddr_in **)sa;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_IfDelete: sysctl: estimate: %s\n",
              strerror(errno));
    return;
  }

  sp = malloc(needed);
  if (sp == NULL)
    return;

  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_IfDelete: sysctl: getroute: %s\n",
              strerror(errno));
    free(sp);
    return;
  }
  ep = sp + needed;

  for (pass = 0; pass < 2; pass++) {
    /*
     * We do 2 passes.  The first deletes all cloned routes.  The second
     * deletes all non-cloned routes.  This is done to avoid
     * potential errors from trying to delete route X after route Y where
     * route X was cloned from route Y (and is no longer there 'cos it
     * may have gone with route Y).
     */
    if (RTF_WASCLONED == 0 && pass == 0)
      /* So we can't tell ! */
      continue;
    for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
      rtm = (struct rt_msghdr *)cp;
      route_ParseHdr(rtm, sa);
      if (sa[RTAX_DST] && sa[RTAX_DST]->sa_family == AF_INET) {
        log_Printf(LogDEBUG, "route_IfDelete: addrs: %x, Netif: %d (%s),"
                   " flags: %x, dst: %s ?\n", rtm->rtm_addrs, rtm->rtm_index,
                   Index2Nam(rtm->rtm_index), rtm->rtm_flags,
                   inet_ntoa(((struct sockaddr_in *)sa[RTAX_DST])->sin_addr));
        if (sa[RTAX_GATEWAY] && rtm->rtm_index == bundle->iface->index &&
            (all || (rtm->rtm_flags & RTF_GATEWAY))) {
          if (sa[RTAX_GATEWAY]->sa_family == AF_INET ||
              sa[RTAX_GATEWAY]->sa_family == AF_LINK) {
            if ((pass == 0 && (rtm->rtm_flags & RTF_WASCLONED)) ||
                (pass == 1 && !(rtm->rtm_flags & RTF_WASCLONED))) {
              log_Printf(LogDEBUG, "route_IfDelete: Remove it (pass %d)\n",
                         pass);
              rt_Set(bundle, RTM_DELETE, in[RTAX_DST]->sin_addr,
                              sa_none, sa_none, 0, 0);
            } else
              log_Printf(LogDEBUG, "route_IfDelete: Skip it (pass %d)\n", pass);
          } else
            log_Printf(LogDEBUG,
                      "route_IfDelete: Can't remove routes of %d family !\n",
                      sa[RTAX_GATEWAY]->sa_family);
        }
      }
    }
  }
  free(sp);
}


/*
 *  Update the MTU on all routes for the given interface
 */
void
route_UpdateMTU(struct bundle *bundle)
{
  struct rt_msghdr *rtm;
  struct sockaddr *sa[RTAX_MAX];
  struct sockaddr_in **in;
  size_t needed;
  char *sp, *cp, *ep;
  int mib[6];

  log_Printf(LogDEBUG, "route_UpdateMTU (%d)\n", bundle->iface->index);
  in = (struct sockaddr_in **)sa;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[2] = 0;
  mib[3] = 0;
  mib[4] = NET_RT_DUMP;
  mib[5] = 0;
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_IfDelete: sysctl: estimate: %s\n",
              strerror(errno));
    return;
  }

  sp = malloc(needed);
  if (sp == NULL)
    return;

  if (sysctl(mib, 6, sp, &needed, NULL, 0) < 0) {
    log_Printf(LogERROR, "route_IfDelete: sysctl: getroute: %s\n",
              strerror(errno));
    free(sp);
    return;
  }
  ep = sp + needed;

  for (cp = sp; cp < ep; cp += rtm->rtm_msglen) {
    rtm = (struct rt_msghdr *)cp;
    route_ParseHdr(rtm, sa);
    if (sa[RTAX_DST] && sa[RTAX_DST]->sa_family == AF_INET &&
        sa[RTAX_GATEWAY] && /* sa[RTAX_NETMASK] && */
        rtm->rtm_index == bundle->iface->index &&
        (sa[RTAX_GATEWAY]->sa_family == AF_INET ||
         sa[RTAX_GATEWAY]->sa_family == AF_LINK)) {
      log_Printf(LogTCPIP, "route_UpdateMTU: Netif: %d (%s), dst %s, mtu %d\n",
                 rtm->rtm_index, Index2Nam(rtm->rtm_index),
                 inet_ntoa(((struct sockaddr_in *)sa[RTAX_DST])->sin_addr),
                 bundle->mtu);
      rt_Update(bundle, in[RTAX_DST]->sin_addr,
                         in[RTAX_GATEWAY]->sin_addr);
    }
  }

  free(sp);
}

int
GetIfIndex(char *name)
{
  int idx;
  const char *got;

  idx = 1;
  while (strcmp(got = Index2Nam(idx), "???"))
    if (!strcmp(got, name))
      return idx;
    else
      idx++;
  return -1;
}

void
route_Change(struct bundle *bundle, struct sticky_route *r,
             struct in_addr me, struct in_addr peer, struct in_addr dns[2])
{
  struct in_addr none, del;

  none.s_addr = INADDR_ANY;
  for (; r; r = r->next) {
    if ((r->type & ROUTE_DSTMYADDR) && r->dst.s_addr != me.s_addr) {
      del.s_addr = r->dst.s_addr & r->mask.s_addr;
      rt_Set(bundle, RTM_DELETE, del, none, none, 1, 0);
      r->dst = me;
      if (r->type & ROUTE_GWHISADDR)
        r->gw = peer;
    } else if ((r->type & ROUTE_DSTHISADDR) && r->dst.s_addr != peer.s_addr) {
      del.s_addr = r->dst.s_addr & r->mask.s_addr;
      rt_Set(bundle, RTM_DELETE, del, none, none, 1, 0);
      r->dst = peer;
      if (r->type & ROUTE_GWHISADDR)
        r->gw = peer;
    } else if ((r->type & ROUTE_DSTDNS0) && r->dst.s_addr != peer.s_addr) {
      del.s_addr = r->dst.s_addr & r->mask.s_addr;
      rt_Set(bundle, RTM_DELETE, del, none, none, 1, 0);
      r->dst = dns[0];
      if (r->type & ROUTE_GWHISADDR)
        r->gw = peer;
    } else if ((r->type & ROUTE_DSTDNS1) && r->dst.s_addr != peer.s_addr) {
      del.s_addr = r->dst.s_addr & r->mask.s_addr;
      rt_Set(bundle, RTM_DELETE, del, none, none, 1, 0);
      r->dst = dns[1];
      if (r->type & ROUTE_GWHISADDR)
        r->gw = peer;
    } else if ((r->type & ROUTE_GWHISADDR) && r->gw.s_addr != peer.s_addr)
      r->gw = peer;
    rt_Set(bundle, RTM_ADD, r->dst, r->gw, r->mask, 1, 0);
  }
}

void
route_Add(struct sticky_route **rp, int type, struct in_addr dst,
          struct in_addr mask, struct in_addr gw)
{
  struct sticky_route *r;
  int dsttype = type & ROUTE_DSTANY;

  r = NULL;
  while (*rp) {
    if ((dsttype && dsttype == ((*rp)->type & ROUTE_DSTANY)) ||
        (!dsttype && (*rp)->dst.s_addr == dst.s_addr)) {
      /* Oops, we already have this route - unlink it */
      free(r);			/* impossible really  */
      r = *rp;
      *rp = r->next;
    } else
      rp = &(*rp)->next;
  }

  if (!r)
    r = (struct sticky_route *)malloc(sizeof(struct sticky_route));
  r->type = type;
  r->next = NULL;
  r->dst = dst;
  r->mask = mask;
  r->gw = gw;
  *rp = r;
}

void
route_Delete(struct sticky_route **rp, int type, struct in_addr dst)
{
  struct sticky_route *r;
  int dsttype = type & ROUTE_DSTANY;

  for (; *rp; rp = &(*rp)->next) {
    if ((dsttype && dsttype == ((*rp)->type & ROUTE_DSTANY)) ||
        (!dsttype && dst.s_addr == ((*rp)->dst.s_addr & (*rp)->mask.s_addr))) {
      r = *rp;
      *rp = r->next;
      free(r);
      break;
    }
  }
}

void
route_DeleteAll(struct sticky_route **rp)
{
  struct sticky_route *r, *rn;

  for (r = *rp; r; r = rn) {
    rn = r->next;
    free(r);
  }
  *rp = NULL;
}

void
route_ShowSticky(struct prompt *p, struct sticky_route *r, const char *tag,
                 int indent)
{
  int def;
  int tlen = strlen(tag);

  if (tlen + 2 > indent)
    prompt_Printf(p, "%s:\n%*s", tag, indent, "");
  else
    prompt_Printf(p, "%s:%*s", tag, indent - tlen - 1, "");

  for (; r; r = r->next) {
    def = r->dst.s_addr == INADDR_ANY && r->mask.s_addr == INADDR_ANY;

    prompt_Printf(p, "%*sadd ", tlen ? 0 : indent, "");
    tlen = 0;
    if (r->type & ROUTE_DSTMYADDR)
      prompt_Printf(p, "MYADDR");
    else if (r->type & ROUTE_DSTHISADDR)
      prompt_Printf(p, "HISADDR");
    else if (r->type & ROUTE_DSTDNS0)
      prompt_Printf(p, "DNS0");
    else if (r->type & ROUTE_DSTDNS1)
      prompt_Printf(p, "DNS1");
    else if (!def)
      prompt_Printf(p, "%s", inet_ntoa(r->dst));

    if (def)
      prompt_Printf(p, "default ");
    else
      prompt_Printf(p, " %s ", inet_ntoa(r->mask));

    if (r->type & ROUTE_GWHISADDR)
      prompt_Printf(p, "HISADDR\n");
    else
      prompt_Printf(p, "%s\n", inet_ntoa(r->gw));
  }
}

struct rtmsg {
  struct rt_msghdr m_rtm;
  char m_space[64];
};

int
rt_Set(struct bundle *bundle, int cmd, struct in_addr dst,
                struct in_addr gateway, struct in_addr mask, int bang, int ssh)
{
  struct rtmsg rtmes;
  int s, nb, wb;
  char *cp;
  const char *cmdstr;
  struct sockaddr_in rtdata;
  int result = 1;

  if (bang)
    cmdstr = (cmd == RTM_ADD ? "Add!" : "Delete!");
  else
    cmdstr = (cmd == RTM_ADD ? "Add" : "Delete");
  s = ID0socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0) {
    log_Printf(LogERROR, "rt_Set: socket(): %s\n", strerror(errno));
    return result;
  }
  memset(&rtmes, '\0', sizeof rtmes);
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = cmd;
  rtmes.m_rtm.rtm_addrs = RTA_DST;
  rtmes.m_rtm.rtm_seq = ++bundle->routing_seq;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  if (cmd == RTM_ADD) {
    if (bundle->ncp.ipcp.cfg.sendpipe > 0) {
      rtmes.m_rtm.rtm_rmx.rmx_sendpipe = bundle->ncp.ipcp.cfg.sendpipe;
      rtmes.m_rtm.rtm_inits |= RTV_SPIPE;
    }
    if (bundle->ncp.ipcp.cfg.recvpipe > 0) {
      rtmes.m_rtm.rtm_rmx.rmx_recvpipe = bundle->ncp.ipcp.cfg.recvpipe;
      rtmes.m_rtm.rtm_inits |= RTV_RPIPE;
    }
  }

  memset(&rtdata, '\0', sizeof rtdata);
  rtdata.sin_len = sizeof rtdata;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  cp = rtmes.m_space;
  memcpy(cp, &rtdata, rtdata.sin_len);
  cp += rtdata.sin_len;
  if (cmd == RTM_ADD) {
    if (gateway.s_addr == INADDR_ANY) {
      if (!ssh)
        log_Printf(LogERROR, "rt_Set: Cannot add a route with"
                   " destination 0.0.0.0\n");
      close(s);
      return result;
    } else {
      rtdata.sin_addr = gateway;
      memcpy(cp, &rtdata, rtdata.sin_len);
      cp += rtdata.sin_len;
      rtmes.m_rtm.rtm_addrs |= RTA_GATEWAY;
    }
  }

  if (dst.s_addr == INADDR_ANY)
    mask.s_addr = INADDR_ANY;

  if (cmd == RTM_ADD || dst.s_addr == INADDR_ANY) {
    rtdata.sin_addr = mask;
    memcpy(cp, &rtdata, rtdata.sin_len);
    cp += rtdata.sin_len;
    rtmes.m_rtm.rtm_addrs |= RTA_NETMASK;
  }

  nb = cp - (char *)&rtmes;
  rtmes.m_rtm.rtm_msglen = nb;
  wb = ID0write(s, &rtmes, nb);
  if (wb < 0) {
    log_Printf(LogTCPIP, "rt_Set failure:\n");
    log_Printf(LogTCPIP, "rt_Set:  Cmd = %s\n", cmdstr);
    log_Printf(LogTCPIP, "rt_Set:  Dst = %s\n", inet_ntoa(dst));
    log_Printf(LogTCPIP, "rt_Set:  Gateway = %s\n",
               inet_ntoa(gateway));
    log_Printf(LogTCPIP, "rt_Set:  Mask = %s\n", inet_ntoa(mask));
failed:
    if (cmd == RTM_ADD && (rtmes.m_rtm.rtm_errno == EEXIST ||
                           (rtmes.m_rtm.rtm_errno == 0 && errno == EEXIST))) {
      if (!bang) {
        log_Printf(LogWARN, "Add route failed: %s already exists\n",
		  dst.s_addr == 0 ? "default" : inet_ntoa(dst));
        result = 0;	/* Don't add to our dynamic list */
      } else {
        rtmes.m_rtm.rtm_type = cmd = RTM_CHANGE;
        if ((wb = ID0write(s, &rtmes, nb)) < 0)
          goto failed;
      }
    } else if (cmd == RTM_DELETE &&
             (rtmes.m_rtm.rtm_errno == ESRCH ||
              (rtmes.m_rtm.rtm_errno == 0 && errno == ESRCH))) {
      if (!bang)
        log_Printf(LogWARN, "Del route failed: %s: Non-existent\n",
                  inet_ntoa(dst));
    } else if (rtmes.m_rtm.rtm_errno == 0) {
      if (!ssh || errno != ENETUNREACH)
        log_Printf(LogWARN, "%s route failed: %s: errno: %s\n", cmdstr,
                   inet_ntoa(dst), strerror(errno));
    } else
      log_Printf(LogWARN, "%s route failed: %s: %s\n",
		 cmdstr, inet_ntoa(dst), strerror(rtmes.m_rtm.rtm_errno));
  }

  log_Printf(LogDEBUG, "wrote %d: cmd = %s, dst = %x, gateway = %x\n",
            wb, cmdstr, (unsigned)dst.s_addr, (unsigned)gateway.s_addr);
  close(s);

  return result;
}

void
rt_Update(struct bundle *bundle, struct in_addr dst, struct in_addr gw)
{
  struct rtmsg rtmes;
  int s, wb;
  struct sockaddr_in rtdata;

  s = ID0socket(PF_ROUTE, SOCK_RAW, 0);
  if (s < 0) {
    log_Printf(LogERROR, "rt_Update: socket(): %s\n", strerror(errno));
    return;
  }

  memset(&rtmes, '\0', sizeof rtmes);
  rtmes.m_rtm.rtm_version = RTM_VERSION;
  rtmes.m_rtm.rtm_type = RTM_CHANGE;
  rtmes.m_rtm.rtm_addrs = RTA_DST;
  rtmes.m_rtm.rtm_seq = ++bundle->routing_seq;
  rtmes.m_rtm.rtm_pid = getpid();
  rtmes.m_rtm.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;

  if (bundle->ncp.ipcp.cfg.sendpipe > 0) {
    rtmes.m_rtm.rtm_rmx.rmx_sendpipe = bundle->ncp.ipcp.cfg.sendpipe;
    rtmes.m_rtm.rtm_inits |= RTV_SPIPE;
  }

  if (bundle->ncp.ipcp.cfg.recvpipe > 0) {
    rtmes.m_rtm.rtm_rmx.rmx_recvpipe = bundle->ncp.ipcp.cfg.recvpipe;
    rtmes.m_rtm.rtm_inits |= RTV_RPIPE;
  }

  rtmes.m_rtm.rtm_rmx.rmx_mtu = bundle->mtu;
  rtmes.m_rtm.rtm_inits |= RTV_MTU;

  memset(&rtdata, '\0', sizeof rtdata);
  rtdata.sin_len = sizeof rtdata;
  rtdata.sin_family = AF_INET;
  rtdata.sin_port = 0;
  rtdata.sin_addr = dst;

  memcpy(rtmes.m_space, &rtdata, rtdata.sin_len);
  rtmes.m_rtm.rtm_msglen = rtmes.m_space + rtdata.sin_len - (char *)&rtmes;

  wb = ID0write(s, &rtmes, rtmes.m_rtm.rtm_msglen);
  if (wb < 0) {
    log_Printf(LogTCPIP, "rt_Update failure:\n");
    log_Printf(LogTCPIP, "rt_Update:  Dst = %s\n", inet_ntoa(dst));
    log_Printf(LogTCPIP, "rt_Update:  Gateway = %s\n", inet_ntoa(gw));

    if (rtmes.m_rtm.rtm_errno == 0)
      log_Printf(LogWARN, "%s: Change route failed: errno: %s\n",
                 inet_ntoa(dst), strerror(errno));
    else
      log_Printf(LogWARN, "%s: Change route failed: %s\n",
		 inet_ntoa(dst), strerror(rtmes.m_rtm.rtm_errno));
  }
  log_Printf(LogDEBUG, "wrote %d: cmd = Change, dst = %x, gateway = %x\n",
            wb, (unsigned)dst.s_addr, (unsigned)gw.s_addr);
  close(s);
}
