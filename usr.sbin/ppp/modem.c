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
 * $Id:$
 *
 *  TODO:
 */
#include "fsm.h"
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "hdlc.h"
#include "lcp.h"
#include "modem.h"
#include "vars.h"

extern int DoChat();

static int mbits;			/* Current DCD status */
static int connect_time;		/* connection time */
static int connect_count;
static struct pppTimer ModemTimer;
static char *uucplock;

extern int uu_lock(), uu_unlock();
extern void PacketMode();

#define	Online	(mbits & TIOCM_CD)

static struct mbuf *modemout;
static struct mqueue OutputQueues[PRI_URGENT+1];
static int dev_is_modem;

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
  if (bp = queue->top) {
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

static time_t uptime;

void
DownConnection()
{
  LogPrintf(LOG_PHASE, "Disconnected!\n");
  LogPrintf(LOG_PHASE, "Connect time: %d secs\n", time(NULL) - uptime);
  LcpDown();
  connect_time = 0;
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
  if (Online)
    connect_time++;
  StartTimer(&ModemTimer);

  if (dev_is_modem) {
    ioctl(modem, TIOCMGET, &mbits);
    change = ombits ^ mbits;
    if (change & TIOCM_CD) {
      if (Online) {
        time(&uptime);
        LogPrintf(LOG_PHASE, "*Connected!\n");
        connect_count++;
        /*
         * In dedicated mode, start packet mode immediate
         * after we detected carrier.
         */
        if (mode & MODE_DEDICATED)
	  PacketMode();
      } else {
	DownConnection();
      }
    }
  } else {
    if (!Online) {
online:
      time(&uptime);
      LogPrintf(LOG_PHASE, "Connected!\n");
      mbits = TIOCM_CD;
      connect_count++;
      connect_time = 0;
    }
  }
}

void
StartModemTimer()
{
  connect_time = 0;
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
    ioctl(modem, TIOCGETA, &rstio);
    rstio.c_cflag &= ~(CSIZE|PARODD|PARENB);
    rstio.c_cflag |= val;
    ioctl(modem, TIOCSETA, &rstio);
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
  LogPrintf(LOG_PHASE, "Connected to %s:%s\n", host, port);

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

int
OpenModem(mode)
int mode;
{
  struct termios rstio;
  int oldflag;
  char *host, *cp, *port;

  mbits = 0;
  if (mode & MODE_DIRECT) {
    if (isatty(0))
      modem = open("/dev/tty", O_RDWR|O_NONBLOCK);
  } else if (modem == 0) {
    if (strncmp(VarDevice, "/dev", 4) == 0) {
      uucplock = rindex(VarDevice, '/')+1;
      if (uu_lock(uucplock) < 0) {
        fprintf(stderr, "modem is in use.\n");
        return(-1);
      }
      modem = open(VarDevice, O_RDWR|O_NONBLOCK);
      if (modem < 0) {
        perror("open modem");
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

  while (modem < 3)
    modem = dup(modem);

  /*
   * If we are working on tty device, change it's mode into
   * the one desired for further operation. In this implementation,
   * we assume that modem is configuted to use CTS/RTS flow control.
   */
  if (dev_is_modem = isatty(modem)) {
    ioctl(modem, TIOCGETA, &rstio);
#ifdef DEBUG
    logprintf("## modem = %d\n", modem);
    logprintf("modem (get): iflag = %x, oflag = %x, cflag = %x\n",
    rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
#endif
#define USE_CTSRTS
#ifdef USE_CTSRTS
    rstio.c_cflag = (CS8 | CREAD | CLOCAL | CCTS_OFLOW|CRTS_IFLOW);
#else
    rstio.c_cflag = (CS8 | CREAD | CLOCAL);
#endif
    if ((mode & MODE_DIRECT) == 0) {
      /*
       * If we are working as direct mode, don't change tty speed.
       */
      rstio.c_cflag &= ~(CSIZE|PARENB|PARODD);
      rstio.c_cflag |= VarParity;
      rstio.c_ispeed = rstio.c_ospeed = VarSpeed;
    }
    rstio.c_iflag |= (IGNBRK | ISTRIP | IGNPAR | IXON | IXOFF);
    rstio.c_iflag &= ~(BRKINT|ICRNL|IXANY|IMAXBEL);
    rstio.c_lflag = 0;

    rstio.c_oflag &= ~OPOST;
#ifdef notdef
    rstio.c_cc[VMIN] = 10;
    rstio.c_cc[VTIME] = 1;
#else
    rstio.c_cc[VMIN] = 1;
    rstio.c_cc[VTIME] = 0;
#endif
    ioctl(modem, TIOCSETA, &rstio);
#ifdef DEBUG
    logprintf("modem (put): iflag = %x, oflag = %x, cflag = %x\n",
    rstio.c_iflag, rstio.c_oflag, rstio.c_cflag);
#endif

#ifdef DEBUG
    ioctl(modem, TIOCMGET, &mbits);
    fprintf(stderr, "modem control = %o\n", mbits);
#endif

    oldflag = fcntl(modem, F_GETFL, 0);
    fcntl(modem, F_SETFL, oldflag & ~O_NDELAY);
  }
  StartModemTimer();

  return(modem);
}

int
ModemSpeed()
{
  struct termios rstio;

  ioctl(modem, TIOCGETA, &rstio);
  return(rstio.c_ispeed);
}

static struct termios modemios;

/*
 * Put modem tty line into raw mode which is necessary in packet mode operation
 */
int
RawModem(modem)
int modem;
{
  struct termios rstio;
  int oldflag;

  if (!isatty(modem))
    return(0);
  if (!(mode & MODE_DIRECT) && modem && !Online) {
#ifdef DEBUG
    logprintf("mode = %d, modem = %d, mbits = %x\n", mode, modem, mbits);
#endif
#ifdef notdef
    return(-1);
#endif
  }
  ioctl(modem, TIOCGETA, &rstio);
  modemios = rstio;
  rstio.c_cflag &= ~(CSIZE|PARENB|PARODD);
  rstio.c_cflag |= CS8;
  rstio.c_iflag &= ~(ISTRIP|IXON|IXOFF|BRKINT|ICRNL|INLCR);
  ioctl(modem, TIOCSETA, &rstio);
  oldflag = fcntl(modem, F_GETFL, 0);
  fcntl(modem, F_SETFL, oldflag | O_NDELAY);
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

  if (isatty(modem)) {
    ioctl(modem, TIOCSETA, &modemios);
    oldflag = fcntl(modem, F_GETFL, 0);
    fcntl(modem, F_SETFL, oldflag & ~O_NDELAY);
  }
}

void
HangupModem(flag)
int flag;
{
  int n = 0;

  if (!isatty(modem)) {
    mbits &= ~TIOCM_DTR;
    close(modem);
    modem = 0;			/* Mark as modem has closed */
    return;
  }

  if (Online) {
    mbits &= ~TIOCM_DTR;
    ioctl(modem, TIOCMSET, &mbits);
    sleep(1);
#ifdef notdef
    mbits &= ~TIOCM_CD;
#endif
  }
  /*
   * If we are working as dedicated mode, never close it
   * until we are directed to quit program.
   */
  if (modem && (flag || !(mode & MODE_DEDICATED))) {
    ModemTimeout();			/* XXX */
    StopTimer(&ModemTimer);		/* XXX */
    ioctl(modem, TIOCFLUSH, &n);
    UnrawModem(modem);
    close(modem);
    (void) uu_unlock(uucplock);
    modem = 0;			/* Mark as modem has closed */
  } else {
    mbits |= TIOCM_DTR;
    ioctl(modem, TIOCMSET, &mbits);
  }
}

CloseModem()
{
  close(modem);
  modem = 0;
  (void) uu_unlock(uucplock);
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
  Enqueue(&OutputQueues[pri], bp);
}

int
ModemQlen()
{
  struct mbuf *bp;
  int len = 0;
  int i;

  for (i = PRI_NORMAL; i <= PRI_URGENT; i++) {
    for (bp = OutputQueues[i].top; bp; bp = bp->pnext)
      len++;
  }

  return(len);
}

void
ModemStartOutput(fd)
int fd;
{
  struct mqueue *queue;
  int nb, nw, i;

  if (modemout == NULL) {
    i = 0;
    for (queue = &OutputQueues[PRI_URGENT]; queue >= OutputQueues; queue--) {
      if (queue->top) {
	modemout = Dequeue(queue);
#ifdef QDEBUG
	if (i < 2) {
	  struct mqueue *q;

	  q = &OutputQueues[0];
	  logprintf("output from queue %d, normal has %d\n", i, q->qlen);
	}
	logprintf("Dequeue(%d): ", i);
#endif
	break;
      }
      i++;
    }
  }
  if (modemout) {
    nb = modemout->cnt;
    if (nb > 300) nb = 300;
    if (fd == 0) fd = 1;
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
    } else if (nw < 0)
      perror("modem write");
  }
}

int
DialModem()
{
  char ScriptBuffer[200];

  strcpy(ScriptBuffer, VarDialScript);
  if (DoChat(ScriptBuffer) > 0) {
    fprintf(stderr, "dial OK!\n");
    strcpy(ScriptBuffer, VarLoginScript);
    if (DoChat(ScriptBuffer) > 0) {
      fprintf(stderr, "login OK!\n");
      return(1);
    } else {
      fprintf(stderr, "login failed.\n");
    }
    ModemTimeout();	/* Dummy call to check modem status */
  }
  else
    fprintf(stderr, "dial failed.\n");
  return(0);
}

int
ShowModemStatus()
{
  int nb;

  printf("device: %s  speed: %d\n", VarDevice, VarSpeed);
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
      printf("odd parity\n");
    else
      printf("even parity\n");
  } else
    printf("none parity\n");
#ifdef DEBUG
  printf("fd = %d, modem control = %o\n", modem, mbits);
#endif
  printf("connect count: %d\n", connect_count);
  ioctl(modem, TIOCOUTQ, &nb);
  printf("outq: %d\n", nb);
  printf("DialScript  = %s\n", VarDialScript);
  printf("LoginScript = %s\n", VarLoginScript);
  printf("PhoneNumber = %s\n", VarPhone);
  return(1);
}
