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
 * $Id: modem.c,v 1.77.2.1 1998/01/29 00:49:26 brian Exp $
 *
 *  TODO:
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <unistd.h>
#include <utmp.h>
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
#include "hdlc.h"
#include "lcp.h"
#include "ip.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"
#include "main.h"
#include "chat.h"
#include "throughput.h"

#undef mode

/* We're defining a physical device, and thus need the real
   headers. */
#define PHYSICAL_DEVICE
#include "link.h"
#include "physical.h"


#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define O_NONBLOCK O_NDELAY
#endif
#endif

static void ModemStartOutput(struct link *);
static int ModemIsActive(struct link *);
static void ModemHangup(struct link *, int);

struct physical phys_modem = {
  {
    PHYSICAL_LINK,
    "modem",
    sizeof(struct physical),
    { 0 },                      /* throughput */
    { 0 },                      /* timer */
    { { 0 } },                  /* queues */
    { 0 },                      /* proto_in */
    { 0 },                      /* proto_out */
    ModemStartOutput,
    ModemIsActive,
    ModemHangup
  },
  -1
};

/* XXX-ML this should probably change when we add support for other
   types of devices */
#define	Online	(modem->mbits & TIOCM_CD)

static void CloseLogicalModem(struct physical *);

static struct speeds {
  int nspeed;
  speed_t speed;
}      speeds[] = {

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

void
DownConnection()
{
  LogPrintf(LogPHASE, "Disconnected!\n");
  LcpDown();
}

/*
 *  ModemTimeout() watches DCD signal and notifies if it's status is changed.
 *
 */
void
ModemTimeout(void *data)
{
  struct physical *modem = data;
  int ombits = modem->mbits;
  int change;

  StopTimer(&modem->link.Timer);
  StartTimer(&modem->link.Timer);

  if (modem->dev_is_modem) {
    if (modem->fd >= 0) {
      if (ioctl(modem->fd, TIOCMGET, &modem->mbits) < 0) {
	LogPrintf(LogPHASE, "ioctl error (%s)!\n", strerror(errno));
	DownConnection();
	return;
      }
    } else
      modem->mbits = 0;
    change = ombits ^ modem->mbits;
    if (change & TIOCM_CD) {
      if (modem->mbits & TIOCM_CD) {
        LogPrintf(LogDEBUG, "ModemTimeout: offline -> online\n");
	/*
	 * In dedicated mode, start packet mode immediate after we detected
	 * carrier.
	 */
#ifdef notyet
	if (modem->is_dedicated)
	  PacketMode(VarOpenMode);
#else
	if (mode & MODE_DEDICATED)
	  PacketMode(VarOpenMode);
#endif
      } else {
        LogPrintf(LogDEBUG, "ModemTimeout: online -> offline\n");
	reconnect(RECON_TRUE);
	DownConnection();
      }
    }
    else
      LogPrintf(LogDEBUG, "ModemTimeout: Still %sline\n",
                Online ? "on" : "off");
  } else if (!Online) {
    /* mbits was set to zero in OpenModem() */
    modem->mbits = TIOCM_CD;
  }
}

static void
StartModemTimer(struct physical *modem)
{
  struct pppTimer *ModemTimer;

  ModemTimer = &modem->link.Timer;

  StopTimer(ModemTimer);
  ModemTimer->state = TIMER_STOPPED;
  ModemTimer->load = SECTICKS;
  ModemTimer->func = ModemTimeout;
  ModemTimer->arg = modem;
  LogPrintf(LogDEBUG, "ModemTimer using ModemTimeout() - %p\n", ModemTimeout);
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
ChangeParity(struct physical *modem, const char *str)
{
  struct termios rstio;
  int val;

  val = GetParityValue(str);
  if (val > 0) {
    modem->parity = val;
    tcgetattr(modem->fd, &rstio);
    rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
    rstio.c_cflag |= val;
    tcsetattr(modem->fd, TCSADRAIN, &rstio);
    return 0;
  }
  LogPrintf(LogWARN, "ChangeParity: %s: Invalid parity\n", str);
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
LockModem(struct physical *modem)
{
  int res;
  FILE *lockfile;
  char fn[MAXPATHLEN];

  if (*VarDevice != '/')
    return 0;

  if (
#ifdef notyet
      !modem->is_direct && 
#else
      !(mode & MODE_DIRECT) &&
#endif
      (res = ID0uu_lock(VarBaseDevice)) != UU_LOCK_OK) {
    if (res == UU_LOCK_INUSE)
      LogPrintf(LogPHASE, "Modem %s is in use\n", VarDevice);
    else
      LogPrintf(LogPHASE, "Modem %s is in use: uu_lock: %s\n",
                VarDevice, uu_lockerr(res));
    return (-1);
  }

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, VarBaseDevice);
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
UnlockModem(struct physical *modem)
{
  char fn[MAXPATHLEN];

  if (*VarDevice != '/')
    return;

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, VarBaseDevice);
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
      ID0uu_unlock(VarBaseDevice) == -1)
    LogPrintf(LogALERT, "Warning: Can't uu_unlock %s\n", fn);
}

static void
HaveModem(struct physical *modem)
{
  throughput_start(&modem->link.throughput);
  modem->connect_count++;
  LogPrintf(LogPHASE, "Connected!\n");
}

int
OpenModem(struct physical *modem)
{
  struct termios rstio;
  int oldflag;
  char *host, *port;
  char *cp;
  char tmpDeviceList[sizeof VarDeviceList];
  char *tmpDevice;

  if (modem->fd >= 0)
    LogPrintf(LogDEBUG, "OpenModem: Modem is already open!\n");
    /* We're going back into "term" mode */
  else if (
#ifdef notyet
	   modem->is_direct
#else
	   mode & MODE_DIRECT
#endif
	   ) {
    struct cmdargs arg;
    arg.cmd = NULL;
    arg.data = (const void *)VAR_DEVICE;
    if (isatty(STDIN_FILENO)) {
      LogPrintf(LogDEBUG, "OpenModem(direct): Modem is a tty\n");
      cp = ttyname(STDIN_FILENO);
      arg.argc = 1;
      arg.argv = (char const *const *)&cp;
      SetVariable(&arg);
      if (LockModem(modem) == -1) {
        close(STDIN_FILENO);
        return -1;
      }
      modem = STDIN_FILENO;
      HaveModem(modem);
    } else {
      LogPrintf(LogDEBUG, "OpenModem(direct): Modem is not a tty\n");
      arg.argc = 0;
      arg.argv = NULL;
      SetVariable(&arg);
      /* We don't call ModemTimeout() with this type of connection */
      HaveModem(modem);
      return modem->fd = STDIN_FILENO;
    }
  } else {
    strncpy(tmpDeviceList, VarDeviceList, sizeof tmpDeviceList - 1);
    tmpDeviceList[sizeof tmpDeviceList - 1] = '\0';

    for(tmpDevice=strtok(tmpDeviceList, ","); tmpDevice && (modem->fd < 0);
	tmpDevice=strtok(NULL,",")) {
      strncpy(VarDevice, tmpDevice, sizeof VarDevice - 1);
      VarDevice[sizeof VarDevice - 1]= '\0';
      VarBaseDevice = strrchr(VarDevice, '/');
      VarBaseDevice = VarBaseDevice ? VarBaseDevice + 1 : "";

      if (strncmp(VarDevice, "/dev/", 5) == 0) {
	if (LockModem(modem) == -1) {
	  modem->fd = -1;
	}
	else {
	  modem->fd = ID0open(VarDevice, O_RDWR | O_NONBLOCK);
	  if (modem->fd < 0) {
	    LogPrintf(LogERROR, "OpenModem failed: %s: %s\n", VarDevice,
		      strerror(errno));
	    UnlockModem(modem);
	    modem->fd = -1;
	  }
	  else {
	    HaveModem(modem);
	    LogPrintf(LogDEBUG, "OpenModem: Modem is %s\n", VarDevice);
	  }
	}
      } else {
	/* PPP over TCP */
	cp = strchr(VarDevice, ':');
	if (cp) {
	  *cp = '\0';
	  host = VarDevice;
	  port = cp + 1;
	  if (*host && *port) {
	    modem->fd = OpenConnection(host, port);
	    *cp = ':';		/* Don't destroy VarDevice */
	    if (modem->fd < 0)
	      return (-1);
	    HaveModem(modem);
	    LogPrintf(LogDEBUG, "OpenModem: Modem is socket %s\n", VarDevice);
	  } else {
	    *cp = ':';		/* Don't destroy VarDevice */
	    LogPrintf(LogERROR, "Invalid host:port: \"%s\"\n", VarDevice);
	    return (-1);
	  }
	} else {
	  LogPrintf(LogERROR,
		    "Device (%s) must be in /dev or be a host:port pair\n",
		    VarDevice);
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
    LogPrintf(LogDEBUG, "OpenModem: modem = %d\n", modem->fd);
    LogPrintf(LogDEBUG, "OpenModem: modem (get): iflag = %x, oflag = %x,"
	      " cflag = %x\n", rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
    cfmakeraw(&rstio);
    if (modem->rts_cts)
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
      rstio.c_cflag |= modem->parity;
      if (cfsetspeed(&rstio, IntToSpeed(modem->speed)) == -1) {
	LogPrintf(LogWARN, "Unable to set modem speed (modem %d to %d)\n",
		  modem->fd, modem->speed);
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
        LogPrintf(LogERROR, "OpenModem: Cannot get modem status: %s\n",
		  strerror(errno));
        CloseLogicalModem(modem);
	return (-1);
      }
    LogPrintf(LogDEBUG, "OpenModem: modem control = %o\n", modem->mbits);

    oldflag = fcntl(modem->fd, F_GETFL, 0);
    if (oldflag < 0) {
      LogPrintf(LogERROR, "OpenModem: Cannot get modem flags: %s\n",
		strerror(errno));
      CloseLogicalModem(modem);
      return (-1);
    }
    (void) fcntl(modem->fd, F_SETFL, oldflag & ~O_NONBLOCK);
  }
  StartModemTimer(modem);

  return (modem->fd);
}

int
ModemSpeed(struct physical *modem)
{
  struct termios rstio;

  tcgetattr(modem->fd, &rstio);
  return (SpeedToInt(cfgetispeed(&rstio)));
}

/*
 * Put modem tty line into raw mode which is necessary in packet mode operation
 */
int
RawModem(struct physical *modem)
{
  struct termios rstio;
  int oldflag;

  if (!isatty(modem->fd) || Physical_IsSync(modem))
    return (0);
  if (
#ifdef notyet
      !modem->is_direct &&
#else
      !(mode & MODE_DIRECT) &&
#endif
      modem->fd >= 0 && !Online) {
    LogPrintf(LogDEBUG, "RawModem: modem = %d, mbits = %x\n",
			  modem->fd, modem->mbits);
  }
  tcgetattr(modem->fd, &rstio);
  cfmakeraw(&rstio);
  if (modem->rts_cts)
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
  return (0);
}

static void
UnrawModem(struct physical *modem)
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
ClosePhysicalModem(struct physical *modem)
{
  LogPrintf(LogDEBUG, "ClosePhysicalModem\n");
  close(modem->fd);
  modem->fd = -1;
  throughput_log(&modem->link.throughput, LogPHASE, "Modem");
}

static void
ModemHangup(struct link *l, int dedicated_force)
{
  struct termios tio;
  struct physical *modem = (struct physical *)l;

  LogPrintf(LogDEBUG, "Hangup modem (%s)\n",
            modem->fd >= 0 ? "open" : "closed");

  if (modem->fd < 0)
    return;

  StopTimer(&modem->link.Timer);
  throughput_stop(&modem->link.throughput);

  if (TermMode) {
    LogPrintf(LogDEBUG, "ModemHangup: Not in 'term' mode\n");
    return;
  }

  if (!isatty(modem->fd)) {
    modem->mbits &= ~TIOCM_DTR;
    ClosePhysicalModem(modem);
    return;
  }

  if (modem->fd >= 0 && Online) {
    modem->mbits &= ~TIOCM_DTR;
    tcgetattr(modem->fd, &tio);
    if (cfsetspeed(&tio, B0) == -1) {
      LogPrintf(LogWARN, "Unable to set modem to speed 0\n");
    }
    tcsetattr(modem->fd, TCSANOW, &tio);
    nointr_sleep(1);
  }

  if (modem->fd >= 0) {
    char ScriptBuffer[SCRIPT_LEN];

    strncpy(ScriptBuffer, VarHangupScript, sizeof ScriptBuffer - 1);
    ScriptBuffer[sizeof ScriptBuffer - 1] = '\0';
    LogPrintf(LogDEBUG, "ModemHangup: Script: %s\n", ScriptBuffer);
    if (dedicated_force ||
#ifdef notyet
	!modem->is_dedicated
#else
	!(mode & MODE_DEDICATED)
#endif
	) {
      DoChat(modem, ScriptBuffer);
      tcflush(modem->fd, TCIOFLUSH);
      UnrawModem(modem);
      CloseLogicalModem(modem);
    } else {
      /*
       * If we are working as dedicated mode, never close it until we are
       * directed to quit program.
       */
      modem->mbits |= TIOCM_DTR;
      ioctl(modem->fd, TIOCMSET, &modem->mbits);
      DoChat(modem, ScriptBuffer);
    }
  }
}

static void
CloseLogicalModem(struct physical *modem)
{
  LogPrintf(LogDEBUG, "CloseLogicalModem\n");
  if (modem->fd >= 0) {
    ClosePhysicalModem(modem);
    if (Utmp) {
      struct utmp ut;
      strncpy(ut.ut_line, VarBaseDevice, sizeof ut.ut_line - 1);
      ut.ut_line[sizeof ut.ut_line - 1] = '\0';
      if (logout(ut.ut_line))
        logwtmp(ut.ut_line, "", ""); 
      else
        LogPrintf(LogERROR, "CloseLogicalModem: No longer logged in on %s\n",
		  ut.ut_line);
      Utmp = 0;
    }
    UnlockModem(modem);
  }
}

static void
ModemStartOutput(struct link *l)
{
  struct physical *modem = (struct physical *)l;
  int nb, nw;

  if (modem->out == NULL) {
    if (link_QueueLen(l) == 0)
      IpStartOutput(l);

    modem->out = link_Dequeue(l);
  }

  if (modem->out) {
    nb = modem->out->cnt;
/* Eh ?  Removed 980130
    if (nb > 1600)
      nb = 1600;
*/
    nw = write(modem->fd, MBUF_CTOP(modem->out), nb);
    LogPrintf(LogDEBUG, "ModemStartOutput: wrote: %d(%d)\n", nw, nb);
    LogDumpBuff(LogDEBUG, "ModemStartOutput: modem write",
		MBUF_CTOP(modem->out), nb);
    if (nw > 0) {
      modem->out->cnt -= nw;
      modem->out->offset += nw;
      if (modem->out->cnt == 0) {
	modem->out = mbfree(modem->out);
	LogPrintf(LogDEBUG, "ModemStartOutput: mbfree\n");
      }
    } else if (nw < 0) {
      if (errno != EAGAIN) {
	LogPrintf(LogERROR, "modem write (%d): %s\n", modem->fd,
		  strerror(errno));
	reconnect(RECON_TRUE);
	DownConnection();
      }
    }
  }
}

static int
ModemIsActive(struct link *l)
{
  return ((struct physical *)l)->fd >= 0;
}

int
DialModem(struct physical *modem)
{
  char ScriptBuffer[SCRIPT_LEN];
  int excode;

  strncpy(ScriptBuffer, VarDialScript, sizeof ScriptBuffer - 1);
  ScriptBuffer[sizeof ScriptBuffer - 1] = '\0';
  if ((excode = DoChat(modem, ScriptBuffer)) > 0) {
    if (VarTerm)
      fprintf(VarTerm, "dial OK!\n");
    strncpy(ScriptBuffer, VarLoginScript, sizeof ScriptBuffer - 1);
    if ((excode = DoChat(modem, ScriptBuffer)) > 0) {
      VarAltPhone = NULL;
      if (VarTerm)
	fprintf(VarTerm, "login OK!\n");
      return EX_DONE;
    } else if (excode == -1)
      excode = EX_SIG;
    else {
      LogPrintf(LogWARN, "DialModem: login failed.\n");
      excode = EX_NOLOGIN;
    }
    ModemTimeout(modem);		/* Dummy call to check modem status */
  } else if (excode == -1)
    excode = EX_SIG;
  else {
    LogPrintf(LogWARN, "DialModem: dial failed.\n");
    excode = EX_NODIAL;
  }
  ModemHangup(&modem->link, 0);
  return (excode);
}

int
ShowModemStatus(struct cmdargs const *arg)
{
  const char *dev;
  struct physical *modem = pppVars.physical;
#ifdef TIOCOUTQ
  int nb;
#endif

  if (!VarTerm)
    return 1;

  dev = *VarDevice ? VarDevice : "network";

  fprintf(VarTerm, "device: %s  speed: ", dev);
  if (Physical_IsSync(modem))
    fprintf(VarTerm, "sync\n");
  else
    fprintf(VarTerm, "%d\n", modem->speed);

  switch (modem->parity & CSIZE) {
  case CS7:
    fprintf(VarTerm, "cs7, ");
    break;
  case CS8:
    fprintf(VarTerm, "cs8, ");
    break;
  }
  if (modem->parity & PARENB) {
    if (modem->parity & PARODD)
      fprintf(VarTerm, "odd parity, ");
    else
      fprintf(VarTerm, "even parity, ");
  } else
    fprintf(VarTerm, "no parity, ");

  fprintf(VarTerm, "CTS/RTS %s.\n", (modem->rts_cts ? "on" : "off"));

  if (LogIsKept(LogDEBUG))
    fprintf(VarTerm, "fd = %d, modem control = %o\n", modem->fd, modem->mbits);
  fprintf(VarTerm, "connect count: %d\n", modem->connect_count);
#ifdef TIOCOUTQ
  if (modem->fd >= 0)
    if (ioctl(modem->fd, TIOCOUTQ, &nb) >= 0)
      fprintf(VarTerm, "outq: %d\n", nb);
    else
      fprintf(VarTerm, "outq: ioctl probe failed: %s\n", strerror(errno));
#endif
  fprintf(VarTerm, "outqlen: %d\n", link_QueueLen(&modem->link));
  fprintf(VarTerm, "DialScript  = %s\n", VarDialScript);
  fprintf(VarTerm, "LoginScript = %s\n", VarLoginScript);
  fprintf(VarTerm, "PhoneNumber(s) = %s\n", VarPhoneList);

  fprintf(VarTerm, "\n");
  throughput_disp(&modem->link.throughput, VarTerm);

  return 0;
}

int
ReportProtocolStatus(struct cmdargs const *arg)
{
  link_ReportProtocolStatus(&phys_modem.link);
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

