/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: tty.c,v 1.6 1999/05/18 01:37:46 brian Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#include <util.h>
#else
#include <libutil.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "id.h"
#include "sync.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "slcompress.h"
#include "iplist.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "physical.h"
#include "mp.h"
#ifndef NORADIUS
#include "radius.h"
#endif
#include "chat.h"
#include "command.h"
#include "bundle.h"
#include "prompt.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "tty.h"

#define	Online(dev)	((dev)->mbits & TIOCM_CD)

struct ttydevice {
  struct device dev;		/* What struct physical knows about */
  struct pppTimer Timer;	/* CD checks */
  int mbits;			/* Current DCD status */
  struct termios ios;		/* To be able to reset from raw mode */
};

#define device2tty(d) ((d)->type == TTY_DEVICE ? (struct ttydevice *)d : NULL)

/*
 * tty_Timeout() watches the DCD signal and mentions it if it's status
 * changes.
 */
static void
tty_Timeout(void *data)
{
  struct physical *p = data;
  struct ttydevice *dev = device2tty(p->handler);
  int ombits, change;

  timer_Stop(&dev->Timer);
  dev->Timer.load = SECTICKS;		/* Once a second please */
  timer_Start(&dev->Timer);
  ombits = dev->mbits;

  if (p->fd >= 0) {
    if (ioctl(p->fd, TIOCMGET, &dev->mbits) < 0) {
      log_Printf(LogPHASE, "%s: ioctl error (%s)!\n", p->link.name,
                 strerror(errno));
      datalink_Down(p->dl, CLOSE_NORMAL);
      timer_Stop(&dev->Timer);
      return;
    }
  } else
    dev->mbits = 0;

  if (ombits == -1) {
    /* First time looking for carrier */
    if (Online(dev))
      log_Printf(LogDEBUG, "%s: %s: CD detected\n", p->link.name, p->name.full);
    else if (p->cfg.cd.required) {
      log_Printf(LogPHASE, "%s: %s: Required CD not detected\n",
                 p->link.name, p->name.full);
      datalink_Down(p->dl, CLOSE_NORMAL);
    } else {
      log_Printf(LogPHASE, "%s: %s doesn't support CD\n",
                 p->link.name, p->name.full);
      timer_Stop(&dev->Timer);
      dev->mbits = TIOCM_CD;
    }
  } else {
    change = ombits ^ dev->mbits;
    if (change & TIOCM_CD) {
      if (dev->mbits & TIOCM_CD)
        log_Printf(LogDEBUG, "%s: offline -> online\n", p->link.name);
      else {
        log_Printf(LogDEBUG, "%s: online -> offline\n", p->link.name);
        log_Printf(LogPHASE, "%s: Carrier lost\n", p->link.name);
        datalink_Down(p->dl, CLOSE_NORMAL);
        timer_Stop(&dev->Timer);
      }
    } else
      log_Printf(LogDEBUG, "%s: Still %sline\n", p->link.name,
                 Online(dev) ? "on" : "off");
  }
}

static void
tty_StartTimer(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  timer_Stop(&dev->Timer);
  dev->Timer.load = SECTICKS * p->cfg.cd.delay;
  dev->Timer.func = tty_Timeout;
  dev->Timer.name = "tty CD";
  dev->Timer.arg = p;
  log_Printf(LogDEBUG, "%s: Using tty_Timeout [%p]\n",
             p->link.name, tty_Timeout);
  dev->mbits = -1;		/* So we know it's the first time */
  timer_Start(&dev->Timer);
}

static int
tty_Raw(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);
  struct termios ios;
  int oldflag;

  if (physical_IsSync(p))
    return 1;

  log_Printf(LogDEBUG, "%s: Entering physical_Raw\n", p->link.name);

  if (p->type != PHYS_DIRECT && p->fd >= 0 && !Online(dev))
    log_Printf(LogDEBUG, "%s: Raw: descriptor = %d, mbits = %x\n",
              p->link.name, p->fd, dev->mbits);

  tcgetattr(p->fd, &ios);
  cfmakeraw(&ios);
  if (p->cfg.rts_cts)
    ios.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else
    ios.c_cflag |= CLOCAL;

  if (p->type != PHYS_DEDICATED)
    ios.c_cflag |= HUPCL;

  tcsetattr(p->fd, TCSANOW, &ios);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0)
    return 0;
  fcntl(p->fd, F_SETFL, oldflag | O_NONBLOCK);

  if (ioctl(p->fd, TIOCMGET, &dev->mbits) == 0)
    tty_StartTimer(p);

  return 1;
}

static void
tty_Offline(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  if (p->fd >= 0) {
    timer_Stop(&dev->Timer);
    dev->mbits &= ~TIOCM_DTR;
    if (Online(dev)) {
      struct termios tio;

      tcgetattr(p->fd, &tio);
      if (cfsetspeed(&tio, B0) == -1)
        log_Printf(LogWARN, "%s: Unable to set physical to speed 0\n",
                   p->link.name);
      else
        tcsetattr(p->fd, TCSANOW, &tio);
    }
  }
}

static void
tty_Cooked(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);
  int oldflag;

  tcflush(p->fd, TCIOFLUSH);
  if (!physical_IsSync(p)) {
    tcsetattr(p->fd, TCSAFLUSH, &dev->ios);
    oldflag = fcntl(p->fd, F_GETFL, 0);
    if (oldflag == 0)
      fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);
  }
}

static void
tty_StopTimer(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  timer_Stop(&dev->Timer);
}

static void
tty_Free(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  free(dev);
}

static int
tty_Speed(struct physical *p)
{
  struct termios ios;

  if (tcgetattr(p->fd, &ios) == -1)
    return 0;

  return SpeedToInt(cfgetispeed(&ios));
}

static const char *
tty_OpenInfo(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);
  static char buf[13];

  if (Online(dev))
    strcpy(buf, "with");
  else
    strcpy(buf, "no");
  strcat(buf, " carrier");
  return buf;
}

static void
tty_device2iov(struct physical *p, struct iovec *iov, int *niov,
               int maxiov, pid_t newpid)
{
  struct ttydevice *dev = p ? device2tty(p->handler) : NULL;

  iov[*niov].iov_base = p ? p->handler : malloc(sizeof(struct ttydevice));
  iov[*niov].iov_len = sizeof(struct ttydevice);
  (*niov)++;

  if (dev->Timer.state != TIMER_STOPPED) {
    timer_Stop(&dev->Timer);
    dev->Timer.state = TIMER_RUNNING;
  }
}

static struct device basettydevice = {
  TTY_DEVICE,
  "tty",
  tty_Raw,
  tty_Offline,
  tty_Cooked,
  tty_StopTimer,
  tty_Free,
  NULL,
  NULL,
  tty_device2iov,
  tty_Speed,
  tty_OpenInfo
};

struct device *
tty_iov2device(int type, struct physical *p, struct iovec *iov, int *niov,
               int maxiov)
{
  if (type == TTY_DEVICE) {
    struct ttydevice *dev = (struct ttydevice *)iov[(*niov)++].iov_base;

    /* Refresh function pointers etc */
    memcpy(&dev->dev, &basettydevice, sizeof dev->dev);

    physical_SetupStack(p, dev->dev.name, PHYSICAL_NOFORCE);
    if (dev->Timer.state != TIMER_STOPPED) {
      dev->Timer.state = TIMER_STOPPED;
      tty_StartTimer(p);
    }
    return &dev->dev;
  }

  return NULL;
}

struct device *
tty_Create(struct physical *p)
{
  struct ttydevice *dev;
  struct termios ios;
  int oldflag;

  if (p->fd < 0 || !isatty(p->fd))
    /* Don't want this */
    return NULL;

  if (*p->name.full == '\0') {
    physical_SetDevice(p, ttyname(p->fd));
    log_Printf(LogDEBUG, "%s: Input is a tty (%s)\n",
               p->link.name, p->name.full);
  } else
    log_Printf(LogDEBUG, "%s: Opened %s\n", p->link.name, p->name.full);

  /* We're gonna return a ttydevice (unless something goes horribly wrong) */

  if ((dev = malloc(sizeof *dev)) == NULL) {
    /* Complete failure - parent doesn't continue trying to ``create'' */
    close(p->fd);
    p->fd = -1;
    return NULL;
  }

  memcpy(&dev->dev, &basettydevice, sizeof dev->dev);
  memset(&dev->Timer, '\0', sizeof dev->Timer);
  tcgetattr(p->fd, &ios);
  dev->ios = ios;

  log_Printf(LogDEBUG, "%s: tty_Create: physical (get): fd = %d,"
             " iflag = %lx, oflag = %lx, cflag = %lx\n", p->link.name, p->fd,
             (u_long)ios.c_iflag, (u_long)ios.c_oflag, (u_long)ios.c_cflag);

  cfmakeraw(&ios);
  if (p->cfg.rts_cts)
    ios.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
  else {
    ios.c_cflag |= CLOCAL;
    ios.c_iflag |= IXOFF;
  }
  ios.c_iflag |= IXON;
  if (p->type != PHYS_DEDICATED)
    ios.c_cflag |= HUPCL;

  if (p->type != PHYS_DIRECT) {
      /* Change tty speed when we're not in -direct mode */
      ios.c_cflag &= ~(CSIZE | PARODD | PARENB);
      ios.c_cflag |= p->cfg.parity;
      if (cfsetspeed(&ios, IntToSpeed(p->cfg.speed)) == -1)
	log_Printf(LogWARN, "%s: %s: Unable to set speed to %d\n",
		  p->link.name, p->name.full, p->cfg.speed);
  }
  tcsetattr(p->fd, TCSADRAIN, &ios);
  log_Printf(LogDEBUG, "%s: physical (put): iflag = %lx, oflag = %lx, "
            "cflag = %lx\n", p->link.name, (u_long)ios.c_iflag,
            (u_long)ios.c_oflag, (u_long)ios.c_cflag);

  if (ioctl(p->fd, TIOCMGET, &dev->mbits) == -1) {
    if (p->type != PHYS_DIRECT) {
      /* Complete failure - parent doesn't continue trying to ``create'' */

      log_Printf(LogWARN, "%s: Open: Cannot get physical status: %s\n",
                 p->link.name, strerror(errno));
      tty_Cooked(p);
      close(p->fd);
      p->fd = -1;
      return NULL;
    } else
      dev->mbits = TIOCM_CD;
  }
  log_Printf(LogDEBUG, "%s: Open: physical control = %o\n",
             p->link.name, dev->mbits);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0) {
    /* Complete failure - parent doesn't continue trying to ``create'' */

    log_Printf(LogWARN, "%s: Open: Cannot get physical flags: %s\n",
               p->link.name, strerror(errno));
    tty_Cooked(p);
    close(p->fd);
    p->fd = -1;
    return NULL;
  } else
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);

  physical_SetupStack(p, dev->dev.name, PHYSICAL_NOFORCE);

  return &dev->dev;
}
