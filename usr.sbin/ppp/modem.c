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
 * $Id: modem.c,v 1.77.2.39 1998/03/20 19:47:19 brian Exp $
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
#include "ip.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"
#include "main.h"
#include "throughput.h"
#include "async.h"
#include "iplist.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "bundle.h"
#include "link.h"
#include "physical.h"
#include "prompt.h"
#include "chat.h"
#include "ccp.h"
#include "auth.h"
#include "chap.h"
#include "datalink.h"


#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define O_NONBLOCK O_NDELAY
#endif
#endif

static void modem_StartOutput(struct link *, struct bundle *);
static int modem_IsActive(struct link *);
static void modem_Hangup(struct link *, int);
static void modem_Destroy(struct link *);
static void modem_DescriptorRead(struct descriptor *, struct bundle *,
                                 const fd_set *);
static int modem_UpdateSet(struct descriptor *, fd_set *, fd_set *, fd_set *,
                           int *);

struct physical *
modem_Create(const char *name, struct datalink *dl)
{
  struct physical *p;

  p = (struct physical *)malloc(sizeof(struct physical));
  if (!p)
    return NULL;

  p->link.type = PHYSICAL_LINK;
  p->link.name = strdup(name);
  p->link.len = sizeof *p;
  memset(&p->link.throughput, '\0', sizeof p->link.throughput);
  memset(&p->link.Timer, '\0', sizeof p->link.Timer);
  memset(p->link.Queue, '\0', sizeof p->link.Queue);
  memset(p->link.proto_in, '\0', sizeof p->link.proto_in);
  memset(p->link.proto_out, '\0', sizeof p->link.proto_out);
  p->link.StartOutput = modem_StartOutput;
  p->link.IsActive = modem_IsActive;
  p->link.Close = modem_Hangup;
  p->link.Destroy = modem_Destroy;

  p->desc.type = PHYSICAL_DESCRIPTOR;
  p->desc.UpdateSet = modem_UpdateSet;
  p->desc.IsSet = Physical_IsSet;
  p->desc.Read = modem_DescriptorRead;
  p->desc.Write = Physical_DescriptorWrite;

  hdlc_Init(&p->hdlc);
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

  p->cfg.is_direct = 0;		/* not yet used */
  p->cfg.is_dedicated = 0;	/* not yet used */
  p->cfg.rts_cts = MODEM_CTSRTS;
  p->cfg.speed = MODEM_SPEED;
  p->cfg.parity = CS8;
  strncpy(p->cfg.devlist, MODEM_LIST, sizeof p->cfg.devlist - 1);
  p->cfg.devlist[sizeof p->cfg.devlist - 1] = '\0';

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

struct timeoutArg {
  struct bundle *bundle;
  struct physical *modem;
};

/*
 *  modem_Timeout() watches DCD signal and notifies if it's status is changed.
 *
 */
static void
modem_Timeout(void *data)
{
  struct timeoutArg *to = data;
  int ombits = to->modem->mbits;
  int change;

  StopTimer(&to->modem->link.Timer);
  StartTimer(&to->modem->link.Timer);

  if (to->modem->dev_is_modem) {
    if (to->modem->fd >= 0) {
      if (ioctl(to->modem->fd, TIOCMGET, &to->modem->mbits) < 0) {
	LogPrintf(LogPHASE, "ioctl error (%s)!\n", strerror(errno));
        link_Close(&to->modem->link, to->bundle, 0, 0);
	return;
      }
    } else
      to->modem->mbits = 0;
    change = ombits ^ to->modem->mbits;
    if (change & TIOCM_CD) {
      if (to->modem->mbits & TIOCM_CD) {
        LogPrintf(LogDEBUG, "modem_Timeout: offline -> online\n");
	/*
	 * In dedicated mode, start packet mode immediate after we detected
	 * carrier.
	 */
      } else {
        LogPrintf(LogDEBUG, "modem_Timeout: online -> offline\n");
        link_Close(&to->modem->link, to->bundle, 0, 0);
      }
    }
    else
      LogPrintf(LogDEBUG, "modem_Timeout: Still %sline\n",
                Online(to->modem) ? "on" : "off");
  } else if (!Online(to->modem)) {
    /* mbits was set to zero in modem_Open() */
    to->modem->mbits = TIOCM_CD;
  }
}

static void
modem_StartTimer(struct bundle *bundle, struct physical *modem)
{
  struct pppTimer *ModemTimer;
  static struct timeoutArg to;

  to.modem = modem;
  to.bundle = bundle;
  ModemTimer = &modem->link.Timer;

  StopTimer(ModemTimer);
  ModemTimer->state = TIMER_STOPPED;
  ModemTimer->load = SECTICKS;
  ModemTimer->func = modem_Timeout;
  ModemTimer->arg = &to;
  LogPrintf(LogDEBUG, "ModemTimer using modem_Timeout() - %p\n", modem_Timeout);
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
  LogPrintf(LogWARN, "modem_SetParity: %s: Invalid parity\n", str);
  return -1;
}

static int
OpenConnection(char *host, char *port)
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
      LogPrintf(LogWARN, "OpenConnection: unknown host: %s\n", host);
      return (-1);
    }
  }
  dest.sin_port = htons(atoi(port));
  if (dest.sin_port == 0) {
    sp = getservbyname(port, "tcp");
    if (sp) {
      dest.sin_port = sp->s_port;
    } else {
      LogPrintf(LogWARN, "OpenConnection: unknown service: %s\n", port);
      return (-1);
    }
  }
  LogPrintf(LogPHASE, "Connecting to %s:%s\n", host, port);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return (sock);
  }
  if (connect(sock, (struct sockaddr *)&dest, sizeof dest) < 0) {
    LogPrintf(LogWARN, "OpenConnection: connection failed.\n");
    return (-1);
  }
  LogPrintf(LogDEBUG, "OpenConnection: modem fd is %d.\n", sock);
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

  if (
#ifdef notyet
      !modem->is_direct && 
#else
      !(mode & MODE_DIRECT) &&
#endif
      (res = ID0uu_lock(modem->name.base)) != UU_LOCK_OK) {
    if (res == UU_LOCK_INUSE)
      LogPrintf(LogPHASE, "Modem %s is in use\n", modem->name.full);
    else
      LogPrintf(LogPHASE, "Modem %s is in use: uu_lock: %s\n",
                modem->name.full, uu_lockerr(res));
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
    LogPrintf(LogALERT, "Warning: Can't create %s: %s\n", fn, strerror(errno));
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
    LogPrintf(LogALERT, "Warning: Can't remove %s: %s\n", fn, strerror(errno));
#else
  ID0unlink(fn);
#endif

  if (
#ifdef notyet
      !modem->is_direct &&
#else
      !(mode & MODE_DIRECT) &&
#endif
      ID0uu_unlock(modem->name.base) == -1)
    LogPrintf(LogALERT, "Warning: Can't uu_unlock %s\n", fn);
}

static void
modem_Found(struct physical *modem)
{
  throughput_start(&modem->link.throughput);
  modem->connect_count++;
  LogPrintf(LogPHASE, "Connected!\n");
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
    LogPrintf(LogDEBUG, "modem_Open: Modem is already open!\n");
    /* We're going back into "term" mode */
  else if (
#ifdef notyet
	   modem->is_direct
#else
	   mode & MODE_DIRECT
#endif
	   ) {
    if (isatty(STDIN_FILENO)) {
      LogPrintf(LogDEBUG, "modem_Open(direct): Modem is a tty\n");
      modem_SetDevice(modem, ttyname(STDIN_FILENO));
      if (modem_lock(modem, bundle->unit) == -1) {
        close(STDIN_FILENO);
        return -1;
      }
      modem->fd = STDIN_FILENO;
      modem_Found(modem);
    } else {
      LogPrintf(LogDEBUG, "modem_Open(direct): Modem is not a tty\n");
      modem_SetDevice(modem, "");
      /* We don't call modem_Timeout() with this type of connection */
      modem_Found(modem);
      return modem->fd = STDIN_FILENO;
    }
  } else {
    strncpy(tmpDeviceList, modem->cfg.devlist, sizeof tmpDeviceList - 1);
    tmpDeviceList[sizeof tmpDeviceList - 1] = '\0';

    for(tmpDevice=strtok(tmpDeviceList, ", "); tmpDevice && (modem->fd < 0);
	tmpDevice=strtok(NULL,", ")) {
      modem_SetDevice(modem, tmpDevice);

      if (*modem->name.full == '/') {
	if (modem_lock(modem, bundle->unit) == -1)
	  modem->fd = -1;
	else {
	  modem->fd = ID0open(modem->name.full, O_RDWR | O_NONBLOCK);
	  if (modem->fd < 0) {
	    LogPrintf(LogERROR, "modem_Open failed: %s: %s\n", modem->name.full,
		      strerror(errno));
	    modem_Unlock(modem);
	    modem->fd = -1;
	  }
	  else {
	    modem_Found(modem);
	    LogPrintf(LogDEBUG, "modem_Open: Modem is %s\n", modem->name.full);
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
	    modem->fd = OpenConnection(host, port);
	    *cp = ':';		/* Don't destroy name.full */
	    if (modem->fd < 0)
	      return (-1);
	    modem_Found(modem);
	    LogPrintf(LogDEBUG, "modem_Open: Modem is socket %s\n",
                      modem->name.full);
	  } else {
	    *cp = ':';		/* Don't destroy name.full */
	    LogPrintf(LogERROR, "Invalid host:port: \"%s\"\n",
                      modem->name.full);
	    return (-1);
	  }
	} else {
	  LogPrintf(LogERROR, "Device (%s) must begin with a '/' or be a"
                    " host:port pair\n", modem->name.full);
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
    LogPrintf(LogDEBUG, "modem_Open: modem = %d\n", modem->fd);
    LogPrintf(LogDEBUG, "modem_Open: modem (get): iflag = %x, oflag = %x,"
	      " cflag = %x\n", rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
    cfmakeraw(&rstio);
    if (modem->cfg.rts_cts)
      rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
    else {
      rstio.c_cflag |= CLOCAL;
      rstio.c_iflag |= IXOFF;
    }
    rstio.c_iflag |= IXON;
    if (
#ifdef notyet
	!modem->is_dedicated
#else
	!(mode & MODE_DEDICATED)
#endif
	)
      rstio.c_cflag |= HUPCL;
    if (
#ifdef notyet
	!modem->is_direct
#else
	!(mode & MODE_DIRECT)
#endif
	) {

      /*
       * If we are working as direct mode, don't change tty speed.
       */
      rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
      rstio.c_cflag |= modem->cfg.parity;
      if (cfsetspeed(&rstio, IntToSpeed(modem->cfg.speed)) == -1) {
	LogPrintf(LogWARN, "Unable to set modem speed (modem %d to %d)\n",
		  modem->fd, modem->cfg.speed);
      }
    }
    tcsetattr(modem->fd, TCSADRAIN, &rstio);
    LogPrintf(LogDEBUG, "modem (put): iflag = %x, oflag = %x, cflag = %x\n",
	      rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);

    if (
#ifdef notyet
	!modem->is_direct
#else
	!(mode & MODE_DIRECT)
#endif
	)
      if (ioctl(modem->fd, TIOCMGET, &modem->mbits)) {
        LogPrintf(LogERROR, "modem_Open: Cannot get modem status: %s\n",
		  strerror(errno));
        modem_LogicalClose(modem);
	return (-1);
      }
    LogPrintf(LogDEBUG, "modem_Open: modem control = %o\n", modem->mbits);

    oldflag = fcntl(modem->fd, F_GETFL, 0);
    if (oldflag < 0) {
      LogPrintf(LogERROR, "modem_Open: Cannot get modem flags: %s\n",
		strerror(errno));
      modem_LogicalClose(modem);
      return (-1);
    }
    (void) fcntl(modem->fd, F_SETFL, oldflag & ~O_NONBLOCK);
  }
  modem_StartTimer(bundle, modem);

  return (modem->fd);
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
  struct timeoutArg to;
  struct termios rstio;
  int oldflag;

  LogPrintf(LogDEBUG, "Entering modem_Raw\n");

  if (!isatty(modem->fd) || Physical_IsSync(modem))
    return 0;

  if (
#ifdef notyet
      !modem->is_direct &&
#else
      !(mode & MODE_DIRECT) &&
#endif
      modem->fd >= 0 && !Online(modem)) {
    LogPrintf(LogDEBUG, "modem_Raw: modem = %d, mbits = %x\n",
			  modem->fd, modem->mbits);
  }
  tcgetattr(modem->fd, &rstio);
  cfmakeraw(&rstio);
  if (modem->cfg.rts_cts)
    rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else
    rstio.c_cflag |= CLOCAL;

  if (
#ifdef notyet
      !modem->is_dedicated
#else
      !(mode & MODE_DEDICATED)
#endif
      )
    rstio.c_cflag |= HUPCL;
  tcsetattr(modem->fd, TCSADRAIN, &rstio);
  oldflag = fcntl(modem->fd, F_GETFL, 0);
  if (oldflag < 0)
    return (-1);
  (void) fcntl(modem->fd, F_SETFL, oldflag | O_NONBLOCK);

  to.modem = modem;
  to.bundle = bundle;
  modem_Timeout(&to);

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
  LogPrintf(LogDEBUG, "modem_PhysicalClose\n");
  close(modem->fd);
  modem->fd = -1;
  throughput_log(&modem->link.throughput, LogPHASE, "Modem");
}

static int force_hack;

static void
modem_Hangup(struct link *l, int dedicated_force)
{
  /* We're about to close (pre hangup script) */
  struct physical *modem = (struct physical *)l;

  force_hack = dedicated_force;
  if (modem->fd >= 0) {
    StopTimer(&modem->link.Timer);
    throughput_stop(&modem->link.throughput);

    if (prompt_IsTermMode(&prompt))
      prompt_TtyCommandMode(&prompt);
  }
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
        LogPrintf(LogWARN, "Unable to set modem to speed 0\n");
      else
        tcsetattr(modem->fd, TCSANOW, &tio);
      /* nointr_sleep(1); */
    }
    LogPrintf(LogPHASE, "%s disconnected!\n", modem->link.name);
  }
}

void
modem_Close(struct physical *modem)
{
  LogPrintf(LogDEBUG, "Close modem (%s)\n",
            modem->fd >= 0 ? "open" : "closed");

  if (modem->fd < 0)
    return;

  if (!isatty(modem->fd)) {
    modem_PhysicalClose(modem);
    return;
  }

  if (modem->fd >= 0) {
    if (force_hack ||
#ifdef notyet
	!modem->is_dedicated
#else
	!(mode & MODE_DEDICATED)
#endif
	) {
      tcflush(modem->fd, TCIOFLUSH);
      modem_Unraw(modem);
      modem_LogicalClose(modem);
    } else {
      /*
       * If we are working as dedicated mode, never close it until we are
       * directed to quit program.
       */
      modem->mbits |= TIOCM_DTR;
      ioctl(modem->fd, TIOCMSET, &modem->mbits);
    }
  }
}

static void
modem_Destroy(struct link *l)
{
  struct physical *p;

  p = link2physical(l);
  if (p->fd != -1)
    modem_Close(p);
  free(l->name);
  free(p);
}

static void
modem_LogicalClose(struct physical *modem)
{
  LogPrintf(LogDEBUG, "modem_LogicalClose\n");
  if (modem->fd >= 0) {
    Physical_Logout(modem);
    modem_PhysicalClose(modem);
    modem_Unlock(modem);
  }
}

static void
modem_StartOutput(struct link *l, struct bundle *bundle)
{
  struct physical *modem = (struct physical *)l;
  int nb, nw;

  if (modem->out == NULL) {
    if (link_QueueLen(l) == 0)
      IpStartOutput(l, bundle);

    modem->out = link_Dequeue(l);
  }

  if (modem->out) {
    nb = modem->out->cnt;
    nw = write(modem->fd, MBUF_CTOP(modem->out), nb);
    LogPrintf(LogDEBUG, "modem_StartOutput: wrote: %d(%d) to %d\n",
              nw, nb, modem->fd);
    if (nw > 0) {
      LogDumpBuff(LogDEBUG, "modem_StartOutput: modem write",
		  MBUF_CTOP(modem->out), nw);
      modem->out->cnt -= nw;
      modem->out->offset += nw;
      if (modem->out->cnt == 0) {
	modem->out = mbfree(modem->out);
	LogPrintf(LogDEBUG, "modem_StartOutput: mbfree\n");
      }
    } else if (nw < 0) {
      if (errno != EAGAIN) {
	LogPrintf(LogERROR, "modem write (%d): %s\n", modem->fd,
		  strerror(errno));
        link_Close(&modem->link, bundle, 0, 0);
      }
    }
  }
}

static int
modem_IsActive(struct link *l)
{
  return ((struct physical *)l)->fd >= 0;
}

int
modem_ShowStatus(struct cmdargs const *arg)
{
  const char *dev;
#ifdef TIOCOUTQ
  int nb;
#endif

  dev = *arg->cx->physical->name.full ?
    arg->cx->physical->name.full : "stdin";

  prompt_Printf(&prompt, "device list:   %s\n", arg->cx->physical->cfg.devlist);
  prompt_Printf(&prompt, "device:        %s\n               ", dev);
  if (Physical_IsSync(arg->cx->physical))
    prompt_Printf(&prompt, "sync");
  else
    prompt_Printf(&prompt, "%dbps", arg->cx->physical->cfg.speed);

  switch (arg->cx->physical->cfg.parity & CSIZE) {
  case CS7:
    prompt_Printf(&prompt, ", cs7");
    break;
  case CS8:
    prompt_Printf(&prompt, ", cs8");
    break;
  }
  if (arg->cx->physical->cfg.parity & PARENB) {
    if (arg->cx->physical->cfg.parity & PARODD)
      prompt_Printf(&prompt, ", odd parity");
    else
      prompt_Printf(&prompt, ", even parity");
  } else
    prompt_Printf(&prompt, ", no parity");

  prompt_Printf(&prompt, ", CTS/RTS %s\n",
                (arg->cx->physical->cfg.rts_cts ? "on" : "off"));

  if (LogIsKept(LogDEBUG))
    prompt_Printf(&prompt, "fd = %d, modem control = %o\n",
                  arg->cx->physical->fd, arg->cx->physical->mbits);
  prompt_Printf(&prompt, "connect count: %d\n",
                arg->cx->physical->connect_count);
#ifdef TIOCOUTQ
  if (arg->cx->physical->fd >= 0)
    if (ioctl(arg->cx->physical->fd, TIOCOUTQ, &nb) >= 0)
      prompt_Printf(&prompt, "outq:          %d\n", nb);
    else
      prompt_Printf(&prompt, "outq: ioctl probe failed: %s\n", strerror(errno));
#endif
  prompt_Printf(&prompt, "outq packets:  %d\n",
                link_QueueLen(&arg->cx->physical->link));
  prompt_Printf(&prompt, "Dial Script:   %s\n", arg->cx->cfg.script.dial);
  prompt_Printf(&prompt, "Login Script:  %s\n", arg->cx->cfg.script.login);
  prompt_Printf(&prompt, "Hangup Script: %s\n", arg->cx->cfg.script.hangup);
  prompt_Printf(&prompt, "Phone List:    %s\n", arg->cx->cfg.phone.list);
  prompt_Printf(&prompt, "Phone Number:  %s\n", arg->cx->phone.chosen);

  prompt_Printf(&prompt, "\n");
  throughput_disp(&arg->cx->physical->link.throughput);

  return 0;
}


/* Dummy linker functions, to keep this quiet.  Might end up a full
   regression test later, right now it is just to be able to track
   external symbols. */
#ifdef TESTMAIN
int main(void) {}

void LogPrintf(int i, const char *a, ...) {}
int  LogIsKept(int garble) {  return 0; }
int  Physical_IsSync(struct physical *phys) {return 0;}
int  DoChat(struct physical *a, char *b) {return 0;}

int mode;

#endif

static void
modem_DescriptorRead(struct descriptor *d, struct bundle *bundle,
                     const fd_set *fdset)
{
  struct physical *p = descriptor2physical(d);
  u_char rbuff[MAX_MRU], *cp;
  int n;

  LogPrintf(LogDEBUG, "descriptor2physical; %p -> %p\n", d, p);

  /* something to read from modem */
  if (p->dl->lcp.fsm.state <= ST_CLOSED)
    nointr_usleep(10000);

  n = Physical_Read(p, rbuff, sizeof rbuff);
  if ((mode & MODE_DIRECT) && n <= 0)
    link_Close(&p->link, bundle, 0, 1);
  else
    LogDumpBuff(LogASYNC, "ReadFromModem", rbuff, n);

  if (p->dl->lcp.fsm.state <= ST_CLOSED) {
    /* In dedicated mode, we just discard input until LCP is started */
    if (!(mode & MODE_DEDICATED)) {
      cp = HdlcDetect(p, rbuff, n);
      if (cp) {
        /* LCP packet is detected. Turn ourselves into packet mode */
        if (cp != rbuff) {
          /* XXX missing return value checks */
          Physical_Write(p, rbuff, cp - rbuff);
          Physical_Write(p, "\r\n", 2);
        }
        datalink_Up(bundle2datalink(bundle, p->link.name), 0, 1);
      } else
        prompt_Printf(&prompt, "%.*s", n, rbuff);
    }
  } else if (n > 0)
    async_Input(bundle, rbuff, n, p);
}

static int
modem_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  return Physical_UpdateSet(d, r, w, e, n, 0);
}
