/*
 * $Id$
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_tun.h>

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include "hdlc.h"
#include "defs.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "log.h"
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
