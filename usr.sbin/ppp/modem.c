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
 * $Id: modem.c,v 1.78 1998/02/19 02:10:11 brian Exp $
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
#ifdef __OpenBSD__
#include <util.h>
#else
#include <libutil.h>
#endif

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define O_NONBLOCK O_NDELAY
#endif
#endif

static int mbits;		/* Current DCD status */
static int connect_count;
static struct pppTimer ModemTimer;

#define	Online	(mbits & TIOCM_CD)

static struct mbuf *modemout;
static struct mqueue OutputQueues[PRI_LINK + 1];
static int dev_is_modem;
static struct pppThroughput throughput;

static void CloseLogicalModem(void);

void
Enqueue(struct mqueue * queue, struct mbuf * bp)
{
  if (queue->last) {
    queue->last->pnext = bp;
    queue->last = bp;
  } else
    queue->last = queue->top = bp;
  queue->qlen++;
  LogPrintf(LogDEBUG, "Enqueue: len = %d\n", queue->qlen);
}

struct mbuf *
Dequeue(struct mqueue *queue)
{
  struct mbuf *bp;

  LogPrintf(LogDEBUG, "Dequeue: len = %d\n", queue->qlen);
  bp = queue->top;
  if (bp) {
    queue->top = queue->top->pnext;
    queue->qlen--;
    if (queue->top == NULL) {
      queue->last = queue->top;
      if (queue->qlen)
	LogPrintf(LogERROR, "Dequeue: Not zero (%d)!!!\n", queue->qlen);
    }
  }
  return (bp);
}

void
SequenceQueues()
{
  LogPrintf(LogDEBUG, "SequenceQueues\n");
  while (OutputQueues[PRI_NORMAL].qlen)
    Enqueue(OutputQueues + PRI_LINK, Dequeue(OutputQueues + PRI_NORMAL));
}

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

static void
ModemSetDevice(const char *name)
{
  strncpy(VarDevice, name, sizeof VarDevice - 1);
  VarDevice[sizeof VarDevice - 1] = '\0';
  VarBaseDevice = strncmp(VarDevice, "/dev/", 5) ? VarDevice : VarDevice + 5;
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
  int ombits = mbits;
  int change;

  StopTimer(&ModemTimer);
  StartTimer(&ModemTimer);

  if (dev_is_modem) {
    if (modem >= 0) {
      if (ioctl(modem, TIOCMGET, &mbits) < 0) {
	LogPrintf(LogPHASE, "ioctl error (%s)!\n", strerror(errno));
	DownConnection();
	return;
      }
    } else
      mbits = 0;
    change = ombits ^ mbits;
    if (change & TIOCM_CD) {
      if (Online) {
        LogPrintf(LogDEBUG, "ModemTimeout: offline -> online\n");
	/*
	 * In dedicated mode, start packet mode immediate after we detected
	 * carrier.
	 */
	if (mode & MODE_DEDICATED)
	  PacketMode(VarOpenMode);
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
    mbits = TIOCM_CD;
  }
}

static void
StartModemTimer(void)
{
  StopTimer(&ModemTimer);
  ModemTimer.state = TIMER_STOPPED;
  ModemTimer.load = SECTICKS;
  ModemTimer.func = ModemTimeout;
  LogPrintf(LogDEBUG, "ModemTimer using ModemTimeout() - %p\n", ModemTimeout);
  StartTimer(&ModemTimer);
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
      return VarParity = pp->set;
    }
  }
  return (-1);
}

int
ChangeParity(const char *str)
{
  struct termios rstio;
  int val;

  val = GetParityValue(str);
  if (val > 0) {
    VarParity = val;
    tcgetattr(modem, &rstio);
    rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
    rstio.c_cflag |= val;
    tcsetattr(modem, TCSADRAIN, &rstio);
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

static char fn[MAXPATHLEN];

static int
LockModem(void)
{
  int res;
  FILE *lockfile;

  if (*VarDevice != '/')
    return 0;

  if (!(mode & MODE_DIRECT) && (res = ID0uu_lock(VarBaseDevice)) != UU_LOCK_OK) {
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
UnlockModem(void)
{
  if (*VarDevice != '/')
    return;

  snprintf(fn, sizeof fn, "%s%s.if", _PATH_VARRUN, VarBaseDevice);
#ifndef RELEASE_CRUNCH
  if (ID0unlink(fn) == -1)
    LogPrintf(LogALERT, "Warning: Can't remove %s: %s\n", fn, strerror(errno));
#else
  ID0unlink(fn);
#endif

  if (!(mode & MODE_DIRECT) && ID0uu_unlock(VarBaseDevice) == -1)
    LogPrintf(LogALERT, "Warning: Can't uu_unlock %s\n", fn);
}

static void
HaveModem(void)
{
  throughput_start(&throughput);
  connect_count++;
  LogPrintf(LogPHASE, "Connected!\n");
}

static struct termios modemios;

int
OpenModem()
{
  struct termios rstio;
  int oldflag;
  char *host, *port;
  char *cp;
  char tmpDeviceList[sizeof VarDeviceList];
  char *tmpDevice;

  if (modem >= 0)
    LogPrintf(LogDEBUG, "OpenModem: Modem is already open!\n");
    /* We're going back into "term" mode */
  else if (mode & MODE_DIRECT) {
    if (isatty(STDIN_FILENO)) {
      LogPrintf(LogDEBUG, "OpenModem(direct): Modem is a tty\n");
      ModemSetDevice(ttyname(STDIN_FILENO));
      if (LockModem() == -1) {
        close(STDIN_FILENO);
        return -1;
      }
      modem = STDIN_FILENO;
      HaveModem();
    } else {
      LogPrintf(LogDEBUG, "OpenModem(direct): Modem is not a tty\n");
      ModemSetDevice("");
      /* We don't call ModemTimeout() with this type of connection */
      HaveModem();
      return modem = STDIN_FILENO;
    }
  } else {
    strncpy(tmpDeviceList, VarDeviceList, sizeof tmpDeviceList - 1);
    tmpDeviceList[sizeof tmpDeviceList - 1] = '\0';

    for(tmpDevice=strtok(tmpDeviceList, ","); tmpDevice && (modem < 0);
	tmpDevice=strtok(NULL,",")) {
      ModemSetDevice(tmpDevice);
      if (strncmp(VarDevice, "/dev/", 5) == 0) {
	if (LockModem() == -1) {
	  modem = -1;
	}
	else {
	  modem = ID0open(VarDevice, O_RDWR | O_NONBLOCK);
	  if (modem < 0) {
	    LogPrintf(LogERROR, "OpenModem failed: %s: %s\n", VarDevice,
		      strerror(errno));
	    UnlockModem();
	    modem = -1;
	  }
	  else {
	    HaveModem();
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
	    modem = OpenConnection(host, port);
	    *cp = ':';		/* Don't destroy VarDevice */
	    if (modem < 0)
	      return (-1);
	    HaveModem();
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

    if (modem < 0) return modem;
  }

  /*
   * If we are working on tty device, change it's mode into the one desired
   * for further operation. In this implementation, we assume that modem is
   * configuted to use CTS/RTS flow control.
   */
  mbits = 0;
  dev_is_modem = isatty(modem) || DEV_IS_SYNC;
  if (DEV_IS_SYNC)
    nointr_sleep(1);
  if (dev_is_modem && !DEV_IS_SYNC) {
    tcgetattr(modem, &rstio);
    modemios = rstio;
    LogPrintf(LogDEBUG, "OpenModem: modem = %d\n", modem);
    LogPrintf(LogDEBUG, "OpenModem: modem (get): iflag = %x, oflag = %x,"
	      " cflag = %x\n", rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
    cfmakeraw(&rstio);
    if (VarCtsRts)
      rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
    else {
      rstio.c_cflag |= CLOCAL;
      rstio.c_iflag |= IXOFF;
    }
    rstio.c_iflag |= IXON;
    if (!(mode & MODE_DEDICATED))
      rstio.c_cflag |= HUPCL;
    if ((mode & MODE_DIRECT) == 0) {

      /*
       * If we are working as direct mode, don't change tty speed.
       */
      rstio.c_cflag &= ~(CSIZE | PARODD | PARENB);
      rstio.c_cflag |= VarParity;
      if (cfsetspeed(&rstio, IntToSpeed(VarSpeed)) == -1) {
	LogPrintf(LogWARN, "Unable to set modem speed (modem %d to %d)\n",
		  modem, VarSpeed);
      }
    }
    tcsetattr(modem, TCSADRAIN, &rstio);
    LogPrintf(LogDEBUG, "modem (put): iflag = %x, oflag = %x, cflag = %x\n",
	      rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);

    if (!(mode & MODE_DIRECT))
      if (ioctl(modem, TIOCMGET, &mbits)) {
        LogPrintf(LogERROR, "OpenModem: Cannot get modem status: %s\n",
		  strerror(errno));
        CloseLogicalModem();
	return (-1);
      }
    LogPrintf(LogDEBUG, "OpenModem: modem control = %o\n", mbits);

    oldflag = fcntl(modem, F_GETFL, 0);
    if (oldflag < 0) {
      LogPrintf(LogERROR, "OpenModem: Cannot get modem flags: %s\n",
		strerror(errno));
      CloseLogicalModem();
      return (-1);
    }
    (void) fcntl(modem, F_SETFL, oldflag & ~O_NONBLOCK);
  }
  StartModemTimer();

  return (modem);
}

int
ModemSpeed()
{
  struct termios rstio;

  tcgetattr(modem, &rstio);
  return (SpeedToInt(cfgetispeed(&rstio)));
}

/*
 * Put modem tty line into raw mode which is necessary in packet mode operation
 */
int
RawModem()
{
  struct termios rstio;
  int oldflag;

  if (!isatty(modem) || DEV_IS_SYNC)
    return (0);
  if (!(mode & MODE_DIRECT) && modem >= 0 && !Online) {
    LogPrintf(LogDEBUG, "RawModem: mode = %d, modem = %d, mbits = %x\n", mode, modem, mbits);
  }
  tcgetattr(modem, &rstio);
  cfmakeraw(&rstio);
  if (VarCtsRts)
    rstio.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else
    rstio.c_cflag |= CLOCAL;

  if (!(mode & MODE_DEDICATED))
    rstio.c_cflag |= HUPCL;
  tcsetattr(modem, TCSADRAIN, &rstio);
  oldflag = fcntl(modem, F_GETFL, 0);
  if (oldflag < 0)
    return (-1);
  (void) fcntl(modem, F_SETFL, oldflag | O_NONBLOCK);
  return (0);
}

static void
UnrawModem(void)
{
  int oldflag;

  if (isatty(modem) && !DEV_IS_SYNC) {
    tcsetattr(modem, TCSAFLUSH, &modemios);
    oldflag = fcntl(modem, F_GETFL, 0);
    if (oldflag < 0)
      return;
    (void) fcntl(modem, F_SETFL, oldflag & ~O_NONBLOCK);
  }
}

void
ModemAddInOctets(int n)
{
  throughput_addin(&throughput, n);
}

void
ModemAddOutOctets(int n)
{
  throughput_addout(&throughput, n);
}

static void
ClosePhysicalModem(void)
{
  LogPrintf(LogDEBUG, "ClosePhysicalModem\n");
  close(modem);
  modem = -1;
  throughput_log(&throughput, LogPHASE, "Modem");
}

void
HangupModem(int flag)
{
  struct termios tio;

  LogPrintf(LogDEBUG, "Hangup modem (%s)\n", modem >= 0 ? "open" : "closed");

  if (modem < 0)
    return;

  StopTimer(&ModemTimer);
  throughput_stop(&throughput);

  if (TermMode) {
    LogPrintf(LogDEBUG, "HangupModem: Not in 'term' mode\n");
    return;
  }

  if (!isatty(modem)) {
    mbits &= ~TIOCM_DTR;
    ClosePhysicalModem();
    return;
  }

  if (modem >= 0 && Online) {
    mbits &= ~TIOCM_DTR;
    tcgetattr(modem, &tio);
    if (cfsetspeed(&tio, B0) == -1) {
      LogPrintf(LogWARN, "Unable to set modem to speed 0\n");
    }
    tcsetattr(modem, TCSANOW, &tio);
    nointr_sleep(1);
  }

  if (modem >= 0) {
    char ScriptBuffer[SCRIPT_LEN];

    strncpy(ScriptBuffer, VarHangupScript, sizeof ScriptBuffer - 1);
    ScriptBuffer[sizeof ScriptBuffer - 1] = '\0';
    LogPrintf(LogDEBUG, "HangupModem: Script: %s\n", ScriptBuffer);
    if (flag || !(mode & MODE_DEDICATED)) {
      DoChat(ScriptBuffer);
      tcflush(modem, TCIOFLUSH);
      UnrawModem();
      CloseLogicalModem();
    } else {
      /*
       * If we are working as dedicated mode, never close it until we are
       * directed to quit program.
       */
      mbits |= TIOCM_DTR;
      ioctl(modem, TIOCMSET, &mbits);
      DoChat(ScriptBuffer);
    }
  }
}

static void
CloseLogicalModem(void)
{
  LogPrintf(LogDEBUG, "CloseLogicalModem\n");
  if (modem >= 0) {
    if (Utmp) {
      ID0logout(VarBaseDevice);
      Utmp = 0;
    }
    ClosePhysicalModem();
    UnlockModem();
  }
}

/*
 * Write to modem. Actualy, requested packets are queued, and goes out
 * to the line when ModemStartOutput() is called.
 */
void
WriteModem(int pri, const char *ptr, int count)
{
  struct mbuf *bp;

  bp = mballoc(count, MB_MODEM);
  memcpy(MBUF_CTOP(bp), ptr, count);

  /*
   * Should be NORMAL and LINK only. All IP frames get here marked NORMAL.
   */
  Enqueue(&OutputQueues[pri], bp);
}

void
ModemOutput(int pri, struct mbuf * bp)
{
  struct mbuf *wp;
  int len;

  len = plength(bp);
  wp = mballoc(len, MB_MODEM);
  mbread(bp, MBUF_CTOP(wp), len);
  Enqueue(&OutputQueues[pri], wp);
  ModemStartOutput(modem);
}

int
ModemQlen()
{
  struct mqueue *queue;
  int len = 0;
  int i;

  for (i = PRI_NORMAL; i <= PRI_LINK; i++) {
    queue = &OutputQueues[i];
    len += queue->qlen;
  }
  return (len);

}

void
ModemStartOutput(int fd)
{
  struct mqueue *queue;
  int nb, nw;
  int i;

  if (modemout == NULL && ModemQlen() == 0)
    IpStartOutput();
  if (modemout == NULL) {
    i = PRI_LINK;
    for (queue = &OutputQueues[PRI_LINK]; queue >= OutputQueues; queue--) {
      if (queue->top) {
	modemout = Dequeue(queue);
	if (LogIsKept(LogDEBUG)) {
	  if (i > PRI_NORMAL) {
	    struct mqueue *q;

	    q = &OutputQueues[0];
	    LogPrintf(LogDEBUG, "ModemStartOutput: Output from queue %d,"
		      " normal has %d\n", i, q->qlen);
	  }
	  LogPrintf(LogDEBUG, "ModemStartOutput: Dequeued %d\n", i);
	}
	break;
      }
      i--;
    }
  }
  if (modemout) {
    nb = modemout->cnt;
    if (nb > 1600)
      nb = 1600;
    nw = write(fd, MBUF_CTOP(modemout), nb);
    LogPrintf(LogDEBUG, "ModemStartOutput: wrote: %d(%d)\n", nw, nb);
    LogDumpBuff(LogDEBUG, "ModemStartOutput: modem write",
		MBUF_CTOP(modemout), nb);
    if (nw > 0) {
      modemout->cnt -= nw;
      modemout->offset += nw;
      if (modemout->cnt == 0) {
	modemout = mbfree(modemout);
	LogPrintf(LogDEBUG, "ModemStartOutput: mbfree\n");
      }
    } else if (nw < 0) {
      if (errno != EAGAIN) {
	LogPrintf(LogERROR, "modem write (%d): %s\n", modem, strerror(errno));
	reconnect(RECON_TRUE);
	DownConnection();
      }
    }
  }
}

int
DialModem()
{
  char ScriptBuffer[SCRIPT_LEN];
  int excode;

  strncpy(ScriptBuffer, VarDialScript, sizeof ScriptBuffer - 1);
  ScriptBuffer[sizeof ScriptBuffer - 1] = '\0';
  if ((excode = DoChat(ScriptBuffer)) > 0) {
    if (VarTerm)
      fprintf(VarTerm, "dial OK!\n");
    strncpy(ScriptBuffer, VarLoginScript, sizeof ScriptBuffer - 1);
    if ((excode = DoChat(ScriptBuffer)) > 0) {
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
    ModemTimeout(NULL);		/* Dummy call to check modem status */
  } else if (excode == -1)
    excode = EX_SIG;
  else {
    LogPrintf(LogWARN, "DialModem: dial failed.\n");
    excode = EX_NODIAL;
  }
  HangupModem(0);
  return (excode);
}

int
ShowModemStatus(struct cmdargs const *arg)
{
  const char *dev;
#ifdef TIOCOUTQ
  int nb;
#endif

  if (!VarTerm)
    return 1;

  dev = *VarDevice ? VarDevice : "network";

  fprintf(VarTerm, "device: %s  speed: ", dev);
  if (DEV_IS_SYNC)
    fprintf(VarTerm, "sync\n");
  else
    fprintf(VarTerm, "%d\n", VarSpeed);

  switch (VarParity & CSIZE) {
  case CS7:
    fprintf(VarTerm, "cs7, ");
    break;
  case CS8:
    fprintf(VarTerm, "cs8, ");
    break;
  }
  if (VarParity & PARENB) {
    if (VarParity & PARODD)
      fprintf(VarTerm, "odd parity, ");
    else
      fprintf(VarTerm, "even parity, ");
  } else
    fprintf(VarTerm, "no parity, ");

  fprintf(VarTerm, "CTS/RTS %s.\n", (VarCtsRts ? "on" : "off"));

  if (LogIsKept(LogDEBUG))
    fprintf(VarTerm, "fd = %d, modem control = %o\n", modem, mbits);
  fprintf(VarTerm, "connect count: %d\n", connect_count);
#ifdef TIOCOUTQ
  if (modem >= 0)
    if (ioctl(modem, TIOCOUTQ, &nb) >= 0)
      fprintf(VarTerm, "outq: %d\n", nb);
    else
      fprintf(VarTerm, "outq: ioctl probe failed: %s\n", strerror(errno));
#endif
  fprintf(VarTerm, "outqlen: %d\n", ModemQlen());
  fprintf(VarTerm, "DialScript  = %s\n", VarDialScript);
  fprintf(VarTerm, "LoginScript = %s\n", VarLoginScript);
  fprintf(VarTerm, "PhoneNumber(s) = %s\n", VarPhoneList);

  fprintf(VarTerm, "\n");
  throughput_disp(&throughput, VarTerm);

  return 0;
}
