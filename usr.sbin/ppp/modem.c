/*
 *		PPP Modem handling module
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
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
 * $Id: modem.c,v 1.77.2.60 1998/04/30 23:53:49 brian Exp $
 *
 *  TODO:
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <unistd.h>
#ifdef __OpenBSD__
#include <util.h>
#else
#include <libutil.h>
#endif

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "modem.h"
#include "throughput.h"
#include "async.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "ccp.h"
#include "link.h"
#include "physical.h"
#include "mp.h"
#include "bundle.h"
#include "prompt.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "datalink.h"
#include "systems.h"


static void modem_DescriptorWrite(struct descriptor *, struct bundle *,
                                  const fd_set *);
static void modem_DescriptorRead(struct descriptor *, struct bundle *,
                                 const fd_set *);
static int modem_UpdateSet(struct descriptor *, fd_set *, fd_set *, fd_set *,
                           int *);

struct physical *
modem_Create(struct datalink *dl, int type)
{
  struct physical *p;

  p = (struct physical *)malloc(sizeof(struct physical));
  if (!p)
    return NULL;

  p->link.type = PHYSICAL_LINK;
  p->link.name = dl->name;
  p->link.len = sizeof *p;
  throughput_init(&p->link.throughput);
  memset(&p->Timer, '\0', sizeof p->Timer);
  memset(p->link.Queue, '\0', sizeof p->link.Queue);
  memset(p->link.proto_in, '\0', sizeof p->link.proto_in);
  memset(p->link.proto_out, '\0', sizeof p->link.proto_out);

  p->desc.type = PHYSICAL_DESCRIPTOR;
  p->desc.UpdateSet = modem_UpdateSet;
  p->desc.IsSet = Physical_IsSet;
  p->desc.Read = modem_DescriptorRead;
  p->desc.Write = modem_DescriptorWrite;
  p->type = type;

  hdlc_Init(&p->hdlc, &p->link.lcp);
  async_Init(&p->async);

  p->fd = -1;
  p->mbits = 0;
  p->dev_is_modem = 0;
  p->out = NULL;
  p->connect_count = 0;
  p->dl = dl;

  *p->name.full = '\0';
  p->name.base = p->name.full;

  p->Utmp = 0;

  p->cfg.rts_cts = MODEM_CTSRTS;
  p->cfg.speed = MODEM_SPEED;
  p->cfg.parity = CS8;
  strncpy(p->cfg.devlist, MODEM_LIST, sizeof p->cfg.devlist - 1);
  p->cfg.devlist[sizeof p->cfg.devlist - 1] = '\0';

  lcp_Init(&p->link.lcp, dl->bundle, &p->link, &dl->fsmp);
  ccp_Init(&p->link.ccp, dl->bundle, &p->link, &dl->fsmp);

  return p;
}

/* XXX-ML this should probably change when we add support for other
   types of devices */
#define	Online(modem)	((modem)->mbits & TIOCM_CD)

static void modem_LogicalClose(struct physical *);

static struct speeds {
  int nspeed;
  speed_t speed;
} speeds[] = {
#ifdef B50
  { 50, B50, },
#endif
#ifdef B75
  { 75, B75, },
#endif
#ifdef B110
  { 110, B110, },
#endif
#ifdef B134
  { 134, B134, },
#endif
#ifdef B150
  { 150, B150, },
#endif
#ifdef B200
  { 200, B200, },
#endif
#ifdef B300
  { 300, B300, },
#endif
#ifdef B600
  { 600, B600, },
#endif
#ifdef B1200
  { 1200, B1200, },
#endif
#ifdef B1800
  { 1800, B1800, },
#endif
#ifdef B2400
  { 2400, B2400, },
#endif
#ifdef B4800
  { 4800, B4800, },
#endif
#ifdef B9600
  { 9600, B9600, },
#endif
#ifdef B19200
  { 19200, B19200, },
#endif
#ifdef B38400
  { 38400, B38400, },
#endif
#ifndef _POSIX_SOURCE
#ifdef B7200
  { 7200, B7200, },
#endif
#ifdef B14400
  { 14400, B14400, },
#endif
#ifdef B28800
  { 28800, B28800, },
#endif
#ifdef B57600
  { 57600, B57600, },
#endif
#ifdef B76800
  { 76800, B76800, },
#endif
#ifdef B115200
  { 115200, B115200, },
#endif
#ifdef B230400
  { 230400, B230400, },
#endif
#ifdef EXTA
  { 19200, EXTA, },
#endif
#ifdef EXTB
  { 38400, EXTB, },
#endif
#endif				/* _POSIX_SOURCE */
  { 0, 0 }
};

static int
SpeedToInt(speed_t speed)
{
  struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->speed == speed) {
      return (sp->nspeed);
    }
  }
  return 0;
}

speed_t
IntToSpeed(int nspeed)
{
  struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->nspeed == nspeed) {
      return (sp->speed);
    }
  }
  return B0;
}

static void
modem_SetDevice(struct physical *physical, const char *name)
{
  strncpy(physical->name.full, name, sizeof physical->name.full - 1);
  physical->name.full[sizeof physical->name.full - 1] = '\0';
  physical->name.base = strncmp(physical->name.full, "/dev/", 5) ?
    physical->name.full : physical->name.full + 5;
}

/*
 *  modem_Timeout() watches DCD signal and notifies if it's status is changed.
 *
 */
static void
modem_Timeout(void *data)
{
  struct physical *modem = data;
  int ombits = modem->mbits;
  int change;

  StopTimer(&modem->Timer);
  StartTimer(&modem->Timer);

  if (modem->dev_is_modem) {
    if (modem->fd >= 0) {
      if (ioctl(modem->fd, TIOCMGET, &modem->mbits) < 0) {
	LogPrintf(LogPHASE, "%s: ioctl error (%s)!\n", modem->link.name,
                  strerror(errno));
        datalink_Down(modem->dl, 0);
	return;
      }
    } else
      modem->mbits = 0;
    change = ombits ^ modem->mbits;
    if (change & TIOCM_CD) {
      if (modem->mbits & TIOCM_CD)
        LogPrintf(LogDEBUG, "%s: offline -> online\n", modem->link.name);
      else {
        LogPrintf(LogDEBUG, "%s: online -> offline\n", modem->link.name);
        LogPrintf(LogPHASE, "%s: Carrier lost\n", modem->link.name);
        datalink_Down(modem->dl, 0);
      }
    } else
      LogPrintf(LogDEBUG, "%s: Still %sline\n", modem->link.name,
                Online(modem) ? "on" : "off");
  } else if (!Online(modem)) {
    /* mbits was set to zero in modem_Open() */
    modem->mbits = TIOCM_CD;
  }
}

static void
modem_StartTimer(struct bundle *bundle, struct physical *modem)
{
  struct pppTimer *ModemTimer;

  ModemTimer = &modem->Timer;

  StopTimer(ModemTimer);
  ModemTimer->load = SECTICKS;
  ModemTimer->func = modem_Timeout;
  ModemTimer->name = "modem CD";
  ModemTimer->arg = modem;
  LogPrintf(LogDEBUG, "%s: Using modem_Timeout [%p]\n",
            modem->link.name, modem_Timeout);
  StartTimer(ModemTimer);
}

static struct parity {
  const char *name;
  const char *name1;
  int set;
} validparity[] = {
  { "even", "P_EVEN", CS7 | PARENB },
  { "odd", "P_ODD", CS7 | PARENB | PARODD },
  { "none", "P_ZERO", CS8 },
  { NULL, 0 },
};

static int
GetParityValue(const char *str)
{
  struct parity *pp;

  for (pp = validparity; pp->name; pp++) {
    if (strcasecmp(pp->name, str) == 0 ||
	strcasecmp(pp->name1, str) == 0) {
      return pp->set;
    }
  }
  return (-1);
}

int
modem_SetParity(struct physical *modem, const char *str)
{
  struct termios rstio;
  int val;

  val = GetParityValue(str);
  if (val > 0) {
    modem->cfg.parity = val;
    tcgetattr(modem->fd, &rstio);
    rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
    rstio.c_cflag |= val;
    tcsetattr(modem->fd, TCSADRAIN, &rstio);
    return 0;
  }
  LogPrintf(LogWARN, "%s: %s: Invalid parity\n", modem->link.name, str);
  return -1;
}

static int
OpenConnection(const char *name, char *host, char *port)
{
  struct sockaddr_in dest;
  int sock;
  struct hostent *hp;
  struct servent *sp;

  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = inet_addr(host);
  if (dest.sin_addr.s_addr == INADDR_NONE) {
    hp = gethostbyname(host);
    if (hp) {
      memcpy(&dest.sin_addr.s_addr, hp->h_addr_list[0], 4);
    } else {
      LogPrintf(LogWARN, "%s: %s: unknown host\n", name, host);
      return (-1);
    }
  }
  dest.sin_port = htons(atoi(port));
  if (dest.sin_port == 0) {
    sp = getservbyname(port, "tcp");
    if (sp) {
      dest.sin_port = sp->s_port;
    } else {
      LogPrintf(LogWARN, "%s: %s: unknown service\n", name, port);
      return (-1);
    }
  }
  LogPrintf(LogPHASE, "%s: Connecting to %s:%s\n", name, host, port);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return (sock);
  }
  if (connect(sock, (struct sockaddr *)&dest, sizeof dest) < 0) {
    LogPrintf(LogWARN, "%s: connect: %s\n", name, strerror(errno));
    return (-1);
  }
  return (sock);
}

static int
modem_lock(struct physical *modem, int tunno)
{
  int res;
  FILE *lockfile;
  char fn[MAXPATHLEN];

  if (*modem->name.full != '/')
    return 0;

  if (modem->type != PHYS_DIRECT &&
      (res = ID0uu_lock(modem->name.base)) != UU_LOCK_OK) {
    if (res == UU_LOCK_INUSE)
      LogPrintf(LogPHASE, "%s: %s is in use\n",
                modem->link.name, modem->name.full);
    else
      LogPrintf(LogPHASE, "%s: %s is in use: uu_lock: %s\n",
                modem->link.name, modem->name.full, uu_lockerr(res));
    return (-1);
  }

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, modem->name.base);
  lockfile = ID0fopen(fn, "w");
  if (lockfile != NULL) {
    fprintf(lockfile, "tun%d\n", tunno);
    fclose(lockfile);
  }
#ifndef RELEASE_CRUNCH
  else
    LogPrintf(LogALERT, "%s: Can't create %s: %s\n",
              modem->link.name, fn, strerror(errno));
#endif

  return 0;
}

static void
modem_Unlock(struct physical *modem)
{
  char fn[MAXPATHLEN];

  if (*modem->name.full != '/')
    return;

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, modem->name.base);
#ifndef RELEASE_CRUNCH
  if (ID0unlink(fn) == -1)
    LogPrintf(LogALERT, "%s: Can't remove %s: %s\n",
              modem->link.name, fn, strerror(errno));
#else
  ID0unlink(fn);
#endif

  if (modem->type != PHYS_DIRECT && ID0uu_unlock(modem->name.base) == -1)
    LogPrintf(LogALERT, "%s: Can't uu_unlock %s\n", modem->link.name, fn);
}

static void
modem_Found(struct physical *modem, struct bundle *bundle)
{
  throughput_start(&modem->link.throughput, "modem throughput",
                   Enabled(bundle, OPT_THROUGHPUT));
  modem->connect_count++;
  LogPrintf(LogPHASE, "%s: Connected!\n", modem->link.name);
}

int
modem_Open(struct physical *modem, struct bundle *bundle)
{
  struct termios rstio;
  int oldflag;
  char *host, *port;
  char *cp;
  char tmpDeviceList[sizeof modem->cfg.devlist];
  char *tmpDevice;

  if (modem->fd >= 0)
    LogPrintf(LogDEBUG, "%s: Open: Modem is already open!\n", modem->link.name);
    /* We're going back into "term" mode */
  else if (modem->type == PHYS_DIRECT) {
    if (isatty(STDIN_FILENO)) {
      LogPrintf(LogDEBUG, "%s: Open(direct): Modem is a tty\n",
                modem->link.name);
      modem_SetDevice(modem, ttyname(STDIN_FILENO));
      if (modem_lock(modem, bundle->unit) == -1) {
        close(STDIN_FILENO);
        return -1;
      }
      modem->fd = STDIN_FILENO;
      modem_Found(modem, bundle);
    } else {
      LogPrintf(LogDEBUG, "%s: Open(direct): Modem is not a tty\n",
                modem->link.name);
      modem_SetDevice(modem, "");
      /* We don't call modem_Timeout() with this type of connection */
      modem_Found(modem, bundle);
      return modem->fd = STDIN_FILENO;
    }
  } else {
    strncpy(tmpDeviceList, modem->cfg.devlist, sizeof tmpDeviceList - 1);
    tmpDeviceList[sizeof tmpDeviceList - 1] = '\0';

    for(tmpDevice=strtok(tmpDeviceList, ", "); tmpDevice && modem->fd < 0;
	tmpDevice=strtok(NULL,", ")) {
      modem_SetDevice(modem, tmpDevice);

      if (*modem->name.full == '/') {
	if (modem_lock(modem, bundle->unit) != -1) {
	  modem->fd = ID0open(modem->name.full, O_RDWR | O_NONBLOCK);
	  if (modem->fd < 0) {
	    LogPrintf(LogERROR, "%s: Open(\"%s\"): %s\n",
                      modem->link.name, modem->name.full, strerror(errno));
	    modem_Unlock(modem);
	  } else {
	    modem_Found(modem, bundle);
	    LogPrintf(LogDEBUG, "%s: Opened %s\n",
                      modem->link.name, modem->name.full);
	  }
	}
      } else {
	/* PPP over TCP */
        /*
         * XXX: Fix me - this should be another sort of link (similar to a
         * physical
         */
	cp = strchr(modem->name.full, ':');
	if (cp) {
	  *cp = '\0';
	  host = modem->name.full;
	  port = cp + 1;
	  if (*host && *port) {
	    modem->fd = OpenConnection(modem->link.name, host, port);
	    *cp = ':';		/* Don't destroy name.full */
	    if (modem->fd < 0)
	      return (-1);
	    modem_Found(modem, bundle);
	    LogPrintf(LogDEBUG, "%s: Opened socket %s\n", modem->link.name,
                      modem->name.full);
	  } else {
	    *cp = ':';		/* Don't destroy name.full */
	    LogPrintf(LogERROR, "%s: Invalid host:port: \"%s\"\n",
                      modem->link.name, modem->name.full);
	    return (-1);
	  }
	} else {
	  LogPrintf(LogERROR, "%s: Device (%s) must begin with a '/' or be a"
                    " host:port pair\n", modem->link.name, modem->name.full);
	  return (-1);
	}
      }
    }

    if (modem->fd < 0)
       return modem->fd;
  }

  /*
   * If we are working on tty device, change it's mode into the one desired
   * for further operation. In this implementation, we assume that modem is
   * configuted to use CTS/RTS flow control.
   */
  modem->mbits = 0;
  modem->dev_is_modem = isatty(modem->fd) || Physical_IsSync(modem);
  if (Physical_IsSync(modem))
    nointr_sleep(1);
  if (modem->dev_is_modem && !Physical_IsSync(modem)) {
    tcgetattr(modem->fd, &rstio);
    modem->ios = rstio;
    LogPrintf(LogDEBUG, "%s: Open: modem (get): fd = %d, iflag = %lx, "
              "oflag = %lx, cflag = %lx\n", modem->link.name, modem->fd,
              (u_long)rstio.c_iflag, (u_long)rstio.c_oflag,
              (u_long)rstio.c_cflag);
    cfmakeraw(&rstio);
    if (modem->cfg.rts_cts)
      rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
    else {
      rstio.c_cflag |= CLOCAL;
      rstio.c_iflag |= IXOFF;
    }
    rstio.c_iflag |= IXON;
    if (modem->type != PHYS_DEDICATED)
      rstio.c_cflag |= HUPCL;

    if (modem->type != PHYS_DIRECT) {
      /* Change tty speed when we're not in -direct mode */
      rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
      rstio.c_cflag |= modem->cfg.parity;
      if (cfsetspeed(&rstio, IntToSpeed(modem->cfg.speed)) == -1)
	LogPrintf(LogWARN, "%s: %s: Unable to set speed to %d\n",
		  modem->link.name, modem->name.full, modem->cfg.speed);
    }
    tcsetattr(modem->fd, TCSADRAIN, &rstio);
    LogPrintf(LogDEBUG, "%s: modem (put): iflag = %lx, oflag = %lx, "
              "cflag = %lx\n", modem->link.name, (u_long)rstio.c_iflag,
              (u_long)rstio.c_oflag, (u_long)rstio.c_cflag);

    if (ioctl(modem->fd, TIOCMGET, &modem->mbits) == -1) {
      if (modem->type != PHYS_DIRECT) {
        LogPrintf(LogERROR, "%s: Open: Cannot get modem status: %s\n",
		  modem->link.name, strerror(errno));
        modem_LogicalClose(modem);
	return (-1);
      } else
        modem->mbits = TIOCM_CD;
    }
    LogPrintf(LogDEBUG, "%s: Open: modem control = %o\n",
              modem->link.name, modem->mbits);

    oldflag = fcntl(modem->fd, F_GETFL, 0);
    if (oldflag < 0) {
      LogPrintf(LogERROR, "%s: Open: Cannot get modem flags: %s\n",
		modem->link.name, strerror(errno));
      modem_LogicalClose(modem);
      return (-1);
    }
    fcntl(modem->fd, F_SETFL, oldflag & ~O_NONBLOCK);

    /* We do the timer only for ttys */
    modem_StartTimer(bundle, modem);
  }

  return modem->fd;
}

int
modem_Speed(struct physical *modem)
{
  struct termios rstio;

  tcgetattr(modem->fd, &rstio);
  return (SpeedToInt(cfgetispeed(&rstio)));
}

/*
 * Put modem tty line into raw mode which is necessary in packet mode operation
 */
int
modem_Raw(struct physical *modem, struct bundle *bundle)
{
  struct termios rstio;
  int oldflag;

  LogPrintf(LogDEBUG, "%s: Entering modem_Raw\n", modem->link.name);

  if (!isatty(modem->fd) || Physical_IsSync(modem))
    return 0;

  if (modem->type != PHYS_DIRECT && modem->fd >= 0 && !Online(modem))
    LogPrintf(LogDEBUG, "%s: Raw: modem = %d, mbits = %x\n",
              modem->link.name, modem->fd, modem->mbits);

  tcgetattr(modem->fd, &rstio);
  cfmakeraw(&rstio);
  if (modem->cfg.rts_cts)
    rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else
    rstio.c_cflag |= CLOCAL;

  if (modem->type != PHYS_DEDICATED)
    rstio.c_cflag |= HUPCL;

  tcsetattr(modem->fd, TCSADRAIN, &rstio);
  oldflag = fcntl(modem->fd, F_GETFL, 0);
  if (oldflag < 0)
    return (-1);
  fcntl(modem->fd, F_SETFL, oldflag | O_NONBLOCK);

  modem_Timeout(modem);

  return 0;
}

static void
modem_Unraw(struct physical *modem)
{
  int oldflag;

  if (isatty(modem->fd) && !Physical_IsSync(modem)) {
    tcsetattr(modem->fd, TCSAFLUSH, &modem->ios);
    oldflag = fcntl(modem->fd, F_GETFL, 0);
    if (oldflag < 0)
      return;
    (void) fcntl(modem->fd, F_SETFL, oldflag & ~O_NONBLOCK);
  }
}

static void
modem_PhysicalClose(struct physical *modem)
{
  LogPrintf(LogDEBUG, "%s: Physical Close\n", modem->link.name);
  close(modem->fd);
  modem->fd = -1;
  StopTimer(&modem->Timer);
  bundle_SetTtyCommandMode(modem->dl->bundle, modem->dl);
  throughput_stop(&modem->link.throughput);
  throughput_log(&modem->link.throughput, LogPHASE, modem->link.name);
}

void
modem_Offline(struct physical *modem)
{
  if (modem->fd >= 0) {
    struct termios tio;

    modem->mbits &= ~TIOCM_DTR;
    if (isatty(modem->fd) && Online(modem)) {
      tcgetattr(modem->fd, &tio);
      if (cfsetspeed(&tio, B0) == -1)
        LogPrintf(LogWARN, "%s: Unable to set modem to speed 0\n",
                  modem->link.name);
      else
        tcsetattr(modem->fd, TCSANOW, &tio);
      /* nointr_sleep(1); */
    }
    LogPrintf(LogPHASE, "%s: Disconnected!\n", modem->link.name);
  }
}

void
modem_Close(struct physical *modem)
{
  if (modem->fd < 0)
    return;

  LogPrintf(LogDEBUG, "%s: Close\n", modem->link.name);

  if (!isatty(modem->fd)) {
    modem_PhysicalClose(modem);
    *modem->name.full = '\0';
    modem->name.base = modem->name.full;
    return;
  }

  if (modem->fd >= 0) {
    tcflush(modem->fd, TCIOFLUSH);
    modem_Unraw(modem);
    modem_LogicalClose(modem);
  }
}

void
modem_Destroy(struct physical *modem)
{
  modem_Close(modem);
  free(modem);
}

static void
modem_LogicalClose(struct physical *modem)
{
  LogPrintf(LogDEBUG, "%s: Logical Close\n", modem->link.name);
  if (modem->fd >= 0) {
    Physical_Logout(modem);
    modem_PhysicalClose(modem);
    modem_Unlock(modem);
  }
  *modem->name.full = '\0';
  modem->name.base = modem->name.full;
}

static void
modem_DescriptorWrite(struct descriptor *d, struct bundle *bundle,
                      const fd_set *fdset)
{
  struct physical *modem = descriptor2physical(d);
  int nb, nw;

  if (modem->out == NULL)
    modem->out = link_Dequeue(&modem->link);

  if (modem->out) {
    nb = modem->out->cnt;
    nw = write(modem->fd, MBUF_CTOP(modem->out), nb);
    LogPrintf(LogDEBUG, "%s: DescriptorWrite: wrote: %d(%d) to %d\n",
              modem->link.name, nw, nb, modem->fd);
    if (nw > 0) {
      modem->out->cnt -= nw;
      modem->out->offset += nw;
      if (modem->out->cnt == 0)
	modem->out = mbfree(modem->out);
    } else if (nw < 0) {
      if (errno != EAGAIN) {
	LogPrintf(LogWARN, "%s: write (%d): %s\n", modem->link.name, modem->fd,
		  strerror(errno));
        datalink_Down(modem->dl, 0);
      }
    }
  }
}

int
modem_ShowStatus(struct cmdargs const *arg)
{
  struct physical *modem = arg->cx->physical;
#ifdef TIOCOUTQ
  int nb;
#endif

  prompt_Printf(arg->prompt, "Name: %s\n", modem->link.name);
  prompt_Printf(arg->prompt, " State:           ");
  if (modem->fd >= 0) {
    if (isatty(modem->fd))
      prompt_Printf(arg->prompt, "open, %s carrier\n",
                    Online(modem) ? "with" : "no");
    else
      prompt_Printf(arg->prompt, "open\n");
  } else
    prompt_Printf(arg->prompt, "closed\n");
  prompt_Printf(arg->prompt, " Device:          %s\n",
                *modem->name.full ?  modem->name.full :
                modem->type == PHYS_DIRECT ? "stdin" : "N/A");

  prompt_Printf(arg->prompt, " Link Type:       %s\n", mode2Nam(modem->type));
  prompt_Printf(arg->prompt, " Connect Count:   %d\n",
                modem->connect_count);
#ifdef TIOCOUTQ
  if (modem->fd >= 0 && ioctl(modem->fd, TIOCOUTQ, &nb) >= 0)
      prompt_Printf(arg->prompt, " Physical outq:   %d\n", nb);
#endif

  prompt_Printf(arg->prompt, " Queued Packets:  %d\n",
                link_QueueLen(&modem->link));
  prompt_Printf(arg->prompt, " Phone Number:    %s\n", arg->cx->phone.chosen);

  prompt_Printf(arg->prompt, "\nDefaults:\n");
  prompt_Printf(arg->prompt, " Device List:     %s\n", modem->cfg.devlist);
  prompt_Printf(arg->prompt, " Characteristics: ");
  if (Physical_IsSync(arg->cx->physical))
    prompt_Printf(arg->prompt, "sync");
  else
    prompt_Printf(arg->prompt, "%dbps", modem->cfg.speed);

  switch (modem->cfg.parity & CSIZE) {
  case CS7:
    prompt_Printf(arg->prompt, ", cs7");
    break;
  case CS8:
    prompt_Printf(arg->prompt, ", cs8");
    break;
  }
  if (modem->cfg.parity & PARENB) {
    if (modem->cfg.parity & PARODD)
      prompt_Printf(arg->prompt, ", odd parity");
    else
      prompt_Printf(arg->prompt, ", even parity");
  } else
    prompt_Printf(arg->prompt, ", no parity");

  prompt_Printf(arg->prompt, ", CTS/RTS %s\n",
                (modem->cfg.rts_cts ? "on" : "off"));


  prompt_Printf(arg->prompt, "\n");
  throughput_disp(&modem->link.throughput, arg->prompt);

  return 0;
}

static void
modem_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                     const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  u_char rbuff[MAX_MRU], *cp;
  int n;

  /* something to read from modem */
  if (p->link.lcp.fsm.state <= ST_CLOSED)
    nointr_usleep(10000);

  n = Physical_Read(p, rbuff, sizeof rbuff);
  if (p->type == PHYS_DIRECT && n <= 0)
    datalink_Down(p->dl, 0);
  else
    LogDumpBuff(LogASYNC, "ReadFromModem", rbuff, n);

  if (p->link.lcp.fsm.state <= ST_CLOSED) {
    /* In -dedicated mode, we just discard input until LCP is started */
    if (p->type != PHYS_DEDICATED) {
      cp = HdlcDetect(p, rbuff, n);
      if (cp) {
        /* LCP packet is detected. Turn ourselves into packet mode */
        if (cp != rbuff) {
          /* XXX missing return value checks */
          Physical_Write(p, rbuff, cp - rbuff);
          Physical_Write(p, "\r\n", 2);
        }
        datalink_Up(p->dl, 0, 1);
      } else
        bundle_WriteTermPrompt(p->dl->bundle, p->dl, rbuff, n);
    }
  } else if (n > 0)
    async_Input(bundle, rbuff, n, p);
}

static int
modem_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  return Physical_UpdateSet(d, r, w, e, n, 0);
}

struct physical *
modem_FromBinary(struct datalink *dl, int fd)
{
  struct physical *p = (struct physical *)malloc(sizeof(struct physical));
  int got;

  /*
   * We expect:
   *  .----------.
   *  | physical |
   *  `----------'
   */

  got = fullread(fd, p, sizeof *p);
  if (got != sizeof *p) {
    LogPrintf(LogWARN, "Cannot receive physical"
              " (got %d bytes, not %d)\n", got, sizeof *p);
    close(fd);
    free(p);
    return NULL;
  }
  p->link.name = dl->name;
  throughput_init(&p->link.throughput);
  memset(&p->Timer, '\0', sizeof p->Timer);
  memset(p->link.Queue, '\0', sizeof p->link.Queue);

  p->desc.UpdateSet = modem_UpdateSet;
  p->desc.IsSet = Physical_IsSet;
  p->desc.Read = modem_DescriptorRead;
  p->desc.Write = modem_DescriptorWrite;
  p->desc.next = NULL;
  p->type = PHYS_DIRECT;
  p->dl = dl;
  p->name.base = strrchr(p->name.full, '/');
  p->mbits = 0;
  p->dev_is_modem = 1;
  p->out = NULL;
  p->connect_count = 1;

  p->link.lcp.fsm.bundle = dl->bundle;
  p->link.lcp.fsm.link = &p->link;
  memset(&p->link.lcp.fsm.FsmTimer, '\0', sizeof p->link.lcp.fsm.FsmTimer);
  memset(&p->link.lcp.fsm.OpenTimer, '\0', sizeof p->link.lcp.fsm.OpenTimer);
  memset(&p->link.lcp.fsm.StoppedTimer, '\0',
         sizeof p->link.lcp.fsm.StoppedTimer);
  p->link.lcp.fsm.parent = &dl->fsmp;
  lcp_SetupCallbacks(&p->link.lcp);

  p->link.ccp.fsm.bundle = dl->bundle;
  p->link.ccp.fsm.link = &p->link;
  /* Our in.state & out.state are NULL (no link-level ccp yet) */
  memset(&p->link.ccp.fsm.FsmTimer, '\0', sizeof p->link.ccp.fsm.FsmTimer);
  memset(&p->link.ccp.fsm.OpenTimer, '\0', sizeof p->link.ccp.fsm.OpenTimer);
  memset(&p->link.ccp.fsm.StoppedTimer, '\0',
         sizeof p->link.ccp.fsm.StoppedTimer);
  p->link.ccp.fsm.parent = &dl->fsmp;
  ccp_SetupCallbacks(&p->link.ccp);

  p->hdlc.lqm.owner = &p->link.lcp;
  p->hdlc.ReportTimer.state = TIMER_STOPPED;
  p->hdlc.lqm.timer.state = TIMER_STOPPED;
  if (p->hdlc.lqm.method && p->hdlc.lqm.timer.load)
    StartLqm(&p->link.lcp);
  hdlc_StartTimer(&p->hdlc);

  p->fd = fd;		/* Now talk down it :-) */
  throughput_start(&p->link.throughput, "modem throughput",
                   Enabled(dl->bundle, OPT_THROUGHPUT));
  /* Don't need a modem timer.... */
  /* Don't need to lock the device in -direct mode */

  return p;
}

int
modem_ToBinary(struct physical *p, int fd)
{
  int link_fd;

  /*
   * We send:
   *  .----------.
   *  | physical |
   *  `----------'
   */

  hdlc_StopTimer(&p->hdlc);
  StopLqrTimer(p);
  StopTimer(&p->link.lcp.fsm.FsmTimer);
  StopTimer(&p->link.ccp.fsm.FsmTimer);
  StopTimer(&p->link.lcp.fsm.OpenTimer);
  StopTimer(&p->link.ccp.fsm.OpenTimer);
  StopTimer(&p->link.lcp.fsm.StoppedTimer);
  StopTimer(&p->link.ccp.fsm.StoppedTimer);
  StopTimer(&p->Timer);
  StopTimer(&p->link.throughput.Timer);

  if (fd != -1 && write(fd, p, sizeof *p) != sizeof *p) {
    LogPrintf(LogERROR, "Failed sending physical\n");
    close(fd);
    fd = -1;
  }

  link_fd = p->fd;
  free(p);

  return link_fd;
}
