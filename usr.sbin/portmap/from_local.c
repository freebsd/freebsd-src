 /*
  * Check if an address belongs to the local system. Adapted from:
  *
  * @(#)pmap_svc.c 1.32 91/03/11 Copyright 1984,1990 Sun Microsystems, Inc.
  * @(#)get_myaddress.c  2.1 88/07/29 4.0 RPCSRC.
  */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#) from_local.c 1.2 93/11/16 21:50:02";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif

#ifdef TEST
#undef perror
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>

#include "pmap_check.h"

#ifndef TRUE
#define	TRUE	1
#define FALSE	0
#endif

#define ROUNDUP(x) ((x) ? (1 + (((x) - 1) | (sizeof(long) - 1))) : sizeof(long))

/* How many interfaces could there be on a computer? */

#define	ESTIMATED_LOCAL 20
static int num_local = -1;
static struct in_addr *addrs;

static void
rtiparse(struct ifa_msghdr *ifam, struct rt_addrinfo *ai)
{
  char *wp;
  int rtax;

  wp = (char *)(ifam + 1);

  ai->rti_addrs = ifam->ifam_addrs;
  for (rtax = 0; rtax < sizeof ai->rti_info / sizeof *ai->rti_info; rtax++)
    if (ifam->ifam_addrs & (1 << rtax)) {
      ai->rti_info[rtax] = (struct sockaddr *)wp;
      wp += ROUNDUP(ai->rti_info[rtax]->sa_len);
    } else
      ai->rti_info[rtax] = NULL;
}

/* find_local - find all IP addresses for this host */

static int
find_local()
{
  int mib[6], n, s, alloced;
  size_t needed;
  char *buf, *end, *ptr;
  struct if_msghdr *ifm;
  struct ifa_msghdr *ifam;
  struct rt_addrinfo ai;
  struct ifreq ifr;
  struct sockaddr_dl *dl;

  mib[0] = CTL_NET;
  mib[1] = PF_ROUTE;
  mib[4] = NET_RT_IFLIST;
  mib[2] = mib[3] = mib[5] = 0;

  if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket");
    return (0);
  }
  if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
    close(s);
    perror("sysctl(NET_RT_IFLIST)");
    return 0;
  }
  if ((buf = (char *)malloc(needed)) == NULL) {
    close(s);
    perror("malloc");
    return 0;
  }
  if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
    close(s);
    free(buf);
    perror("sysctl(NET_RT_IFLIST)(after malloc)");
    return 0;
  }

  if (addrs) {
    free(addrs);
    addrs = NULL;
  }
  num_local = 0;
  alloced = 0;
  end = buf + needed;

  for (ptr = buf; ptr < end; ptr += ifm->ifm_msglen) {
    ifm = (struct if_msghdr *)ptr;
    dl = (struct sockaddr_dl *)(ifm + 1);

    if (ifm->ifm_index != dl->sdl_index || dl->sdl_nlen == 0)
      /* Skip over remaining ifa_msghdrs */
      continue;

    n = dl->sdl_nlen > sizeof ifr.ifr_name ?
        sizeof ifr.ifr_name : dl->sdl_nlen;
    strncpy(ifr.ifr_name, dl->sdl_data, n);
    if (n < sizeof ifr.ifr_name)
      ifr.ifr_name[n] = '\0';

    /* we only want the first address from each interface */
    if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
      fprintf(stderr, "%.*s: SIOCGIFFLAGS: %s\n", n, ifr.ifr_name,
              strerror(errno));
    else if (ifr.ifr_flags & IFF_UP) {    /* active interface */
      ifam = (struct ifa_msghdr *)(ptr + ifm->ifm_msglen);
      while ((char *)ifam < end && ifam->ifam_type == RTM_NEWADDR) {
        rtiparse(ifam, &ai);

        if (ai.rti_info[RTAX_IFA] != NULL &&
            ai.rti_info[RTAX_IFA]->sa_family == AF_INET) {
          if (alloced < num_local + 1) {
            alloced += ESTIMATED_LOCAL;
            addrs = (struct in_addr *)realloc(addrs, alloced * sizeof addrs[0]);
            if (addrs == NULL) {
              perror("malloc/realloc");
              num_local = 0;
              break;
            }
          }
          addrs[num_local++] = ((struct sockaddr_in *)
            ai.rti_info[RTAX_IFA])->sin_addr;

        }
        ifam = (struct ifa_msghdr *)((char *)ifam + ifam->ifam_msglen);
      }
    }
  }
  free(buf);
  close(s);

  return num_local;
}

/* from_local - determine whether request comes from the local system */

int
from_local(addr)
    struct sockaddr_in *addr;
{
    int     i;

    if (num_local == -1 && find_local() == 0)
	syslog(LOG_ERR, "cannot find any active local network interfaces");

    for (i = 0; i < num_local; i++) {
	if (memcmp((char *) &(addr->sin_addr), (char *) &(addrs[i]),
		   sizeof(struct in_addr)) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

#ifdef TEST

int
main(argc, argv)
    int argc;
    char **argv;
{
    char   *inet_ntoa();
    int     i;

    find_local();
    for (i = 0; i < num_local; i++)
	printf("%s\n", inet_ntoa(addrs[i]));

    return 0;
}

#endif
