/*
 * $Id: tun.c,v 1.2 1997/11/17 00:42:41 brian Exp $
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif
#include <net/if_tun.h>

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "hdlc.h"
#include "defs.h"
#include "loadalias.h"
#include "vars.h"
#include "tun.h"

void
tun_configure(int mtu, int speed)
{
  struct tuninfo info;

  info.type = 23;
  info.mtu = mtu;
  if (VarPrefMTU != 0 && VarPrefMTU < mtu)
    info.mtu = VarPrefMTU;
  info.baudrate = speed;
#ifdef __OpenBSD__                                           
  info.flags = IFF_UP|IFF_POINTOPOINT;                             
#endif
  if (ioctl(tun_out, TUNSIFINFO, &info) < 0)
    LogPrintf(LogERROR, "tun_configure: ioctl(TUNSIFINFO): %s\n",
	      strerror(errno));
}
