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
 * $Id: modem.c,v 1.39 1997/05/24 17:32:41 brian Exp $
 *
 *  TODO:
 */
#include "fsm.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <errno.h>
#include <time.h>
#include <libutil.h>
#include "hdlc.h"
#include "lcp.h"
#include "ip.h"
#include "modem.h"
#include "loadalias.h"
#include "vars.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define O_NONBLOCK O_NDELAY
#endif
#endif

extern int DoChat();

static int mbits;			/* Current DCD status */
static int connect_count;
static struct pppTimer ModemTimer;

extern void PacketMode(), TtyTermMode(), TtyCommandMode();
extern int TermMode;

#define	Online	(mbits & TIOCM_CD)

static struct mbuf *modemout;
static struct mqueue OutputQueues[PRI_LINK+1];
static int dev_is_modem;

#undef QDEBUG

void
Enqueue(queue, bp)
struct mqueue *queue;
struct mbuf *bp;
{
  if (queue->last) {
    queue->last->pnext = bp;
    queue->last = bp;
  } else
    queue->last = queue->top = bp;
  queue->qlen++;
#ifdef QDEBUG
  logprintf("Enqueue: len = %d\n", queue->qlen);
#endif
}

struct mbuf *
Dequeue(queue)
struct mqueue *queue;
{
  struct mbuf *bp;

#ifdef QDEBUG
  logprintf("Dequeue: len = %d\n", queue->qlen);
#endif
  bp = queue->top;
  if (bp) {
    queue->top = queue->top->pnext;
    queue->qlen--;
    if (queue->top == NULL) {
      queue->last = queue->top;
#ifdef QDEBUG
      if (queue->qlen)
	logprintf("!!! not zero (%d)!!!\n", queue->qlen);
#endif
    }
  }
  return(bp);
}

static struct speeds{
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
#endif  /*_POSIX_SOURCE */
  { 0, 0 }
};

int SpeedToInt(speed)
speed_t speed;
{
  struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->speed == speed) {
      return(sp->nspeed);
    }
  }
  return 0;
}

speed_t IntToSpeed(nspeed)
int nspeed;
{
  struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->nspeed == nspeed) {
      return(sp->speed);
    }
  }
  return B0;
}

static time_t uptime;

void
DownConnection()
{
  LogPrintf(LOG_PHASE_BIT, "Disconnected!\n");
  LogPrintf(LOG_PHASE_BIT, "Connect time: %d secs\n", time(NULL) - uptime);
  if (!TermMode) {
    CloseModem();
    LcpDown();
  }
}

/*
 *  ModemTimeout() watches DCD signal and notifies if it's status is changed.
 *
 */
void
ModemTimeout()
{
  int ombits = mbits;
  int change;

  StopTimer(&ModemTimer);
  StartTimer(&ModemTimer);

  if (dev_is_modem) {
    if (modem >= 0) {
      if (ioctl(modem, TIOCMGET, &mbits) < 0) {
        LogPrintf(LOG_PHASE_BIT, "ioctl error (%s)!\n", strerror(errno));
        DownConnection();
        return;
      }
    } else
      mbits = 0;
    change = ombits ^ mbits;
    if (change & TIOCM_CD) {
      if (Online) {
        time(&uptime);
        LogPrintf(LOG_PHASE_BIT, "*Connected!\n");
        connect_count++;
        /*
         * In dedicated mode, start packet mode immediate
         * after we detected carrier.
         */
        if (mode & MODE_DEDICATED)
	  PacketMode();
      } else {
        reconnect(RECON_TRUE);
	DownConnection();
      }
    }
  } else {
    if (!Online) {
      time(&uptime);
      LogPrintf(LOG_PHASE_BIT, "Connected!\n");
      mbits = TIOCM_CD;
      connect_count++;
    } else if (uptime == 0) {
      time(&uptime);
    }
  }
}

void
StartModemTimer()
{
  StopTimer(&ModemTimer);
  ModemTimer.state = TIMER_STOPPED;
  ModemTimer.load = SECTICKS;
  ModemTimer.func = ModemTimeout;
  StartTimer(&ModemTimer);
}

struct parity {
  char *name;
  char *name1;
  int  set;
} validparity[] = {
  { "even", "P_EVEN", CS7 | PARENB }, { "odd", "P_ODD", CS7 | PARENB | PARODD },
  { "none", "P_ZERO", CS8 },          { NULL,  0 },
};

int
GetParityValue(str)
char *str;
{
  struct parity *pp;

  for (pp = validparity; pp->name; pp++) {
    if (strcasecmp(pp->name, str) == 0 ||
	strcasecmp(pp->name1, str) == 0) {
      VarParity = pp->set;
      return(pp->set);
    }
  }
  return(-1);
}

int
ChangeParity(str)
char *str;
{
  struct termios rstio;
  int val;

  val = GetParityValue(str);
  if (val > 0) {
    VarParity = val;
    tcgetattr(modem, &rstio);
    rstio.c_cflag &= ~(CSIZE|PARODD|PARENB);
    rstio.c_cflag |= val;
    tcsetattr(modem, TCSADRAIN, &rstio);
  }
  return(val);
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

int
OpenConnection(host, port)
char *host, *port;
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
      bcopy(hp->h_addr_list[0], &dest.sin_addr.s_addr, 4);
    } else {
      printf("unknown host: %s\n", host);
      return(-1);
    }
  }
  dest.sin_port = htons(atoi(port));
  if (dest.sin_port == 0) {
    sp = getservbyname(port, "tcp");
    if (sp) {
      dest.sin_port = sp->s_port;
    } else {
      printf("unknown service: %s\n", port);
      return(-1);
    }
  }
  LogPrintf(LOG_PHASE_BIT, "Connected to %s:%s\n", host, port);

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    return(sock);
  }
  if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
    printf("connection failed.\n");
    return(-1);
  }
  return(sock);
}

static struct termios modemios;

int
OpenModem(mode)
int mode;
{
  struct termios rstio;
  int oldflag;
  char *host, *cp, *port;
  int res;

  mbits = 0;
  if (mode & MODE_DIRECT) {
    if (isatty(0)) {
      modem = open(ctermid(NULL), O_RDWR|O_NONBLOCK);
      if (modem < 0) {
	LogPrintf(LOG_PHASE_BIT, "Open Failed %s\n", ctermid(NULL));
        return(modem);
      }
    } else if (modem < 0)
	return(modem);
  } else if (modem < 0) {
    if (strncmp(VarDevice, "/dev/", 5) == 0) {
      if ((res = uu_lock(VarBaseDevice)) != UU_LOCK_OK) {
        if (res == UU_LOCK_INUSE)
          LogPrintf(LOG_PHASE_BIT, "Modem %s is in use\n", VarDevice);
        else
          LogPrintf(LOG_PHASE_BIT, "Modem %s is in use: uu_lock: %s\n",
                    VarDevice, uu_lockerr(res));
        return(-1);
      }
      modem = open(VarDevice, O_RDWR|O_NONBLOCK);
      if (modem < 0) {
        LogPrintf(LOG_PHASE_BIT, "Open Failed %s\n", VarDevice);
        (void) uu_unlock(VarBaseDevice);
        return(modem);
      }
    } else {
      /* XXX: PPP over TCP */
      cp = index(VarDevice, ':');
      if (cp) {
	*cp = 0;
	host = VarDevice;
	port =  cp + 1;
	if (*host && *port) {
	  modem = OpenConnection(host, port);
          *cp = ':';	/* Don't destroy VarDevice */
	  if (modem < 0) return(-1);
	} else {
          *cp = ':';	/* Don't destroy VarDevice */
	  return(-1);
	}
      } else
	return(-1);
    }
  }

  /* This code gets around the problem of closing descriptor 0
   * when it should not have been closed and closing descriptor 1
   * when the telnet connection dies.  Since this program always
   * opens a descriptor for the modem in auto and direct mode,
   * having to dup the descriptor here is a fatal error.
   *
   * With the other changes I have made this should no longer happen.
   * JC
  */
  while (modem < 3)
  {
    logprintf("Duping modem fd %d\n", modem);
    modem = dup(modem);
  }

  /*
   * If we are working on tty device, change it's mode into
   * the one desired for further operation. In this implementation,
   * we assume that modem is configuted to use CTS/RTS flow control.
   */
  dev_is_modem = isatty(modem) || DEV_IS_SYNC;
  if (DEV_IS_SYNC)
    sleep(1);
  if (dev_is_modem && !DEV_IS_SYNC) {
    tcgetattr(modem, &rstio);
    modemios = rstio;
#ifdef DEBUG
    logprintf("## modem = %d\n", modem);
    logprintf("modem (get): iflag = %x, oflag = %x, cflag = %x\n",
    rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
#endif
    cfmakeraw(&rstio);
    if (VarCtsRts)
	rstio.c_cflag |= CLOCAL | CCTS_OFLOW|CRTS_IFLOW;
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
      rstio.c_cflag &= ~(CSIZE|PARODD|PARENB);
      rstio.c_cflag |= VarParity;
      if (cfsetspeed(&rstio, IntToSpeed(VarSpeed)) == -1) {
	logprintf("Unable to set modem speed (modem %d to %d)\n",
		  modem, VarSpeed);
      }
    }
    tcsetattr(modem, TCSADRAIN, &rstio);
#ifdef DEBUG
    logprintf("modem (put): iflag = %x, oflag = %x, cflag = %x\n",
    rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
#endif

    if (!(mode & MODE_DIRECT))
      if (ioctl(modem, TIOCMGET, &mbits))
	return(-1);
#ifdef DEBUG
    fprintf(stderr, "modem control = %o\n", mbits);
#endif

    oldflag = fcntl(modem, F_GETFL, 0);
    if (oldflag < 0)
       return(-1);
    (void)fcntl(modem, F_SETFL, oldflag & ~O_NONBLOCK);
  }
  StartModemTimer();

  return(modem);
}

int
ModemSpeed()
{
  struct termios rstio;

  tcgetattr(modem, &rstio);
  return(SpeedToInt(cfgetispeed(&rstio)));
}

/*
 * Put modem tty line into raw mode which is necessary in packet mode operation
 */
int
RawModem(modem)
int modem;
{
  struct termios rstio;
  int oldflag;

  if (!isatty(modem) || DEV_IS_SYNC)
    return(0);
  if (!(mode & MODE_DIRECT) && modem >= 0 && !Online) {
#ifdef DEBUG
    logprintf("mode = %d, modem = %d, mbits = %x\n", mode, modem, mbits);
#endif
#ifdef notdef
    return(-1);
#endif
  }
  tcgetattr(modem, &rstio);
  cfmakeraw(&rstio);
  if (VarCtsRts)
      rstio.c_cflag |= CLOCAL | CCTS_OFLOW|CRTS_IFLOW;
  else
      rstio.c_cflag |= CLOCAL;

  if (!(mode & MODE_DEDICATED))
    rstio.c_cflag |= HUPCL;
  tcsetattr(modem, TCSADRAIN, &rstio);
  oldflag = fcntl(modem, F_GETFL, 0);
  if (oldflag < 0)
    return(-1);
  (void)fcntl(modem, F_SETFL, oldflag | O_NONBLOCK);
#ifdef DEBUG
  oldflag = fcntl(modem, F_GETFL, 0);
  logprintf("modem (put2): iflag = %x, oflag = %x, cflag = %x\n",
   rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
  logprintf("flag = %x\n", oldflag);
#endif
  return(0);
}

void
UnrawModem(modem)
int modem;
{
  int oldflag;

  if (isatty(modem) && !DEV_IS_SYNC) {
    tcsetattr(modem, TCSAFLUSH, &modemios);
    oldflag = fcntl(modem, F_GETFL, 0);
    if (oldflag < 0)
       return;
    (void)fcntl(modem, F_SETFL, oldflag & ~O_NONBLOCK);
  }
}

void
HangupModem(flag)
int flag;
{
  struct termios tio;

  if (!isatty(modem)) {
    mbits &= ~TIOCM_DTR;
    close(modem);
    modem = -1;                  /* Mark as modem has closed */
    return;
  }

  if (modem >= 0 && Online) {
    mbits &= ~TIOCM_DTR;
#ifdef __bsdi__ /* not a POSIX way */
    ioctl(modem, TIOCMSET, &mbits);
#else
    tcgetattr(modem, &tio);
    if (cfsetspeed(&tio, B0) == -1) {
      logprintf("Unable to set modem to speed 0\n");
    }
    tcsetattr(modem, TCSANOW, &tio);
#endif
    sleep(1);
#ifdef notdef
    mbits &= ~TIOCM_CD;
#endif
  }
  /*
   * If we are working as dedicated mode, never close it
   * until we are directed to quit program.
   */
  if (modem >= 0 && (flag || !(mode & MODE_DEDICATED))) {
    ModemTimeout();			/* XXX */
    StopTimer(&ModemTimer);		/* XXX */

    /* ModemTimeout() may call DownConection() to close the modem
     * resulting in modem == 0.
    */
    if (modem >= 0)
    {
	tcflush(modem, TCIOFLUSH);
	UnrawModem(modem);
	close(modem);
    }
    modem = -1;                 /* Mark as modem has closed */
    (void) uu_unlock(VarBaseDevice);
  } else if (modem >= 0) {
    mbits |= TIOCM_DTR;
#ifndef notyet
    ioctl(modem, TIOCMSET, &mbits);
#else
    tcgetattr(modem, &ts);
    cfsetspeed(&ts, IntToSpeed(VarSpeed));
    tcsetattr(modem, TCSADRAIN, &ts);
#endif
  }
}

void
CloseModem()
{
  if (modem >= 0)
  {
      close(modem);
      modem = -1;
  }
  (void) uu_unlock(VarBaseDevice);
}

/*
 * Write to modem. Actualy, requested packets are queued, and goes out
 * to the line when ModemStartOutput() is called.
 */
void
WriteModem(pri, ptr, count)
int pri;
char *ptr;
int count;
{
  struct mbuf *bp;

  bp = mballoc(count, MB_MODEM);
  bcopy(ptr, MBUF_CTOP(bp), count);

  /* Should be NORMAL and LINK only.
   * All IP frames get here marked NORMAL.
  */
  Enqueue(&OutputQueues[pri], bp);
}

void
ModemOutput(pri, bp)
int pri;
struct mbuf *bp;
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

  for ( i = PRI_NORMAL; i <= PRI_LINK; i ++ ) {
        queue = &OutputQueues[i];
	len += queue->qlen;
  }
  return(len);

}

void
ModemStartOutput(fd)
int fd;
{
  struct mqueue *queue;
  int nb, nw;
#ifdef QDEBUG
  int i;
#endif

    if (modemout == NULL && ModemQlen() == 0)
      IpStartOutput();
  if (modemout == NULL) {
#ifdef QDEBUG
    i = PRI_LINK;
#endif
    for (queue = &OutputQueues[PRI_LINK]; queue >= OutputQueues; queue--) {
      if (queue->top) {
	modemout = Dequeue(queue);
#ifdef QDEBUG
	if (i > PRI_NORMAL) {
	  struct mqueue *q;

	  q = &OutputQueues[0];
	  logprintf("output from queue %d, normal has %d\n", i, q->qlen);
	}
	logprintf("Dequeue(%d): ", i);
#endif
	break;
      }
#ifdef QDEBUG
      i--;
#endif
    }
  }
  if (modemout) {
    nb = modemout->cnt;
    if (nb > 1600) nb = 1600;
    if (fd == 0) fd = 1;	/* XXX WTFO!  This is bogus */
    nw = write(fd, MBUF_CTOP(modemout), nb);
#ifdef QDEBUG
    logprintf("wrote: %d(%d)\n", nw, nb);
    LogDumpBuff(LOG_HDLC, "modem write", MBUF_CTOP(modemout), nb);
#endif
    if (nw > 0) {
      modemout->cnt -= nw;
      modemout->offset += nw;
      if (modemout->cnt == 0) {
	modemout = mbfree(modemout);
#ifdef QDEBUG
	logprintf(" mbfree\n");
#endif
      }
    } else if (nw < 0) {
      if (errno != EAGAIN)
        perror("modem write");
    }
  }
}

int
DialModem()
{
  char ScriptBuffer[200];
  int excode = 0;

  strncpy(ScriptBuffer, VarDialScript,sizeof(ScriptBuffer)-1);
  ScriptBuffer[sizeof(ScriptBuffer)-1] = '\0';
  if (DoChat(ScriptBuffer) > 0) {
    if ((mode & (MODE_INTER|MODE_AUTO)) == MODE_INTER)
      fprintf(stderr, "dial OK!\n");
    strncpy(ScriptBuffer, VarLoginScript,sizeof(ScriptBuffer)-1);
    if (DoChat(ScriptBuffer) > 0) {
      if ((mode & (MODE_INTER|MODE_AUTO)) == MODE_INTER)
	fprintf(stderr, "login OK!\n");
      return(1);
    } else {
      if ((mode & (MODE_INTER|MODE_AUTO)) == MODE_INTER) {
	fprintf(stderr, "login failed.\n");
        excode = EX_NOLOGIN;
      }
    }
    ModemTimeout();	/* Dummy call to check modem status */
  }
  else {
    if ((mode & (MODE_INTER|MODE_AUTO)) == MODE_INTER) {
      fprintf(stderr, "dial failed.\n");
      excode = EX_NODIAL;
    }
  }
  HangupModem(0);
  return(excode);
}

int
ShowModemStatus()
{
#ifdef TIOCOUTQ
  int nb;
#endif

  printf("device: %s  speed: ", VarDevice);
  if (DEV_IS_SYNC)
    printf("sync\n");
  else
    printf("%d\n", VarSpeed);

  switch (VarParity & CSIZE) {
  case CS7:
    printf("cs7, ");
    break;
  case CS8:
    printf("cs8, ");
    break;
  }
  if (VarParity & PARENB) {
    if (VarParity & PARODD)
      printf("odd parity, ");
    else
      printf("even parity, ");
  } else
    printf("no parity, ");

  printf("CTS/RTS %s.\n", (VarCtsRts? "on" : "off"));

#ifdef DEBUG
  printf("fd = %d, modem control = %o\n", modem, mbits);
#endif
  printf("connect count: %d\n", connect_count);
#ifdef TIOCOUTQ
  if (ioctl(modem, TIOCOUTQ, &nb) > 0)
     printf("outq: %d\n", nb);
  else
     printf("outq: ioctl probe failed.\n");
#endif
  printf("outqlen: %d\n", ModemQlen());
  printf("DialScript  = %s\n", VarDialScript);
  printf("LoginScript = %s\n", VarLoginScript);
  printf("PhoneNumber(s) = %s\n", VarPhoneList);
  return(1);
}
