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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/un.h>
#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/ioctl.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"
#include "mp.h"
#include "chat.h"
#include "auth.h"
#include "chap.h"
#include "cbcp.h"
#include "datalink.h"
#include "main.h"
#include "tty.h"

#if defined(__mac68k__) || defined(__macppc__)
#undef	CRTS_IFLOW
#undef	CCTS_OFLOW
#define	CRTS_IFLOW	CDTRCTS
#define	CCTS_OFLOW	CDTRCTS
#endif

#define	Online(dev)	((dev)->mbits & TIOCM_CD)

struct ttydevice {
  struct device dev;		/* What struct physical knows about */
  struct pppTimer Timer;	/* CD checks */
  int mbits;			/* Current DCD status */
  int carrier_seconds;		/* seconds before CD is *required* */
  struct termios ios;		/* To be able to reset from raw mode */
};

#define device2tty(d) ((d)->type == TTY_DEVICE ? (struct ttydevice *)d : NULL)

int
tty_DeviceSize(void)
{
  return sizeof(struct ttydevice);
}

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
      /* we must be a pty ? */
      if (p->cfg.cd.necessity != CD_DEFAULT)
        log_Printf(LogWARN, "%s: Carrier ioctl not supported, "
                   "using ``set cd off''\n", p->link.name);
      timer_Stop(&dev->Timer);
      dev->mbits = TIOCM_CD;
      return;
    }
  } else
    dev->mbits = 0;

  if (ombits == -1) {
    /* First time looking for carrier */
    if (Online(dev))
      log_Printf(LogPHASE, "%s: %s: CD detected\n", p->link.name, p->name.full);
    else if (++dev->carrier_seconds >= dev->dev.cd.delay) {
      if (dev->dev.cd.necessity == CD_REQUIRED)
        log_Printf(LogPHASE, "%s: %s: Required CD not detected\n",
                   p->link.name, p->name.full);
      else {
        log_Printf(LogPHASE, "%s: %s doesn't support CD\n",
                   p->link.name, p->name.full);
        dev->mbits = TIOCM_CD;		/* Dodgy null-modem cable ? */
      }
      timer_Stop(&dev->Timer);
      /* tty_AwaitCarrier() will notice */
    } else {
      /* Keep waiting */
      log_Printf(LogDEBUG, "%s: %s: Still no carrier (%d/%d)\n",
                 p->link.name, p->name.full, dev->carrier_seconds,
                 dev->dev.cd.delay);
      dev->mbits = -1;
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
  dev->Timer.load = SECTICKS;
  dev->Timer.func = tty_Timeout;
  dev->Timer.name = "tty CD";
  dev->Timer.arg = p;
  log_Printf(LogDEBUG, "%s: Using tty_Timeout [%p]\n",
             p->link.name, tty_Timeout);
  timer_Start(&dev->Timer);
}

static int
tty_AwaitCarrier(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  if (dev->dev.cd.necessity == CD_NOTREQUIRED || physical_IsSync(p))
    return CARRIER_OK;

  if (dev->mbits == -1) {
    if (dev->Timer.state == TIMER_STOPPED) {
      dev->carrier_seconds = 0;
      tty_StartTimer(p);
    }
    return CARRIER_PENDING;			/* Not yet ! */
  }

  return Online(dev) ? CARRIER_OK : CARRIER_LOST;
}

static int
tty_Raw(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);
  struct termios ios;
  int oldflag;

  log_Printf(LogDEBUG, "%s: Entering tty_Raw\n", p->link.name);

  if (p->type != PHYS_DIRECT && p->fd >= 0 && !Online(dev))
    log_Printf(LogDEBUG, "%s: Raw: descriptor = %d, mbits = %x\n",
              p->link.name, p->fd, dev->mbits);

  if (!physical_IsSync(p)) {
    tcgetattr(p->fd, &ios);
    cfmakeraw(&ios);
    if (p->cfg.rts_cts)
      ios.c_cflag |= CLOCAL | CCTS_OFLOW | CRTS_IFLOW;
    else
      ios.c_cflag |= CLOCAL;

    if (p->type != PHYS_DEDICATED)
      ios.c_cflag |= HUPCL;

    if (tcsetattr(p->fd, TCSANOW, &ios) == -1)
      log_Printf(LogWARN, "%s: tcsetattr: Failed configuring device\n",
                 p->link.name);
  }

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0)
    return 0;
  fcntl(p->fd, F_SETFL, oldflag | O_NONBLOCK);

  return 1;
}

static void
tty_Offline(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);

  if (p->fd >= 0) {
    timer_Stop(&dev->Timer);
    dev->mbits &= ~TIOCM_DTR;	/* XXX: Hmm, what's this supposed to do ? */
    if (Online(dev)) {
      struct termios tio;

      tcgetattr(p->fd, &tio);
      if (cfsetspeed(&tio, B0) == -1 || tcsetattr(p->fd, TCSANOW, &tio) == -1)
        log_Printf(LogWARN, "%s: Unable to set physical to speed 0\n",
                   p->link.name);
    }
  }
}

static void
tty_Cooked(struct physical *p)
{
  struct ttydevice *dev = device2tty(p->handler);
  int oldflag;

  tty_Offline(p);	/* In case of emergency close()s */

  tcflush(p->fd, TCIOFLUSH);

  if (!physical_IsSync(p) && tcsetattr(p->fd, TCSAFLUSH, &dev->ios) == -1)
    log_Printf(LogWARN, "%s: tcsetattr: Unable to restore device settings\n",
               p->link.name);

  if ((oldflag = fcntl(p->fd, F_GETFL, 0)) != -1)
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);
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

  tty_Offline(p);	/* In case of emergency close()s */
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
tty_device2iov(struct device *d, struct iovec *iov, int *niov,
               int maxiov, int *auxfd, int *nauxfd)
{
  struct ttydevice *dev = device2tty(d);
  int sz = physical_MaxDeviceSize();

  iov[*niov].iov_base = realloc(d, sz);
  if (iov[*niov].iov_base == NULL) {
    log_Printf(LogALERT, "Failed to allocate memory: %d\n", sz);
    AbortProgram(EX_OSERR);
  }
  iov[*niov].iov_len = sz;
  (*niov)++;

  if (dev->Timer.state != TIMER_STOPPED) {
    timer_Stop(&dev->Timer);
    dev->Timer.state = TIMER_RUNNING;
  }
}

static struct device basettydevice = {
  TTY_DEVICE,
  "tty",
  { CD_VARIABLE, DEF_TTYCDDELAY },
  tty_AwaitCarrier,
  NULL,
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
               int maxiov, int *auxfd, int *nauxfd)
{
  if (type == TTY_DEVICE) {
    struct ttydevice *dev = (struct ttydevice *)iov[(*niov)++].iov_base;

    dev = realloc(dev, sizeof *dev);	/* Reduce to the correct size */
    if (dev == NULL) {
      log_Printf(LogALERT, "Failed to allocate memory: %d\n",
                 (int)(sizeof *dev));
      AbortProgram(EX_OSERR);
    }

    /* Refresh function pointers etc */
    memcpy(&dev->dev, &basettydevice, sizeof dev->dev);

    physical_SetupStack(p, dev->dev.name, PHYSICAL_NOFORCE);
    if (dev->Timer.state != TIMER_STOPPED) {
      dev->Timer.state = TIMER_STOPPED;
      p->handler = &dev->dev;		/* For the benefit of StartTimer */
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
  dev->mbits = -1;
  tcgetattr(p->fd, &ios);
  dev->ios = ios;

  if (p->cfg.cd.necessity != CD_DEFAULT)
    /* Any override is ok for the tty device */
    dev->dev.cd = p->cfg.cd;

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

  if (tcsetattr(p->fd, TCSADRAIN, &ios) == -1) {
    log_Printf(LogWARN, "%s: tcsetattr: Failed configuring device\n",
               p->link.name);
    if (p->type != PHYS_DIRECT && p->cfg.speed > 115200)
      log_Printf(LogWARN, "%.*s             Perhaps the speed is unsupported\n",
                 (int)strlen(p->link.name), "");
  }

  log_Printf(LogDEBUG, "%s: physical (put): iflag = %lx, oflag = %lx, "
            "cflag = %lx\n", p->link.name, (u_long)ios.c_iflag,
            (u_long)ios.c_oflag, (u_long)ios.c_cflag);

  oldflag = fcntl(p->fd, F_GETFL, 0);
  if (oldflag < 0) {
    /* Complete failure - parent doesn't continue trying to ``create'' */

    log_Printf(LogWARN, "%s: Open: Cannot get physical flags: %s\n",
               p->link.name, strerror(errno));
    tty_Cooked(p);
    close(p->fd);
    p->fd = -1;
    free(dev);
    return NULL;
  } else
    fcntl(p->fd, F_SETFL, oldflag & ~O_NONBLOCK);

  physical_SetupStack(p, dev->dev.name, PHYSICAL_NOFORCE);

  return &dev->dev;
}
